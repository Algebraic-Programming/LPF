
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

#include "dispatch.hpp"
#include "blob.hpp"
#include "log.hpp"
#include "config.hpp"
#include "state.hpp"
#include "machineparams.hpp"

#include "linkage.hpp"

namespace lpf { namespace hybrid {


    // note: can't use anonymous namespace, because MPI implementation
    // requires external linkage for the launchers.

    void _LPFLIB_LOCAL launchThreadStage( Thread::ctx_t lpf, 
            Thread::pid_t pid, Thread::pid_t nprocs, Thread::args_t args )
    {
        BufBlob input;
        UnbufBlob output(args.output, args.output_size, args.output_size);

        bool root = false;
        if (pid == 0 && output.size() > 0)
        {   lpf_err_t garbage;
            output.pop( garbage ); 
            root = true;
        }


        Thread::err_t rc = Thread::SUCCESS;
        NodeState * nodeStatePtr = NULL;
        if ( pid == 0 )
        {
            try {
                input.push(args.input, args.input_size);
            }
            catch( std::bad_alloc & e)
            {
                rc = Thread::ERR_OUT_OF_MEMORY;
            }
            input.pop( nodeStatePtr );
        }

        Thread thread(lpf, pid, nprocs);

        rc = rc != Thread::SUCCESS ? rc : broadcast( thread, 0, nodeStatePtr );
        if ( Thread::SUCCESS != rc ) {
            lpf_err_t fatal = LPF_ERR_FATAL;
            if (root) output.push(fatal);
            return;
        }

        try {
            ThreadState newThreadState( nodeStatePtr, thread );

            lpf_args_t newArgs;
            newArgs.input = input.size()==0?NULL:input.data();
            newArgs.input_size = input.size();
            newArgs.output = output.size()==0?NULL:output.data();
            newArgs.output_size = output.size();
            newArgs.f_symbols = args.f_symbols;
            newArgs.f_size = args.f_size - 1;

            lpf_spmd_t spmd = reinterpret_cast<lpf_spmd_t>(
                    args.f_symbols[ args.f_size-1]
                    );
            (*spmd)( &newThreadState, newThreadState.pid(), 
                    newThreadState.nprocs(), newArgs );
            lpf_err_t success = LPF_SUCCESS;
            if (root) output.push( success );
        }
        catch(std::bad_alloc & e)
        {
            LOG(1, "Insufficient memory to run SPMD function on thread " << pid << " of node " 
                    << nodeStatePtr->nodeId() );
            lpf_err_t fatal = LPF_ERR_OUT_OF_MEMORY ;
            if (root) output.push( fatal );
        }
        catch(...)
        {
            LOG(1, "SPMD function of thread " << pid << " of node " 
                    << nodeStatePtr->nodeId() << " threw an unexpected exception");
            lpf_err_t fatal = LPF_ERR_FATAL;
            if (root) output.push( fatal );
        }
    }

