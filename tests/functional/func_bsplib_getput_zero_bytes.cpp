
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

#include <lpf/bsplib.h>

#include "gtest/gtest.h"

#include <stdint.h>

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, 3, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    char x = 'x';
    char y = 'y';
    rc = bsplib_push_reg(bsplib, &x, sizeof( x ) );
    rc = bsplib_sync(bsplib);

    const size_t zero_bytes = 0;
    
    // the following puts and gets are no-ops, despite some
    // other illegal arguments, because they all write zero bytes
    rc = bsplib_put(bsplib, 0, &y, NULL, 10, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_put(bsplib, 0, &y, &x, -5, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_put(bsplib, 0, &y, &y, 0, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_put(bsplib, 0, &y, &x, sizeof(x)+1, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_hpput(bsplib, 0, &y, NULL, 10, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_hpput(bsplib, 0, &y, &x, -5, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_hpput(bsplib, 0, &y, &y, 0, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_hpput(bsplib, 0, &y, &x, sizeof(x)+1, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_get(bsplib, 0, NULL, 10, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_get(bsplib, 0, &x, -5, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_get(bsplib, 0, &y, 0, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_get(bsplib, 0, &x, sizeof(x)+1, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_hpget(bsplib, 0, NULL, 10, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_hpget(bsplib, 0, &x, -5, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_hpget(bsplib, 0, &y, 0, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_hpget(bsplib, 0, &x, sizeof(x)+1, &y, zero_bytes);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );


    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Any put or get with zero bytes is legal
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_getput_zero_bytes )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

