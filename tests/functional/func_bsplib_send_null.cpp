
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
#include "Test.h"

#include <stdint.h>

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, 0, &bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    size_t tagSize = sizeof( int );
    size_t nmsg = -1, bytes = -1;
    size_t status = -1;
    size_t oldTagSize = 0;

    // set tag size which go in effect next super-step
    oldTagSize = bsplib_set_tagsize(bsplib, tagSize );
    EXPECT_EQ( "%zu", ( size_t ) 0, oldTagSize );

    const int x = 0x12345678;
    //const int y = 0x87654321;
    const int z = 0x12344321;

    rc = bsplib_send(bsplib, 0, NULL, &x, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_send(bsplib, 1, &z, NULL, 0 );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_qsize(bsplib, &nmsg, &bytes );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    EXPECT_EQ( "%zu", ( size_t ) ( bsplib_pid(bsplib) == 0 ? bsplib_nprocs(bsplib) : 0 ),
        nmsg );
    EXPECT_EQ( "%zu", ( size_t ) ( bsplib_pid(bsplib) ==
            0 ? bsplib_nprocs(bsplib) * sizeof( x ) : 0 ), bytes );

    int tag = -1;
    size_t nMessages = 0;
    while ( rc = bsplib_get_tag(bsplib, &status, &tag ), status != (size_t) -1 )
    {
        EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

        EXPECT_EQ( "%d", -1, tag );
        EXPECT_NE( "%zu", ( size_t ) -1, status );

        int a = -1;
        rc = bsplib_move(bsplib, &a, sizeof( a ) );
        EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
        ++nMessages;

        // after the move the values returned by qsize decrease
        bytes -= status;
        size_t msgs2 = -1, bytes2 = -1;
        rc = bsplib_qsize(bsplib, &msgs2, &bytes2 );
        EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
        EXPECT_EQ( "%zu", bytes, bytes2 );
        EXPECT_EQ( "%zu", msgs2, bsplib_nprocs(bsplib) - nMessages );
        EXPECT_EQ( "%zu", ( size_t ) sizeof( x ), status );
        EXPECT_EQ( "%d", x, a );
    }

    EXPECT_EQ( "%u", 
            bsplib_pid(bsplib) == 0 ? bsplib_nprocs(bsplib) : 0,
           (unsigned) nMessages );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_qsize(bsplib, &nmsg, &bytes );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    EXPECT_EQ( "%zu", ( size_t ) ( bsplib_pid(bsplib) == 1 ? bsplib_nprocs(bsplib) : 0 ),
        nmsg );
    EXPECT_EQ( "%zu", ( size_t ) 0, bytes );

    tag = -1;
    nMessages = 0;
    while ( rc = bsplib_get_tag(bsplib, &status, &tag ), status != (size_t) -1 )
    {
        EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
        EXPECT_EQ( "%d", z, tag );
        EXPECT_NE( "%zu", ( size_t ) -1, status );

        int a = -1;
        rc = bsplib_move(bsplib, &a, sizeof( a ) );
        EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
        ++nMessages;

        // after the move the values returned by qsize decrease
        bytes -= status;
        size_t msgs2 = -1, bytes2 = -1;
        rc = bsplib_qsize(bsplib, &msgs2, &bytes2 );
        EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
        EXPECT_EQ( "%zu", bytes, bytes2 );
        EXPECT_EQ( "%zu", msgs2, bsplib_nprocs(bsplib) - nMessages );
        EXPECT_EQ( "%zu", ( size_t ) 0, status );
        EXPECT_EQ( "%d", -1, a );
    }

    EXPECT_EQ( "%u", 
            bsplib_pid(bsplib) == 1 ? bsplib_nprocs(bsplib) : 0, 
            (unsigned) nMessages );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests bsplib_send with an empty message
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( func_bsplib_send_null )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    return 0;
}

