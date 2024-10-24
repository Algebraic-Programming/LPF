
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

long max(long a, long b) { return a < b ? b : a; }

void spmd(lpf_t ctx, lpf_pid_t s, lpf_pid_t p, lpf_args_t args) {
  (void)args; // ignore any arguments passed through call to lpf_exec
  lpf_memslot_t data_slot;
  lpf_coll_t coll;
  lpf_err_t rc;

  rc = lpf_resize_message_queue(ctx, max(p + 1, 2 * ((long)p) - 3));
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(ctx, 2);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  const long size = 11;
  char *data = new char[size];
  EXPECT_NE(nullptr, data);

  if (s == p / 2) {
    for (long i = 0; i < size; ++i) {
      data[i] = -1;
    }
  } else {
    for (long i = 0; i < size; ++i) {
      data[i] = +1;
    }
  }

  rc = lpf_register_global(ctx, data, size, &data_slot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_collectives_init(ctx, s, p, 1, 0, size, &coll);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_broadcast(coll, data_slot, data_slot, size, p / 2);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  for (long i = 0; i < size; ++i) {
    EXPECT_EQ((char)-1, data[i]);
  }

  rc = lpf_collectives_destroy(coll);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_deregister(ctx, data_slot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  delete[] data;
}

/**
 * \test Broadcasts an object whose size > P is not (easily) divisible by P but
 * also small so that in the second phase of 2-phase broadcast have nothing to
 * send. \pre P >= 2 \return Exit code: 0
 */
TEST(COLL, func_lpf_broadcast_small_prime_size_object) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
