
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

#include "hall2all.hpp"

#include <gtest/gtest.h>
#include <mpi.h>


using namespace lpf::mpi;

extern "C" const int LPF_MPI_AUTO_INITIALIZE=0;


/** 
 * \pre P >= 1
 * \pre P <= 2
 */
class HAll2AllTests : public testing::Test {

    protected:

    static void SetUpTestSuite() {

       MPI_Init(NULL, NULL);
       Lib::instance();

        MPI_Comm_rank( MPI_COMM_WORLD, &my_pid );
        MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

    }

    static void TearDownTestSuite() {
        MPI_Finalize();
    }

    static int my_pid;
    static int nprocs;
};

int HAll2AllTests::my_pid = -1;
int HAll2AllTests::nprocs = -1;

TEST_F( HAll2AllTests, Create )
{
    HoeflerAllToAll x(9, 10);
}

TEST_F( HAll2AllTests, Reserve )
{
    HoeflerAllToAll x( 4,10);
    x.reserve( 50 , 100);
}

TEST_F( HAll2AllTests, Send )
{
    HoeflerAllToAll x( my_pid, nprocs );
    x.reserve( nprocs , sizeof(int));
    for (int i = 0; i <= my_pid ; ++i)
        x.send( (my_pid + 1) % nprocs, &i, sizeof(int) );

    bool prerandomize = true;
    int error = x.exchange( Lib::instance().world(), prerandomize, NULL);
    EXPECT_TRUE( !error );
}

TEST_F( HAll2AllTests, Ring )
{
    HoeflerAllToAll x(my_pid, nprocs);
    x.reserve( nprocs , sizeof(int));
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


TEST_F( HAll2AllTests, ManyMsgs )
{
    HoeflerAllToAll x(my_pid, nprocs );
    const int nMsgs = 10000;
    x.reserve( nMsgs , sizeof(int));

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

TEST_F( HAll2AllTests, LargeSend )
{
    HoeflerAllToAll x( my_pid, nprocs );

    std::vector<char> data( size_t(std::numeric_limits<int>::max()) + 10u );
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = char(i + my_pid) ;

    x.reserve( 1 , data.size() );
    x.send( (my_pid + 1) % nprocs, data.data(), data.size() );

    bool prerandomize = false;
    int error = x.exchange( Lib::instance().world(), prerandomize, NULL);
    EXPECT_TRUE( !error );

    x.recv( data.data(), data.size() );
    int j = (nprocs != 1?1:0);
    for (size_t i = 0; i < data.size(); ++i)
        EXPECT_EQ( char(i + (my_pid + nprocs - j) % nprocs), data[i] ) ;
}


