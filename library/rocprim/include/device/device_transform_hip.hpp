// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef ROCPRIM_DEVICE_DEVICE_TRANSFORM_HIP_HPP_
#define ROCPRIM_DEVICE_DEVICE_TRANSFORM_HIP_HPP_

#include <type_traits>
#include <iterator>

#include "../config.hpp"
#include "../detail/various.hpp"

#include "detail/device_transform.hpp"

BEGIN_ROCPRIM_NAMESPACE

/// \addtogroup devicemodule_hip
/// @{

namespace detail
{

template<
    unsigned int BlockSize,
    unsigned int ItemsPerThread,
    class InputIterator,
    class OutputIterator,
    class UnaryFunction
>
__global__
void transform_kernel(InputIterator input,
                      const size_t size,
                      OutputIterator output,
                      UnaryFunction transform_op)
{
    transform_kernel_impl<BlockSize, ItemsPerThread>(
        input, size, output, transform_op
    );
}

#define ROCPRIM_DETAIL_HIP_SYNC(name, size, start) \
    if(debug_synchronous) \
    { \
        std::cout << name << "(" << size << ")"; \
        auto error = hipStreamSynchronize(stream); \
        if(error != hipSuccess) return error; \
        auto end = std::chrono::high_resolution_clock::now(); \
        auto d = std::chrono::duration_cast<std::chrono::duration<double>>(end - start); \
        std::cout << " " << d.count() * 1000 << " ms" << '\n'; \
    }

#define ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR(name, size, start) \
    { \
        auto error = hipPeekAtLastError(); \
        if(error != hipSuccess) return error; \
        if(debug_synchronous) \
        { \
            std::cout << name << "(" << size << ")"; \
            auto error = hipStreamSynchronize(stream); \
            if(error != hipSuccess) return error; \
            auto end = std::chrono::high_resolution_clock::now(); \
            auto d = std::chrono::duration_cast<std::chrono::duration<double>>(end - start); \
            std::cout << " " << d.count() * 1000 << " ms" << '\n'; \
        } \
    }

} // end of detail namespace

/// \brief HIP parallel transform primitive for device level.
///
/// transform function performs a device-wide transformation operation
/// using unary \p transform_op operator.
///
/// \par Overview
/// * Ranges specified by \p input and \p output must have at least \p size elements.
///
/// \tparam InputIterator - random-access iterator type of the input range. Must meet the
/// requirements of a C++ InputIterator concept. It can be a simple pointer type.
/// \tparam OutputIterator - random-access iterator type of the output range. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
/// \tparam UnaryFunction - type of unary function used for transform.
///
/// \param [in] input - iterator to the first element in the range to transform.
/// \param [out] output - iterator to the first element in the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] transform_op - unary operation function object that will be used for transform.
/// The signature of the function should be equivalent to the following:
/// <tt>U f(const T &a);</tt>. The signature does not need to have
/// <tt>const &</tt>, but function object must not modify the object passed to it.
/// \param [in] stream - [optional] HIP stream object. The default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. The default value is \p false.
///
/// \par Example
/// \parblock
/// In this example a device-level transform operation is performed on an array of
/// integer values (<tt>short</tt>s are transformed into <tt>int</tt>s).
///
/// \code{.cpp}
/// #include <rocprim.hpp>
///
/// // custom transform function
/// auto transform_op =
///     [] __device__ (int a) -> int
///     {
///         return a + 5;
///     };
///
/// // Prepare input and output (declare pointers, allocate device memory etc.)
/// size_t input_size;    // e.g., 8
/// short * input;        // e.g., [1, 2, 3, 4, 5, 6, 7, 8]
/// int * output;         // empty array of 8 elements
///
/// // perform transform
/// rocprim::transform(
///     input, output, input_size, transform_op
/// );
/// // output: [6, 7, 8, 9, 10, 11, 12, 13]
/// \endcode
/// \endparblock
template<
    class InputIterator,
    class OutputIterator,
    class UnaryFunction
>
inline
hipError_t transform(InputIterator input,
                     OutputIterator output,
                     const size_t size,
                     UnaryFunction transform_op,
                     const hipStream_t stream = 0,
                     bool debug_synchronous = false)
{
    // TODO: Those values should depend on type size
    constexpr unsigned int block_size = 256;
    constexpr unsigned int items_per_thread = 4;
    constexpr auto items_per_block = block_size * items_per_thread;

    // Start point for time measurements
    std::chrono::high_resolution_clock::time_point start;

    auto number_of_blocks = (size + items_per_block - 1)/items_per_block;
    if(debug_synchronous)
    {
        std::cout << "block_size " << block_size << '\n';
        std::cout << "number of blocks " << number_of_blocks << '\n';
        std::cout << "items_per_block " << items_per_block << '\n';
    }

    if(debug_synchronous) start = std::chrono::high_resolution_clock::now();
    hipLaunchKernelGGL(
        HIP_KERNEL_NAME(detail::transform_kernel<
            block_size, items_per_thread,
            InputIterator, OutputIterator, UnaryFunction
        >),
        dim3(number_of_blocks), dim3(block_size), 0, stream,
        input, size, output, transform_op
    );
    ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR("transform_kernel", size, start);

    return hipSuccess;
}

#undef ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR
#undef ROCPRIM_DETAIL_HIP_SYNC

/// @}
// end of group devicemodule_hip

END_ROCPRIM_NAMESPACE

#endif // ROCPRIM_DEVICE_DEVICE_TRANSFORM_HIP_HPP_
