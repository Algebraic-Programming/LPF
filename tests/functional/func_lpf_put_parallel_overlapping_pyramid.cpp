
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
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

#include "gtest/gtest.h"
#include <lpf/core.h>

void spmd(lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args) {
  (void)args; // ignore args parameter
  lpf_err_t rc = LPF_SUCCESS;
  const size_t MTU = 2000;
  const size_t n = 2 * nprocs * MTU;
  size_t i;
  int *xs, *ys;
  ys = (int *)malloc(sizeof(ys[0]) * n);
  xs = (int *)malloc(sizeof(xs[0]) * n);
  for (i = 0; i < n; ++i) {
    xs[i] = i * n + pid;
    ys[i] = 0;
  }

  rc = lpf_resize_message_queue(lpf, nprocs + 1);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(lpf, 2);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  lpf_memslot_t xslot = LPF_INVALID_MEMSLOT;
  lpf_memslot_t yslot = LPF_INVALID_MEMSLOT;
  rc = lpf_register_local(lpf, xs, sizeof(xs[0]) * n, &xslot);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_register_global(lpf, ys, sizeof(ys[0]) * n, &yslot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  // Check that data is OK.
  for (i = 0; i < n; ++i) {
    EXPECT_EQ((int)(i * n + pid), xs[i]);
    EXPECT_EQ(0, ys[i]);
  }

  // Each processor copies his row to processor zero.
  size_t start = pid;
  size_t end = 2 * nprocs - pid;
  rc = lpf_put(lpf, xslot, start * MTU * sizeof(xs[0]), 0, yslot,
               start * MTU * sizeof(xs[0]), (end - start) * MTU * sizeof(xs[0]),
               LPF_MSG_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  if (0 == pid) {
    /**
     *     The array 'xs' is sent to processor 0 as a pyramid. Writes
     *     will overlap in the middle of the block. On those
     *     places a specific sequential ordering has to be inplace.
     *
     *     proc 0:  0123456789
     *     proc 1:   abcdefgh
     *     proc 2:    pqrstu
     *     proc 3:     vwxy
     *     proc 4:      z.
     *
     *     --------------------
     *     proc 0:  0apvz.yuh9
     *     or p 1:  0123456789
     *     or something in between
     *
     **/
    for (i = 0; i < n; i += MTU) {
      // check the contents of a block
      int pid1 = ys[i] - xs[i];
      int pid2 = ys[n - i - 1] - xs[n - i - 1];
      EXPECT_EQ(pid1, pid2);
      EXPECT_LE(pid1, (int)i);
      EXPECT_LE(pid1, (int)(n - i - 1));

      EXPECT_LE(pid1, (int)nprocs);

      // check that all values in the block are from the same processor
      size_t j;
      for (j = 0; j < MTU; ++j) {
        EXPECT_EQ(pid1, ys[i + j] - xs[i + j]);
        EXPECT_EQ(pid1, ys[n - i - 1 - j] - xs[n - i - 1 - j]);
      }
    }
  } else {
    // on the other processors nothing has changed
    for (i = 0; i < n; ++i) {
      EXPECT_EQ((int)(i * n + pid), xs[i]);
      EXPECT_EQ(0, ys[i]);
    }
  }

  rc = lpf_deregister(lpf, xslot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_deregister(lpf, yslot);
  EXPECT_EQ(LPF_SUCCESS, rc);
}

/**
 * \test Test lpf_put writing to partial overlapping memory areas from multiple
 * processors. \pre P >= 1 \return Exit code: 0
 */
TEST(API, func_lpf_put_parallel_overlapping_pyramid) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
