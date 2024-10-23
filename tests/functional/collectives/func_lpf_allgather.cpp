
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
#include <lpf/collectives.h>
#include <lpf/core.h>

void spmd(lpf_t ctx, const lpf_pid_t s, lpf_pid_t p, lpf_args_t args) {
  (void)args; // ignore any arguments passed through call to lpf_exec
  const size_t size = (1 << 19);

  lpf_memslot_t src_slot, dst_slot;
  lpf_coll_t coll;
  lpf_err_t rc;

  rc = lpf_resize_message_queue(ctx, 2 * p);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(ctx, 3);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  char *src = new char[size];
  EXPECT_NE(nullptr, src);

  char *dst = new char[p * size];
  EXPECT_NE(nullptr, dst);

  for (size_t i = 0; i < size; ++i) {
    src[i] = (char)s;
  }
  rc = lpf_register_global(ctx, src, size, &src_slot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  for (size_t i = 0; i < p * size; ++i) {
    dst[i] = -1;
  }
  rc = lpf_register_global(ctx, dst, p * size, &dst_slot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_collectives_init(ctx, s, p, 1, 0, p * size, &coll);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_allgather(coll, src_slot, dst_slot, size, true);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ((char)s, src[i]);
  }

  for (lpf_pid_t k = 0; k < p; ++k) {
    for (size_t i = 0; i < size; ++i) {
      const size_t index = k * size + i;
      if (k == s) {
        EXPECT_EQ((char)-1, dst[index]);
      } else {
        EXPECT_EQ((char)k, dst[index]);
      }
    }
  }

  rc = lpf_collectives_destroy(coll);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_deregister(ctx, src_slot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_deregister(ctx, dst_slot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  delete[] src;
  delete[] dst;
}

/**
 * \test Initialises one \a lpf_coll_t object, performs an allgather, and
 * deletes the \a lpf_coll_t object. \pre P >= 1 \return Exit code: 0
 */
TEST(COLL, func_lpf_allgather) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
