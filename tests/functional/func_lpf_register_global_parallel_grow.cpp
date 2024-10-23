
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

  size_t maxMsgs = 20, maxRegs = 7;
  rc = lpf_resize_message_queue(lpf, maxMsgs);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(lpf, maxRegs);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  char buffer[21] = "Ditiseentestmettandr";
  lpf_memslot_t slots[10];

  int i;
  for (i = 0; i < 5; ++i) {
    rc = lpf_register_global(lpf, &buffer[i * 2], sizeof(buffer[0]) * 2,
                             &slots[i]);
    EXPECT_EQ(LPF_SUCCESS, rc);
  }

  rc = lpf_resize_memory_register(lpf, maxRegs + 3);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  for (i = 0; i < 5; ++i) {
    rc = lpf_register_global(lpf, &buffer[(i + 5) * 2], sizeof(buffer[0]) * 2,
                             &slots[i + 5]);
    EXPECT_EQ(LPF_SUCCESS, rc);
  }

  EXPECT_STREQ("Ditiseentestmettandr", buffer);

  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  for (i = 0; i < 10; ++i) {
    lpf_put(lpf, slots[i], 0u, (pid + i) % nprocs, slots[i], 1u,
            sizeof(buffer[0]), LPF_MSG_DEFAULT);
  }

  lpf_sync(lpf, LPF_SYNC_DEFAULT);

  EXPECT_STREQ("DDttsseettssmmttaadd", buffer);

  for (i = 0; i < 10; ++i)
    lpf_deregister(lpf, slots[i]);
}

/**
 * \test Allocate some registers, use some, and allocate some more.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_lpf_register_global_parallel_grow) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
