
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

  size_t maxMsgs = 20, maxRegs = 10;
  rc = lpf_resize_message_queue(lpf, maxMsgs);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(lpf, maxRegs);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  char buffer[21] = "Ditiseentestmettandr";
  lpf_memslot_t slots[10];

  // register 10 entries
  int i;
  for (i = 0; i < 10; ++i) {
    rc = lpf_register_global(lpf, &buffer[i * 2], sizeof(buffer[0]) * 2,
                             &slots[i]);
    EXPECT_EQ(LPF_SUCCESS, rc);
  }

  // deregister 4 in an atypical order
  int nDelRegs = 4;
  size_t delRegs[] = {8, 0, 5, 9};
  size_t otherRegs[] = {1, 2, 3, 4, 6, 7};
  for (i = 0; i < nDelRegs; ++i) {
    rc = lpf_deregister(lpf, slots[delRegs[i]]);
    EXPECT_EQ(LPF_SUCCESS, rc);
  }

  // reduce by 4 which gets accepted
  rc = lpf_resize_memory_register(lpf, maxRegs - 4);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  EXPECT_STREQ("Ditiseentestmettandr", buffer);

  // test that the remaining registrations still work
  size_t k;
  for (k = 0; k < 10; ++k) {
    int j;
    for (j = 0; j < nDelRegs; ++j) {
      if (delRegs[j] == k)
        break;
    }
    // if slot wasn't deregistered, then send something
    if (j == nDelRegs) {
      lpf_put(lpf, slots[k], 0u, (pid + k) % nprocs, slots[k], 1u,
              sizeof(buffer[0]), LPF_MSG_DEFAULT);
    }
  }

  lpf_sync(lpf, LPF_SYNC_DEFAULT);

  EXPECT_STREQ("Dittsseettstmmttandr", buffer);

  for (i = 0; i < 6; ++i)
    lpf_deregister(lpf, slots[otherRegs[i]]);
}

/**
 * \test Allocate some registers, use some and then delete a few.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_lpf_register_global_parallel_shrink) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
