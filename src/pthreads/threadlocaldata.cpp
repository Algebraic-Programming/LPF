
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

#include "threadlocaldata.hpp"
#include <config.hpp>
#include <log.hpp>
#include <assert.hpp>

#include <cstdlib>
#include <sstream>
#include <string>

#include <pthread.h> // for pthreads

namespace lpf {

namespace {

#ifdef LPF_CORE_PTHREAD_USE_HWLOC
    class HwLoc 
    {
    public:
        static HwLoc & instance() 
        {
            static HwLoc obj;
            return obj;
        }

        std::vector< hwloc_cpuset_t > parseCpusets(
                const std::string & bitmaps)
        {
            std::vector< hwloc_cpuset_t > cpusets;
            if (bitmaps.empty())
                return cpusets;

            std::istringstream in( bitmaps );
            std::string bitmap;
            while ( getline( in, bitmap, '+' ) )
            {
                hwloc_cpuset_t set
                    = hwloc_bitmap_alloc();
                m_bitmaps.push_back( set );
                cpusets.push_back( set );

                int rc = hwloc_bitmap_sscanf( set, bitmap.c_str() );
                if (rc != 0 )
                {
                    LOG( 1, "Could not parse hwloc cpuset. Check LPF_PROC_PINNING");
                    abort();
                }
            }

            return cpusets;
        }


        void bind( hwloc_const_cpuset_t cpuset ) 
        {
            int rc  = hwloc_set_cpubind( m_hwloc_topo,
                cpuset, static_cast<int>(HWLOC_CPUBIND_THREAD) );
            if (rc != 0) 
            {
                LOG(2, "Could not pin thread to core " );
            }
            else
            {
                LOG(3, "Thread is pinned" );
            }

            rc  = hwloc_set_membind( m_hwloc_topo, cpuset, HWLOC_MEMBIND_BIND, 
                    static_cast<int>(HWLOC_MEMBIND_THREAD) | 
                    static_cast<int>(HWLOC_MEMBIND_STRICT) );
            if (rc != 0) 
            {
                LOG(2, "Could not set default memory binding policy to always bind to NUMA node running this thread" );
            }
            else
            {
                LOG(3, "Local memory will be bound to NUMA node running this thread" );
            }
        }


     private:
        HwLoc()
        {
           int rc = hwloc_topology_init( &m_hwloc_topo );
           if (rc != 0) {
               LOG(1, "Could not initialize hwloc library");
               abort();
           }
           rc = hwloc_topology_load( m_hwloc_topo );
           if (rc != 0) {
               LOG(1, "Could not load topology with hwloc library");
               abort();
           }
        }

        ~HwLoc()
        {
            while (!m_bitmaps.empty()) {  //lint !e1551 std::vector::empty never throws
                hwloc_bitmap_free( m_bitmaps.back() ); //lint !e1551 back() never throws
                m_bitmaps.pop_back();     //lint !e1551 pop_back() never throws
            }
            hwloc_topology_destroy( m_hwloc_topo );
            std::memset( &m_hwloc_topo, 0, sizeof(m_hwloc_topo) );
        }

        HwLoc( const HwLoc & ); // prohibit copying
        HwLoc & operator=( const HwLoc & ); // prohibit assignment
        
        hwloc_topology_t m_hwloc_topo;
        std::vector< hwloc_bitmap_t > m_bitmaps;
    };
#endif

    pid_t sizeOfLocalPart( pid_t global_n, pid_t pid, pid_t nprocs )
    {
        return ( global_n + nprocs - pid - 1 ) / nprocs; 
    }


    struct SpmdDispatchParams
    {
        spmd_t spmd;
        GlobalState * state;
        pid_t pid;
        pid_t nprocs;
        pid_t availableProcs;
        pid_t allProcs;
        lpf_args_t args;
        err_t status;
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
        hwloc_cpuset_t * hwloc_cpuset;
        pid_t hwloc_cpuset_count;
#endif
    };

