// Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DALI_OPERATORS_GENERIC_PERMUTE_BATCH_H_
#define DALI_OPERATORS_GENERIC_PERMUTE_BATCH_H_

#include <vector>
#include "dali/pipeline/operator/operator.h"
#include "dali/kernels/common/scatter_gather.h"

namespace dali {

template <typename Backend>
class PermuteBatchBase : public Operator<Backend> {
 public:
  explicit PermuteBatchBase(const OpSpec &spec) : Operator<Backend>(spec) {
    has_indices_input_ = spec.HasTensorArgument("indices");
  }

  bool SetupImpl(vector<OutputDesc> &outputs, const workspace_t<Backend> &ws) override {
    auto curr_batch_size = ws.GetInputBatchSize(0);
    outputs.resize(1);
    auto &input = ws.template InputRef<Backend>(0);
    const auto &in_shape = input.shape();
    outputs[0].type = input.type();

    if (has_indices_input_) {
      GetPerSampleArgument<int>(indices_, "indices", this->spec_, ws, curr_batch_size);
    } else {
      this->spec_.TryGetRepeatedArgument(indices_, "indices");
    }

    auto &out_shape = outputs[0].shape;
    int D = in_shape.sample_dim();
    out_shape.resize(indices_.size(), D);
    for (int i = 0; i < out_shape.num_samples(); i++) {
      auto out_ts = out_shape.tensor_shape_span(i);
      DALI_ENFORCE(indices_[i] >= 0 && indices_[i] < in_shape.num_samples(), make_string(
        "Sample index out of range. indices[", i, "] = ", indices_[i], " is not a valid index for "
        "an input batch of ", in_shape.num_samples(), " tensors."));
      auto in_ts = in_shape.tensor_shape_span(indices_[i]);
      for (int d = 0; d < D; d++)
        out_ts[d] = in_ts[d];
    }
    return true;
  }

  bool CanInferOutputs() const override {
    return true;
  }


 protected:
  vector<int> indices_;
  bool has_indices_input_ = false;
};

template <class Backend>
class PermuteBatch;

template <>
class PermuteBatch<CPUBackend> : public PermuteBatchBase<CPUBackend> {
 public:
  using PermuteBatchBase::PermuteBatchBase;

  void RunImpl(HostWorkspace &ws) override;
};

template <>
class PermuteBatch<GPUBackend> : public PermuteBatchBase<GPUBackend> {
 public:
  explicit PermuteBatch(const OpSpec &spec)
  : PermuteBatchBase<GPUBackend>(spec)
  , sg_(1<<18, spec.GetArgument<int>("max_batch_size")) {}


  void RunImpl(DeviceWorkspace &ws) override;

 private:
  kernels::ScatterGatherGPU sg_;
};

}  // namespace dali

#endif  // DALI_OPERATORS_GENERIC_PERMUTE_BATCH_H_
