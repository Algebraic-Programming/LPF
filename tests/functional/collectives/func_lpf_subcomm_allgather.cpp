
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

#define ITERS 10

#include <lpf/core.h>
#include <lpf/collectives.h>
#include <lpf/mpi.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <math.h>

extern "C" const int LPF_MPI_AUTO_INITIALIZE=0;


void spmd( lpf_t ctx, const lpf_pid_t s, const lpf_pid_t p, const lpf_args_t args )
{
    /*
     * part 1 : LPF
     */
    (void) args; // ignore any arguments passed through call to lpf_exec
    lpf_memslot_t src_slot, dst_slot;
    lpf_coll_t coll;
    lpf_err_t rc;

    rc = lpf_resize_message_queue( ctx, 2*p );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( ctx, 3 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    const size_t size = 1 << 20; //1 << 19;
    std::vector<char> data(size);

    for( size_t i = 0; i < size; ++i ) {
        data[ i ] = (char) s;
    }

    std::vector<char> allgatheredData(size * p/2);

    for( size_t i = 0; i < (p/2) * size; ++i ) {
         allgatheredData[ i ] = (char) -1;
    }


    rc = lpf_register_global( ctx, data.data(), size * sizeof(char), &src_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_register_global( ctx, allgatheredData.data(), p/2 * size * sizeof(char), &dst_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
    EXPECT_EQ(LPF_SUCCESS, rc);

    /**
     * Choose a subset of peers
     */
    lpf_pid_t peers[p/2];

    for (lpf_pid_t _k = 0; _k < p/2 ; _k++) {
        peers[_k] = _k;
    }

    // explicitly set this collective to include p/2 peers only!
    //rc = lpf_collectives_init( ctx, s, p, 0, NULL, 1, 0, p * size, &coll );

    /* start block to benchmark */
    auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ITERS; i++) {
        rc = lpf_collectives_init( ctx, s, p, p/2, peers, 1, 0, size, &coll );
        EXPECT_EQ( LPF_SUCCESS, rc );
        // modified collective iterating over subset of processes
        rc = lpf_subcomm_allgather( coll, src_slot, dst_slot, size, false );
        EXPECT_EQ( LPF_SUCCESS, rc );

        // modified sync waiting on a subset of (p/2) received messages
        rc = lpf_counting_sync_per_slot( ctx, LPF_SYNC_DEFAULT, dst_slot, 0, p/2);
        //rc = lpf_sync( ctx, LPF_SYNC_DEFAULT);
        EXPECT_EQ( LPF_SUCCESS, rc );
        rc = lpf_collectives_destroy( coll );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    if (s == 0) std::cout << "LPF linear allgather duration (ms) = " << ms_double.count()/ITERS << std::endl;

    EXPECT_THAT(data, testing::Each((char)s));

    const bool involved = coll.involved;
    if (involved) {
        printf("Rank %d is involved!\n", s);
        for( lpf_pid_t k = 0; k < p/2; ++k ) {
            std::vector<char> expected(size, (char)k);
            EXPECT_EQ(std::vector(allgatheredData.begin() + size * k, allgatheredData.begin() + size * (k+1) ) , expected);
        }
    }


    rc = lpf_sync(ctx, LPF_SYNC_DEFAULT);
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_deregister( ctx, src_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_deregister( ctx, dst_slot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    
    /*
     * part 2 : MPI
     */

    t1 = std::chrono::high_resolution_clock::now();
    MPI_Comm newcomm;
    for (size_t i = 0; i < ITERS; i++) {
        MPI_Comm_split(MPI_COMM_WORLD, involved, s, &newcomm);
    }
    t2 = std::chrono::high_resolution_clock::now();
    ms_double = t2 - t1;
    if (s == 0) std::cout << "MPI split duration (ms) = " << ms_double.count()/ITERS << std::endl;

    t1 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ITERS; i++) {
        MPI_Allgather(data.data(), size, MPI_CHAR, allgatheredData.data(), size, MPI_CHAR, newcomm);
    }
    t2 = std::chrono::high_resolution_clock::now();
    ms_double = t2 - t1;
    if (s == 0) std::cout << "MPI allgather duration (ms) = " << ms_double.count()/ITERS << std::endl;

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

    MPI_Init(nullptr, nullptr);
    lpf_init_t init;
    lpf_err_t rc = lpf_mpi_initialize_with_mpicomm(MPI_COMM_WORLD, &init);
    EXPECT_EQ(LPF_SUCCESS, rc);
    rc = lpf_hook(init, &spmd, LPF_NO_ARGS);
    EXPECT_EQ(LPF_SUCCESS, rc);
    rc = lpf_mpi_finalize(init);
    EXPECT_EQ(LPF_SUCCESS, rc);
    MPI_Finalize();

}

