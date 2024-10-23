
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

  lpf_machine_t subMachine = LPF_INVALID_MACHINE;
  lpf_err_t rc = lpf_probe(lpf, &subMachine);
  EXPECT_EQ(LPF_SUCCESS, rc);

  EXPECT_EQ(nprocs, subMachine.p);
  EXPECT_EQ(1u, subMachine.free_p);
  EXPECT_LT(0.0, (*(subMachine.g))(1, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(subMachine.l))(1, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(subMachine.g))(subMachine.p, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(subMachine.l))(subMachine.p, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0,
            (*(subMachine.g))(subMachine.p, (size_t)(-1), LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0,
            (*(subMachine.l))(subMachine.p, (size_t)(-1), LPF_SYNC_DEFAULT));

  if (0 == pid) {
    lpf_machine_t *machine = (lpf_machine_t *)args.input;
    EXPECT_EQ(args.input_size, sizeof(lpf_machine_t));

    EXPECT_EQ(nprocs, machine->p);
    EXPECT_EQ(nprocs, machine->free_p);
  }
}

/**
 * \test Test lpf_probe function on a parallel section where all processes are
 * used immediately. \note Extra lpfrun parameters: -probe 1.0 \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_lpf_probe_parallel_full) {
  lpf_err_t rc = LPF_SUCCESS;

  lpf_machine_t machine = LPF_INVALID_MACHINE;

  rc = lpf_probe(LPF_ROOT, &machine);
  EXPECT_EQ(LPF_SUCCESS, rc);

  EXPECT_LE(1u, machine.p);
  EXPECT_LE(1u, machine.free_p);
  EXPECT_LE(machine.p, machine.free_p);
  EXPECT_LT(0.0, (*(machine.g))(1, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(machine.l))(1, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(machine.g))(machine.p, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(machine.l))(machine.p, 0, LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(machine.g))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT));
  EXPECT_LT(0.0, (*(machine.l))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT));

  lpf_args_t args;
  args.input = &machine;
  args.input_size = sizeof(machine);
  args.output = NULL;
  args.output_size = 0;
  args.f_symbols = NULL;
  args.f_size = 0;

  rc = lpf_exec(LPF_ROOT, LPF_MAX_P, &spmd, args);
  EXPECT_EQ(LPF_SUCCESS, rc);
}
