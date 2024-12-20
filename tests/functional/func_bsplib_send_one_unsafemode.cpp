
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
    rc = bsplib_create( lpf, pid, nprocs, 0, 0, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    size_t tagSize = sizeof( int );
    size_t nmsg = -1, bytes = -1;
    size_t oldTagSize = 0;

    // set tag size which go in effect next super-step
    oldTagSize = bsplib_set_tagsize(bsplib, tagSize );
    EXPECT_EQ(  ( size_t ) 0, oldTagSize );

    const int x = 0x12345678;
    //const int y = 0x87654321;

    rc = bsplib_send(bsplib, 0, NULL, &x, sizeof( x ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_qsize(bsplib, &nmsg, &bytes );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    EXPECT_EQ(  ( size_t ) ( bsplib_pid(bsplib) == 0 ? bsplib_nprocs(bsplib) : 0 ),
        nmsg );
    EXPECT_EQ(  ( size_t ) ( bsplib_pid(bsplib) ==
            0 ? bsplib_nprocs(bsplib) * sizeof( x ) : 0 ), bytes );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests bsplib_send with just one message in unsafe mode
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( API, func_bsplib_send_one_unsafemode)
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

