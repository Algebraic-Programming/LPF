
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
#include <string.h>

void spmd(lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args) {
  (void)lpf; // ignore lpf context variable

  EXPECT_LE((lpf_pid_t)1, nprocs);
  EXPECT_LE((lpf_pid_t)0, pid);
  EXPECT_EQ((size_t)0, args.input_size);
  EXPECT_EQ((size_t)0, args.output_size);
  EXPECT_EQ((void *)NULL, args.input);
  EXPECT_EQ((void *)NULL, args.output);
}

/**
 * \test Test single lpf_exec() call without arguments on maximum number of
 * processes \pre P >= 1 \return Exit code: 0
 */
TEST(API, func_lpf_exec_single_call_no_arg_max_proc) {
  lpf_err_t rc = LPF_SUCCESS;
  rc = lpf_exec(LPF_ROOT, LPF_MAX_P, &spmd, LPF_NO_ARGS);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
