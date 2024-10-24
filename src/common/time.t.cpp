
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

#include "time.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <cmath>

using namespace lpf;

const double eps = std::numeric_limits<double>::epsilon();

/** 
 * \test Time tests
 * \pre P <= 1
 * \return Exit code: 0
 */
TEST( Time, zero)
{
    Time zero = Time::fromSeconds(0.0);
    
    // test methods
    EXPECT_LT( std::fabs( zero.toSeconds()), eps );
    EXPECT_TRUE( zero.equals(zero) );
    EXPECT_FALSE( zero.lessThan(zero) );
    
    std::ostringstream s;
    zero.output(s);
    EXPECT_EQ( std::string("0.000000000"), s.str() );

    // test operators
    EXPECT_EQ( zero, zero );
    EXPECT_EQ( zero, zero + zero );
    EXPECT_EQ( zero, zero - zero );
    EXPECT_EQ( zero, 0 * zero );
    EXPECT_EQ( zero, 1 * zero );
    EXPECT_EQ( zero, zero * 0 );
    EXPECT_EQ( zero, zero * -1 );

    EXPECT_LE( zero, zero);
    EXPECT_GE( zero, zero);

    std::ostringstream t;
    t << zero;
    EXPECT_EQ( std::string("0.000000000"), t.str() );

    Time t0 = Time::fromSeconds(1.0);
    EXPECT_EQ( zero, 0 * t0 );
    EXPECT_EQ( zero, t0 * 0 );
    EXPECT_NE( zero, t0 );

    EXPECT_LT( zero - t0, zero );
    EXPECT_GT( zero + t0, zero );
    EXPECT_GT( zero, zero - t0 );
    EXPECT_LT( zero, zero + t0 );
    EXPECT_EQ( t0, zero + t0);
    EXPECT_EQ( t0, t0 + zero);
}

TEST( Time, oneSecond)
{
    Time one = Time::fromSeconds(1.0);
    Time zero = Time::fromSeconds(0.0);
    
    // test methods
    EXPECT_LT( std::fabs( one.toSeconds() - 1.0), eps );
    EXPECT_TRUE( one.equals(one) );
    EXPECT_FALSE( one.lessThan(one) );
    
    std::ostringstream s;
    one.output(s);
    EXPECT_EQ( std::string("1.000000000"), s.str() );

    // test operators
    EXPECT_EQ( one, one );
    EXPECT_LT( one, one + one );
    EXPECT_EQ( zero, one - one );
    EXPECT_EQ( zero, 0 * one );
    EXPECT_EQ( one, 1 * one );
    EXPECT_EQ( zero, one * 0 );
    EXPECT_EQ( zero - one, one * -1 );

    EXPECT_LE( one, one );
    EXPECT_GE( one, one );

    std::ostringstream t;
    t << one;
    EXPECT_EQ( std::string("1.000000000"), t.str() );
}

TEST( Time, piSeconds)
{
    double pi_sec = 3.141592653;
    Time pi = Time::fromSeconds(pi_sec);
    Time zero = Time::fromSeconds(0.0);
    
    // test methods
    EXPECT_LT( std::fabs( pi.toSeconds() - pi_sec), std::numeric_limits<double>::epsilon() );
    EXPECT_TRUE( pi.equals(pi) );
    EXPECT_FALSE( pi.lessThan(pi) );
    
    std::ostringstream s;
    pi.output(s);
    EXPECT_EQ( std::string("3.141592653"), s.str() );

    // test operators
    EXPECT_EQ( pi, pi );
    EXPECT_LT( pi, pi + pi );
    EXPECT_EQ( zero, pi - pi );
    EXPECT_EQ( zero, 0 * pi);
    EXPECT_EQ( zero - pi, -1 * pi);
    EXPECT_EQ( zero, pi * 0 );
    EXPECT_EQ( pi, pi * 1 );

    EXPECT_LE( pi, pi);
    EXPECT_GE( pi, pi);

    std::ostringstream t;
    t << pi;
    EXPECT_EQ( std::string("3.141592653"), t.str() );
}

TEST( Time, busyWait )
{
    Time t0 = Time::now();
    Time timeout = Time::fromSeconds(0.001);
    busyWait( timeout );
    Time t1 = Time::now();
    
    EXPECT_GE( t1 - t0, timeout );
}

TEST( Time, extraArithmetic )
{
    Time a = Time::fromSeconds( 1.0 );
    Time b = Time::fromSeconds( 0.9 );
    Time c = Time::fromSeconds( -0.1 );
    Time d = Time::fromSeconds( 0.1 );
    Time e = Time::fromSeconds( 0.0 );

    EXPECT_EQ( c, b - a);
    EXPECT_EQ( d, a - b );
    EXPECT_EQ( a, b - c );
    EXPECT_EQ( a, b + d );
    EXPECT_EQ( e, c + d);
}

TEST( Time, extraComparisons)
{
    Time a = Time::fromSeconds(2.0 );
    Time b = Time::fromSeconds(2.2 );
    Time c = Time::fromSeconds(3.0 );

    EXPECT_FALSE( a.lessThan(a) );
    EXPECT_TRUE( a.lessThan(b) );
    EXPECT_FALSE( b.lessThan(a) );
    EXPECT_TRUE( a.lessThan(c) );
    EXPECT_FALSE( c.lessThan(a) );
    EXPECT_TRUE( b.lessThan(c) );
    EXPECT_FALSE( c.lessThan(b) );
}

TEST( Time, strings)
{
    Time a = Time::fromSeconds( 2.456);
    Time c = Time::fromSeconds( -4.567 );

    EXPECT_LT( fabs( a.toSeconds() - 2.456), eps );
    EXPECT_LT( fabs( c.toSeconds() + 4.567), eps );

    std::ostringstream s;
    a.output(s);
    EXPECT_EQ( std::string("2.456000000"), s.str());
    s.str("");
    c.output(s);
    EXPECT_EQ( std::string("-4.567000000"), s.str());

    s.str("");
    Time d = a + c;
    s << d;
    EXPECT_EQ( std::string("-2.111000000"), s.str());
}

