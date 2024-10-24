
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

/**
 * \test Test lpf_resize function on LPF_ROOT allocating nothing
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_lpf_resize_root_zero) {
  lpf_err_t rc = LPF_SUCCESS;
  size_t maxMsgs = 0, maxRegs = 0;
  rc = lpf_resize_message_queue(LPF_ROOT, maxMsgs);
  EXPECT_EQ(LPF_SUCCESS, rc);
  rc = lpf_resize_memory_register(LPF_ROOT, maxRegs);
  EXPECT_EQ(LPF_SUCCESS, rc);

  rc = lpf_sync(LPF_ROOT, LPF_SYNC_DEFAULT);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
