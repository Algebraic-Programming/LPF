
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

#include "memreg.hpp"

#include <gtest/gtest.h>

using namespace lpf;


typedef MemoryRegister<char> Memreg;
typedef Memreg::Slot Slot;

/** 
 * \test Memreg tests
 * \pre P <= 1
 * \return Exit code: 0
 */
TEST( MemoryRegister, Empty )
{
    Memreg empty;
    EXPECT_EQ( 0u, empty.capacity() );
}

TEST( MemoryRegister, Some )
{
    Memreg some;

    EXPECT_EQ( 0u, some.capacity() );

    some.reserve(5);
    EXPECT_EQ( 5u, some.capacity() );

    Slot a = some.add( 'a' );
    Slot b = some.add( 'b' );
    EXPECT_EQ( 5u, some.capacity() );

    EXPECT_EQ( 'b', some.lookup( b ) );
    EXPECT_EQ( 'a', some.lookup( a) );

    some.remove( b );
}


TEST( MemoryRegister, ReGrowEmpty)
{
    Memreg some;
    some.reserve( 2 );

    EXPECT_EQ( 2u, some.capacity() );


    some.reserve( 4 );
    EXPECT_EQ( 4u, some.capacity() );
    Slot a = some.add('a');

    EXPECT_EQ( 'a', some.lookup(a) );
}

TEST( MemoryRegister, ReGrowNonEmpty)
{
    Memreg some;
    some.reserve( 2 );

    EXPECT_EQ( 2u, some.capacity() );
    Slot a = some.add('a');
    some.reserve( 4 );
    EXPECT_EQ( 4u, some.capacity() );

    EXPECT_EQ( 'a', some.lookup(a) );
}

TEST( MemoryRegister, ShrinkEmpty )
{
    Memreg reg;
    reg.reserve( 4 );
    Slot a = reg.add('a');
    EXPECT_EQ( 'a', reg.lookup(a) );
    reg.reserve( 2 );
    EXPECT_EQ( 'a', reg.lookup(a) );
    Slot b = reg.add('b');
    EXPECT_EQ( 'b', reg.lookup(b) );
    EXPECT_EQ( 'a', reg.lookup(a) );
}


TEST( MemoryRegister, GrowOutOfMemory )
{
    Memreg reg;
    try
    { reg.reserve( std::numeric_limits<size_t>::max() );
      FAIL();
    } catch( std::bad_alloc & e)
    {
    }
    EXPECT_EQ(static_cast<size_t>(0), reg.capacity() );
}

// tests that the lowest memory slot is always given out first
TEST( MemoryRegister, GrowWithStops )
{
    Memreg P, Q;
    P.reserve( 1 );
    P.reserve( 2 );
    P.reserve( 3 );
    Q.reserve( 3 );
    Slot a1= P.add('a');
    Slot a2= Q.add('a');
    EXPECT_EQ( a1, a2 );
    P.reserve( 1 );
    Q.reserve( 1 );
    EXPECT_EQ('a', P.lookup(a1) );
    EXPECT_EQ('a', Q.lookup(a2) );
    EXPECT_EQ(1u, P.capacity() );
    EXPECT_EQ(1u, Q.capacity() );
}

// tests that sometimes the capacity can't be reduced
TEST( MemoryRegister, CantShrink )
{
    Memreg P;
    P.reserve( 3 );
    Slot a = P.add('a');
    Slot b = P.add('b');
    Slot c = P.add('c');
    P.remove( b );
    P.reserve( 2 );
    EXPECT_EQ('a', P.lookup(a) );
    EXPECT_EQ('c', P.lookup(c) );
    EXPECT_EQ(3u, P.capacity() );
}

TEST( MemoryRegister, CantShrink2 )
{
    Memreg P;
    P.reserve( 3 );
    Slot a = P.add('a');
    Slot b = P.add('b');
    Slot c = P.add('c');
    P.remove( b );
    P.reserve( 2 );
    EXPECT_EQ('a', P.lookup(a) );
    EXPECT_EQ('c', P.lookup(c) );
    EXPECT_EQ(3u, P.capacity() );

    Slot d = P.add('d');
    P.remove(c);
    P.reserve( 2 );
    EXPECT_EQ('a', P.lookup(a) );
    EXPECT_EQ('d', P.lookup(d) );
    EXPECT_EQ(2u, P.capacity() );
}

// Test the case where one process as able to 
// reserve more slots than another. This should be OK
// as long as we don't use more than the global minimum
// number of reserved slots.
TEST( MemoryRegister, Parallel)
{
    Memreg P, Q;
    P.reserve( 3 );
    Q.reserve( 10);
    Slot a1 = P.add('a');
    Slot b1 = P.add('b');
    Slot c1 = P.add('c');
    Slot a2 = Q.add('a');
    Slot b2 = Q.add('b');
    Slot c2 = Q.add('c');
    EXPECT_EQ( a1, a2);
    EXPECT_EQ( b1, b2);
    EXPECT_EQ( c1, c2);
    P.remove( b1 );
    Q.remove( b2 );
    P.remove( c1 );
    Q.remove( c2 );

    // resize only one of them
    P.reserve( 2 );

    Slot d1 = P.add('d');
    Slot d2 = Q.add('d');
    EXPECT_EQ( d1, d2);
    
}



