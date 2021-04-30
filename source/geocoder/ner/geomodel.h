// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <memory>
#include <iostream>
#include <torch/torch.h>

#pragma GCC diagnostic pop

namespace sentio::geocoder
{

/**
 * Distinct classes of address components that this model is
 * able to recognize from a free-text address string.
 */
enum class address_label : int32_t
{
  unknown   = 0,
  street    = 1,
  building  = 2,
  city      = 3,
  zipcode   = 4,
  suite     = 5,
  other     = 6
};

/**
 * Returns the number of different types of address components
 * that are recognized by this model. There is always the "other"
 * class/label that specifies that something is either a whitespace
 * or not recognized at all and could be ignored.
 */
size_t labels_count();

/**
 * This neural network based model is used to perform Named Entity Recognition
 * on parts of free-text addresses. When an address is first entered by the user
 * in free text form, it is passed to an instance of this class and split into 
 * parts by their address component so that they can be later on queried in the
 * database. 
 * 
 * For example: "Wiejska 35a bialystok 15-318"
 * will be interpreted as: 
 *  - "street: wiejska", 
 *  - "building: 35a", 
 *  - "city: bialystok", 
 *  - "zip: 15-318"
 */
class geomodel_impl : public torch::nn::Module
{
private: // hyperparameters
  static constexpr int64_t lstm_layers = 1;
  static constexpr int64_t hidden_size = 256;
  static constexpr bool bidirectional = false;

public:
  /**
   * The number of distinct characters that this model recognizes.
   * The model first normalizes all characters before attempting to 
   * recognize the address component. Those normalizations include:
   * 
   *  - converting everything to lowercase
   *  - converting accent letters to their base form, e.g. ś -> s, ń -> n
   *  - removing useless symbols that don't occur in addresses, e.g. $, ^, @
   *  - removing punctuation marks 
   */
  static size_t alphabet_size();

  /**
   * Converts a given character from the address string into its
   * model-internal representation that can be used in a feature 
   * vector as an input to the neural network.
   * 
   * This function is made public, so that it can be used by
   * components of the system that implement model training.
   */
  static int32_t encode_char(wchar_t);

  /**
   * Converts an entire string to a vector of character ids that can
   * be used to construct input tensors to this model. This essentially
   * constructs a vector of encode_char(c) for each character in the strign.
   */
  static std::vector<int32_t> encode_string(std::string const& s);

public:
  
  /**
   * Initialize a new instance of the geomodel implementation and
   * specifies the computational device and memory to use for holding
   * weights and tensors within this computational graph.
   */
  geomodel_impl(torch::DeviceType device = torch::DeviceType::CPU);

public:
  /**
   * Invokes a forward pass on a one-hot encoded tensor of address character sequence.
   * 
   * This function can be invoked for one sequence of an entire batch. It figures out which
   * input mode is used by looking at the dimentionality of the input tensor. If it has 2
   * dimentions, then it uses the single-sequence mode, otherwise it invokes the batch made.
   * 
   * Single sequence tensor is used during production inference and has the following shape:
   *      (sequence_length, alphabet_size())
   * The output of this forward mode is a tensor with the logits of the last linear layer.
   * The logits are returned because it is what the loss function expects (CrossEntropy).
   * using the std::string overload of this operator will also apply softmax and argmax to 
   * the logits and return concrete label indecies.
   * 
   * Batch tensor is used during training and has the following shape:
   *      (batch_size, max_sequence_length, alphabet_size())
   * The output of the batch pass is (batch_size, a, b) where a and b are same as
   * the output from the single-sequence pass.
   */
  torch::Tensor operator()(torch::Tensor input);

  /**
   * Same as operator(), in fact, the operator forwards its call to this method.
   * It is explicitly exposed as a public member for torch library infrastructure
   * helper classes to properly interop with utils such as automatic serialization, 
   * etc.
   */
  torch::Tensor forward(torch::Tensor input);

public: // r/o model info
  /**
   * Returns the device that is used for computations for this instance of the model.
   */
  torch::DeviceType device() const;

private:
  torch::Tensor forward_one(torch::Tensor input);
  torch::Tensor forward_batch(torch::Tensor input);

private:
  torch::DeviceType device_;

private: // neural layers
  torch::nn::Linear i2h_;
  torch::nn::ReLU relu_;
  torch::nn::LSTM lstm_;
  torch::nn::Linear h2o_;
};

/**
 * The public API to using the geomodel named entity recognition 
 * for polish addresses.
 * 
 * Wraps the geomodel impl in a torch module holder, enabling 
 * an array of features, such as automatic serialization and 
 * deserialization,
 */
class geomodel 
  : public torch::nn::ModuleHolder<geomodel_impl>
{
public:
  using torch::nn::ModuleHolder<geomodel_impl>::ModuleHolder;

public:
  /**
   * Initialize a new instance of the geomodel with random weights.
   * using this constructor puts the model in training mode, and is
   * useless for production inference. Because its used mainly for
   * training, thus the default device is GPU.
   */
  geomodel(torch::DeviceType device = torch::DeviceType::CUDA);

public: // weights i/o
  /**
   * Constructs a new instance of the model from a saved snapshot
   * of trained weights. This constructor is meant to be used for
   * production inference scenarios. The default device for this
   * constructor is the CPU, because our AWS instances don't have
   * GPUs attached, and the CPU is fast enough for the forward pass.
   */
  geomodel(
    std::string modelpath,
    torch::DeviceType device = torch::DeviceType::CPU);
    
  geomodel(
    std::istream& modelstream,
    torch::DeviceType device = torch::DeviceType::CPU);

  /**
   * Saves a snapshot of the current weights to a file or stream.
   * 
   * The resulting file could be used later to construct an instance of
   * this model with those trained weights using the second constructor.
   * 
   * Before saving model weights and parameters, it moves all its values
   * from the current computational device to the CPU device so that it
   * can be loaded for inference in CPU mode.
   */
  void save_snapshot(std::ostream& modelstream);
  void save_snapshot(std::string const& modelpath);

public:
  /**
   * Invokes a forward pass on the neural network for either a 
   * single address or a batch of addresses.
   * 
   * Returns a set of logits of the output layer. 
   * 
   * This overload is used mostly for training that's why no softmax
   * or argmax is applied to the logits output, so that the used 
   * cross entropy loss function could compute losses properly for
   * the backwards pass.
   */
  torch::Tensor operator()(torch::Tensor);

  /**
   * This is the public API method used for client inference.
   * It takes an address string and returnes that string separated
   * into address components.
   */
  torch::Tensor operator()(std::string const&);
};

/**
 * This function creates an instance of the geomodel from
 * the set of pretrained weights that was embedded in the 
 * containing binary at compile time by the build system.
 */
geomodel load_embedded_model(torch::DeviceType device = torch::DeviceType::CPU);

}
