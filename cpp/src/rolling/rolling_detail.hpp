/*
 * Copyright (c) 2019, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ROLLING_DETAIL_HPP
#define ROLLING_DETAIL_HPP

#include <cudf/detail/utilities/device_operators.cuh>
#include <cudf/detail/aggregation/aggregation.hpp>
#include <cudf/utilities/traits.hpp>

namespace cudf
{

// helper functions - used in the rolling window implementation and tests
namespace detail
{
  // return true if ColumnType is arithmetic type or
  // AggOp is min_op/max_op/count_op for wrapper (non-arithmetic) types
  template <typename ColumnType, class AggOp, cudf::experimental::aggregation::Kind op, bool is_mean>
  static constexpr bool is_supported()
  {
    constexpr bool is_comparable_countable_op =
      std::is_same<AggOp, DeviceMin>::value ||
      std::is_same<AggOp, DeviceMax>::value ||
      std::is_same<AggOp, DeviceCount>::value;

    constexpr bool is_operation_supported = (op == experimental::aggregation::SUM) or
                                             (op == experimental::aggregation::MIN) or
                                             (op == experimental::aggregation::MAX) or
                                             (op == experimental::aggregation::COUNT_VALID) or
                                             (op == experimental::aggregation::COUNT_ALL) or
                                             (op == experimental::aggregation::MEAN);

    constexpr bool is_valid_timestamp_agg = cudf::is_timestamp<ColumnType>() and
                                             (op == experimental::aggregation::MIN or
                                              op == experimental::aggregation::MAX or 
                                              op == experimental::aggregation::COUNT_VALID or
                                              op == experimental::aggregation::COUNT_ALL or
                                              op == experimental::aggregation::MEAN);


    constexpr bool is_valid_numeric_agg = (cudf::is_numeric<ColumnType>() or
                                          is_comparable_countable_op) and 
                                          is_operation_supported;

    constexpr bool is_valid_rolling_agg = !std::is_same<ColumnType, cudf::string_view>::value and
                                          (is_valid_timestamp_agg or is_valid_numeric_agg);

    return is_valid_rolling_agg and
           cudf::experimental::detail::is_valid_aggregation<ColumnType, op>();
  }

  template <typename ColumnType, class AggOp, cudf::experimental::aggregation::Kind Op>
  static constexpr bool is_string_supported()
  {
      return std::is_same<ColumnType, cudf::string_view>::value and
          ((cudf::experimental::aggregation::MIN == Op and std::is_same<AggOp, DeviceMin>::value) or
           (cudf::experimental::aggregation::MAX == Op and std::is_same<AggOp, DeviceMax>::value) or
           (cudf::experimental::aggregation::COUNT_VALID == Op and std::is_same<AggOp, DeviceCount>::value) or
           (cudf::experimental::aggregation::COUNT_ALL == Op and std::is_same<AggOp, DeviceCount>::value));
  }

  // store functor
  template <typename T, bool is_mean>
  struct store_output_functor
  {
    CUDA_HOST_DEVICE_CALLABLE void operator()(T &out, T &val, size_type count)
    {
      out = val;  
    }
  };

  // Specialization for MEAN
  template <typename _T>
  struct store_output_functor<_T, true>
  {
    // SFINAE for non-bool types
    template <typename T = _T,
      std::enable_if_t<!(cudf::is_boolean<T>() || cudf::is_timestamp<T>())>* = nullptr>
    CUDA_HOST_DEVICE_CALLABLE void operator()(T &out, T &val, size_type count)
    {
      out = val / count;
    }

    // SFINAE for bool type
    template <typename T = _T, std::enable_if_t<cudf::is_boolean<T>()>* = nullptr>
    CUDA_HOST_DEVICE_CALLABLE void operator()(T &out, T &val, size_type count)
    {
      out = static_cast<int32_t>(val) / count;
    }

    // SFINAE for timestamp types
    template <typename T = _T, std::enable_if_t<cudf::is_timestamp<T>()>* = nullptr>
    CUDA_HOST_DEVICE_CALLABLE void operator()(T &out, T &val, size_type count)
    {
      out = val.time_since_epoch() / count;
    }
  };
}  // namespace cudf::detail

} // namespace cudf

#endif

