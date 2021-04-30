// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <thread>
#include <iostream>

#include <sqlite3.h>
#include <torch/torch.h>
#include <model/address.h>
#include <import/region_reader.h>
#include <geocoder/ner/geomodel.h>

#include "train.h"
#include "dataset.h"

// hyperparameters
const size_t epochs_num = 2;
const size_t batch_size = 32;
const int64_t max_samples = 1'000'000;
const size_t threads_num = std::thread::hardware_concurrency();


size_t params_count(sentio::geocoder::geomodel const& model)
{
  size_t params_count = 0;
  for (auto const& pset: model->parameters()) {
    size_t pset_count = 1;
    for (auto const& dim: pset.sizes()) {
      pset_count *= dim;
    }
    params_count += pset_count;
  }
  std::cout << "model trainable params count: " 
            << params_count << std::endl;
  std::cout << "training using device: " 
            << torch::DeviceTypeName(model->device())
            << std::endl;
  return params_count;
}

void do_train(
  sentio::geocoder::geomodel& model, 
  sentio::geomodel::address_dataset& train_data, 
  size_t epochs_count)
{
  auto train_loader = torch::data::make_data_loader<
    torch::data::samplers::RandomSampler>(
      train_data.map(torch::data::transforms::Stack<>()), 
      torch::data::DataLoaderOptions()
        .batch_size(batch_size)
        .workers(threads_num));

  model->train(true);
  auto criterion = torch::nn::CrossEntropyLoss();
  torch::optim::Adam optimizer(model->parameters());

  for (size_t epoch = 1; epoch <= epochs_count; ++epoch) {
    
    float  loss_val = 0.0;
    size_t loss_count = 0;
    size_t batch_index = 0;
    size_t accum_count = 1;
    size_t batches_count = size_t(
      std::ceil(
        float(train_data.size().value()) / 
        float(batch_size)));

    auto before = std::chrono::high_resolution_clock::now();
    for (auto& batch: *train_loader) {
      model->zero_grad();
      
      torch::Tensor prediction = model(batch.data).transpose(1, 2);
      torch::Tensor loss = criterion->forward(prediction, batch.target);

      loss.backward();
      optimizer.step();
      
      loss_val += loss.item<float>();
      ++loss_count;

      if (++batch_index % accum_count == 0) {
        float progress = (float(batch_index) / float(batches_count)) * 100;
        loss_val /= accum_count;
        std::cout << "\033[A\33[2K\rtraining epoch " << epoch << " (" 
                  << batch_index << "/" << batches_count << " | "
                  << progress << "%)" << " / loss: " 
                  << std::setprecision(5) << loss_val 
                  << std::setprecision(2) << std::endl;
        loss_val = 0.0;
      }
    }
    
    auto after = std::chrono::high_resolution_clock::now();
    std::cout << "epoch trained in "
              << sentio::geomodel::print_duration(after - before)
              << std::endl << std::endl << std::endl;
  }

  model->train(false);
}

float validate(
  sentio::geocoder::geomodel& model,
  sentio::geomodel::address_dataset& test_data)
{
  using namespace sentio::geomodel;

  size_t report_frequency = 1000;
  size_t total = 0, correct = 0, sampleno = 0;
  auto testsize = test_data.size().value();
  std::chrono::milliseconds elapsed(0);
  std::chrono::milliseconds total_elapsed(0);  
  for (auto const& sample: test_data) {
    ++sampleno;

    std::string address(sample.text());

    auto tstart = std::chrono::high_resolution_clock::now();
    auto prediction = model(address);
    
    auto truth = sample.output();
    truth = truth.narrow(0, 0, prediction.size(0)); // trim from batch size to 
    truth = torch::argmax(truth, 1, true);
    auto tend = std::chrono::high_resolution_clock::now();
    elapsed = (elapsed + std::chrono::duration_cast<std::chrono::milliseconds>(tend - tstart));

    auto equal_vals = torch::eq(prediction, truth);
    for (int64_t i = 0; i < equal_vals.size(0); ++i) {
      ++total;

      if (equal_vals[i].item().toBool()) {
        ++correct;
      }

      if (total % report_frequency == 0) {
        auto time_per_pass = elapsed.count() / report_frequency;

        std::cout << "\033[A\33[2K\rtesting accuracy... " 
                  << sampleno << "/" << testsize
                  << " | acc: " << ((float(correct) / float(total)) * 100)
                  << "% | progress: " <<  ((float(sampleno) / float(testsize)) * 100)
                  << "% | avg pass time: " << time_per_pass << "ms" << std::endl;
        total_elapsed += elapsed;
        elapsed = std::chrono::milliseconds::zero();
      }
    }
  }

  float accuracy = (float(correct) / float(total)) * 100;
  std::cout << "test samples: " << total << std::endl;
  std::cout << "correct prediction: " << correct << std::endl;
  std::cout << "incorrect predictions: " << (total - correct) << std::endl;
  std::cout << "test dataset accuracy: " << accuracy << "%" << std::endl;
  std::cout << "testing time: " << print_duration(total_elapsed) << std::endl;

  return accuracy;
}


