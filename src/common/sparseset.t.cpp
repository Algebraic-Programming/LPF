
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

#include "sparseset.hpp"

#include <gtest/gtest.h>
#include <limits>

using namespace lpf;

TEST( SparseSet, Empty)
{
   SparseSet<int> x;


   EXPECT_TRUE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(0), x.size() );
   EXPECT_EQ( x.begin(), x.end() );
   EXPECT_EQ( x.end(), x.find( 1 ) );
   EXPECT_FALSE( x.contains( 2 ) );
}

TEST( SparseSet, Insert)
{
   SparseSet<int> x(1, 2);

   EXPECT_TRUE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(0), x.size() );

   x.insert( 1 );
   EXPECT_FALSE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(1), x.size() );

   x.insert( 1 );
   EXPECT_FALSE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(1), x.size() );

   x.insert( 2 );
   EXPECT_FALSE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(2), x.size() );
}

TEST( SparseSet, Erase)
{
   SparseSet<int> x(1, 3);

   EXPECT_TRUE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(0), x.size() );

   x.insert( 1 );
   EXPECT_FALSE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(1), x.size() );

   x.insert( 1 );
   EXPECT_FALSE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(1), x.size() );

   x.erase( 1 );
   EXPECT_TRUE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(0), x.size() );

   x.insert( 2 );
   x.insert( 3 );
   EXPECT_FALSE( x.empty() );
   EXPECT_EQ( static_cast<size_t>(2), x.size() );

   x.erase( 2 );
   EXPECT_EQ( static_cast<size_t>(1), x.size() );
}


TEST( SparseSet, ResetFromEmpty )
{
    SparseSet<int> x;

    x.reset( -10, 10 );

    EXPECT_EQ( static_cast<size_t>(0), x.size() );
    EXPECT_EQ( x.begin(), x.end() );
    EXPECT_EQ( x.end(), x.find( 1 ) );
    EXPECT_FALSE( x.contains( 2 ) );
}

TEST( SparseSet, ExtendFromEmpty )
{
    SparseSet<int> x;

    x.extend( 10 );

    EXPECT_EQ( static_cast<size_t>(0), x.size() );
    EXPECT_EQ( x.begin(), x.end() );
    EXPECT_EQ( x.end(), x.find( 101 ) );
    EXPECT_FALSE( x.contains( 5 ) );
}

TEST( SparseSet, ResetFromSmall )
{
    SparseSet<int> x;

    x.reset( -10, 10 );
    x.reset( -50, 200 );

    EXPECT_EQ( static_cast<size_t>(0), x.size() );
    EXPECT_EQ( x.begin(), x.end() );
    EXPECT_EQ( x.end(), x.find( 1 ) );
    EXPECT_FALSE( x.contains( 2 ) );
}

TEST( SparseSet, ExtendFromSmall )
{
    SparseSet<int> x;

    x.reset( -10, 10 );

    x.insert( 5 );

    x.extend(200);
    x.insert( 100);

    EXPECT_EQ( static_cast<size_t>(2), x.size() );
    EXPECT_FALSE( x.contains( 2 ) );
    EXPECT_TRUE( x.contains( 5 ) );
    EXPECT_TRUE( x.contains(100) );
}

TEST( SparseSet, ResetToTooBig )
{
    SparseSet<size_t> x;

    x.reset( 0u, 10u );


    EXPECT_EQ( static_cast<size_t>(0), x.size() );
    EXPECT_EQ( x.begin(), x.end() );
    EXPECT_EQ( x.end(), x.find( 1 ) );
    EXPECT_FALSE( x.contains( 2 ) );

    x.insert( 5u );
    x.insert( 2u );

    EXPECT_EQ( static_cast<size_t>(2), x.size() );
    EXPECT_EQ( 2u, std::distance(x.begin(), x.end()) );
    EXPECT_TRUE( x.contains( 2 ) );
    EXPECT_FALSE( x.contains( 3 ) );

    std::size_t big = x.max_size();
    EXPECT_THROW( x.reset( 1u, big), std::bad_alloc );

    EXPECT_EQ( static_cast<size_t>(2), x.size() );
    EXPECT_EQ( 2u, std::distance(x.begin(), x.end()) );
    EXPECT_TRUE( x.contains( 2 ) );
    EXPECT_TRUE( x.contains( 5 ) );
    EXPECT_FALSE( x.contains( 3 ) );
}

