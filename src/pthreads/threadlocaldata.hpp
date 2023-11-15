
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

#ifndef LPF_CORE_THREADLOCALDATA_HPP
#define LPF_CORE_THREADLOCALDATA_HPP

#include "types.hpp"
#include "globalstate.hpp"
#include "linkage.hpp"

#include <vector>

#ifdef LPF_CORE_PTHREAD_USE_HWLOC
#include <hwloc.h>
#endif

namespace lpf {


class _LPFLIB_LOCAL ThreadLocalData
{
public:
    static ThreadLocalData & root(); // throws bad_alloc
    
    // Instantiates a thread context that share a global shared GlobalState.
    ThreadLocalData( pid_t pid, pid_t nprocs, pid_t subprocs, pid_t allprocs,
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
            const hwloc_cpuset_t * cpusets, pid_t nCpuSets,
#endif
            GlobalState * state );

    ~ThreadLocalData();

    lpf_err_t exec( pid_t P, spmd_t spmd, args_t args); // nothrow

    static lpf_err_t hook( GlobalState * state, pid_t pid, pid_t nprocs, 
            spmd_t spmd, args_t args ); // nothrow

    void put( memslot_t srcSlot, size_t srcOffset, 
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
            size_t size ) // nothrow
    { 
        m_state->put( m_pid, srcSlot, srcOffset,
            dstPid, dstSlot, dstOffset,
            size );
    }

    void get( pid_t srcPid, memslot_t srcSlot, size_t srcOffset, 
            memslot_t dstSlot, size_t dstOffset,
            size_t size ) // nothrow
    { 
        m_state->get( srcPid, srcSlot, srcOffset,
            m_pid, dstSlot, dstOffset,
            size );
    }

    static memslot_t invalidSlot() { return GlobalState::invalidSlot(); }

    memslot_t registerGlobal( void * mem, size_t size )  // nothrow
    { return m_state->registerGlobal(m_pid, mem, size); }

    memslot_t registerLocal( void * mem, size_t size )  // nothrow
    { return m_state->registerLocal(m_pid, mem, size); }

    void deregister( memslot_t slot ) // nothrow
    { return m_state->deregister(m_pid, slot ); }

    lpf_pid_t getNoSubProcs() const // nothrow
    { return m_subprocs; }

    lpf_pid_t getNoAllProcs() const
    { return m_allprocs; }

    lpf_pid_t getPid() const 
    { return m_pid; }

    lpf_pid_t getNprocs() const
    { return m_nprocs; }

    err_t resizeMesgQueue( size_t nMsgs ); // nothrow
    err_t resizeMemreg( size_t nRegs ); // nothrow

    void abort() // nothrow
    { m_state->abort(); }

    bool isAborted() const
    { return m_state->isAborted(); }

    void broadcastAtExit();
    bool anyAtExit() const
    { return m_atExit[0]; }
 
    err_t sync( bool expectExit = false ); // nothrow
    err_t countingSyncPerSlot( bool expectExit = false, lpf_memslot_t slot = LPF_INVALID_MEMSLOT, size_t expected_sent = 0, size_t expected_rcvd = 0); // nothrow
    err_t syncPerSlot( bool expectExit = false, lpf_memslot_t slot = LPF_INVALID_MEMSLOT); // nothrow
       
private:
    ThreadLocalData( const ThreadLocalData & ) ; // prohibit copying
    ThreadLocalData & operator=( const ThreadLocalData & ); // prohibit assignment 

    GlobalState * m_state;
    pid_t m_pid;
    pid_t m_nprocs;
    pid_t m_subprocs;
    pid_t m_allprocs;
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
    std::vector< hwloc_cpuset_t > m_childCpusets;
#endif
    int m_atExit[2]; memslot_t m_atExitSlot;
};

}
#endif
