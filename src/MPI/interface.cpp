
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

#include <cstdlib>

#include "interface.hpp"
#include "process.hpp"
#include "mpilib.hpp"
#include "trysync.hpp"
#include "assert.hpp"
#include "log.hpp"

namespace lpf  {

Interface * Interface :: s_root = NULL;

Interface :: MachineParams & Interface :: machineParams()
{ static MachineParams obj; return obj ; }

void Interface :: setMachineParams( lpf_t lpf, lpf_pid_t pid,
        lpf_pid_t nprocs, lpf_args_t args )
{
    (void) args;
    machineParams() = MachineParams::hookProbe( lpf, pid, nprocs );
}

void Interface :: doProbe( const mpi::Comm & comm)
{
    if (LPF_SUCCESS != hook( comm, setMachineParams, LPF_NO_ARGS) )
    {
        LOG(1, "Failed to probe machine parmeters" );
    }
}

void Interface :: initRoot(int *argc, char ***argv)
{
    // 0. all processes enter here
    // Initialize MPI
    (void) mpi::Lib::instance(argc, argv);

    // 1. Measure the machine parameters, and save on all processes
    doProbe( mpi::Lib::instance().world() );
    
    // 2. Make slave processes enter wait cycle
    static Process rootProcess( mpi::Lib::instance().world() );
    // 2. only root continues immediately, slave process continue when master
    //    calls exit()
    // 3. The root allocates memory for the root interface. For the slaves this
    //    is irrelevant
    static Interface root( mpi::Lib::instance().self(), rootProcess );
    s_root = & root;

    // Only the 'root' process, i.e with pid = 0, may return from this function
    // all others are slave, which should exit the program before 'main()' is 
    // started
    if ( 0 != mpi::Lib::instance().world().pid() )
        exit( EXIT_SUCCESS );
}


Interface :: Interface( mpi::Comm machine, Process & subprocess )
try : m_comm( machine )
    , m_subprocess( subprocess )
    , m_mesgQueue( m_comm )
    , m_aborted( false )
{
     if ( machine.allreduceOr( false ) )
     {
         LOG(2, "Some other process has not enough memory to initialize" );
         throw std::bad_alloc();
     }
}
catch ( const std::bad_alloc & e)
{
    LOG(2, "Process '" << machine.pid() << "' does not have enough memory to initialize" );
    (void) machine.allreduceOr( true );
    throw;
}

void Interface :: put( memslot_t srcSlot, size_t srcOffset, 
        pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
        size_t size ) 
{
    m_mesgQueue.put( srcSlot, srcOffset,
            dstPid, dstSlot, dstOffset, 
            size );
}

void Interface :: getRcvdMsgCountPerSlot(size_t * msgs, SlotID slot) {
    m_mesgQueue.getRcvdMsgCountPerSlot(msgs, slot);
}

void Interface :: getRcvdMsgCount(size_t * msgs) {
    m_mesgQueue.getRcvdMsgCount(msgs);
}

void Interface :: get( pid_t srcPid, memslot_t srcSlot, size_t srcOffset, 
        memslot_t dstSlot, size_t dstOffset,
        size_t size )
{
    m_mesgQueue.get( srcPid, srcSlot, srcOffset,
            dstSlot, dstOffset,
            size );
}

memslot_t Interface :: registerGlobal( void * mem, size_t size )
{
    return m_mesgQueue.addGlobalReg( mem, size );
}

memslot_t Interface :: registerLocal( void * mem, size_t size ) 
{
    return m_mesgQueue.addLocalReg( mem, size );
}

void Interface :: deregister( memslot_t slot )
{
    m_mesgQueue.removeReg( slot );
}

err_t Interface :: resizeMemreg( size_t nRegs ) 
{
    return m_mesgQueue.resizeMemreg( nRegs );
}

err_t Interface :: resizeMesgQueue( size_t nMsgs ) 
{
    return m_mesgQueue.resizeMesgQueue( nMsgs );
}

void Interface :: abort()
{
    ASSERT( 0 == m_aborted );
    // signal all other processes at the start of the next 'sync' that
    // this process aborted.
    int vote = 1;
    int voted;
    m_comm.allreduceSum(&vote, &voted, 1);
    m_aborted = voted;
}

pid_t Interface  :: isAborted() const
{
    return m_aborted;
}

err_t Interface ::  sync()
{
    if ( 0 == m_aborted )
    {
        m_aborted = m_mesgQueue.sync( false );
    }
    
    if ( 0 == m_aborted )
    {
        return LPF_SUCCESS;
    }
    else
    {
        return LPF_ERR_FATAL;
    }
}

err_t Interface :: exec( pid_t P, spmd_t spmd, args_t args ) 
{
    return m_subprocess.exec( P, spmd, args );
}

err_t Interface :: hook( const mpi::Comm & comm, spmd_t spmd, args_t args ) 
{
    // FIXME: cannot handle out-of-memory situations
    Process subprocess( mpi::Lib::instance().self() );

    return  Process::hook( comm, subprocess, spmd, args );
}

err_t Interface :: rehook( spmd_t spmd, args_t args )
{
    err_t rc = LPF_SUCCESS;
    mpi::Comm clone;

    TRYSYNC_BEGIN( m_comm ) {
        clone = m_comm.clone();
    } TRYSYNC_END( rc );

    if (LPF_SUCCESS == rc) {
       rc = Process::hook( clone, m_subprocess, spmd, args);
    }
    return rc;
}

double Interface::messageGap( 
        lpf_pid_t p, 
        size_t min_msg_size, 
        lpf_sync_attr_t attr 
        ) 
{
    (void)p;
    (void)attr;
#if defined LPF_CORE_IMPL_ID && defined LPF_CORE_IMPL_CONFIG
    using namespace LPF_CORE_IMPL_ID :: LPF_CORE_IMPL_CONFIG;
#endif
 
    return machineParams().getMessageGap( min_msg_size );
}

double Interface::latency( lpf_pid_t p, size_t min_msg_size, lpf_sync_attr_t attr ) {
    (void)p;
    (void)attr;
#if defined LPF_CORE_IMPL_ID && defined LPF_CORE_IMPL_CONFIG
    using namespace LPF_CORE_IMPL_ID :: LPF_CORE_IMPL_CONFIG;
#endif
    return machineParams().getLatency(min_msg_size);
}


void Interface :: probe( machine_t & machine )
{
    machine.p = mpi::Lib::instance().total_nprocs();
    machine.free_p = m_subprocess.nprocs();
    machine.g = &Interface::messageGap;
    machine.l = &Interface::latency;
}

} // namespace lpf
