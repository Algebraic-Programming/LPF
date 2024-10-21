
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

#include <lpf/core.h>
#include <string.h>
#include "gtest/gtest.h"

#define SAMPLE_INPUT_ARG "This is an input argument"
#define SAMPLE_INPUT_ARG_LENGTH 10

#define SAMPLE_OUTPUT_ARG "Some output"
#define SAMPLE_OUTPUT_ARG_LENGTH 9


void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) lpf; // ignore lpf context variable
    EXPECT_EQ( 1, (int) nprocs );
    EXPECT_EQ( 0, (int) pid);
    
    EXPECT_EQ( 0, memcmp( args.input, SAMPLE_INPUT_ARG, SAMPLE_INPUT_ARG_LENGTH ) );
    memcpy( args.output, SAMPLE_OUTPUT_ARG, SAMPLE_OUTPUT_ARG_LENGTH );
}


/** 
 * \test Test single lpf_exec() call with a single arg on a single processor
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_lpf_exec_single_call_single_arg_single_proc )
{
    lpf_err_t rc = LPF_SUCCESS;
    char input_arg[] = SAMPLE_INPUT_ARG;
    char output_arg[ SAMPLE_OUTPUT_ARG_LENGTH ];
    lpf_args_t args;
    args.input = input_arg;
    args.input_size = SAMPLE_INPUT_ARG_LENGTH;
    args.output = output_arg;
    args.output_size = SAMPLE_OUTPUT_ARG_LENGTH;
    args.f_symbols = NULL;
    args.f_size = 0;

    rc = lpf_exec( LPF_ROOT, 1, &spmd, args );
    EXPECT_EQ( rc, LPF_SUCCESS );

    EXPECT_EQ( 0, memcmp( args.output, SAMPLE_OUTPUT_ARG, SAMPLE_OUTPUT_ARG_LENGTH ) );
}
