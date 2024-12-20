
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

#ifndef LPF_CORE_MPI_INTERFACE_HPP
#define LPF_CORE_MPI_INTERFACE_HPP

#include "types.hpp"
#include "mesgqueue.hpp"
#include "mpilib.hpp"
#include "machineparams.hpp"
#include "linkage.hpp"
#include "assert.hpp"

namespace lpf
{
    class _LPFLIB_LOCAL Process;

class _LPFLIB_LOCAL Interface  
{
public:
    static Interface * root() 
    { 
        ASSERT( s_root !=  0 && "LPF_ROOT was not initalized" );
        return s_root; 
    }

    _LPFLIB_API
    static void initRoot(int *argc, char ***argv);

    Interface( mpi::Comm machine, Process & subprocess );

    void put( memslot_t srcSlot, size_t srcOffset, 
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
            size_t size ) ; // nothrow

    void get( pid_t srcPid, memslot_t srcSlot, size_t srcOffset, 
            memslot_t dstSlot, size_t dstOffset,
            size_t size ) ;// nothrow

    memslot_t registerGlobal( void * mem, size_t size ) ; // nothrow

    memslot_t registerLocal( void * mem, size_t size ) ;  // nothrow

    void deregister( memslot_t slot ) ; // nothrow

    err_t resizeMemreg( size_t nRegs ) ; // nothrow
    err_t resizeMesgQueue( size_t nMsgs ) ; // nothrow

    void abort() ; // nothrow

    pid_t isAborted() const ;
 
    err_t sync(); // nothrow

    err_t exec( pid_t P, spmd_t spmd, args_t args ) ;

    static err_t hook( const mpi::Comm & comm , spmd_t spmd, args_t args );

    // only for HiCR
    // #if
    err_t countingSyncPerSlot(memslot_t slot, size_t expected_sent, size_t expected_rcvd);
                                                                                           
    err_t syncPerSlot(memslot_t slot);

    typedef size_t SlotID;

    void getRcvdMsgCountPerSlot(size_t * msgs, SlotID slot);

    void getSentMsgCountPerSlot(size_t * msgs, SlotID slot);

    void getRcvdMsgCount(size_t * msgs);

    void flushSent();

    void flushReceived();

    void lockSlot( memslot_t srcSlot, size_t srcOffset, 
		    pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
		    size_t size );

    void unlockSlot( memslot_t srcSlot, size_t srcOffset, 
		    pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
		    size_t size );

    // only for HiCR
//#endif
    err_t rehook( spmd_t spmd, args_t args);

    void probe( machine_t & machine ) ;

    static void doProbe(const mpi::Comm & comm);

private:
    mpi::Comm m_comm;
    Process & m_subprocess;
    MessageQueue m_mesgQueue;
    pid_t m_aborted;

    static Interface * s_root;

#if defined LPF_CORE_IMPL_ID && defined LPF_CORE_IMPL_CONFIG
    typedef LPF_CORE_IMPL_ID :: LPF_CORE_IMPL_CONFIG :: MachineParams MachineParams;
#else
    typedef ::lpf::MachineParams MachineParams;
#endif

    static void setMachineParams(lpf_t lpf, lpf_pid_t pid,
            lpf_pid_t nprocs, lpf_args_t args );
    static MachineParams & machineParams();
    static double messageGap( lpf_pid_t p, size_t min_msg_size, lpf_sync_attr_t);
    static double latency( lpf_pid_t p, size_t min_msg_size, lpf_sync_attr_t);
};

}

#endif
