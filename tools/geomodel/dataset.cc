// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <random>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <functional>
#include <unordered_map>

#include <boost/locale.hpp>
#include <import/region_reader.h>
#include <geocoder/ner/geomodel.h>

#include "dataset.h"
#include "transform.h"

namespace sentio::geomodel
{

sample::sample() {}

sample::sample(
  chars_type chars, 
  labels_type labels, 
  torch::DeviceType device,
  std::optional<model::building> underlying)
  : device_(device)
  , chars_(std::move(chars))
  , labels_(std::move(labels))
  , building_(std::move(underlying))
{
  if (chars_.size() != labels_.size()) {
    throw std::invalid_argument(
      "chars and labels length must be equal");
  }
}

torch::Tensor sample::input() const
{ 
  torch::Tensor output = torch::zeros({ 
    static_cast<int64_t>(length()),
    static_cast<int64_t>(geocoder::geomodel_impl::alphabet_size())
  }, torch::TensorOptions()
      .device(device_)
      .dtype(torch::kFloat));

  size_t row = 0;
  for (auto const& c: chars()) {
    output[row][c] = 1;
    ++row;
  }
  return output;
}

void sample::pad_to(size_t len)
{
  using namespace sentio::geocoder;
  for (auto i = length(); i <= len; ++i) {
    chars_.push_back(geocoder::geomodel_impl::encode_char(L'•'));
    labels_.push_back(address_label::unknown);
  }
}

std::string sample::text() const
{
  if (building_.has_value()) {
    std::stringstream ss;
    ss << building_->street << " ";
    ss << building_->number << " ";
    ss << building_->city << " ";
    ss << building_->zipcode;
    return ss.str();
  } else {
    return std::string();
  }
}

address_dataset::element_iterator address_dataset::begin() const 
{ return samples_.begin(); }

address_dataset::element_iterator address_dataset::end() const 
{ return samples_.end(); }

torch::Tensor sample::output() const
{ 
  torch::Tensor output = torch::zeros({ 
    static_cast<int64_t>(length()),
    static_cast<int64_t>(geocoder::labels_count())
  }, torch::TensorOptions()
      .device(device_)
      .dtype(torch::kFloat));

  size_t row = 0;
  for (auto const& lbl: labels()) {
    output[row][static_cast<int32_t>(lbl)] = 1;
    ++row;
  } 
  
  return output;
}

std::vector<geocoder::address_label> const& 
sample::labels() const
{ return labels_; }

std::vector<int32_t> const& sample::chars() const
{ return chars_; }

size_t sample::length() const
{ return chars_.size(); }

std::vector<sample> import_samples(
  std::string path, 
  torch::DeviceType device, 
  int64_t /*max_samples*/)
{
  std::vector<sample> output;
  import::region_addressbook addrbook(path);

  using permutations = std::vector<sample>;
  using building = model::building;

  auto _1 = std::placeholders::_1;
  std::vector<std::function<permutations(building const&)>> transforms {
    std::bind(make_plain, _1, device)
  };

  int64_t counter = 0;
  const int64_t report_every = 100;

  // for each building, run all transforms
  // and insert all generated variants of all 
  // transforms
  std::cout << std::endl << std::endl;
  for (auto const& bldg: addrbook) {
    if (++counter % report_every == 0) {
      std::cout << "\033[A\33[2K\rimporting dataset: " 
                << counter << " samples" << std::endl; 
    }

    for (auto const& transform: transforms) {
      for (auto& s: transform(bldg)) {
        output.push_back(std::move(s));
      }
    }
  }

  std::cout << "import complete." << std::endl;

  return output;
}

address_dataset::address_dataset(std::string filename, torch::DeviceType device)
  : address_dataset(import_samples(std::move(filename)), device)
{
}

address_dataset::address_dataset(std::vector<sample> samples, torch::DeviceType device)
  : samples_(std::move(samples))
  , devicetype_(device)
  , maxlength_(std::max_element(samples_.begin(), samples_.end(), 
      [](auto const& left, auto const& right) { 
        return left.length() < right.length(); 
      })->length())
{
  // normalize dimentions of all addresses, to make batching possible
  std::for_each(samples_.begin(), samples_.end(), 
    [this](auto& s) { s.pad_to(maxlength_); });
}

torch::data::Example<> address_dataset::get(size_t index)
{ 
  return { 
    samples_[index].input(), 
    torch::argmax(samples_[index].output(), 1, false) 
  }; 
}

torch::optional<size_t> address_dataset::size() const
{ return samples_.size(); }

// todo: implement this as a custom torch data loader so that
// available memory isn't our limiting factor and we could truly train 
// the model on the entire 7.5 mln addresses we have in the database.
std::pair<address_dataset, address_dataset> import_datasets(
  std::string filename, float ratio, torch::DeviceType device, int64_t max_samples)
{
  std::random_device rd;
  std::mt19937 g(rd());

  // here we intentionally import the entire dataset first, then trim it to the 
  // max size, because we want to shuffle it first, to get more randomness as the
  // dataset is sliced for training. Otherwise we might have one dominant city or
  // region of the country that is not representative of the entire country.
  std::vector<sample> all_samples = import_samples(filename, device, max_samples);
  std::shuffle(all_samples.begin(), all_samples.end(), g); // randomize order

  if (max_samples != -1 && 
      all_samples.size() > static_cast<size_t>(max_samples)) {
    all_samples.resize(static_cast<size_t>(max_samples));
  }

  size_t cutoff_offset = ratio * all_samples.size();

  std::vector<sample> train_samples(
    std::make_move_iterator(all_samples.begin()),
    std::make_move_iterator(all_samples.begin() + cutoff_offset));

  std::vector<sample> test_samples(
    std::make_move_iterator(all_samples.begin() + cutoff_offset),
    std::make_move_iterator(all_samples.end()));

  return std::make_pair(
    address_dataset(std::move(train_samples), device),
    address_dataset(std::move(test_samples), device));
}

}