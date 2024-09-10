
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

void min( const size_t n, const void * const _in, void * const _out ) {
    double * const out = (double*) _out;
    const double * const array = (const double*) _in;
    for( size_t i = 0; i < n; ++i ) {
        if( array[ i ] < *out ) {
            *out = array[ i ];
        } 
    }
}

void spmd( lpf_t ctx, const lpf_pid_t s, const lpf_pid_t p, const lpf_args_t args )
{
    (void) args; // ignore any arguments passed through call to lpf_exec
    lpf_memslot_t elem_slot;
    lpf_coll_t coll;
    lpf_err_t rc;

    rc = lpf_resize_message_queue( ctx, 2*p - 2);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( ctx, 2 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    double reduced_value       = INFINITY;
    const size_t byte_size = (1 << 19) / sizeof(double);
    const size_t      size = byte_size / sizeof(double);
    double * data              = new double[size];
    EXPECT_NE( nullptr, data );

    for( size_t i = 0; i < size; ++i ) {
        data[ i ] = s * size + i;
    }

    rc = lpf_register_global( ctx, &reduced_value, sizeof(double), &elem_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_collectives_init( ctx, s, p, 1, sizeof(double), 0, &coll );
    EXPECT_EQ( LPF_SUCCESS, rc );

    min( size, data, &reduced_value );
    rc = lpf_allreduce( coll, &reduced_value, elem_slot, sizeof(double), &min );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    EXPECT_EQ( 0.0, reduced_value );

    rc = lpf_collectives_destroy( coll );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_deregister( ctx, elem_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    delete data;
}

/** 
 * \test Initialises one \a lpf_coll_t objects, performs an allreduce, and deletes the \a lpf_coll_t object.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( COLL, func_lpf_zero_cost_sync )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

