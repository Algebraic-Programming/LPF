
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

#include "lpf/core.h"
#include "assert.hpp"
#include "rng.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <limits.h>


typedef enum { ALL, SCATTER, GATHER, GRID, RANDOM } pattern_t;
static const char * pattern_str[] =
    { "all-to-all", "scatter", "gather", "grid2d", "random" };

lpf_pid_t root_p( lpf_pid_t p )
{
    lpf_pid_t result = 2;
    while (result*result <= p)
        result += 1;
    return result-1;
}



lpf_err_t all2all( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, 
        size_t n_msgs, size_t size, int with_collisions,
        pattern_t pattern,
        double * time )  
{
    size_t i = 0, j = 0;
    lpf_err_t rc = LPF_SUCCESS;
    char * buffer = NULL;
    lpf_memslot_t slot = LPF_INVALID_MEMSLOT;
    struct timespec t0, t1;

    rng_state_t rng;
    rng = rng_create(0);
    rng_parallelize( &rng, pid, nprocs);

    /* check for potential overflow */
    if ( n_msgs > SIZE_MAX / nprocs ) return LPF_ERR_FATAL;
    if ( size != 0 && n_msgs * nprocs > SIZE_MAX / size ) return LPF_ERR_FATAL;
    
    /* allocate memory for communication */
    switch (pattern ) {
        case ALL:
        case SCATTER:
        case GATHER:
        case RANDOM:
            rc = lpf_resize_message_queue( lpf, n_msgs * nprocs );
            break;
        case GRID:
            rc = lpf_resize_message_queue( lpf, n_msgs );
            break;
    }
    if ( rc != LPF_SUCCESS ) return rc;
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    if ( rc != LPF_SUCCESS ) return rc;

    /* allocate buffer & fill with some data (non-zero) */
    buffer = malloc( n_msgs * size * nprocs );
    for (i = 0; i < n_msgs * nprocs * size; ++i) {
        buffer[i] = (char ) i;
    }

    // register the buffer
    lpf_register_global(lpf, buffer, n_msgs*size*nprocs, &slot );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    if ( rc != LPF_SUCCESS ) return rc;
    
    clock_gettime( CLOCK_MONOTONIC, &t0 );

    switch ( pattern ) {
        case ALL:
            for ( j = 0; j < nprocs; ++j)
                if (j != pid ) for ( i = 0; i < n_msgs; ++i) 
                    lpf_put( lpf, 
                            slot, pid*n_msgs*size + i*size, 
                            j, slot, (with_collisions?j:pid)*n_msgs*size+i*size, 
                            size, 
                            LPF_MSG_DEFAULT
                            );
            break;
        
        case SCATTER:
            if ( 0 == pid ) 
              for ( j = 1; j < nprocs; ++j)
                for ( i = 0; i < n_msgs; ++i) 
                    lpf_put( lpf, 
                            slot, pid*n_msgs*size + i*size, 
                            j, slot, (with_collisions?j:pid)*n_msgs*size+i*size, 
                            size, 
                            LPF_MSG_DEFAULT
                            );
            break;

        case GATHER:
            if ( 0 != pid ) 
                for ( i = 0; i < n_msgs; ++i) 
                    lpf_put( lpf, 
                            slot, pid*n_msgs*size + i*size, 
                            0, slot, (with_collisions?0:pid)*n_msgs*size+i*size, 
                            size, 
                            LPF_MSG_DEFAULT
                            );
            break;

        case GRID:
            { lpf_pid_t q = root_p(nprocs);
              lpf_pid_t peers[] = { (pid + q) % nprocs, (nprocs + pid - q) % nprocs,
                                    (pid + 1) % nprocs, (nprocs + pid - 1) % nprocs};
              for ( i = 0; i < n_msgs; ++i)  {
                lpf_pid_t dest = peers[i % 4];
                if (dest != pid)
                    lpf_put( lpf, 
                            slot, pid*n_msgs*size + i*size, 
                            dest, slot, (with_collisions?dest:pid)*n_msgs*size+i*size, 
                            size, 
                            LPF_MSG_DEFAULT
                            );
              }
            }
            break;

        case RANDOM:
            for ( i = 0; i < n_msgs; ++i)  {
                lpf_pid_t dest = (double) rng_next(&rng) / (1.0 + UINT64_MAX) * nprocs;
                if (dest != pid)
                    lpf_put( lpf, 
                            slot, pid*n_msgs*size + i*size, 
                            dest, slot, (with_collisions?dest:pid)*n_msgs*size+i*size, 
                            size, 
                            LPF_MSG_DEFAULT
                            );
            }
            break;
    }

    // do the sync
    lpf_sync( lpf, LPF_SYNC_DEFAULT );
    clock_gettime( CLOCK_MONOTONIC, &t1 );

    // free the slot
    lpf_deregister( lpf, slot );
    free( buffer );

    *time = (double) (t1.tv_sec - t0.tv_sec) + 1e-09 * (t1.tv_nsec - t0.tv_nsec );
    return LPF_SUCCESS;
}

