
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

#include <climits>
#include <vector>

#include "mpilib.hpp"
#include "interface.hpp"
#include "process.hpp"
#include "trysync.hpp"
#include "log.hpp"
#include "assert.hpp"

namespace lpf {

Process :: Process( const mpi::Comm & comm )
    : m_world( comm )
    , m_aborted(false)
{
    if ( ROOT != m_world.pid() )
    {
        slave();
    }
}

Process :: ~Process()
{
    if ( !m_aborted )
    {   // signal all slaves that process will end
        pid_t requestedProcs = 0;
        try {
            m_world.broadcast( requestedProcs, ROOT);
        }
        catch(...)
        {
           LOG(1, "Root process has difficulty terminate gracefully due to communication problem"); 
        }
    }
}

void Process :: slave()
{
    pid_t requestedProcs = 0;
    // wait while the master calls lpf_exec
    while ( m_world.broadcast( requestedProcs, ROOT ), requestedProcs > 0)
    {
        // and call lpf_exec too
        (void) exec( requestedProcs, NULL, LPF_NO_ARGS );
    }
    m_aborted = true;
}

void Process :: broadcastSymbol( 
        Communication & comm, 
        Symbol & symbol 
        )
{
    std::string symbolName;
    std::string fileName;

    // Step 1: The root broadcasts the name of the symbol
    TRYSYNC_BEGIN( comm ) {
        symbolName = symbol.getSymbolName();
        fileName = symbol.getFileName();
    } TRYSYNC_RETHROW_END() 
    
    comm.broadcast( fileName, ROOT ); 
    comm.broadcast( symbolName, ROOT );
    
    // Step 2: The slaves look-up the symbol from the name
    TRYSYNC_BEGIN( comm ) {
        if ( ROOT != comm.pid() ) 
        {
            symbol = Symbol( fileName, symbolName );
        }
    } TRYSYNC_RETHROW_END()
     
    // if successful, the symbol won't be NULL
    ASSERT( 0 != symbol.getSymbol() );
}

err_t Process :: exec( pid_t P, spmd_t spmd, args_t args ) 
{
    if ( m_aborted )
    {
        LOG(3, "lpf_exec fails because collection of processes is in "
               "process of terminating" );
        return LPF_ERR_FATAL;
    }

    // Since looking up symbols can fail in many ways, do it immediately
    // on the master thread. On failure, there is no need to synchronize
    // to communicate the error.
    Symbol spmdFunction;
    // NOTE: The use of reinterpret_cast is compulsory in C++98/C++03, but
    // can be changed to static_cast in C++11 and beyond. The reason is that
    // C++ allows code and data to reside in a different memories
    try { 
        if ( NULL != spmd )
        {
            spmdFunction = Symbol( * reinterpret_cast<void**>(&spmd)  );
        }
    }
    catch( Symbol::LookupException & e ) {
        void * ptr = * reinterpret_cast<void**>(&spmd);
        LOG(1, "lpf_exec failed because it could not find the name "
                " of the symbol at address " << ptr <<
               " which is the user spmd function. ");
        return LPF_ERR_FATAL;
    }
    catch( std::bad_alloc & e ) {
        LOG(1, "lpf_exec failed because we ran out of memory while "
               "looking up symbol name of user spmd function.");
        return LPF_ERR_OUT_OF_MEMORY;
    }
    std::vector< Symbol > auxSymbols;
    for (size_t i = 0 ; i < args.f_size; ++i)
    {
        const void * aux = 
            * reinterpret_cast<void * const * >(&args.f_symbols[i]);
        try {
                auxSymbols.push_back( Symbol(aux) );
        }
        catch( Symbol::LookupException & e ) {
            LOG(1, "lpf_exec failed because it could not find the name "
                    " of the symbol at address " << aux <<
                   " which is the " << i << "th symbol to be forwarded to"
                   " the user spmd function. ");
            return LPF_ERR_FATAL;
        }
        catch( std::bad_alloc & e ) {
            LOG(1, "lpf_exec failed because we ran out of memory while "
                   "looking up a symbol name to be forwarded with the user "
                   "spmd function.");
            return LPF_ERR_OUT_OF_MEMORY;
        }
    }

    // Now we're ready to let the slaves join
    pid_t requestedProcs = P;

    //lint -save -e525 Do not complain about negative indentation of
    //                 synchronisation annotations in the comments
    if ( ROOT == m_world.pid() )
    {  
         ASSERT( spmd != NULL );
         ASSERT( P  > 0);
         // wake-up all the slave processes from their loop in Process::slave()
         requestedProcs = std::min<pid_t>( m_world.nprocs(), P );
/*S=0*/  m_world.broadcast( requestedProcs, ROOT );
    }

    ///////////// CODE BELOW IS TRUE SPMD ///////////////////////
    // Comments show global barrier synchronizations
    //  S=0..2 => a global barrier on MPI_COMM_WORLD
    //  T=0..X => a barrier on requested subset of processes
    

    // Select the requested processes
    const bool inRequestedGroup = pid_t( m_world.pid() ) < requestedProcs;
    // all non-allocated processes and their masters are in subgroups.
    const int subgroupNr = m_world.pid() % int(requestedProcs);
    
    // do the MPI commands to split the groups.
/*S=1*/ mpi::Comm machine = m_world.split( inRequestedGroup, m_world.pid() );
    ASSERT( ROOT != m_world.pid() || ROOT == machine.pid() );

    // Group the processes that are not required at the moment together
/*S=2*/ Process subprocess( m_world.split( subgroupNr, m_world.pid() ) );
    err_t status = LPF_SUCCESS;

    if ( pid_t( m_world.pid() ) < requestedProcs ) 
    {
        // now allocate some memory to look-up forwarded symbols through args
/*T=1*/ machine.broadcast( args.f_size, ROOT );
        std::vector< lpf_func_t > fSymbols;

        TRYSYNC_BEGIN( machine ) {
            auxSymbols.resize( args.f_size );
            fSymbols.resize( args.f_size );
/*T=2*/ } TRYSYNC_END( status )

        ASSERT( fSymbols.size() <= args.f_size );
        ASSERT( fSymbols.size() <= auxSymbols.size() );

        try {
         // Look-up the various symbols: starting with the spmd function
/*T=3*/     broadcastSymbol( machine, spmdFunction );
            spmdFunction.getSymbol(spmd);

            // subsequently, lookup the symbols in args
            for ( size_t i = 0; i < fSymbols.size() ; ++i )
            {
/*T=4*/        broadcastSymbol( machine, auxSymbols[i]);
               auxSymbols[i].getSymbol(fSymbols[i]);
            }

            ASSERT( args.f_size == fSymbols.size() );
            args.f_symbols = fSymbols.empty()?0:&fSymbols[0];

/*T=5*/     status = hook( machine, subprocess, spmd, args);
        }
        catch( std::bad_alloc & e )
        {
            status = LPF_ERR_OUT_OF_MEMORY;
            LOG(1, "lpf_exec ran out of memory while communicating symbols");
        }
        catch( std::exception & e )
        {
            status = LPF_ERR_FATAL;
            LOG(1, "lpf_exec failed because " << e.what());
        }
    }
    ///////////// END OF SPMD ///////////////////////
    //lint -restore

    return status;
} // exec


err_t Process :: hook( const mpi::Comm & machine, Process & subprocess,
        spmd_t spmd, args_t args ) 
{
    lpf_err_t status = LPF_SUCCESS;

    // synchronous statements are marked with an /*S=ID*/
    //lint -save -e525 Do not complain about negative indentation of
    //                 synchronisation annotations in the comments
    
    try {
/*S=1*/ Interface runtime( machine, subprocess );

/*S=2*/ machine.barrier();
        try
        {
            (*spmd)( &runtime, machine.pid(), machine.nprocs(), args);

            if ( runtime.isAborted() == 0 )
            {   // perhaps I am stopping early... let's do a count
/*S=3*/         runtime.abort();
                
                if ( runtime.isAborted() != pid_t(machine.nprocs()) )
                {
                    // in which case  I stopped early
                    LOG(2, "This process called lpf_sync fewer times than in"
                            " the other processes" );
                    status = LPF_ERR_FATAL;
                }
            }
            else
            {
                // in case somebody stopped early, set our status to FATAL as
                // well
                LOG(2, "Several ( " << runtime.isAborted() << " ) processes"
                        " but not this process aborted the user SPMD "
                        "function." );
                status = LPF_ERR_FATAL;
            }
        }
        catch(std::exception &e )
        {
            LOG(1, "Caught exception '" << e.what() << "' while executing "
                    "user SPMD function. Aborting..." );
/*S=3*/     runtime.abort();
            status = LPF_ERR_FATAL;
        }
        catch(...)
        {
            LOG(1, "Caught exception of unknown type while executing "
                    "user SPMD function. Aborting..." );
/*S=3*/     runtime.abort();
            status = LPF_ERR_FATAL;
        }
    }
    catch( std::bad_alloc & e )
    {
        LOG(1, "Not enough memory to create parallel context to run "
                "user SPMD function. Aborting..." );
        status = LPF_ERR_OUT_OF_MEMORY;
    }
    //lint -restore

    return status;
}

} // namespace lpf
