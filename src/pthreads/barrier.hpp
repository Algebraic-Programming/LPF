
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

#ifndef PLATFORM_CORE_BARRIER_HPP
#define PLATFORM_CORE_BARRIER_HPP

#include <stdexcept>

#include "linkage.hpp"
#include "types.hpp"

namespace lpf {

class _LPFLIB_LOCAL Barrier
{
public:
    enum SpinMode {
        SPIN_FAST,   /* no waiting */
        SPIN_PAUSE,  /* run _mm_pause */
        SPIN_YIELD,  /* spin locks that yield */ 
        SPIN_COND /* use pthread condition wait */
    };

    explicit Barrier( pid_t nprocs, SpinMode  ) ;
    ~Barrier();

    void init(pid_t pid); // does processor local initialization
    bool execute(pid_t pid) ; // nothrow, returns true iff barrier was aborted

    void abort()               // nothrow
    { m_abort = true; }

    bool isAborted() const     // nothrow
    { return m_abort; }
 

    // Collectively autotunes the barrier operation for
    // newly created barrier objects. This should be
    // called from all threads
    static void autoTune( pid_t pid, pid_t nprocs, SpinMode mode);  

private:
    Barrier( const Barrier & );
    Barrier & operator=( const Barrier & );

    template <SpinMode mode>
    struct Impl ;

    pid_t m_nprocs;
    Impl< SPIN_FAST >  * m_spinfast;
    Impl< SPIN_PAUSE > * m_spinnice;
    Impl< SPIN_YIELD > * m_spinht;
    Impl< SPIN_COND> * m_spincond;
    volatile bool m_abort;
};

} // namespace lpf

#endif