struct params {
    size_t n_msgs;
    size_t size;
    int with_collisions;
    pattern_t pattern;
};

struct test_result {
    unsigned type;
    struct timespec timestamp;
    lpf_err_t status;
    double time;
};

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    int rc = LPF_SUCCESS;
    struct params ps; 
    lpf_memslot_t ps_slot = LPF_INVALID_MEMSLOT;
    struct test_result out;
    double time;

    /* PID 0 reads the input & initializes the output */
    ASSERT( pid != 0 || sizeof(ps) == args.input_size );
    ASSERT( pid != 0 || sizeof(out) == args.output_size );
    if ( pid == 0 ) {
        memcpy( &ps, args.input, sizeof(ps) );    
        memset( &out, 0, sizeof(out) );
        memcpy( args.output, &out, sizeof(out));
    }

    /* Broadcast the input parameters */
    rc = lpf_resize_memory_register( lpf, 1);
    if (LPF_SUCCESS != rc) return;

    rc = lpf_resize_message_queue( lpf, nprocs);
    if (LPF_SUCCESS != rc) return;

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    if (LPF_SUCCESS != rc) return;

    lpf_register_global( lpf, &ps, sizeof(ps), &ps_slot);
    if (pid != 0)
        lpf_get( lpf, 0, ps_slot, 0, ps_slot, 0, sizeof(ps), LPF_MSG_DEFAULT);

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    if (LPF_SUCCESS != rc) return;
    lpf_deregister( lpf, ps_slot );
    
    /* run the benchmark */
    rc = all2all(lpf, pid, nprocs, ps.n_msgs, ps.size, ps.with_collisions, 
            ps.pattern, &time);
    if (LPF_SUCCESS != rc) return;

    out.time = time;

    if ( pid == 0 )
        memcpy( args.output, &out, sizeof(out) );
}