    _LPFLIB_SPMD
    void launchMpiStage( MPI::ctx_t lpf, MPI::pid_t pid,
            MPI::pid_t nprocs, MPI::args_t args )
    {
        // scatter the array with thread allocations
        MPI mpi(lpf, pid, nprocs);

        std::vector< Thread::pid_t > threads;
        using LPF_CORE_IMPL_CONFIG::MachineParams;
        MachineParams machineParams;
        BufBlob input; 
        UnbufBlob output(args.output, args.output_size, args.output_size);

        MPI::err_t rc = MPI::SUCCESS;
        if (pid==0)
        {
            try {
                input.push(args.input, args.input_size); 
                threads.resize( nprocs );
                input.pop( machineParams );
                input.pop( threads );
            }
            catch(std::bad_alloc & e)
            {
                rc = MPI::ERR_OUT_OF_MEMORY;
            }
        }

        rc = rc != MPI::SUCCESS? rc : broadcast( mpi, 0, threads );
        rc = rc != MPI::SUCCESS? rc : broadcast( mpi, 0, machineParams );
        
        if (rc != MPI::SUCCESS) {
            if (pid==0) {
                lpf_err_t garbage = LPF_SUCCESS ;
                lpf_err_t frc = 
                    rc == MPI::ERR_OUT_OF_MEMORY ? LPF_ERR_OUT_OF_MEMORY 
                                                 : LPF_ERR_FATAL;
                output.pop( garbage );
                output.push( frc );
            }
            return;   
        }

        // Construct the node state object and store a reference to
        // the input arguments
        NodeState nodeState( threads, mpi );
        nodeState.machineParams( ) = machineParams ;
        NodeState * nodeStatePtr = & nodeState;
        
        char buffer[ sizeof(nodeStatePtr) ];
        UnbufBlob newInput( buffer, 0, sizeof(buffer) );
        Thread::args_t threadArgs;
        if (pid == 0) {
            input.push( nodeStatePtr );
            threadArgs.input = input.data();
            threadArgs.input_size = input.size();
        }
        else
        {
            newInput.push( nodeStatePtr );
            threadArgs.input = newInput.data();
            threadArgs.input_size = newInput.size();
        }

        threadArgs.output = output.data();
        threadArgs.output_size = output.size();
        threadArgs.f_symbols = args.f_symbols;
        threadArgs.f_size = args.f_size;
       

        Thread threadRoot( Thread::ROOT, 0, 1 );
        Thread :: err_t trc =
            threadRoot.exec( threads[pid], launchThreadStage, threadArgs);
        
        // save the exit code
        lpf_err_t frc = LPF_SUCCESS;
        if (trc == Thread::ERR_OUT_OF_MEMORY)
            frc = LPF_ERR_OUT_OF_MEMORY;
        else if (trc != Thread::SUCCESS )
            frc = LPF_ERR_FATAL;

        if (pid == 0)
        {
            lpf_err_t garbage = LPF_SUCCESS ;
            output.pop( garbage );
            output.push( frc );
        }
    }

    _LPFLIB_SPMD
    void getMaxThreadsPerNode_spmd( MPI::ctx_t lpf, MPI::pid_t pid,
            MPI::pid_t nprocs, MPI::args_t args )
    {
        MPI mpi(lpf, pid, nprocs);
        MPI::err_t rc = mpi.resize_memory_register( 2 );
        if ( rc != MPI::SUCCESS ) return;

        rc = mpi.resize_message_queue( nprocs );
        if ( rc != MPI::SUCCESS ) return;

        Thread::machine_t probe ;
        Thread thread( Thread::ROOT, 0, 1 );
        rc = thread.probe( & probe );
        if ( rc != MPI::SUCCESS ) return;

        rc = mpi.sync( );
        if ( rc != MPI::SUCCESS ) return;

        Thread::pid_t * threads = NULL;
        if (pid == 0)
            threads = static_cast< Thread::pid_t *>( args.output );

        rc = gather( mpi, 0, probe.free_p, threads, args.output_size/sizeof(Thread::pid_t) );

        if ( rc != MPI::SUCCESS ) return;
    }

    _LPFLIB_LOCAL
    void initMaxThreadsPerNode(std::vector< Thread::pid_t > & threadsPerNode ) 
    {
        MPI mpi(MPI::ROOT, 0, 1);
        MPI::machine_t  mpiMachine;
        MPI::err_t rc = mpi.probe( & mpiMachine );
        if ( MPI::SUCCESS != rc )
        {
            LOG(1, "Could not explore MPI machine, mpi::probe returned " << rc );
            return;
        }

        const MPI::pid_t mpiNodes = mpiMachine.free_p; 
        try { 
            threadsPerNode.resize( mpiNodes );
        }
        catch( std::bad_alloc & )
        {
            LOG(1, "Insufficient memory while exploring machine"  );
            return;
        }

        MPI::args_t args;
        args.input = NULL;
        args.input_size = 0;
        args.output = threadsPerNode.data();
        args.output_size = threadsPerNode.size() * sizeof(lpf_pid_t);
        args.f_symbols = NULL;
        args.f_size = 0;

        rc = mpi.exec( mpiNodes, getMaxThreadsPerNode_spmd, args );
        if ( MPI::SUCCESS != rc )
        {
            LOG(1, "Could not launch MPI job to explore machine"  );
            threadsPerNode.clear();
            return;
        }
            

        if (*std::min_element( threadsPerNode.begin(), threadsPerNode.end()) <= 0)
        {
            LOG(1, "Some MPI node can't have any threads"  );
            threadsPerNode.clear();
            return;
        }
    }


