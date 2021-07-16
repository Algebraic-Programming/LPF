
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

#ifndef PLATFORM_CORE_GLOBALSTATE_HPP
#define PLATFORM_CORE_GLOBALSTATE_HPP


#include "types.hpp"
#include "barrier.hpp"
#include "msgqueue.hpp"
#include "linkage.hpp"

namespace lpf {

    
class _LPFLIB_LOCAL GlobalState
{
public:
    explicit GlobalState( pid_t nprocs );

    void init( pid_t pid ); // throws bad_alloc

    void destroy( pid_t pid );

    bool sync( pid_t pid ); // throws Abort::BarrierException 

    void put( pid_t srcPid, memslot_t srcSlot, size_t srcOffset, 
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
            size_t size ); // nothrow
    void get( pid_t srcPid, memslot_t srcSlot, size_t srcOffset, 
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
            size_t size ); // nothrow

    static memslot_t invalidSlot() 
    { return CombinedMemoryRegister<Memory>::invalidSlot(); }

    memslot_t registerGlobal( pid_t pid, void * mem, size_t size ); // nothrow
    memslot_t registerLocal( pid_t pid, void * mem, size_t size ); // nothrow
    void deregister( pid_t pid, memslot_t slot ); // nothrow

    err_t resizeMemreg( pid_t pid, size_t nRegs ); // throws bad_alloc
    err_t resizeMesgQueue( pid_t pid, size_t nMsgs ); // throws bad_alloc

    void abort() // nothrow
    { m_barrier.abort(); }
         
    bool isAborted() const // nothrow
    { return m_barrier.isAborted(); }

    pid_t nprocs() const
    { return m_nprocs; }

private:
    pid_t m_nprocs;
    Barrier m_barrier;

    MsgQueue m_msgQueue;
    std::vector<size_t> m_nextMsgQueueCapacity;
    std::vector<size_t> m_msgQueueCapacity;

    struct Memory { 
        char * addr; 
#ifndef NDEBUG
        size_t size; 
#endif    
    };
    typedef std::vector< CombinedMemoryRegister<Memory> > Register;
    Register m_register;
    std::vector<size_t> m_nextRegisterCapacity;
};


} // namespace lpf

#endif
