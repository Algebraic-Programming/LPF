
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
#include "Test.h"

#include <math.h>

void elementwise_add( const size_t n, const void * const _in, void * const _out ) {
          double * const out   =       (double*) _out;
    const double * const array = (const double*) _in;
    for( size_t i = 0; i < n; ++i ) {
        out[ i ] += array[ i ];
    }
}

void spmd( lpf_t ctx, const lpf_pid_t s, const lpf_pid_t p, const lpf_args_t args )
{
    (void) args; // ignore any arguments passed through call to lpf_exec
    lpf_memslot_t data_slot;
    lpf_coll_t coll;
    lpf_err_t rc;

    rc = lpf_resize_message_queue( ctx, 2*p );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( ctx, 2 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    const size_t byte_size = (1 << 19) / sizeof(double);
    const size_t      size = byte_size / sizeof(double);
    double * data = malloc( byte_size );
    EXPECT_NE( "%p", NULL, data );

    for( size_t i = 0; i < size; ++i ) {
        data[ i ] = s;
    }

    rc = lpf_register_global( ctx, data, byte_size, &data_slot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_init( ctx, s, p, 1, 0, byte_size, &coll );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_combine( coll, data, data_slot, size, sizeof(double), &elementwise_add, p / 2 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    if( s == p / 2 ) {
        for( size_t i = 0; i < size; ++i ) {
              const double num = (double)(coll.P) / 2.0;
              const double max = (double)(coll.P-1);
              EXPECT_EQ( "%lf",  num * max, data[i] );
        }
    } else {
        //standard declares contents of data as undefined
    }

    rc = lpf_collectives_destroy( coll );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_deregister( ctx, data_slot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    free( data );
}

/** 
 * \test Initialises one \a lpf_coll_t objects, performs a combine, and deletes the \a lpf_coll_t object.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( func_lpf_combine )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    return 0;
}

