// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <fstream>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>

#include "geomodel.h"
#include "utils/log.h"


extern "C" 
{

// those symbols point to the range of memory that
// has a pretrained model embedded in the containing 
// binary.

extern char _binary_assets_poland_model_end[];
extern char _binary_assets_poland_model_start[];

}

namespace sentio::geocoder
{

static std::wstring_convert<std::codecvt_utf8<wchar_t>> g_conv;
static std::unordered_map<wchar_t, int32_t> g_char2idx = {
  {L' ', 56}, {L',', 57 }, {L'•', 0},
  {L'1', 1 }, {L'2', 2 }, {L'3', 3 }, {L'4', 4},   {L'5', 5},
  {L'6', 6 }, {L'7', 7 }, {L'8', 8 }, {L'9', 9},   {L'0', 10},
  {L'a', 11}, {L'ą', 12}, {L'b', 13}, {L'c', 14},  {L'ć', 15},
  {L'd', 16}, {L'e', 17}, {L'ę', 18}, {L'f', 19},  {L'g', 20},
  {L'h', 21}, {L'i', 22}, {L'j', 23}, {L'k', 24},  {L'l', 25},
  {L'ł', 26}, {L'm', 27}, {L'n', 28}, {L'ń', 29},  {L'o', 30},
  {L'ó', 31}, {L'p', 32}, {L'q', 33}, {L'r', 34},  {L's', 35},
  {L'ś', 36}, {L't', 37}, {L'u', 38}, {L'v', 39},  {L'w', 40},
  {L'x', 41}, {L'y', 42}, {L'z', 43}, {L'ż', 44},  {L'ź', 45},
  {L'/', 46}, {L'-', 47}, {L'.', 48}, {L'(', 49},  {L')', 50},
  {L'#', 51}, {L'\\', 52}, {L'"', 53}, {L'\'', 54}, {L'?', 55}
};

static constexpr int64_t directions_count(bool bidirectional)
{ return bidirectional ? 2 : 1; }

size_t labels_count()
{ return 7; }

size_t geomodel_impl::alphabet_size()
{ return g_char2idx.size(); }

int32_t geomodel_impl::encode_char(wchar_t c)
{
  if (auto it = g_char2idx.find(c); it != g_char2idx.end()) {
    return it->second;
  } else {
    return g_char2idx[L'•'];
  }
}

std::vector<int32_t> geomodel_impl::encode_string(std::string const& str)
{
  std::vector<int32_t> output;
  std::wstring wstr = g_conv.from_bytes(str);
  std::transform(
    wstr.begin(), wstr.end(), 
    std::back_inserter(output),
    [](auto c) { return encode_char(std::tolower(c)); });
  return output;
}

geomodel_impl::geomodel_impl(torch::DeviceType device)
  : device_(device)
  , i2h_(register_module("i2h", torch::nn::Linear(alphabet_size(), alphabet_size())))
  , relu_(register_module("relu", torch::nn::ReLU()))
  , lstm_(register_module("lstm", torch::nn::LSTM(
      torch::nn::LSTMOptions(alphabet_size(), hidden_size)
        .num_layers(lstm_layers)
        .bidirectional(bidirectional))))
  , h2o_(register_module("h2o", torch::nn::Linear(
      directions_count(bidirectional) * hidden_size, labels_count())))
{
}

torch::DeviceType geomodel_impl::device() const
{ return device_; }

torch::Tensor geomodel_impl::forward_one(torch::Tensor input)
{ 
  // model:
  // Linear -> ReLU  -> LSTM -> Linear
  //  i2h   -> relu  -> lstm -> h2o
  
  int64_t directions = bidirectional ? 2 : 1;
  
  auto lstm_state = std::make_tuple(
    torch::zeros({directions * lstm_layers, 1, hidden_size}, 
      torch::TensorOptions().device(device_)),
    torch::zeros({directions * lstm_layers, 1, hidden_size}, 
      torch::TensorOptions().device(device_)));

  // input layers (linear -> ReLU)
  torch::Tensor output = relu_(i2h_(input))
    .view({input.size(0), 1, -1});

  // LSTM recurrent layer
  std::tie(output, lstm_state) = lstm_(output, lstm_state);

  // linear logits output
  output = h2o_(output.view({input.size(0), -1}));
  
  return output; // logits
}

torch::Tensor geomodel_impl::forward_batch(torch::Tensor input)
{
  std::vector<torch::Tensor> output;
  for (int64_t i = 0; i < input.size(0); ++i) {
    output.push_back(forward_one(input[i]));
  }
  return torch::stack(output);
}

torch::Tensor geomodel_impl::forward(torch::Tensor input)
{
  size_t dims = input.dim();
  switch (dims) {
    case 2: return forward_one(input);
    case 3: return forward_batch(input);
    default: throw std::runtime_error("input is expected to have 2 or 3 dmientions");
  }
}

torch::Tensor geomodel_impl::operator()(torch::Tensor input)
{ return forward(input); }

torch::Tensor geomodel::operator()(std::string const& address)
{
  auto in_vector = geomodel_impl::encode_string(address);
  torch::Tensor input = torch::zeros({ 
    static_cast<int64_t>(in_vector.size()),
    static_cast<int64_t>(geomodel_impl::alphabet_size())
  }, torch::TensorOptions()
      .device(impl_->device())
      .dtype(torch::kFloat));

  // one-hot encoding
  size_t row = 0;
  for (auto const& c: in_vector) {
    input[row][c] = 1;
    ++row;
  }

  // return a more developer friendly NER output
  auto output = impl_->forward(input);
  output = torch::softmax(output, 1);
  output = torch::argmax(output, 1, true);
  return output;
}

torch::Tensor geomodel::operator()(torch::Tensor t)
{ return impl_->forward(t); }

geomodel::geomodel(
  std::istream& modelstream,
  torch::DeviceType device)
  : geomodel(device)
{
  impl_->train(false);
  torch::load(*this, modelstream);
}

geomodel::geomodel(
  std::string modelpath,
  torch::DeviceType device)
  : geomodel(device)
{
  impl_->train(false);
  std::ifstream instream(
    std::move(modelpath), 
    std::ios::binary);
  torch::load(*this, instream);
}

geomodel::geomodel(torch::DeviceType device)
  : ModuleHolder(device)
{ 
  impl_->to(device); 
  impl_->train(true);
}

void geomodel::save_snapshot(std::ostream& modelstream)
{
  torch::DeviceType prevdev = impl_->device();
  impl_->to(torch::DeviceType::CPU);
  torch::save(*this, modelstream); 
  impl_->to(prevdev);
}

void geomodel::save_snapshot(std::string const& modelpath)
{ 
  std::ofstream outstream(modelpath, std::ios::binary);
  save_snapshot(outstream); 
}

geomodel load_embedded_model(torch::DeviceType device)
{
  size_t size = 
    _binary_assets_poland_model_end - 
    _binary_assets_poland_model_start;
  boost::iostreams::stream<
    boost::iostreams::array_source> instream(
      _binary_assets_poland_model_start, size);
  return geomodel(static_cast<std::istream&>(instream), device);
}

}