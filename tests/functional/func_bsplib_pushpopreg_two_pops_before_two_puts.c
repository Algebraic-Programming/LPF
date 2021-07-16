
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
#include <lpf/bsplib.h>
#include "Test.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, 0, &bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );


    int c[2] = { 0, 0};
    int a[2];
    memset( a, 0, sizeof(a));
    rc = bsplib_push_reg( bsplib, a, sizeof( a ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_push_reg( bsplib, a, sizeof( int ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_sync( bsplib  );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_pop_reg( bsplib, a );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_pop_reg( bsplib, a );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    int b = 2;
    rc = bsplib_put( bsplib, 0, &b, a, 0, sizeof( int ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_put( bsplib, 0, c, a, 0, sizeof( c) );
    EXPECT_EQ( "%d", BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE, rc );

    EXPECT_EQ( "%d", 0, a[0] );
    EXPECT_EQ( "%d", 2, b );
    rc = bsplib_sync( bsplib );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    EXPECT_EQ( "%d", bsplib_pid( bsplib ) == 0 ? 2 : 0, a[0] );
    EXPECT_EQ( "%d", 2, b );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
}

/** 
 * \test Test a case where two pop_regs are issued before the two puts
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( func_bsplib_pushpopreg_two_pops_before_two_puts )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    return 0;
}

