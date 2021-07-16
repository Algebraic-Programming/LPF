
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

#include "spall2all.h"
#include "spall2all.hpp"
#include "mpilib.hpp"
#include "assert.hpp"
#include <cstdlib>
#include <cmath>
#include <string>
#include <limits>

#include <gtest/gtest.h>
#include <mpi.h>


using namespace lpf::mpi;

extern "C" const int LPF_MPI_AUTO_INITIALIZE=1;


TEST( Spall2allC, EnoughMemory )
{
    lpf::mpi::Lib::instance().world();

    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

using namespace std;
using namespace lpf::mpi;

// Parameters
// Maximum number of items to send to any process
const int N = 10;

// The number of bytes to send N items
const int M = N * (6 + 2*(int) ceil(1+log10(nprocs)) );

// Note: the number of bytes is derived from the message format, as below
#define NEW_MSG( buf, my_pid, dst_pid, character) \
    snprintf( (buf), sizeof(buf), "(%d, %d)%c;", (my_pid), (dst_pid), (character) )

    sparse_all_to_all_t spt;
    double epsilon = 1e-20; // bound of probability of failure (using Chernoff bounds)
    size_t max_tmp_bytes = size_t( ceil(3-log(epsilon)/N)*M );
    size_t max_tmp_msgs = size_t( ceil(3-log(epsilon)/N)*N );
    uint64_t rng_seed = 0;
    sparse_all_to_all_create( &spt, my_pid, nprocs, rng_seed, 1, 
            std::numeric_limits<int>::max() );
    int error = sparse_all_to_all_reserve( &spt, max_tmp_bytes, max_tmp_msgs );
    EXPECT_EQ( 0, error );

    srand(my_pid*1000);

    std::vector<char> a2a_send(nprocs*M);
    std::vector<char> a2a_recv(nprocs*M*100);
    std::vector<int> a2a_send_counts(nprocs);
    std::vector<int> a2a_send_offsets(nprocs);
    std::vector<int> a2a_recv_counts(nprocs);
    std::vector<int> a2a_recv_offsets(nprocs);

    for (int i = 0; i < nprocs; ++i)
    {
        a2a_send_counts[i] = 0;
        a2a_send_offsets[i] = i*M;
        a2a_recv_counts[i] = 100*M;
        a2a_recv_offsets[i] = i*100*M;
    }

    int n = rand() / (1.0 + RAND_MAX) * N;

    for (int i = 0; i < n; ++i) {
        char c = rand() % 26 + 'A';
        int dst_pid = rand() / (1.0 + RAND_MAX) * nprocs;
        char buf[M];
        size_t size = NEW_MSG(buf, my_pid, dst_pid, c); 
        int rc = sparse_all_to_all_send( &spt, dst_pid, buf, size );
        EXPECT_EQ( 0, rc );

        char * a2a_buf = &a2a_send[0] + a2a_send_offsets[dst_pid] + a2a_send_counts[dst_pid];
        memcpy( a2a_buf , buf, size );
        a2a_send_counts[dst_pid] += size;
        ASSERT( a2a_send_counts[dst_pid] <= M );
    }



    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Alltoall( &a2a_send_counts[0], 1, MPI_INT, 
                  &a2a_recv_counts[0], 1, MPI_INT, MPI_COMM_WORLD);
    MPI_Alltoallv( &a2a_send[0], &a2a_send_counts[0], &a2a_send_offsets[0], MPI_BYTE,
            &a2a_recv[0], &a2a_recv_counts[0], &a2a_recv_offsets[0], MPI_BYTE,
            MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);


    error = 1;
    int vote  = my_pid; int ballot = -1;
    MPI_Barrier(MPI_COMM_WORLD);
    error = sparse_all_to_all( MPI_COMM_WORLD, &spt, 1, &vote, &ballot  );

    EXPECT_GE( error, 0 );
    EXPECT_EQ( (nprocs-1)*nprocs/2, ballot );
    
    std::vector< std::string > spall2all_recv_buf(nprocs);
    char buf[M];
    size_t size = sizeof(buf);
    while (0 == sparse_all_to_all_recv( &spt, buf, &size ))
    {
        spall2all_recv_buf.push_back( std::string( buf, buf+size) );
        size = sizeof(buf) ;
    }

    a2a_recv_offsets.push_back( a2a_recv.size() );
    std::vector< std::string > a2a_recv_buf(nprocs);
    for ( int i = 0 ; i < nprocs; ++i)
    {
        const char * ptr = &a2a_recv[0] + a2a_recv_offsets[i] ;
        while ( ptr - &a2a_recv[0] < a2a_recv_offsets[i] + a2a_recv_counts[i] ) {
            const char * end = &a2a_recv[0] + a2a_recv_offsets[i+1];
            end = std::find( ptr, end, ';')+1;

            a2a_recv_buf.push_back( std::string( ptr, end  ));
            ptr = end;
        }
    }
    std::sort( spall2all_recv_buf.begin(), spall2all_recv_buf.end() );
    std::sort( a2a_recv_buf.begin(), a2a_recv_buf.end() );

    EXPECT_EQ( a2a_recv_buf, spall2all_recv_buf );

#if 0
    std::cout << "[" << my_pid << "] Total elements spall2all: " << spall2all_recv_buf.size() 
        << "\n" << "[" << my_pid << "] Total elements a2a: " << a2a_recv_buf.size()<<std::endl;
    
    std::cout << "\n\n";
    for (int i = 0; i < spall2all_recv_buf.size(); ++i)
        std::cout << spall2all_recv_buf[i] << "|";
     std::cout << "\n\n";
    for (int i = 0; i < a2a_recv_buf.size(); ++i)
        std::cout << a2a_recv_buf[i] << "|";
    std::cout << std::endl;
#endif

    sparse_all_to_all_destroy( &spt );
}

