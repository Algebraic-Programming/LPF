
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

#include <cstdio>

#include <cassert>
#include <iostream>

#include <cstdlib>
#include <list>

#define LPF_LIST_DEBUG 1
#include "lpf_list.hpp"

#include "lpf_sort.hpp"


#include <time.h>

template <class It>
void print( lpf::machine & ctx, It b, It e)
{
    while (b != e ) {
        if (b.is_valid() ) {
            std::cout << *b << ' ' << std::flush;
        }
        ctx.fence();

        ++b;
    }
    if (ctx.pid() == 0 )
        std::cout << '\n';
}


void small_test( lpf::machine & ctx )
{
    lpf::list< int > xs( ctx );

    typedef lpf::list<int> :: iterator it;

    assert( xs.begin() == xs.end() );

    int as[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    xs.insert( 0, xs.begin(), &as[0] );
    it a = xs.insert( 1, xs.begin(), &as[1] );
    xs.insert( 2, xs.begin(), &as[2] );
    a = xs.insert( 0, a, &as[3] );
    xs.insert( 0, a, &as[4] );
    xs.insert( 0, a, &as[5] );
    xs.insert( 0, xs.end(), &as[6] );
    xs.insert( 1, a, &as[7] );

    
    xs.dump(std::cout);
    print( ctx, xs.begin(), xs.end() );


    lpf::list< size_t > ranking(ctx);
    xs.rank( ranking );

    ranking.dump( std::cout );

    print( ctx, ranking.begin(), ranking.end() );


    if (ctx.pid() == 0 ) std::cout << "Adding 4 * nprocs elements\n";

    int bs[] = { 8, 5, 3, 6 } ;
    for (int i = 0; i < 4; ++i) bs[i] = bs[i] * 100 + ctx.pid();


#if 1
    { size_t count = xs.count();
      size_t imb = xs.imbalance();
    
      if (ctx.pid() == 0 )
        std::cout << "Count = " << count << ", imbalance = " << imb << std::endl;
    }
#endif

    xs.insert( a, bs, bs+4 );

    if (ctx.pid() == 0 ) std::cout << "Rebalancing\n";
    xs.rebalance();


#if 1
    { size_t count = xs.count();
      size_t imb = xs.imbalance();
    
      if (ctx.pid() == 0 )
        std::cout << "Count = " << count << ", imbalance = " << imb << std::endl;
    }
#endif

    xs.dump(std::cout);

    //xs.dump( std::cout );
    
    print( ctx, xs.begin(), xs.end() );

    xs.rank( ranking );
    ranking.dump( std::cout );

    print( ctx, ranking.begin(), ranking.end() );


}


void large_ranking_test( lpf::machine & ctx ) 
{
    lpf::list< int > rnd_list( ctx );

    unsigned int seed = 0;

    const lpf_pid_t P = ctx.nprocs();
    const int N = 1000;

    // generate a random list
    for (int i = 0; i < N; ++i) {
        const lpf_pid_t pid = rand_r(&seed) / (1.0 + RAND_MAX ) * P;

        rnd_list.insert( pid, rnd_list.end(), &i );
    }

    rnd_list.dump( std::cout );
    print( ctx, rnd_list.begin(), rnd_list.end() );

    lpf::list< size_t > rank( ctx );
    rnd_list.rank( rank );
    print( ctx, rank.begin(), rank.end() );

    rank.dump( std::cout );
}

void test_sort( lpf::machine & ctx )
{
    using namespace lpf;
    temporary_space tmp(ctx);
    const size_t N = 10;
    global< std::vector< double >, persistent > xs(ctx, N, 0, ctx.p_rel );

    for (size_t i = 0; i < N; ++i)
        xs[i] = ctx.random();

    if (ctx.pid() == 0)
        std::cout << "Before sort:\n";
    print( std::cout, xs );

    sort( ctx, tmp, xs );

    if (ctx.pid() == 0)
        std::cout << "\n\nAfter sort:\n";
    print(std::cout, xs);
}

void another_large_ranking_test( lpf::machine & ctx ) 
{
    const size_t N = 10000000;
    lpf::list< int > rnd_list( ctx );
    rnd_list.generate_random( N, 0, 100);

    const int ITER = 2;
    struct timespec t0, t1;
    t0.tv_sec = t0.tv_nsec = 0;
    for (int i = 0 ; i <= ITER; ++i) {
        if (i == 1) clock_gettime( CLOCK_MONOTONIC, &t0 );
        rnd_list.generate_random( N, 0, 100);
    }
    clock_gettime( CLOCK_MONOTONIC, &t1 );

    double seconds = (t1.tv_sec - t0.tv_sec ) + 1e-9*(t1.tv_nsec - t0.tv_nsec);
    if (!ctx.pid())
        std::cout << "Parallel Generation time: " << seconds / ITER << '\n';

    //rnd_list.dump( std::cout );
    //print( ctx, rnd_list.begin(), rnd_list.end() );

    lpf::list< size_t > rank( ctx );
    t0.tv_sec = t0.tv_nsec = 0;
    for (int i = 0 ; i <= ITER; ++i) {
        if (i == 1) clock_gettime( CLOCK_MONOTONIC, &t0 );
        rnd_list.rank( rank );
    }
    clock_gettime( CLOCK_MONOTONIC, &t1 );

   // print( ctx, rank.begin(), rank.end() );
    

    seconds = (t1.tv_sec - t0.tv_sec ) + 1e-9*(t1.tv_nsec - t0.tv_nsec);
    if (!ctx.pid())
        std::cout << "Ranking time: " << seconds / ITER << '\n';

}

void seq_list_rank()
{
    const int N = 10000000;
    const int ITER = 2;
    std::list<int> xs;
    std::vector< std::list< int > :: iterator > index(N);
    struct timespec t0, t1;
    t0.tv_sec = t0.tv_nsec = 0;
    for (int j = 0; j <= ITER; ++j) {
        if (j == 1) clock_gettime( CLOCK_MONOTONIC, &t0 );
        xs.clear();
        index[0] = xs.end();
        for (int i = 0; i < N; ++i) {
            int k = rand() / (RAND_MAX + 1.0) * i;
            index[i] = xs.insert( index[k], i );
        }
    }
    clock_gettime( CLOCK_MONOTONIC, &t1 );

    double seconds = (t1.tv_sec - t0.tv_sec ) + 1e-9*(t1.tv_nsec - t0.tv_nsec);
    std::cout << "Sequential Generation time: " << seconds / ITER << '\n';

    std::list<int> ys;
    t0.tv_sec = t0.tv_nsec = 0;
    for (int j = 0; j <= ITER; ++j) {
        if (j == 1) clock_gettime( CLOCK_MONOTONIC, &t0 );
        ys.clear();
        int x = 0;
        for (std::list<int>::const_iterator i = xs.begin(); i != xs.end(); ++i)
            ys.push_back( x++ );
    }
    clock_gettime( CLOCK_MONOTONIC, &t1 );
    seconds = (t1.tv_sec - t0.tv_sec ) + 1e-9*(t1.tv_nsec - t0.tv_nsec);
    std::cout << "Ranking time time: " << seconds / ITER << '\n';
}


int main( int argc, char ** argv)
{
    (void) argc; (void) argv;
#if 0
    lpf::machine::root().exec( LPF_MAX_P, small_test );
    lpf::machine::root().exec( LPF_MAX_P, large_ranking_test );
    lpf::machine::root().exec( LPF_MAX_P, test_sort );
#endif
    lpf::machine::root().exec( LPF_MAX_P, another_large_ranking_test );

    seq_list_rank();



    return 0;
}