TEST( SparseSet, ExtendToTooBig )
{
    SparseSet<size_t> x;

    x.reset( 1u, 10u );


    EXPECT_EQ( static_cast<size_t>(0), x.size() );
    EXPECT_EQ( x.begin(), x.end() );
    EXPECT_EQ( x.end(), x.find( 1 ) );
    EXPECT_FALSE( x.contains( 2 ) );

    x.insert( 5u );
    x.insert( 2u );

    EXPECT_EQ( static_cast<size_t>(2), x.size() );
    EXPECT_EQ( 2u, std::distance(x.begin(), x.end()) );
    EXPECT_TRUE( x.contains( 2 ) );
    EXPECT_FALSE( x.contains( 3 ) );

    std::size_t big = x.max_size();
    EXPECT_THROW( x.extend( big), std::bad_alloc );

    EXPECT_EQ( static_cast<size_t>(2), x.size() );
    EXPECT_EQ( 2u, std::distance(x.begin(), x.end()) );
    EXPECT_TRUE( x.contains( 2 ) );
    EXPECT_TRUE( x.contains( 5 ) );
    EXPECT_FALSE( x.contains( 3 ) );
}

TEST( SparseSet, ShrinkBelowCurrentMax )
{
    SparseSet<int> x(-10, 50);

    x.insert( 5 );
    x.insert( 10);
    x.insert( 9);
    x.insert( 7 );

    EXPECT_EQ( static_cast<size_t>(4), x.size() );

    EXPECT_FALSE( x.contains( 2 ) );
    EXPECT_TRUE( x.contains( 5 ) );
    EXPECT_TRUE( x.contains(10) );
    EXPECT_TRUE( x.contains(9) );
    EXPECT_FALSE( x.contains(8) );
    EXPECT_TRUE( x.contains(7) );

    // shrinking below the current largest value, does not remove any values
    // from the set.
    x.shrink( 5 );

    EXPECT_EQ( static_cast<size_t>(4), x.size() );
    EXPECT_FALSE( x.contains( 2 ) );
    EXPECT_TRUE( x.contains( 5 ) );
    EXPECT_TRUE( x.contains(10) );
    EXPECT_TRUE( x.contains(9) );
    EXPECT_FALSE( x.contains(8) );
    EXPECT_TRUE( x.contains(7) );
}

TEST( SparseSet, Shrink )
{
    SparseSet<int> x(-10, 50);

    x.insert( 5 );
    x.insert( 10);
    x.insert( 9);
    x.insert( 7 );

    EXPECT_EQ( static_cast<size_t>(4), x.size() );

    EXPECT_FALSE( x.contains( 2 ) );
    EXPECT_TRUE( x.contains( 5 ) );
    EXPECT_TRUE( x.contains(10) );
    EXPECT_TRUE( x.contains(9) );
    EXPECT_FALSE( x.contains(8) );
    EXPECT_TRUE( x.contains(7) );

    // shrinking below the current largest value, does not remove any values
    // from the set.
    x.shrink( 20 );

    EXPECT_EQ( static_cast<size_t>(4), x.size() );
    EXPECT_FALSE( x.contains( 2 ) );
    EXPECT_TRUE( x.contains( 5 ) );
    EXPECT_TRUE( x.contains(10) );
    EXPECT_TRUE( x.contains(9) );
    EXPECT_FALSE( x.contains(8) );
    EXPECT_TRUE( x.contains(7) );
}

