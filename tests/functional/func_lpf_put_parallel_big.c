
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Test.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t arg )
{
    (void) arg;

    lpf_resize_message_queue( lpf, 2*nprocs-2 );
    lpf_resize_memory_register( lpf, 2);

    lpf_sync( lpf, LPF_SYNC_DEFAULT );

    const size_t blocksize = 99999;
    const size_t bufsize = nprocs * blocksize;
    char * srcbuf = calloc( bufsize, 1 );
    char * dstbuf = calloc( bufsize, 1 );

    lpf_memslot_t srcslot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t dstslot = LPF_INVALID_MEMSLOT;

    lpf_register_global( lpf, srcbuf, bufsize, &srcslot);
    lpf_register_global( lpf, dstbuf, bufsize, &dstslot);

    int try = 0;
    for (try = 0; try < 3; ++try ) {
       lpf_err_t rc = lpf_sync( lpf, LPF_SYNC_DEFAULT ) ;
       EXPECT_EQ( "%d", LPF_SUCCESS, rc );
       size_t dstoffset = 0;
       size_t srcoffset = 0;
       unsigned p;
       for ( p = 0; p < nprocs; ++p ) {
           dstoffset = p * blocksize; 
           if (pid != p) lpf_put( lpf, srcslot, srcoffset, p, dstslot, dstoffset, blocksize, LPF_MSG_DEFAULT);
           srcoffset += blocksize;
       }
    }

    lpf_err_t rc = lpf_sync( lpf, LPF_SYNC_DEFAULT ) ;
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    lpf_deregister( lpf, srcslot );
    lpf_deregister( lpf, dstslot );
}


/** 
 * \test Test lpf_put by sending messages larger than max MPI message size
 * \pre P >= 2
 * \note Extra lpfrun parameters: -max-mpi-msg-size 500
 * \return Exit code: 0
 */
TEST( func_lpf_put_parallel_big )
{
    (void) argc;
    (void) argv;

    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, LPF_NO_ARGS );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    return 0;
}