void manual_test_samples(
  sentio::geocoder::geomodel& model)
{
  std::vector<std::string> rep {
    "wiejska 35a bialystok",
    "jana pawła II 9",
    "Sienkiewicza 1/1 bialystok",
    "mickiewicza 18b bialystok",
    "transportowa 2d/157, białystok, 15-399",
    "transportowa 2d / 157, białystok, 15-399",
    "transportowa 2d m157, białystok, 15-399",
    "transportowa 2d mieszkanie 157, białystok, 15-399",
    "wiejska 18B m30"
  };

  for (auto const& example: rep) {
    std::cout << "example: " << example << std::endl;
    auto eval = model(example);
    std::cout << "eval output: " << std::endl 
              << eval.view({1, eval.size(0)}) << std::endl;
  }
}

void test_restore_model(std::string const& outputmodel)
{
  sentio::geocoder::geomodel restored(outputmodel);
  std::cout << "inference model device: " 
            << torch::DeviceTypeName(restored->device())
            << std::endl;
  std::cout << "output from a restored model: " << std::endl;
  auto res_output = restored("transportowa 2d bialystok 15-399");
  std::cout << res_output << std::endl;
  
  size_t count = 0;
  size_t iter_count = 1000;
  std::cout << "measuring cpu inference on " 
            << iter_count << " iterations" 
            << std::endl;

  auto start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < iter_count; ++i) {
    auto res_output = restored("transportowa 2d bialystok 15-399");
    if (res_output.defined()) {
      ++count;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  std::cout << "ran " << count << " inference operatins in "
            << elapsed << "ms, averaging " 
            << (elapsed / count) << "ms per pass." 
            << std::endl;
}

int main(int argc, const char** argv)
{
  using namespace sentio::geomodel;

  std::cout << "Sentio Trasa Geomodel Training Tool 1.0" << std::endl;

  if (argc != 3) {
    std::cerr << "usage: " << argv[0] 
              << " <region-input-csv> <output-model-file>" 
              << std::endl;
    return 1;
  }

  std::cout << std::fixed;
  const std::string inputcsv(argv[1]);
  const std::string outputmodel(argv[2]);

  std::cout << "dataset path: " << inputcsv << std::endl;
  std::cout << "output model path: " << outputmodel << std::endl;

  torch::DeviceType device = torch::cuda::is_available()
    ? torch::DeviceType::CUDA
    : torch::DeviceType::CPU;
  
  
  auto [trainds, testds] = import_datasets(inputcsv, 0.7, device, max_samples);
  
  std::cout << "batch size: " << batch_size << std::endl;
  std::cout << "train dataset size: " << trainds.size().value() << std::endl;
  std::cout << "test dataset size: " << testds.size().value() << std::endl;

  // model
  sentio::geocoder::geomodel model(device);

  // print some stats
  params_count(model);

  auto tstart = std::chrono::high_resolution_clock::now();
  std::cout << "training started..." 
            << std::endl << std::endl
            << std::endl;

  // run training loop
  do_train(model, testds, epochs_num);

  // save trained model: 
  std::cout << "saving trained model to: " << outputmodel << "...";
  model.save_snapshot(outputmodel);
  std::cout << std::endl;

  // run validation loop
  validate(model, testds);

  // various tests:
  manual_test_samples(model);
  test_restore_model(outputmodel);

  auto tend = std::chrono::high_resolution_clock::now();
  std::cout << "training completed in " 
            << print_duration(tend - tstart)
            << " for " << epochs_num 
            << " epochs." << std::endl;

  return 0;
}
