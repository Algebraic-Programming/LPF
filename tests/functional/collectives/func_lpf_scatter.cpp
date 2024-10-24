
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
  lpf_memslot_t data_slot;
  lpf_coll_t coll;
  lpf_err_t rc;

  rc = lpf_resize_message_queue(ctx, p - 1);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(ctx, 2);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  const size_t size = (1 << 19);
  char *data = NULL;
  if (s == p / 2) {
    data = new char[size * p];
  } else {
    data = new char[size];
  }
  EXPECT_NE(nullptr, data);

  if (s == p / 2) {
    for (size_t i = 0; i < size * p; ++i) {
      data[i] = (char)i;
    }
    rc = lpf_register_global(ctx, data, p * size, &data_slot);
  } else {
    for (size_t i = 0; i < size; ++i) {
      data[i] = -1;
    }
    rc = lpf_register_global(ctx, data, size, &data_slot);
  }
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_collectives_init(ctx, s, p, 1, 0, (1 << 19), &coll);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_scatter(coll, data_slot, data_slot, size, p / 2);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  if (s == p / 2) {
    for (size_t i = 0; i < size * p; ++i) {
      EXPECT_EQ((char)i, data[i]);
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ((char)(s * size + i), data[i]);
    }
  }

  rc = lpf_collectives_destroy(coll);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_deregister(ctx, data_slot);
  EXPECT_EQ(LPF_SUCCESS, rc);

  delete[] data;
}

/**
 * \test Initialises one \a lpf_coll_t object, performs a scatter, and deletes
 * the \a lpf_coll_t object. \pre P >= 1 \return Exit code: 0
 */
TEST(COLL, func_lpf_scatter) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
