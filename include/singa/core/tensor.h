/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SINGA_CORE_TENSOR_H_
#define SINGA_CORE_TENSOR_H_

#include <vector>
#include <tuple>

#include "singa/core/common.h"
#include "singa/core/device.h"
#include "singa/proto/core.pb.h"
#include "singa/utils/logging.h"

using std::vector;
using std::tuple;
namespace singa {

typedef vector<size_t> Shape;
/// hardcode the width of types defined in DataType
const size_t kDataWidth[] = {sizeof(float), sizeof(float) / 2, sizeof(int),
                             sizeof(char), sizeof(double)};
inline size_t SizeOf(DataType t) {
  static_assert(kNumDataType == sizeof(kDataWidth) / sizeof(size_t),
                "Num of data types not match num of data width");
  CHECK_GT(kNumDataType, t);
  return kDataWidth[t];
}

/// A Tensor instance is a multi-dimensional array resident on a Device
/// (default device is the host CPU). The internal data is allocated in lazy
/// manner.
/// Linear algebra, neural net and random operations are provided against
/// Tensor.
/// For all operations, if the result tensor is passed as an argument,
/// then it must be set up correctly (shape, device). Otherwise, runtime error
/// like SegmentFault would happen. Simply type/device check would be conducted.
class Tensor {
 public:
  ~Tensor();
  Tensor();
  explicit Tensor(Shape &&shape, const DataType dtype = kFloat32);
  explicit Tensor(const Shape &shape, const DataType dtype = kFloat32);
  Tensor(Shape &&shape, Device *dev, const DataType dtype = kFloat32);
  Tensor(const Shape &shape, Device *dev, const DataType dtype = kFloat32);

  /// Copy Tensor to share the internal data.  No deep copy.
  Tensor(const Tensor &from);
  /// Copy Tensor to share the internal data.  No deep copy.
  Tensor(Tensor &&from);

  /// For functions in xx_math.cc to access the blob.
  /// Users should not operate against Blob directly.
  /// blob_ is allocated in constructors.
  Blob *blob() const { return blob_; }

  Device *device() const { return device_; }

  /// return immutable Tensor values with given type.
  template <typename SType>
  SType data() const {
    return static_cast<SType>(blob()->data());
  }

  /// data type, including kFloat16, kFloat32, kInt
  const DataType data_type() const { return data_type_; }

  const Shape &shape() const { return shape_; }

  const size_t shape(const size_t idx) const {
    CHECK_LT(idx, shape_.size());
    return shape_.at(idx);
  }

  size_t nDim() const { return shape_.size(); }

  bool transpose() const { return transpose_; }

  /// return number of total elements
  size_t Size() const {
    CHECK_EQ(blob_->size() % SizeOf(data_type_), 0u);
    return blob_->size() / SizeOf(data_type_);
  }

  /// return memory size (i.e., Bytes)
  size_t MemSize() const { return blob_->size(); }

  /// Reset the tensor shape, it may reallocate blob, if MemSize() changes.
  void Reshape(const Shape &shape);
  void Reshape(Shape &&shape);

  /// Reset the shape, device, and data type as given tensor.
  /// If blob size changes, then reallocate a new blob. The previous blob would
  /// be deleted.
  void ResetLike(const Tensor &t);

  /// Reset the data type, it would reallocate blob if type changes.
  void AsType(const DataType type);

  /// Reset the device.
  /// If the target device is a diff device, then do deep data copy.
  void ToDevice(Device *dev);

  /// Equivalent to ToDevice(host_dev).
  void ToHost();

  /// Set each element of the tensor to be x
  template <typename SType>
  void SetValue(const SType x);

  /// For init the tensor values, copy 'num' elements.
  template <typename SType>
  void CopyDataFromHostPtr(const SType *src, const size_t num);

  /// Copy data from another Tensor which may be on a diff device.
  /// Meta data would not be copied!
  void CopyData(const Tensor &other);

  /// return an exactly the same Tensor with data been deep copied.
  Tensor Clone() const;

  // Tensor operations

  /// Matrix transpose.  Valid only if shape.size() == 2.
  /// No data copy, just set the transpose_ filed of the returned tensor.
  Tensor T() const;

  /// Copy the meta info with data blob shared.
  Tensor &operator=(const Tensor &in);

  /// Copy the meta info with data blob shared.
  Tensor &operator=(Tensor &&in);

  Tensor &operator+=(const Tensor &in);
  // void operator+=(Tensor&& in);
  Tensor &operator-=(const Tensor &in);
  // void operator-=(Tensor&& in);
  Tensor &operator*=(const Tensor &in);
  // void operator*=(Tensor&& in);
  Tensor &operator/=(const Tensor &in);
  // void operator/=(Tensor&& in);

  // Scalar operations.

