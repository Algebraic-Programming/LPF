
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

#include <math.h>

void spmd( lpf_t ctx, const lpf_pid_t s, const lpf_pid_t p, const lpf_args_t args )
{
    (void) args; // ignore any arguments passed through call to lpf_exec
    lpf_memslot_t src_slot, dst_slot;
    lpf_coll_t coll;
    lpf_err_t rc;

    rc = lpf_resize_message_queue( ctx, 2*p - 2);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( ctx, 3 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    const size_t byte_size = (1 << 19) / sizeof(double);
    const size_t      size = byte_size / sizeof(double);
    double * data              = new double[size];
    EXPECT_NE( nullptr, data );

    for( size_t i = 0; i < size; ++i ) {
        data[ i ] = s * size + i;
    }

    double * allgatheredData = new double[size * p/2];
    rc = lpf_register_global( ctx, data, size * sizeof(double), &src_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_register_global( ctx, allgatheredData, p/2 * size * sizeof(double), &dst_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    /**
     * Choose a subset of peers
     */
    lpf_pid_t peers[p/2];

    for (lpf_pid_t _k = 0; _k < p/2 ; _k++) {
        peers[_k] = _k;
    }

    // explicitly set this collective to include p/2 peers only!
    rc = lpf_collectives_init( ctx, s, p/2, peers, 1, p/2 * sizeof(double), 0, &coll );
    EXPECT_EQ( LPF_SUCCESS, rc );

    // modified collective iterating over subset of processes
    rc = lpf_subcomm_allgather( coll, src_slot, dst_slot, sizeof(double), false );
    EXPECT_EQ( LPF_SUCCESS, rc );

    // modified sync waiting on a subset of received messages
    rc = lpf_counting_sync_per_slot( ctx, LPF_SYNC_DEFAULT, dst_slot, 0, p/2);
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_collectives_destroy( coll );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_deregister( ctx, src_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_deregister( ctx, dst_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    delete[] data;

}

/** 
 * \test Performs an allgather on a subset of processes, relying on zero engine 
 * semantics 
 * \pre P >= 8
 * \pre P <= 8
 * \return Exit code: 0
 */
TEST( COLL, func_lpf_subcomm_allgather)
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

