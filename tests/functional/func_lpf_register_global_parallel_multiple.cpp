
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

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) args; // ignore args parameter
    EXPECT_LT( (int) pid, (int) nprocs );
    char a[1] = { 'i' };
    char b[2] = { 'p', 'q' };
    char c[3] = { 'a', 'b', 'c'};
    char d[6] = "hallo";

    lpf_memslot_t aSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t bSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t cSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t dSlot = LPF_INVALID_MEMSLOT;
    lpf_err_t rc = LPF_SUCCESS;

    rc = lpf_resize_message_queue( lpf, 1);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, 4);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_register_global( lpf, &a, sizeof(a), &aSlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_register_global( lpf, &c, sizeof(c), &cSlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_register_global( lpf, &d, sizeof(d), &dSlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    EXPECT_EQ( 'i', a[0]);
    EXPECT_EQ( 'p', b[0]);
    EXPECT_EQ( 'q', b[1]);
    EXPECT_EQ( 'a', c[0]);
    EXPECT_EQ( 'b', c[1]);
    EXPECT_EQ( 'c', c[2]);
    EXPECT_EQ( 'h', d[0]);
    EXPECT_EQ( 'a', d[1]);
    EXPECT_EQ( 'l', d[2]);
    EXPECT_EQ( 'l', d[3]);
    EXPECT_EQ( 'o', d[4]);
    EXPECT_EQ( '\0', d[5]);

    if ( 0 == pid )
    {
        rc = lpf_register_local( lpf, &b, sizeof(b), &bSlot );
        EXPECT_EQ( LPF_SUCCESS, rc );

        rc = lpf_put( lpf, bSlot, 1u * sizeof(b[0]), 
                1u, dSlot, 2u*sizeof(d[0]), sizeof(b[0]), LPF_MSG_DEFAULT );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    EXPECT_EQ( 'i', a[0]);
    EXPECT_EQ( 'p', b[0]);
    EXPECT_EQ( 'q', b[1]);
    EXPECT_EQ( 'a', c[0]);
    EXPECT_EQ( 'b', c[1]);
    EXPECT_EQ( 'c', c[2]);
    EXPECT_EQ( 'h', d[0]);
    EXPECT_EQ( 'a', d[1]);
    EXPECT_EQ( pid == 1 ? 'q' : 'l', d[2]);
    EXPECT_EQ( 'l', d[3]);
    EXPECT_EQ( 'o', d[4]);
    EXPECT_EQ( '\0', d[5]);


    lpf_deregister( lpf, dSlot );
    lpf_deregister( lpf, cSlot );
    if (pid == 0) lpf_deregister( lpf, bSlot );
    lpf_deregister( lpf, aSlot );
}

/** 
 * \test Test registering global variables in the context of a parallel program.
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( API, func_lpf_register_global_parallel_multiple )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc);
}
