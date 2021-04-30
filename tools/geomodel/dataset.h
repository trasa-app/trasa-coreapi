// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <optional>

#include <torch/torch.h>
#include <model/address.h>
#include <import/region_reader.h>
#include <geocoder/ner/geomodel.h>

namespace sentio::geomodel
{
/**
 * This type represents a single address split into individual characters, 
 * with each character labeled as belonging to a given class.
 */
class sample
{
public:
  using char_type = int32_t;
  using label_type = sentio::geocoder::address_label;
  using chars_type = std::vector<int32_t>;
  using labels_type = std::vector<label_type>;

public:
  /**
   * This constructor builds a sample from a prelabeled address
   * string. It is used most commonly when constructing variations
   * and permuations of a given building instance.
   */
  sample();

  sample(
    chars_type chars,
    labels_type labels,
    torch::DeviceType = torch::DeviceType::CPU,
    std::optional<model::building> underlying = {});

public: // neural network data
  torch::Tensor input() const;
  torch::Tensor output() const;

public: // character-level data
  chars_type const& chars() const;
  labels_type const& labels() const;

public: // batching and normalization
  void pad_to(size_t len);

public: // underlying data object
  size_t length() const;
  std::string text() const;

private:
  torch::DeviceType device_;
  chars_type chars_;
  labels_type labels_;
  std::optional<model::building> building_;
};

/**
 * Given an iterator over building entries, this function will prepare
 * trainable character sequences, with each character labeled as belonging
 * to a given component of the address.
 */
std::vector<sample> import_samples(std::string path, 
  torch::DeviceType device = torch::DeviceType::CPU, 
  int64_t max_samples = -1);

class address_dataset
  : public torch::data::Dataset<address_dataset>
{
public:
  using element_iterator = std::vector<sample>::const_iterator;

public:
  address_dataset(
    std::string filename, 
    torch::DeviceType device = torch::DeviceType::CPU);

  address_dataset(
    std::vector<sample> samples, 
    torch::DeviceType device = torch::DeviceType::CPU);

public: // torch::data::DataSet implementation
  torch::optional<size_t> size() const override;
  torch::data::Example<> get(size_t index) override;

public:
  element_iterator begin() const;
  element_iterator end() const;

private:
  std::vector<sample> samples_;
  torch::DeviceType devicetype_;
  size_t maxlength_;
};

/**
 * Opens the database file and imports all samples into two address_dataset: training and testing
 * the split between training and testing is defined by the ratio value. The first dataset is
 * for training (ratio%) and the second is for testing (1 - ratio%).
 */
std::pair<address_dataset, address_dataset> import_datasets(
  std::string filename, float ratio = 0.6, 
  torch::DeviceType device = torch::DeviceType::CPU, 
  int64_t max_samples = -1);

}