/******************************************************************************
 * Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

#include "wavefront_primitives.hpp"

#include <nvshmem.h>

#include <numeric>

using namespace rocshmem;


/******************************************************************************
 * DEVICE TEST KERNEL
 *****************************************************************************/
__global__ void WaveFrontPrimitiveTest(int loop, int skip,
                                       long long int *start_time,
                                       long long int *end_time, char *source,
                                       char *dest, size_t size, TestType type,
                                       int ctx_type, int wf_size) {
  int wg_id = get_flat_grid_id();

  // Calculate start index for each warpfront
  int wf_id = get_flat_block_id() / wf_size;
  int wg_offset = wg_id * ((get_flat_block_size() - 1 ) / wf_size + 1);
  int idx = wf_id + wg_offset;
  size_t offset = size * idx;
  source += offset;
  dest += offset;

  for (int i = 0; i < loop + skip; i++) {
    if (i == skip) {
      // Ensures all RMA calls from the skip loops are completed
      if(is_thread_zero_in_warp()) {
        nvshmem_quiet();
      }
      __syncthreads();
      start_time[idx] = wall_clock64();
    }
    switch (type) {
      case WAVEGetTestType:
        nvshmemx_getmem_warp(dest, source, size, 1);
        break;
      case WAVEGetNBITestType:
        nvshmemx_getmem_nbi_warp(dest, source, size, 1);
        break;
      case WAVEPutTestType:
        nvshmemx_putmem_warp(dest, source, size, 1);
        break;
      case WAVEPutNBITestType:
        nvshmemx_putmem_nbi_warp(dest, source, size, 1);
        break;
      default:
        break;
    }
  }

  if (is_thread_zero_in_warp()) {
    nvshmem_quiet();
    end_time[idx] = wall_clock64();
  }
}

/******************************************************************************
 * HOST TESTER CLASS METHODS
 *****************************************************************************/
WaveFrontPrimitiveTester::WaveFrontPrimitiveTester(TesterArguments args)
    : Tester(args) {
  size_t buff_size = args.max_msg_size * args.num_wgs * num_warps;
  source = (char *)nvshmem_malloc(buff_size);
  dest = (char *)nvshmem_malloc(buff_size);

  if (source == nullptr || dest == nullptr) {
    std::cerr << "Error allocating memory from symmetric heap" << std::endl;
    std::cerr << "source: " << source << ", dest: " << dest << std::endl;
    if (source) {
      nvshmem_free(source);
    }
    if (dest) {
      nvshmem_free(dest);
    }
    nvshmem_global_exit(1);
  }

  for(size_t i = 0; i < buff_size; i++) {
    source[i] = static_cast<char>('a' + i % 26);
  }
}

WaveFrontPrimitiveTester::~WaveFrontPrimitiveTester() {
  nvshmem_free(source);
  nvshmem_free(dest);
}

void WaveFrontPrimitiveTester::resetBuffers(size_t size) {
  size_t buff_size = size * args.num_wgs * num_warps;
  memset(dest, '1', buff_size);
}

void WaveFrontPrimitiveTester::launchKernel(dim3 gridSize, dim3 blockSize,
                                           int loop, size_t size) {
  size_t shared_bytes = 0;

  hipLaunchKernelGGL(WaveFrontPrimitiveTest, gridSize, blockSize, shared_bytes,
                     stream, loop, args.skip, start_time, end_time,
                     source, dest, size, _type, _shmem_context,
                     wf_size);

  num_msgs = (loop + args.skip) * gridSize.x * num_warps;
  num_timed_msgs = loop * gridSize.x * num_warps;
}

void WaveFrontPrimitiveTester::verifyResults(size_t size) {
  int check_id = (_type == WAVEGetTestType || _type == WAVEGetNBITestType)
                     ? 0
                     : 1;

  if (args.myid == check_id) {
    size_t buff_size = size * args.num_wgs * num_warps;
    size_t verify_wg_size = std::min((size_t) 1024, buff_size);
    size_t verify_num_wgs = buff_size / verify_wg_size;

    hipLaunchKernelGGL(verify_results_kernel_char, verify_num_wgs, verify_wg_size, 0, stream,
                       source, dest, buff_size, verification_error);
    CHECK_HIP(hipStreamSynchronize(stream));

    if (*verification_error) {
      for (size_t i = 0; i < buff_size; i++) {
        if (dest[i] != source[i]) {
          std::cerr << "Data validation error at idx " << i << std::endl;
          std::cerr << " Got " << dest[i] << ", Expected "
                    << source[i] << std::endl;
          exit(-1);
        }
      }
      *verification_error = false;
    }
  }
}
