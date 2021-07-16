
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct {
    int expected_nprocs;
    int actual_nprocs;
    int error;
} params_t;


void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    unsigned i;
    lpf_resize_message_queue( lpf, 2*nprocs );
    lpf_resize_memory_register( lpf, 2 );

    params_t params;
    if (pid == 0)  {
        assert( args.input_size == sizeof(params));
        memcpy( &params, args.input, sizeof(params));
    }

    char * mem = calloc( nprocs, sizeof(char));
    for (i = 0; i < nprocs; ++i)
        mem[i] = pid + 'A';

    lpf_sync( lpf, LPF_SYNC_DEFAULT );

    lpf_memslot_t params_slot = LPF_INVALID_MEMSLOT;
    lpf_register_global( lpf, &params, sizeof(params), &params_slot );

    lpf_memslot_t mem_slot = LPF_INVALID_MEMSLOT;
    lpf_register_global( lpf, mem, nprocs, &mem_slot );

    if (pid != 0) 
        lpf_get( lpf, 0, params_slot, 0, params_slot, 0, sizeof(params), LPF_MSG_DEFAULT );

    for (i = 0; i < nprocs; ++i) {
        if ( i != pid )
            lpf_put( lpf, mem_slot, pid, i, mem_slot, pid, sizeof(mem[0]), LPF_MSG_DEFAULT );
    }

    lpf_sync( lpf, LPF_SYNC_DEFAULT );


    assert( params.error == 1 );
    params.error = 0;
    assert( params.error == 0 );

    for (i = 0; i < nprocs; ++i) {
        if ( mem[i] != 'A' + i )
            params.error = 1;
    }
    if (!params.error)
        params.actual_nprocs = nprocs;

    lpf_sync( lpf, LPF_SYNC_DEFAULT );

    if (params.error && pid != 0)
        lpf_put( lpf, params_slot, 0, 0, params_slot, 0, sizeof(params), LPF_MSG_DEFAULT);


    lpf_sync( lpf, LPF_SYNC_DEFAULT );

    if (pid == 0) {
        assert( args.output_size == sizeof(params));
        memcpy( args.output, &params, sizeof(params));
    }
}

int main( int argc, char ** argv )
{
    assert( argc > 1 && "usage: ./test <expected nprocs>" );

    const int expected_nprocs = atoi( argv[1] );

    params_t params;
    params.expected_nprocs = expected_nprocs ;
    params.actual_nprocs = 0;
    params.error = 1;

    lpf_args_t args;
    args.input = &params;
    args.input_size = sizeof(params);
    args.output = &params;
    args.output_size = sizeof(params);
    args.f_symbols = NULL;
    args.f_size = 0;

    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, args );


    if (rc != LPF_SUCCESS ) {
        fprintf(stderr, "Runtime error while running parallel program\n");
        return EXIT_FAILURE;
    } else if ( params.actual_nprocs != expected_nprocs ) {
        fprintf(stderr, "Got unexpected number of processes %d instead of %d\n",
                params.actual_nprocs, expected_nprocs);
        return EXIT_FAILURE;
    }
    else if ( params.error ) {
        fprintf(stderr, "Other unexpected error happened\n");
        return EXIT_FAILURE;
    }

    printf("Got expected number of processes %d on engine %s\n",
            expected_nprocs, getenv("LPF_ENGINE"));

    return EXIT_SUCCESS;
}
