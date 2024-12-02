
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
#include <lpf/collectives.h>
#include "gtest/gtest.h"

#include <stdbool.h>

void spmd( lpf_t ctx, lpf_pid_t s, lpf_pid_t p, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec
    lpf_coll_t coll1, coll2, coll3, coll4, coll5;
    lpf_err_t rc;

    rc = lpf_resize_memory_register( ctx, 5 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    //make sure the base case is OK
    rc = lpf_collectives_init( ctx, s, p, (1<<7), (1<<7), (1<<7), &coll1 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    //now let us create some overflows
    const size_t tooBig = (size_t)(-1);

    //overflow in the number of calls: may or may not be encountered by an implementation:
    const lpf_err_t rc1 = lpf_collectives_init( ctx, s, p, tooBig, (1<<7), (1<<7), &coll2 );
    bool success = (rc1 == LPF_SUCCESS) || (rc1 == LPF_ERR_OUT_OF_MEMORY);
    EXPECT_EQ( true, success );

    //overflow in the element size required for reduction buffers: an implementation MUST detect this:
    rc = lpf_collectives_init( ctx, s, p, (1<<7), tooBig, (1<<7), &coll3 );
    EXPECT_EQ( LPF_ERR_OUT_OF_MEMORY, rc );

    //overflow in the collective buffer size: may or may not be encountered by an implementation:
    const lpf_err_t rc2 = lpf_collectives_init( ctx, s, p, (1<<7), (1<<7), tooBig, &coll4 );
    success = (rc2 == LPF_SUCCESS) || (rc2 == LPF_ERR_OUT_OF_MEMORY);
    EXPECT_EQ( true, success );

    //overflow that if not detected would lead to a very small buffer: an implementation MUST detect this:
    if( p > 1 ) {
        rc = lpf_collectives_init( ctx, s, p, (1<<7), tooBig / p + 1, (1<<7), &coll5 );
        EXPECT_EQ( LPF_ERR_OUT_OF_MEMORY, rc );
    }

    rc = lpf_collectives_destroy( coll1 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    if( rc1 == LPF_SUCCESS ) {
        rc = lpf_collectives_destroy( coll2 );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }

    if( rc2 == LPF_SUCCESS ) {
        rc = lpf_collectives_destroy( coll4 );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }
}

/** 
 * \test Checks four ways in which a buffer could overflow; two of them MUST be detected-- for the other two ways this test accepts an LPF_ERR_OUT_OF_MEMORY and accepts a LPF_SUCCESS.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( COLL, func_lpf_collectives_init_overflow )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