    void * spmdDispatch( void * p )
    {
        SpmdDispatchParams * params = static_cast<SpmdDispatchParams *>(p);

        const pid_t pid= params->pid;
        const pid_t nprocs = params->nprocs;
        const pid_t availableProcs = params->availableProcs;
        const pid_t allProcs = params->allProcs;
        const pid_t newSubP = sizeOfLocalPart( availableProcs, pid, nprocs );
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
        const hwloc_cpuset_t * cpuset = params->hwloc_cpuset;
        const pid_t nCpusets = params->hwloc_cpuset_count;
        ASSERT( 0 == nCpusets || newSubP == nCpusets );

        // set the cpu affinity
        if ( cpuset && nCpusets > 0)
        {
            HwLoc::instance().bind( cpuset[0] );
        }
        else {
            LOG(4, "Thread is not pinned");
        }
#endif

        try
        {
            ThreadLocalData runtime( 
                    pid, nprocs, newSubP, allProcs, 
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
                    cpuset, nCpusets , 
#endif
                    params->state
                 );

            // shield pthread_create from any C++ exceptions
            try
            {
                (*params->spmd)( &runtime, pid, nprocs, params->args );

                // send a message to the other processes that it reached the exit
                runtime.broadcastAtExit();
                // wait for the message to arrive.
                (void) runtime.sync(true);
            }
            catch ( ... )
            {  
                LOG(1, "In thread '" << pid << "' the SPMD function threw an unexpected exception");
                // any abnormal termination of this thread aborts all others
                runtime.abort();
            }
            
            // flag any abnormal terminations
            if ( runtime.isAborted() )
            {
                params->status = LPF_ERR_FATAL;
            }
        }
        catch ( std::bad_alloc & )
        {
            LOG(1, "In thread '" << pid << "' there was insufficient memory"
                   " to start the SPMD function");
            // handle out of memory in ThreadLocalData constructor
            params->state->abort();
            params->status = LPF_ERR_FATAL;
        }

        return p;
    }


} // anonymous namespace 


ThreadLocalData :: ThreadLocalData( pid_t pid, pid_t nprocs, pid_t subprocs,
        pid_t allprocs, 
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
        const hwloc_cpuset_t * cpusets, pid_t nCpuSets,
#endif
        GlobalState * state )
    : m_state( state )
    , m_pid( pid )
    , m_nprocs( nprocs )
    , m_subprocs( subprocs )
    , m_allprocs( allprocs )
    , m_atExit()
    , m_atExitSlot( LPF_INVALID_MEMSLOT )
{
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
    if (cpusets) {
        m_childCpusets.resize( nCpuSets );
        std::copy( cpusets, cpusets + nCpuSets, m_childCpusets.begin() );
    }

    if ( nCpuSets != 0 && m_subprocs != nCpuSets )
    {
        LOG(1, "Number of cpusets in LPF_PROC_PINNING does not match LPF_MAX_P");
        abort();
    }
#endif
    m_state->init( m_pid );
    m_atExit[0] = 0; 
    m_atExit[1] = 0;
    err_t status = m_state->resizeMemreg( m_pid, 1 );
    // no sync is necessary, because the implementation of the used memory
    // register immediately puts into effect the larger buffer.
    if ( LPF_SUCCESS == status)
    {
        m_atExitSlot = m_state->registerGlobal( m_pid, m_atExit, sizeof(m_atExit) );
    }
    else
    {
        m_state->abort();
    }

    status = m_state->resizeMesgQueue( m_pid, m_nprocs );
    if ( LPF_SUCCESS != status) 
    {
        m_state->abort();
    }
}

ThreadLocalData :: ~ThreadLocalData()
{
    try {
       m_state->destroy( m_pid );
    }
    catch(...) 
    {
        // GlobalState::destroy cannot be allowed to throw exceptions
        LOG(1, "Thread '" << m_pid << " was unable to exit gracefully");
    }
    m_state = NULL;
}

err_t ThreadLocalData :: exec( pid_t P, spmd_t spmd, args_t args )
{
    // make sure that P is within reasonable range
    ASSERT( P > 0 );
    P = std::min( m_subprocs, P );
    err_t status = LPF_SUCCESS;
    try
    {
        GlobalState globalState(P);

        SpmdDispatchParams params = 
            { spmd, &globalState, 0, P, m_subprocs, m_allprocs, args, LPF_SUCCESS,
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
              NULL, 0u
#endif
            };
        std::vector< SpmdDispatchParams > threadParams( P, params );
        std::vector< pthread_t > threadId( P );
        pid_t nThreads = 0;
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
        pid_t pinOffset = 0;
#endif
        for ( pid_t p = 0; p < P; ++p )
        {
            threadParams[p].pid = p;
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
            threadParams[p].hwloc_cpuset = 
                m_childCpusets.empty() ? NULL : &m_childCpusets[pinOffset];
            threadParams[p].hwloc_cpuset_count = 
                sizeOfLocalPart( m_childCpusets.size(), p, P ) ;
            pinOffset += sizeOfLocalPart( m_childCpusets.size(), p, P ) ;
#endif
            if ( 0 == p )
            {
                threadParams[p].args = args;
            }
            else
            {
                args_t onlySymbols = {NULL, 0, NULL, 0, args.f_symbols, args.f_size};
                threadParams[p].args = onlySymbols;
            }
        }
        // start threads [1,P) using Posix threads
        for ( pid_t p = 1; p < P; ++p)
        {
            if ( 0 == pthread_create( &threadId[p], NULL, spmdDispatch, &threadParams[p] ))
            {
                ++nThreads;
            }
            else
            {   // in case of an error, enter the failure state, and stop
                globalState.abort();
                status = LPF_ERR_FATAL;
                break;
            }
        }

        // Run thread 0
        if (status == LPF_SUCCESS)
        {
            (void) spmdDispatch(&threadParams[0]);
            status = threadParams[0].status;
        }

        // Thread 0 has finished. Now we wait for the rest to finish.
        for ( pid_t p = 1; p <= nThreads; ++p )
        {
            if ( 0 == pthread_join( threadId[p], NULL ) )
            {
                // read the return status of any successfully joined thread
                if ( LPF_SUCCESS != threadParams[p].status )
                {
                    status = threadParams[p].status;
                }
            }
            else
            {
                globalState.abort();
                status = LPF_ERR_FATAL;
            }
        }
    }
    catch( std::bad_alloc & )
    {
        LOG(2, "Insufficient memory to start the SPMD function");
        status = LPF_ERR_OUT_OF_MEMORY;
    }
    return status;
}

err_t ThreadLocalData :: hook( GlobalState * state, pid_t pid, pid_t nprocs, 
        spmd_t spmd, args_t args )
{
    ASSERT( nprocs == state->nprocs() );
    SpmdDispatchParams params = 
        { spmd, state, pid, nprocs, 1, nprocs, args, LPF_SUCCESS, 
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
            NULL, 0
#endif
        };
    (void) spmdDispatch(  &params ); 
    return params.status;
}


void ThreadLocalData :: broadcastAtExit()
{
    m_atExit[1] = 1;
    for (pid_t p = 0; p < m_nprocs; ++p)
    {
        m_state->put( m_pid, m_atExitSlot, sizeof(m_atExit[0]), 
                      p, m_atExitSlot, 0, sizeof(m_atExit[0]) );
    }
}

err_t ThreadLocalData :: resizeMesgQueue( size_t nMsgs ) // nothrow
{ 
    if ( nMsgs  <= std::numeric_limits<size_t>::max() - m_nprocs )
    {
        return m_state->resizeMesgQueue( m_pid, nMsgs + m_nprocs ); 
    }
    else
    {
        return LPF_ERR_OUT_OF_MEMORY;
    }
}

err_t ThreadLocalData :: resizeMemreg( size_t nRegs ) // nothrow
{ 
    if ( nRegs <= std::numeric_limits<size_t>::max() - 1)
    {
        return m_state->resizeMemreg( m_pid, nRegs + 1 ); 
    }
    else
    {
        return LPF_ERR_OUT_OF_MEMORY;
    }
}

err_t ThreadLocalData ::  sync( bool expectExit )
{ 
    if ( m_state->sync(m_pid) )
    {
        LOG( 2, "One of the threads aborted the lpf_sync()" );
        return LPF_ERR_FATAL;
    }

    if ( expectExit != anyAtExit() )
    {
        LOG( 2, "One of the threads returned early from the SPMD function" );
        m_state->abort();
        return LPF_ERR_FATAL;
    }

    return LPF_SUCCESS;
}

namespace {
    int getNumberOfProcs()
    {
        int P = Config::instance().getSysProcs();

        lpf_pid_t userP = Config::instance().getMaxProcs();
        if ( 0 != userP ) 
            P = userP;

        return P;
    }