    _LPFLIB_LOCAL
    lpf_pid_t getThreadsPerNode( lpf_pid_t nprocs, 
            std::vector< Thread::pid_t > & result )
    {
        std::vector< Thread::pid_t > maxThreadsPerNode;
        initMaxThreadsPerNode( maxThreadsPerNode );
        if ( maxThreadsPerNode.empty() )
            return 0;

        ASSERT( !maxThreadsPerNode.empty() );
            
        result.resize( maxThreadsPerNode.size(), 0u );
        Thread::pid_t total = 0;
        for ( size_t p = 0; p < result.size(); ++p)
        {
            ASSERT( nprocs > total );
            lpf_pid_t choice = 
                  std::min( maxThreadsPerNode[p], nprocs - total );
            result[p] = choice;
            total += choice;

            if (total >= nprocs) {
                result.resize( p+1 );
                break;
            }
        }
        return total;
    }

} } // lpf, hybrid

_LPFLIB_API lpf_err_t lpf_exec( lpf_t ctx, lpf_pid_t nprocs, lpf_spmd_t spmd, lpf_args_t args)
{
    using namespace lpf::hybrid;
    if (nprocs <= 0u) 
        return LPF_ERR_FATAL;

    if ( ! Thread::is_linked_correctly() || ! MPI::is_linked_correctly() ) {
        LOG(0, "Hybrid engine of LPF was not linked correctly. Error codes are not distinct" );
        return LPF_ERR_FATAL;
    }

    if (ctx != LPF_ROOT) {
        try {
            (*spmd)(LPF_SINGLE_PROCESS, 0, 1, args);
            return LPF_SUCCESS;
        }
        catch(...) {
            LOG(1, "SPMD function threw an unexpected exception"); 
            return LPF_ERR_FATAL;
        }
    }

    lpf_err_t rc = LPF_SUCCESS;
    BufBlob input, output; 
    std::vector< lpf_func_t > symbols;
    MPI::pid_t mpiProcs = MPI::MAX_P;
    try 
    {
        std::vector< Thread::pid_t > threads;
        getThreadsPerNode( nprocs, threads ) ;

        if (nprocs == 0)
            return LPF_ERR_FATAL;

        mpiProcs = threads.size();
        
        input.push( args.input, args.input_size);
        input.push( threads );
        input.push( NodeState::machineParams() );

        // allocate place for a pointer, see launchMpiStage
        void * ptr = NULL;
        input.push( ptr );
        input.pop( ptr );

        symbols.insert( symbols.end(),
               args.f_symbols, args.f_symbols + args.f_size
        );
        symbols.push_back( reinterpret_cast<lpf_func_t>(spmd) );

        output.push( args.output, args.output_size );
        output.push( rc );
    }
    catch( std::bad_alloc & e)
    {
        return LPF_ERR_OUT_OF_MEMORY; 
    }
    catch( std::length_error & e)
    {
        return LPF_ERR_OUT_OF_MEMORY; 
    }

    MPI::args_t mpiArgs;
    mpiArgs.input = input.data();
    mpiArgs.input_size = input.size();
    mpiArgs.output = output.data();
    mpiArgs.output_size = output.size();
    mpiArgs.f_symbols = symbols.data();
    mpiArgs.f_size = symbols.size();

    MPI mpi(MPI::ROOT, 0, 1);
    rc = mpi.exec( mpiProcs, launchMpiStage, mpiArgs );

    lpf_err_t nrc; 
    output.pop(nrc);
    output.pop( args.output, args.output_size);
    return nrc != LPF_SUCCESS ? nrc : rc;
}



