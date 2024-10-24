
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

void function_1() {}
void function_2() {}
void function_3() {}

void spmd2(lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args) {
  (void)lpf; // ignore lpf context variable

  EXPECT_LE(nprocs, 2);

  if (0 == pid) {
    EXPECT_EQ(sizeof(int), args.input_size);
    EXPECT_EQ(sizeof(int), args.output_size);

    int n = *(int *)args.input;
    EXPECT_LE(10, n);
    EXPECT_LT(n, 10 + 2);

    *(int *)args.output = 9;
  } else {
    EXPECT_EQ((size_t)0, args.input_size);
    EXPECT_EQ((size_t)0, args.output_size);
    EXPECT_EQ((void *)NULL, args.input);
    EXPECT_EQ((void *)NULL, args.output);
  }

  EXPECT_EQ((size_t)2, args.f_size);
  EXPECT_EQ(&function_2, args.f_symbols[0]);
  EXPECT_EQ(&function_3, args.f_symbols[1]);
}

void spmd1(lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args) {
  EXPECT_LE(nprocs, 2);

  if (0 == pid) {
    EXPECT_EQ(sizeof(int), args.input_size);
    EXPECT_EQ(sizeof(int), args.output_size);

    int n = *(int *)args.input;
    EXPECT_EQ(3, n);
    EXPECT_EQ(-1, *(int *)args.output);

    *(int *)args.output = 7;
  } else {
    EXPECT_EQ((size_t)0, args.input_size);
    EXPECT_EQ((size_t)0, args.output_size);
    EXPECT_EQ((void *)NULL, args.input);
    EXPECT_EQ((void *)NULL, args.output);
  }

  EXPECT_EQ((size_t)3, args.f_size);
  EXPECT_EQ(&function_1, args.f_symbols[0]);
  EXPECT_EQ(&function_2, args.f_symbols[1]);
  EXPECT_EQ(&function_3, args.f_symbols[2]);

  int x = 10 + pid;
  lpf_args_t newArgs;
  newArgs.input = &x;
  newArgs.input_size = sizeof(x);
  int number = -1;
  newArgs.output = &number;
  newArgs.output_size = sizeof(number);
  newArgs.f_symbols = args.f_symbols + 1;
  newArgs.f_size = args.f_size - 1;

  lpf_err_t rc = lpf_exec(lpf, 2, &spmd2, newArgs);
  EXPECT_EQ(LPF_SUCCESS, rc);

  EXPECT_EQ(9, number);
}

/**
 * \test Test nested lpf_exec() call with single argument on two processors.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_lpf_exec_nested_call_single_arg_dual_proc) {
  lpf_err_t rc = LPF_SUCCESS;
  int three = 3;
  int number = -1;
  void (*function_pointers[3])() = {&function_1, &function_2, &function_3};
  lpf_args_t args;
  args.input = &three;
  args.input_size = sizeof(three);
  args.output = &number;
  args.output_size = sizeof(number);
  args.f_symbols = function_pointers;
  args.f_size = sizeof(function_pointers) / sizeof(function_pointers[0]);

  rc = lpf_exec(LPF_ROOT, 2, &spmd1, args);
  EXPECT_EQ(LPF_SUCCESS, rc);
  EXPECT_EQ(number, 7);
}
