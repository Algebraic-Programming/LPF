
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
#include <lpf/bsplib.h>
#include <lpf/core.h>

#include <stdint.h>

void spmd(lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args) {
  (void)args; // ignore any arguments passed through call to lpf_exec

  bsplib_err_t rc = BSPLIB_SUCCESS;

  bsplib_t bsplib;
  rc = bsplib_create(lpf, pid, nprocs, 1, 0, &bsplib);
  EXPECT_EQ(BSPLIB_SUCCESS, rc);

  int i, j;
  const int m = 100;
  const int n = m * (m + 1) / 2;
  uint32_t *memory = (uint32_t *)malloc(2 + n * sizeof(uint32_t));
  uint32_t *array = memory + 2;
  uint32_t value[m];
  rc = bsplib_push_reg(bsplib, value, sizeof(value));
  EXPECT_EQ(BSPLIB_SUCCESS, rc);

  for (i = 0; i < n; ++i) {
    memory[i] = 0xAAAAAAAAu;
  }

  for (i = 0; i < m; ++i) {
    value[i] = 0x12345678;
  }

  rc = bsplib_sync(bsplib);
  EXPECT_EQ(BSPLIB_SUCCESS, rc);

  for (i = 1, j = 0; i <= m; j += i, ++i) {
    rc = bsplib_hpget(bsplib, (bsplib_pid(bsplib) + 1) % bsplib_nprocs(bsplib),
                      value, 0, array + j, i * sizeof(uint32_t));
    EXPECT_EQ(BSPLIB_SUCCESS, rc);
  }
  rc = bsplib_pop_reg(bsplib, value);
  EXPECT_EQ(BSPLIB_SUCCESS, rc);

  rc = bsplib_sync(bsplib);
  EXPECT_EQ(BSPLIB_SUCCESS, rc);

  for (i = 0; i < n; ++i) {
    if (i < 2) {
      EXPECT_EQ(0xAAAAAAAAu, memory[i]);
    } else {
      EXPECT_EQ(0x12345678u, memory[i]);
    }
  }

  for (i = 0; i < m; ++i) {
    EXPECT_EQ(0x12345678u, value[i]);
  }

  rc = bsplib_destroy(bsplib);
  EXPECT_EQ(BSPLIB_SUCCESS, rc);

  free(memory);
}

/**
 * \test Tests sending a lot of messages through bsp_hpget
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_bsplib_hpget_many) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