TEST( Spall2all, Create )
{
    SparseAllToAll x(9, 10);
}

TEST( Spall2all, Reserve )
{
    SparseAllToAll x( 4,10);
}

TEST( Spall2all, ReserveUnequal )
{
    Lib::instance();
    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

    SparseAllToAll x( my_pid, nprocs );

    // simulate the case where one of the processes can't 
    // allocate enough buffer space
    if (my_pid != 0 )
        x.reserve( nprocs * ceil(1+log2(nprocs)), sizeof(int) );

    bool prerandomize = true;
    int error = x.exchange( Lib::instance().world(), prerandomize, NULL);
    EXPECT_TRUE( !error );
}

TEST( Spall2all, Send )
{
    Lib::instance();
    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );


    SparseAllToAll x( my_pid, nprocs );
    x.reserve( nprocs * ceil(1+log2(nprocs)) , sizeof(int));
    for (int i = 0; i <= my_pid ; ++i)
        x.send( (my_pid + 1) % nprocs, &i, sizeof(i) );

    bool prerandomize = true;
    int error = x.exchange( Lib::instance().world(), prerandomize, NULL);
    EXPECT_TRUE( !error );
}

TEST( Spall2all, Ring )
{
    Lib::instance();
    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );


    SparseAllToAll x(my_pid, nprocs);
    EXPECT_TRUE(  x.empty() );
    x.reserve( nprocs * ceil(1+log2(nprocs)) , sizeof(int));
    x.send( (my_pid + 1) % nprocs, &my_pid, sizeof(my_pid) );

    EXPECT_FALSE(  x.empty() );

    bool prerandomize = true;
    int error = x.exchange( Lib::instance().world(), prerandomize, NULL);
    EXPECT_TRUE( !error );

    EXPECT_FALSE(  x.empty() );
   
    int y = -1;
    x.recv( &y, sizeof(y)); 
    EXPECT_EQ( (my_pid + nprocs -1) % nprocs, y );

    EXPECT_TRUE(  x.empty() );
}