    void execAutoTuneBarrier( lpf_t , pid_t pid, pid_t nprocs, lpf_args_t )
    {
        const Config::SpinMode cmode = Config::instance().getSpinMode();
        Barrier::SpinMode bmode = Barrier::SPIN_COND;
        switch (cmode) {
            case Config::SPIN_FAST : bmode = Barrier::SPIN_FAST; break;
            case Config::SPIN_PAUSE : bmode = Barrier::SPIN_PAUSE; break;
            case Config::SPIN_YIELD : bmode = Barrier::SPIN_YIELD; break;
            case Config::SPIN_COND : bmode = Barrier::SPIN_COND; break;
        }

        Barrier::autoTune(pid, nprocs, bmode);
    }
}


ThreadLocalData & ThreadLocalData :: root()
{
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
    static std::vector< hwloc_cpuset_t> cpuset
        = HwLoc::instance().parseCpusets(
              Config::instance().getPinBitmap()
          );
#endif
    static GlobalState currentProcess(1);
    static ThreadLocalData object(
                0,  // PID = 0
                1, // nprocs = 1
                getNumberOfProcs(),  // all processors
                getNumberOfProcs(),  // all processors
#ifdef LPF_CORE_PTHREAD_USE_HWLOC
                cpuset.data(), cpuset.size(),
#endif
                &currentProcess
            );
    static int autoTuned = 0;
    if (!autoTuned) {
        err_t rc = object.exec( LPF_MAX_P, &execAutoTuneBarrier, LPF_NO_ARGS);
        autoTuned=1;
        if (rc != LPF_SUCCESS) {
            LOG( 1, "Failed to auto-tune barrier" );
        }
    }

    return object;
}


}