  /// SType is a scalar type
  template <typename SType>
  Tensor &operator+=(const SType x);

  /// SType is a scalar type
  template <typename SType>
  Tensor &operator-=(const SType x);

  /// SType is a scalar type
  template <typename SType>
  Tensor &operator*=(const SType x);

  /// SType is a scalar type
  template <typename SType>
  Tensor &operator/=(const SType x);

  float L2() const;

 protected:
  bool transpose_ = false;
  DataType data_type_ = kFloat32;
  Device *device_ = nullptr;
  /// Note: blob_ is allocated in lazy manner to avoid frequent malloc/free.
  /// If you want to get an allocated Blob, use blob() instead of blob_.
  Blob *blob_ = nullptr;
  Shape shape_ = {};
};

typedef Shape::iterator ShapeIter;
inline size_t Product(const Shape &shape, int start = 0, size_t len = 0) {
  if (len == 0) len = shape.size();
  CHECK_LE(len, shape.size());
  size_t v = 1;
  for (unsigned int i = start; i < len; i++) v *= shape[i];
  return v;
}

inline void CheckDataTypeAndLang(const Tensor &in1, const Tensor &in2) {
  CHECK_EQ(in1.data_type(), in2.data_type());
  CHECK_EQ(in1.device()->lang(), in2.device()->lang());
}

template <typename FromType, typename ToType>
ToType TypeCast(const FromType &x) {
  // TODO(wangwei) cast fp16; prevent some casts, e.g., float to char
  return static_cast<ToType>(x);
}

Tensor Reshape(const Tensor &in, const Shape &s);
Tensor Reshape(const Tensor &in, Shape &&s);

// For tensors with sparse content, e.g., missing columns or rows.
// class SparseTensor : public Tensor {};

/// Copy 'num' elements of src to dst.
/// The first 'src_offset' ('dst_offset') elements will be skipped.
void CopyDataToFrom(Tensor *dst, const Tensor &src, const size_t num,
                    const size_t src_offset = 0, const size_t dst_offset = 0);

// =============Element-wise operations====================================
Tensor Abs(const Tensor &in);
Tensor Exp(const Tensor &in);
Tensor Log(const Tensor &in);
Tensor ReLU(const Tensor &in);
Tensor Sigmoid(const Tensor &in);
Tensor Sign(const Tensor &in);
Tensor Sqrt(const Tensor &in);
Tensor Square(const Tensor &in);
Tensor Tanh(const Tensor &in);

/// Element-wise opeartion, out[i]=in[i]^x
template <typename SType>
Tensor Pow(const Tensor &in, const SType x);
/// Element-wise opeartion, out[i]=in[i]^x
template <typename SType>
void Pow(const Tensor &in, const SType x, Tensor *out);
/// Element-wise opeartion, out[i]=baes[i]^exp[i]
Tensor Pow(const Tensor &base, const Tensor &exp);
/// Element-wise opeartion, out[i]=baes[i]^exp[i]
void Pow(const Tensor &base, const Tensor &exp, Tensor *out);

/// Element-wise operation, out[i]= (in[i] < x) ? 1.f : 0.f
template <typename SType>
Tensor operator<(const Tensor &in, const SType x);
template <typename SType>
void LT(const Tensor &in, const SType x, Tensor *out);

/// Element-wise operation, out[i]= (in[i] <= x) ? 1.f : 0.f
template <typename SType>
Tensor operator<=(const Tensor &in, const SType x);
template <typename SType>
void LE(const Tensor &in, const SType x, Tensor *out);

/// Element-wise operation, out[i]= (in[i] > x) ? 1.f : 0.f
template <typename SType>
Tensor operator>(const Tensor &in, const SType x);
template <typename SType>
void GT(const Tensor &in, const SType x, Tensor *out);

/// Element-wise operation, out[i]= (in[i] >= x) ? 1.f : 0.f
template <typename SType>
Tensor operator>=(const Tensor &in, const SType x);
template <typename SType>
void GE(const Tensor &in, const SType x, Tensor *out);

Tensor operator+(const Tensor &lhs, const Tensor &rhs);
void Add(const Tensor &lhs, const Tensor &rhs, Tensor *out);
Tensor operator-(const Tensor &lhs, const Tensor &rhs);
void Sub(const Tensor &lhs, const Tensor &rhs, Tensor *out);
Tensor operator*(const Tensor &lhs, const Tensor &rhs);
void EltwiseMult(const Tensor &lhs, const Tensor &rhs, Tensor *out);
Tensor operator/(const Tensor &lhs, const Tensor &rhs);
void Div(const Tensor &lhs, const Tensor &rhs, Tensor *out);

template <typename SType>
Tensor operator+(const Tensor &in, const SType x);
template <typename SType>
void Add(const Tensor &in, const SType x, Tensor *out);

template <typename SType>
Tensor operator-(const Tensor &in, const SType x);
template <typename SType>
void Sub(const Tensor &in, const SType x, Tensor *out);

template <typename SType>
Tensor operator*(const Tensor &in, const SType x);
template <typename SType>
void EltwiseMult(const Tensor &in, const SType x, Tensor *out);

/// For each element e of Tensor 'in', compute e / x
template <typename SType>
Tensor operator/(const Tensor &in, const SType x);
/// For each element e of Tensor 'in', compute e / x into out
template <typename SType>
void Div(const Tensor &in, const SType x, Tensor *out);

/// For each element e of Tensor 'in', compute x/e
template <typename SType>
Tensor Div(const SType x, const Tensor &in);
/// For each element e of Tensor 'in', compute x/e into 'out'
template <typename SType>
void Div(const SType x, const Tensor &in, Tensor *out);

template <typename SType>
SType Sum(const Tensor &in);

// ============Matrix (row/column) operations==================================
/// Average elements in the Tensor, currently only support vector and matrix.
/// if 'axis' is 0, average all rows into a single row
/// if 'axis' is 1, average all columns into a single column
/// TODO(wangwei) support arbitrary Tensor like numpy.average
Tensor Average(const Tensor &in, const int axis);
/// Sum elements in the Tensor, currently only support vector and matrix.
/// if 'axis' is 0, sum all rows into a single row
/// if 'axis' is 1, sum all columns into a single column
/// TODO(wangwei) support arbitrary Tensor like numpy.sum
Tensor Sum(const Tensor &in, const int axis);
/// Regarding the internal data as 2d, with shape_[0]*...*shape_[axis-1] rows,
/// and shape_[axis]*...*shape_[nDim()] columns.
/// and do softmax along each row.
Tensor SoftMax(const Tensor &in, const int axis = 0);
void SoftMax(const Tensor &in, const int axis, Tensor *out);

/// Add column 'v' with each column of matrix M
void AddColumn(const Tensor &v, Tensor *M);
/// For each column 'c' of matrix out, do c=alpha*v + beta*c
template <typename SType>
void AddColumn(const SType alpha, const SType beta, const Tensor &v,
               Tensor *out);
/// Add row 'v' with each row of matrix M; write results into 'out'
void AddRow(const Tensor &v, Tensor *out);
/// For each row 'r' of matrix out, do r=alpha*v + beta*r
template <typename SType>
void AddRow(const SType alpha, const SType beta, const Tensor &v, Tensor *M);
/// Divide column 'v' by each column of matrix M; write results into 'out'
void DivColumn(const Tensor &v, Tensor *M);
/// Divide row 'v' by each row of matrix M; write results into 'out'
void DivRow(const Tensor &v, Tensor *M);
/// Multiply column 'v' and each column of matrix M; write results into 'out'
void MultColumn(const Tensor &v, Tensor *M);
/// Multiply row 'v' with each row of matrix M; write results into 'out'
void MultRow(const Tensor &v, Tensor *M);
/// Sub column 'v' by each column of matrix M
void SubColumn(const Tensor &v, Tensor *M);
/// Sub row 'v' by each row of matrix M; write results into 'out'
void SubRow(const Tensor &v, Tensor *M);
/// Sum all columns of matrix M into a single column as 'out'
void SumColumns(const Tensor &M, Tensor *out);
/// Sum all rows of matrix M into a single row as 'out'
void SumRows(const Tensor &M, Tensor *out);

// ================Random operations==========================================
/// For each element x set x = 1 if random() < p; otherwise x = 1.
template <typename SType>
void Bernoulli(const SType p, Tensor *out);
/// Fill in Tensor 't' following Gaussian distribution.
template <typename SType>
void Gaussian(const SType mean, const SType std, Tensor *out);
/// Fill in Tensor 't' following uniform distribution.
template <typename SType>
void Uniform(const SType low, const SType high, Tensor *out);

// ================Blas operations============================================
// TODO(wangwei) make amax/amin/asum a member function of tensor

/// out = alpha*in + out
template <typename SType>
void Axpy(SType alpha, const Tensor &in, Tensor *out);

/// Do matrix vector multipication or matrix matrix multiplication depdending
/// on the Tensor shape.  result = A * B
Tensor Mult(const Tensor &A, const Tensor &B);
/// Do matrix vector multipication or matrix matrix multiplication depdending
/// on the Tensor shape.  C = A * B
void Mult(const Tensor &A, const Tensor &B, Tensor *C);

/// Do matrix vector multipication or matrix matrix multiplication depdending
/// on the Tensor shape. out = alpha lhs * rhs + beta * out
template <typename SType>
void Mult(const SType alpha, const Tensor &A, const Tensor &B, const SType beta,
          Tensor *C);
}  // namespace singa

#endif  // SINGA_CORE_TENSOR_H_