TEST( Spall2all, Access )
{
    Lib::instance();
    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

    SparseAllToAll x(my_pid, nprocs);
    x.reserve( nprocs * ceil(1+log2(nprocs)) , sizeof(int) );
    EXPECT_TRUE(  x.empty() );

    for (int i = 0; i <= my_pid ; ++i)
        x.send( (my_pid + i + 1) % nprocs, &i, sizeof(i) );

    EXPECT_FALSE(  x.empty() );

    for (int i = 0; i<= my_pid; ++i) {
        int y;
        x.recv( &y, sizeof(y) );
        EXPECT_EQ( my_pid - i , y);
    }
    EXPECT_TRUE(  x.empty() );
}

TEST( Spall2all, SmallBuf )
{
    Lib::instance();

    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

    SparseAllToAll x(my_pid, nprocs);
    const int nMsgs = 5;
    x.reserve( nMsgs , sizeof(int) );

    for (int i = 0; i < nMsgs; ++i)
    {
        x.send( (my_pid + i) % nprocs, &i, sizeof(int) );
    }

    bool prerandomize = true;
    int error = 0;
    for (int i = 0; i < 100 && !error; ++i)
    {
         error = x.exchange( Lib::instance().world(), prerandomize, NULL, 1 );
    }
    EXPECT_TRUE( nprocs == 1 || error );

}

TEST( Spall2all, SmallBufProc1 )
{
    Lib::instance();

    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

    SparseAllToAll x(my_pid, nprocs);
    const int nMsgs = 100;
    // only one process has a very small buffer space. Still
    // that has high likelihood to fail
    x.reserve( my_pid == 1 ? nMsgs : (nMsgs * nMsgs) , sizeof(int) );

    int failures = 0;
    for (int j = 0; j < 100 ; ++j) {
        x.clear();

        for (int i = 0; i < nMsgs; ++i)
        {
            x.send( (my_pid + i) % nprocs, & i, sizeof(i) );
        }

        bool prerandomize = true;
        int error = x.exchange( Lib::instance().world(), prerandomize, NULL, 1 );
        failures += ( error ? 1 : 0 ) ;
    }
    EXPECT_GE( 90.0 / nprocs + 10 , 100.0-failures  );

}

TEST( Spall2all, ManyMsgs )
{
    Lib::instance();

    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

    SparseAllToAll x(my_pid, nprocs);
    const int nMsgs = 10000;
    x.reserve( nMsgs * 2 , sizeof(int) );

    for (int j = 0; j < 10 ; ++j) {
        x.clear();

        for (int i = 0; i < nMsgs; ++i)
        {
            x.send( (my_pid + i) % nprocs, & i, sizeof(i) );
        }

        bool prerandomize = true;
        int trials = 5;
        int error = x.exchange( Lib::instance().world(), prerandomize, 
                NULL, trials);
        EXPECT_FALSE( error );

        for (int i = 0; i < nMsgs; ++i)
        {
            EXPECT_FALSE( x.empty() );
            int k = -1;
            x.recv( &k, sizeof(k));
            EXPECT_GE( k, 0 );
            EXPECT_LT( k, nMsgs );
        }
        EXPECT_TRUE( x.empty() );
    }
}

TEST( Spall2all, LargeSend )
{
    Lib::instance();
    int my_pid = -1, nprocs = -1;
    MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

    SparseAllToAll x( my_pid, nprocs );

    std::vector<char> data( size_t(std::numeric_limits<int>::max()) + 10u );
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = char(i + my_pid) ;

    x.reserve( 1 , data.size() );
    x.send( (my_pid + 1) % nprocs, data.data(), data.size() );

    bool prerandomize = false;
    int error = x.exchange( Lib::instance().world(), prerandomize, NULL);
    EXPECT_TRUE( !error );

    std::fill(data.begin(), data.end(), '\0' );
    x.recv( data.data(), data.size() );
    int j = (nprocs != 1?1:0);
    for (size_t i = 0; i < data.size(); ++i)
        EXPECT_EQ( char(i + (my_pid + nprocs - j) % nprocs), data[i] ) ;

}


