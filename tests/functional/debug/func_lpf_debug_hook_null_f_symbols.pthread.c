
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
#include <lpf/pthread.h>
#include "Test.h"

#include <pthread.h>
#include <unistd.h>

pthread_key_t pid_key;

struct thread_local_data {
    long P, s;
};

void lpf_spmd( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{ (void) ctx; (void) pid; (void) nprocs; (void) args; }

void * pthread_spmd( void * _data ) {
    EXPECT_NE( "%p", _data, NULL );

    const struct thread_local_data data = * ((struct thread_local_data*) _data);
    const int pts_rc = pthread_setspecific( pid_key, _data );
    lpf_args_t args;
    args.input = _data;
    args.input_size  = sizeof(data);
    args.output =  NULL;
    args.output_size = 0;
    args.f_symbols = NULL;
    args.f_size = 5;
    lpf_init_t init;
    lpf_err_t rc = LPF_SUCCESS;

    EXPECT_EQ( "%d", pts_rc, 0 );

    rc = lpf_pthread_initialize(
        (lpf_pid_t)data.s,
        (lpf_pid_t)data.P,
        &init
    );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    rc = lpf_hook( init, &lpf_spmd, args );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    rc = lpf_pthread_finalize( init );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    return NULL;
}

/** 
 * \test Tests lpf_hook on pthread implementation with NULL f_symbols
 * \pre P <= 1
 * \pre P >= 1
 * \return Message: NULL f_symbols argument while f_size is non-zero
 * \return Exit code: 6
 */
TEST( func_lpf_hook_null_f_symbols )
{
    long k = 0;
    const long P = sysconf( _SC_NPROCESSORS_ONLN );

    const int ptc_rc = pthread_key_create( &pid_key, NULL );
    EXPECT_EQ( "%d", ptc_rc, 0 );

    pthread_t * const threads = (pthread_t*) malloc( P * sizeof(pthread_t) );
    EXPECT_NE( "%p", threads, NULL );

    struct thread_local_data * const data = (struct thread_local_data*) malloc( P * sizeof(struct thread_local_data) );
    EXPECT_NE( "%p", data, NULL );

    for( k = 0; k < P; ++k ) {
        data[ k ].P = P;
        data[ k ].s = k;
        const int rval = pthread_create( threads + k, NULL, &pthread_spmd, data + k );
        EXPECT_EQ( "%d", rval, 0 );
    }

    for( k = 0; k < P; ++k ) {
        const int rval = pthread_join( threads[ k ], NULL );
        EXPECT_EQ( "%d", rval, 0 );
    }

    const int ptd_rc = pthread_key_delete( pid_key );
    EXPECT_EQ( "%d", ptd_rc, 0 );

    return 0;
}


