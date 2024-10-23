
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
  (void)args; // ignore any arguments passed through call to lpf_exec
  (void)pid;
  (void)nprocs;

  lpf_err_t rc = LPF_SUCCESS;

  // reserve space for 16 messages
  size_t maxMsgs = 32, maxRegs = 2;
  rc = lpf_resize_message_queue(lpf, maxMsgs);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(lpf, maxRegs);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  std::string buf1 = "abcdefghijklmnop";
  std::string buf2 = "ABCDEFGHIJKLMNOP";
  lpf_memslot_t slot1, slot2;

  rc = lpf_register_global(lpf, &buf1[0], sizeof(buf1), &slot1);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_register_global(lpf, &buf2[0], sizeof(buf2), &slot2);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  // resize to 0 messages again.
  maxMsgs = 0;
  rc = lpf_resize_message_queue(lpf, maxMsgs);
  EXPECT_EQ(LPF_SUCCESS, rc);

  unsigned i;
  for (i = 0; i < 16; ++i)
    lpf_put(lpf, slot1, i, (pid + 1) % nprocs, slot2, i, 1, LPF_MSG_DEFAULT);

  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  EXPECT_STREQ(buf2.c_str(), "abcdefghijklmnop");

  rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);

  lpf_deregister(lpf, slot1);
  lpf_deregister(lpf, slot2);
}

/**
 * \test Tests whether the message queues are indeed shrunk in a delayed manner
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_lpf_resize_delayed_shrinking_message_queues) {
  lpf_err_t rc = lpf_exec(LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
