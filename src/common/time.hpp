
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

#ifndef LPFLIB_COMMON_TIME_H
#define LPFLIB_COMMON_TIME_H

#include <time.h>
#include <iosfwd>

#include "linkage.hpp"

namespace lpf {

/// A high precision timer.
class _LPFLIB_LOCAL Time
{
public:
    static Time now();
    static Time fromSeconds( double t );
    static Time zero();

    double toSeconds() const;

    Time &  operator-=( Time x );
    Time & operator+=( Time x );
    Time & operator*=( double x);

    bool lessThan( Time x ) const;
    bool equals( Time x ) const;

    void output( std::ostream & out ) const;

    const timespec & getTimespec() const
    { return m_time; }
private:
    Time();
    explicit Time( double x );
    explicit Time( long x );

    struct timespec m_time;
};

inline _LPFLIB_LOCAL
Time operator+( Time a, Time b ) { return a += b; }

inline _LPFLIB_LOCAL
Time operator-( Time a, Time b ) { return a -= b; }

inline _LPFLIB_LOCAL
Time operator*( double a, Time b ) { return b *= a; }

inline _LPFLIB_LOCAL
Time operator*( Time a, double b ) { return a *= b; }

inline _LPFLIB_LOCAL
bool operator<( Time a, Time b )
{ return a.lessThan( b ); }

inline _LPFLIB_LOCAL
bool operator==(Time a, Time b )
{ return a.equals(b); }

inline _LPFLIB_LOCAL
bool operator<=( Time a, Time b )
{ return a.lessThan(b) || a.equals(b) ; }

inline _LPFLIB_LOCAL
bool operator>=( Time a, Time b )
{ return ! ( a.lessThan(b) ); }

inline _LPFLIB_LOCAL
bool operator>( Time a, Time b )
{ return ! ( a.lessThan(b) ) && !( a.equals(b) ); }

inline _LPFLIB_LOCAL
bool operator!=( Time a, Time b )
{ return ! ( a.equals( b ) ) ; }

extern _LPFLIB_LOCAL
void busyWait( Time timeout );

extern _LPFLIB_LOCAL
std::ostream & operator<<( std::ostream & out, Time a );


}

#endif
