
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
#include "assert.hpp"
#include <math.h>

#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace lpf {

Time :: Time()
    : m_time()
{
#ifdef LPF_ON_MACOS
    // On MacOS we need the CLOCK_MONOTONIC_RAW, because it will give
    // nanosecond resolution. The ordinary CLOCK_MONOTONIC will only be in
    // microseconds. Some self-tuning mechanisms (e.g. lpf_probe) rely on it.
    clockid_t clock = CLOCK_MONOTONIC_RAW;
#else
    // On Linux this will give nanosecond resolution
    clockid_t clock = CLOCK_MONOTONIC;
#endif
    if (clock_gettime( clock, &m_time ) == -1) {
        throw std::runtime_error( "Unable to query high-precision system clock" );
    }
}

Time :: Time( double t )
    : m_time()
{
    m_time.tv_sec = static_cast<long>(floor(t));
    
    double oneSecond = 1e+9;
    m_time.tv_nsec = 
        static_cast<long>(round(oneSecond * (t - m_time.tv_sec)));
}

Time :: Time( long x )
    : m_time()
{
    m_time.tv_sec = x;
    m_time.tv_nsec = 0;
}


Time Time :: now(  )
{
    return Time();
}

Time Time :: fromSeconds( double t )
{
    return Time(t);
}

Time Time :: zero()
{
    return Time(0L);
}


double Time :: toSeconds( ) const
{
    return m_time.tv_sec + m_time.tv_nsec * 1e-9;
}

Time & Time :: operator-=( Time x )
{
    m_time.tv_sec -= x.m_time.tv_sec;
    if ( m_time.tv_nsec < x.m_time.tv_nsec )
    {
        long oneSecond = 1000000000;
        m_time.tv_nsec = oneSecond - x.m_time.tv_nsec + m_time.tv_nsec;
        m_time.tv_sec -= 1;
    }
    else
    {
        m_time.tv_nsec -= x.m_time.tv_nsec;
    }

    return *this;
}

Time & Time :: operator+=( Time x )
{
    m_time.tv_sec += x.m_time.tv_sec;
    long oneSecond = 1000000000;
    if ( m_time.tv_nsec >= oneSecond - x.m_time.tv_nsec )
    {
        m_time.tv_sec += 1;
        m_time.tv_nsec = m_time.tv_nsec + x.m_time.tv_nsec - oneSecond;
    }
    else
    {
        m_time.tv_nsec += x.m_time.tv_nsec;
    }

    return *this;
}



Time & Time :: operator*=( double a )
{
    double oneSecond = 1000000000;
    double nsec = a * m_time.tv_nsec;
    m_time.tv_nsec = ( long ) fmod( nsec, oneSecond );
    m_time.tv_sec = ( time_t ) ( a * m_time.tv_sec + nsec / oneSecond );

    if (m_time.tv_nsec < 0)
    {
        m_time.tv_sec -= 1;
        m_time.tv_nsec += static_cast<long>(oneSecond);
    }
    return *this;
}


bool Time :: equals( Time x ) const
{
    return m_time.tv_sec == x.m_time.tv_sec 
        && m_time.tv_nsec == x.m_time.tv_nsec;
}

bool Time :: lessThan( Time x ) const
{
    if ( m_time.tv_sec > x.m_time.tv_sec )
    {
        return 0;
    }
    else if ( m_time.tv_sec == x.m_time.tv_sec && m_time.tv_nsec >= x.m_time.tv_nsec )
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void Time :: output( std::ostream & out) const
{
    long sec = m_time.tv_sec;
    long nsec = m_time.tv_nsec;

    if (sec < 0) 
    {
        long oneSecond = 1000000000;
        nsec = oneSecond - nsec; 
        sec += 1; 
    } 

    ASSERT( nsec >= 0);

    out << sec << '.' << std::setfill('0') << std::setw(9) << nsec;
}

void busyWait ( Time timeout )
{
    Time deadline = Time :: now() + timeout;
    while ( Time :: now() < deadline )
    {
        /* do nothing */
    }
}

std::ostream & operator<<( std::ostream & out, Time a)
{
    a.output(out);
    return out;
}

}