int get_timestamp(struct timespec current_time, char * buffer, size_t bufsize)
{
    struct tm tm;
    gmtime_r( &current_time.tv_sec, &tm );
    return snprintf( buffer, bufsize , "%02d-%02d-%02dT%02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
}

#define STRFY2( x ) #x
#define STRFY( x ) STRFY2( x )

int main( int argc, char * argv[] )
{
    (void) argc;
    (void) argv;

    int deadline = INT_MAX;
    if (argc >=3 )
    {
        deadline = atoi(argv[2]); // number of seconds;
        if (deadline <= 0)
            deadline = INT_MAX;
    }
    enum { JUNIT, JMETER_CSV } format = JUNIT;
    if (argc >= 4)
    {
        if (strcmp("csv", argv[3]) == 0)
            format = JMETER_CSV;
    }
    const char * ident = "";
    if (argc >= 5)
    {
        ident=argv[4];
    }
    FILE * out = stdout;
    int write_header = 1;
    if (argc >= 2 ) {
        if (format == JUNIT)
        {
            out = fopen( argv[1], "w");
        }
        else if (format == JMETER_CSV )
        {
            out = fopen( argv[1], "r");
            // if the file already exists, we're going to append
            if (out != NULL) {
                fclose(out);
                write_header=0;
            }
            out = fopen( argv[1], "a" );
        }
        if (out == NULL) out = stdout;
    }


    int all_tests = 0, errors = 0, failures = 0;
    double total_time = 0.0 ;
    struct timespec start_time;
    char timestamp[20]; 

#ifdef LPF_CORE_IMPL_ID
    const char * impl_id = STRFY( LPF_CORE_IMPL_ID );
#else
    const char * impl_id = "regular";
#endif

#ifdef LPF_CORE_IMPL_CONFIG
    const char * impl_config = STRFY( LPF_CORE_IMPL_CONFIG );
#else
    const char * impl_config = "default";
#endif

    /* generate an ISO 8601 timestamp */
    clock_gettime( CLOCK_REALTIME, &start_time );
    get_timestamp( start_time, timestamp, sizeof(timestamp) );

    const size_t EXPERIMENTS = 100;
    size_t n_msgs[] = { 0, 1,       300, 300, 1   , 1      , 1     , 8     , 8     };
    size_t sizes[] =  { 0, 100000,  1  , 1  , 1000, 1000   , 1000  , 1000  , 1000  };
    int    colls[] =  { 0, 0     ,  0  , 1  , 0   , 0      , 0     , 0     , 0     };
    pattern_t pattern[]={ ALL, ALL, ALL, ALL, ALL , SCATTER, GATHER, GRID, RANDOM };
    unsigned counters[sizeof(n_msgs)/sizeof(n_msgs[0])];
    struct test_result results[EXPERIMENTS*sizeof(n_msgs)/sizeof(n_msgs[0])];
    const unsigned N = sizeof(results)/sizeof(results[0]);
    const unsigned M = sizeof(n_msgs)/sizeof(n_msgs[0]);
    unsigned i;
    memset( counters, 0, sizeof(counters) );
    memset( results, 0, sizeof(results));
    /* do the measurements */
    for ( i = 0; i < N; ++i) 
    {
        unsigned type;

        struct timespec sample_time;
        clock_gettime( CLOCK_REALTIME, & sample_time );

        if ( sample_time.tv_sec - start_time.tv_sec > deadline)
            break;
      
        // select a random n_msgs and sizes;
        do 
        {
            type = (double) rand() / (1.0 + RAND_MAX) * M;
        }
        while ( counters[type] == EXPERIMENTS);

        ASSERT( type < M);
        ASSERT( counters[type] < EXPERIMENTS );

        unsigned j = EXPERIMENTS*type + counters[type];
        counters[type] += 1;

        // conduct the experiment
        struct params ps;
        ps.n_msgs = n_msgs[type];
        ps.size = sizes[type];
        ps.with_collisions = colls[type];
        ps.pattern=pattern[type];
        lpf_args_t args;
        args.input = &ps;
        args.input_size = sizeof(ps);
        args.output = &results[j];
        args.output_size = sizeof(struct test_result);
        args.f_symbols = NULL;
        args.f_size = 0u;

        lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, args);

        if (rc != LPF_SUCCESS)
            results[j].status = rc;

        results[j].type = type;
        results[j].timestamp = sample_time;

        errors += results[j].status != LPF_SUCCESS ? 1 : 0;
        total_time += results[j].time;
    }

     
    /* generate JUnit output */
    if ( format == JUNIT ) {
        fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(out, "<testsuites tests=\"%d\" errors=\"%d\" failures=\"%d\" timestamp=\"%s\" "
                        "time=\"%f\" name=\"performance_tests\">\n", 
                all_tests, errors, failures, timestamp, total_time);

        fprintf(out, "<testsuite  name=\"%s_%s_%s\" tests=\"%d\" failures=\"%d\" "
                        "errors=\"%d\" timestamp=\"%s\" time=\"%f\">\n",
                        impl_id, impl_config, ident, all_tests, failures, errors, 
                        timestamp, total_time );
    }
    else if (format == JMETER_CSV)
    {
        if (write_header)
            fprintf(out, "timestamp,elapsed,responseCode,success,label\n");

    }

    for (i = 0 ; i < M; ++i) 
    {
        unsigned type = i;
        unsigned k;
        for (k = 0; k < counters[i]; ++k) 
        {
            unsigned j = k + EXPERIMENTS*i;
            size_t n = n_msgs[ type ];
            size_t s = sizes[ type ];
            int coll = colls[ type ];
            double t = results[j].time * 1000000.0;
            const char * pat = pattern_str[ pattern[type] ];
            get_timestamp( results[j].timestamp, timestamp, sizeof(timestamp));

            if (format == JUNIT ) {
                fprintf(out, "<testcase name=\"nmsgs%lu.size%lu.coll%d\" "
                        "time=\"%.3f\" classname=\"%s\" timestamp=\"%s\" >\n",
                        n, s, coll, t, pat, timestamp );

                if (results[i].status != LPF_SUCCESS) {
                    lpf_err_t rc = results[j].status;
                    fprintf(out, "<error>%s</error>\n", 
                            rc == LPF_ERR_FATAL?"LPF_ERR_FATAL":
                            rc == LPF_ERR_OUT_OF_MEMORY? "LPF_ERR_OUT_OF_MEMORY":
                            "unknown" );
                }
                fprintf(out, "</testcase>\n");
            }
            else if (format == JMETER_CSV)
            {
                long ts = 1000l * results[j].timestamp.tv_sec 
                    + results[j].timestamp.tv_nsec / 1000000 ;
                const char * success = results[j].status == LPF_SUCCESS?"true":"false"; 
                fprintf(out, "%ld,%ld,N/A,%s,%s.nmsgs%lu.size%lu.coll%d.%s.%s.%s\n"
                        , ts, (long) (t*1000.0), success, pat, n, s, coll, impl_id, impl_config, ident);

            }
        }
    }

    if (format == JUNIT)
        fprintf(out, "</testsuite></testsuites>\n");

    fclose(out);

    return 0;
}

