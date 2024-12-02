
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
#include "gtest/gtest.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 0, 0, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );


    int a = 0;
    int c = -1;
    rc = bsplib_push_reg( bsplib, &a, sizeof( a ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_push_reg( bsplib, &c, sizeof( c ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_sync( bsplib  );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    int b = 2;
    rc = bsplib_put( bsplib, 0, &b, &a, 0, sizeof( a ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    EXPECT_EQ( 0, a );
    EXPECT_EQ( 2, b );
    EXPECT_EQ( -1, c );

    rc = bsplib_pop_reg( bsplib, &a );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_pop_reg( bsplib, &c );  // non-stack order!
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_sync( bsplib );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    EXPECT_EQ( bsplib_pid( bsplib ) == 0 ? 2 : 0, a );
    EXPECT_EQ( 2, b );
    EXPECT_EQ( -1, c );


    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Test a basic use case with bsp_push_reg and bsp_pop_reg in unsafe mode
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_pushpopreg_normal_unsafemode )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

