
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

#include "machineparams.hpp"
#include "time.hpp"
#include "config.hpp"
#include "log.hpp"
#include "assert.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

#define STRINGIFY2( param ) #param
#define STRINGIFY( param ) STRINGIFY2( param)


namespace lpf {
   
    namespace LPF_CORE_IMPL_ID { namespace LPF_CORE_IMPL_CONFIG {

    const size_t MachineParams :: BLOCK_SIZES[ DATAPOINTS ]
        = { 1u, 8u, 128u, 1024u, (1u<<13), (1u<<17), (1u<<20)};


    MachineParams :: MachineParams()
        : m_set( false )
        , m_totalProcs(0)
    {   
        std::memcpy( &m_blockSize[0], BLOCK_SIZES, sizeof(BLOCK_SIZES));
        std::memset( &m_messageGap[0], 0, sizeof(m_messageGap) );
        std::memset( &m_latency[0], 0, sizeof(m_latency) );
    }

    MachineParams MachineParams :: execProbe( lpf_t lpf, lpf_pid_t nprocs)
    {
        MachineParams result;

        lpf_args_t args;
        args.input = NULL;
        args.input_size = 0;
        args.output = & result;
        args.output_size = sizeof(result);
        args.f_symbols = NULL;
        args.f_size = 0;

        lpf_err_t rc = lpf_exec( lpf, nprocs, probe, args );
        if (rc != LPF_SUCCESS ) {
            LOG( 1, "Could not probe machine"
                    << (rc == LPF_ERR_OUT_OF_MEMORY? 
                        " because of insufficient memory": "" ) );
        }

        return result;
    }

    MachineParams MachineParams :: hookProbe( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs)
    {
        MachineParams result;

        lpf_args_t args;
        args.input = NULL;
        args.input_size = 0;
        args.output = & result;
        args.output_size = sizeof(result);
        args.f_symbols = NULL;
        args.f_size = 0;

        probe( lpf, pid, nprocs, args );

        return result;
    }

    namespace { lpf_err_t LPF_PROBE_ERR_TIMEOUT = -1; }

    void MachineParams :: probe( lpf_t lpf, lpf_pid_t pid,
            lpf_pid_t nprocs, lpf_args_t args)
    {
        (void) args;

        MachineParams dummy, empty;
        MachineParams * machine = & dummy;

        if (args.output != NULL) {
            machine = static_cast<MachineParams *>(args.output);
            *machine = empty;
        }

        lpf_err_t rc = LPF_SUCCESS;


        // Only execute the probe for the engine that's currently running
        const double maxSeconds = 
            Config :: instance().getEngine() == STRINGIFY( LPF_CORE_IMPL_ID ) ?
            Config :: instance().getMaxSecondsForProbe() : 0.0;

        size_t totalWorkload=0;
        for (size_t i = 0; i < DATAPOINTS; ++i)
            totalWorkload+=BLOCK_SIZES[i];

        double maxSecondsPerProbe[DATAPOINTS];
        double hardDeadline[DATAPOINTS];
        hardDeadline[0] = maxSeconds;
        for (size_t i =0; i < DATAPOINTS; ++i) {
            maxSecondsPerProbe[i] = 0.01*maxSeconds + 0.99*maxSeconds / totalWorkload * BLOCK_SIZES[i];
            if (i > 0)
                hardDeadline[i] = hardDeadline[i-1] - maxSecondsPerProbe[i-1];
        }

        machine->m_set = true;
        machine->m_totalProcs = nprocs;
        std::memcpy( &machine->m_blockSize[0], BLOCK_SIZES, sizeof(BLOCK_SIZES) );
        std::memset( &machine->m_messageGap[0], 0, sizeof(machine->m_messageGap));
        std::memset( &machine->m_latency[0], 0, sizeof(machine->m_latency));


        if (maxSeconds > 0.0)
        {
            std::vector< char > readBuffer;
            std::vector< char > writeBuffer;
            double latency
                = std::numeric_limits<double>::infinity();
            double gap
                = std::numeric_limits<double>::infinity();

            for (size_t i = 0; i < DATAPOINTS; ++i)
            {
                rc = findMachineParams( lpf, pid, nprocs, 
                        machine->m_blockSize[i], maxSecondsPerProbe[i],
                        hardDeadline[i],
                        readBuffer, writeBuffer,
                        latency, gap );

                machine->m_messageGap[i]  = gap;
                machine->m_latency[i] = latency;
                if ( LPF_SUCCESS != rc )
                {
                    // when it runs out of memory, the estimate for small message
                    // block sizes is often a good upperbound estimate for
                    // large block sizes.
                    for (size_t j = i+1; j < DATAPOINTS; ++j)
                    {
                        machine->m_messageGap[j]  = gap;
                        machine->m_latency[j] = latency;
                    }
                    break;
                }
            }
        }
    }
 
    double MachineParams :: getLatency( size_t blockSize ) const
    {
        ASSERT( DATAPOINTS > 0 );
        // lookup index of blockSize in m_blockSize 
        // s.t. m_blockSize[i-1] <= blockSize < m_blockSize[i]
        size_t i = std::distance( &m_blockSize[0], 
                std::upper_bound( &m_blockSize[0], &m_blockSize[DATAPOINTS], 
                    blockSize )
                );

        double l1 = i > 0 ? m_latency[i-1] : m_latency[i];
        double l2 = i < DATAPOINTS ? m_latency[i] : m_latency[i-1];

        double b1 = m_blockSize[i-1];
        double b2 = m_blockSize[i];
        double b = blockSize;

        return l1 + (l2-l1) * (b-b1)/(b2-b1);
    }
   
    double MachineParams :: getMessageGap( size_t blockSize ) const
    {
        // lookup index of blockSize in m_blockSize 
        // s.t. m_blockSize[i-1] <= blockSize < m_blockSize[i]
        size_t i = std::distance( &m_blockSize[0], 
                std::upper_bound( &m_blockSize[0], &m_blockSize[DATAPOINTS], 
                    blockSize )
                );

        double g1 = i > 0 ? m_messageGap[i-1] : m_messageGap[i];
        double g2 = i < DATAPOINTS ? m_messageGap[i] : m_messageGap[i-1];

        double b1 = m_blockSize[i-1];
        double b2 = m_blockSize[i];
        double b = blockSize;

        return g1 + (g2-g1) * (b-b1)/(b2-b1);
    }


    namespace {
        enum ErrorCondition { Continue, Stop, Error };

        // A synchronous function that performs at most (nprocs-1) requests.
        ErrorCondition allgather( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, ErrorCondition * allC, lpf_memslot_t allCSlot )
        {
            ErrorCondition result = Continue;
            // synchronize to guard memory of 'allC' array.
            lpf_err_t rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS == rc )
            {
                // broadcast 'allC[pid]' value
                for (lpf_pid_t p = 0; p < nprocs; ++p)
                {
                    if (p != pid )
                    {
                        size_t offset = pid * sizeof(ErrorCondition);
                        rc = lpf_put( lpf, allCSlot, offset, 
                                p, allCSlot, offset, sizeof(ErrorCondition), 
                                LPF_MSG_DEFAULT );

                        ASSERT( LPF_SUCCESS == rc );
                    }
                }


                // synchronize
                rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
                if ( LPF_SUCCESS == rc )
                {
                    // and summarize success value
                    for (lpf_pid_t p = 0; p < nprocs; ++p)
                    {
                        if ( result != Error && allC[p] == Stop )
                        {
                            result = Stop;
                        }
                        else if (allC[p] == Error )
                        {
                            result = Error;
                            break;
                        }
                    }

                    // synchronize to protect 'allC' array from subsequent
                    // supersteps
                    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
                    if ( LPF_SUCCESS != rc )
                    {
                        result = Error;
                    }
                }
                else
                {
                    result = Error; 
                }
            }
            else
            {
                result = Error;
            }

            return result;
        }
    }

    namespace {
    void sendMsgs( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, size_t nMsgs, int mode,
            size_t blockSize, lpf_memslot_t readSlot, lpf_memslot_t writeSlot )
    {
        for (size_t m = 0; m < nMsgs; ++m)
        {
            for (lpf_pid_t p = 0; p < nprocs; ++p)
            {
                lpf_err_t rc;
                size_t i = m * nprocs + p;
                size_t j = m * nprocs + pid;
                if ( mode == 0 || ( mode == 1 && m % 2 == 0 ) )
                {
                    rc = lpf_put( lpf, readSlot, i * blockSize,  
                            p, writeSlot, j * blockSize, blockSize, 
                            LPF_MSG_DEFAULT );
                    ASSERT( LPF_SUCCESS == rc );
                }
                else
                {
                    rc = lpf_get( lpf, p, readSlot, i * blockSize,  
                            writeSlot, j * blockSize, blockSize, 
                            LPF_MSG_DEFAULT );
                    ASSERT( LPF_SUCCESS == rc );
                }
            }
            
        }
    } } // anonymous namespace


    lpf_err_t findMachineParams( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs,  
            size_t blockSize, double softMaxSeconds, double hardMaxSeconds,
            std::vector<char> & readBuffer, std::vector<char> & writeBuffer,
            double & latency, double & messageGap )
    {
        const size_t AllToAllSize = blockSize * nprocs ;

        const size_t nRegs = 4;

        lpf_err_t rc = LPF_SUCCESS;
        std::vector< ErrorCondition > allStatus;

        const size_t preferredSamples = 100u;
        const size_t minSamples = 10;
        const size_t maxRange=60;
        const size_t minRange=3;

        const size_t N = 4;
        double timeHRels[N];
        size_t sizeHRels[N];
        std::vector< double > allTimings;

        std::memset( timeHRels, 0, sizeof(timeHRels));
        std::memset( sizeHRels, 0, sizeof(sizeHRels));

        // allocate memory for status codes in other processes
        try
        {
            allStatus.resize( nprocs );
            allTimings.resize( N*nprocs, std::numeric_limits<double>::max() );
        }
        catch( std::bad_alloc & )
        {
            LOG( 2, "Unable to allocate a sample table to store the results of the benchmark" );
            return LPF_ERR_FATAL;
        }
        catch( std::length_error & )
        {
            LOG( 2, "Unable to allocate a sample table to store the results of the benchmark" );
            return LPF_ERR_FATAL;
        }

        // allocate minimum size of message queues
        rc = lpf_resize_memory_register( lpf, nRegs );
        if ( LPF_SUCCESS != rc ) {
            LOG(2, "Machine probe could not allocate message buffers with lpf_resize_memory_register");
            return LPF_ERR_FATAL; 
        }

        rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
        if ( rc != LPF_SUCCESS ) 
        {
            LOG(2, "Machine probe could not allocate communication buffer");
            // remove data point
            return LPF_ERR_FATAL;
        }

        // register error status variables
        lpf_memslot_t allStatusSlot = LPF_INVALID_MEMSLOT;
        rc = lpf_register_global( lpf, &allStatus[0], 
                allStatus.size() * sizeof(ErrorCondition) ,
                & allStatusSlot                    
             );
        ASSERT( LPF_SUCCESS == rc );
 
        lpf_memslot_t allTimingsSlot = LPF_INVALID_MEMSLOT;
        rc = lpf_register_global( lpf, 
              allTimings.data(), allTimings.size() * sizeof(double),
              & allTimingsSlot );
        ASSERT( LPF_SUCCESS == rc );

        // Explore what range of h-relations we can measure
        Time startTime = Time::now();
        ErrorCondition loopExit = Continue;
        size_t range;
        Time commTime = Time::fromSeconds(0.0);
        for ( range = minRange; range < maxRange; ++range)
        {
            // init status variables
            std::fill_n( allStatus.begin(), nprocs, Continue );

            // try to increase message buffer size
            size_t nMsgs = 1ull << range;
            rc = lpf_resize_message_queue( lpf, nprocs * ( 1 + nMsgs ) );

            // allocate large buffers for benchmarking
            const size_t TotalSize = AllToAllSize * nMsgs ;

            try
            {
                readBuffer.resize( TotalSize + sizeof(double) );
                writeBuffer.resize( TotalSize + sizeof(double) );
            }
            catch (std::bad_alloc & )
            {
                LOG(2, "Unable to allocate communication buffer for the benchmark");
                rc = LPF_ERR_OUT_OF_MEMORY;
            }
            catch (std::length_error & )
            {
                LOG(2, "Unable to allocate communication buffer for the benchmark");
                rc = LPF_ERR_OUT_OF_MEMORY;
            }

            // determine loop condition locally
            const Time softDeadline = 
                startTime + Time::fromSeconds( softMaxSeconds / preferredSamples );
            const Time hardDeadline = 
                startTime + Time::fromSeconds( hardMaxSeconds - 
                        commTime.toSeconds() * minSamples);

            allStatus[pid] 
                = hardDeadline < Time::now()    ? Error :                                                
                  ( range > minRange && softDeadline < Time::now()) 
                                                ? Stop : 
                  LPF_ERR_OUT_OF_MEMORY == rc   ? Stop : 
                  LPF_SUCCESS == rc         ? Continue : 
                                                  Error ;

            loopExit = allgather( lpf, pid, nprocs, &allStatus[0], allStatusSlot);
            if ( Stop == loopExit )
            {
                LOG(3, "Machine probe was stopped, likely because time exhausted" );
                break;
            }
            else if (Error == loopExit ) 
            {
                rc = lpf_deregister( lpf, allTimingsSlot );
                ASSERT( LPF_SUCCESS == rc );
                rc = lpf_deregister( lpf, allStatusSlot );
                ASSERT( LPF_SUCCESS == rc );
                return LPF_PROBE_ERR_TIMEOUT;
            }

            // register the buffer, again
            lpf_memslot_t readSlot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_global( lpf, 
                    readBuffer.data(), readBuffer.size(),
                    &readSlot
                 );
            ASSERT( LPF_SUCCESS == rc );
            lpf_memslot_t writeSlot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_global( lpf, 
                    writeBuffer.data(), writeBuffer.size(),
                    &writeSlot
                 );
            ASSERT( LPF_SUCCESS == rc );


            rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
            if ( rc != LPF_SUCCESS ) 
            {
                LOG(2, "Machine probe was interrupted, before measuring data points" );
                return LPF_ERR_FATAL;
            }

            Time t0 = Time::now();

            sendMsgs( lpf, pid, nprocs, nMsgs, 0, 
                      blockSize, readSlot, writeSlot );
            rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );

            if ( rc != LPF_SUCCESS ) 
            {
                LOG(2, "Machine probe was interrupted" );
                return LPF_ERR_FATAL;
            }
            Time t1 = Time::now();

            commTime = t1-t0;

            // deregister the buffer
            rc = lpf_deregister( lpf, readSlot );
            ASSERT( LPF_SUCCESS == rc );
            rc = lpf_deregister( lpf, writeSlot );
            ASSERT( LPF_SUCCESS == rc );
        } // for different number of messages.

        // adjust range by 1 because that was the last 
        // accurately measured h-relation
        range -= 1;

        // now that we know the range, we can take two data points
        
        // register the buffer
            lpf_memslot_t readSlot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_global( lpf, 
                    readBuffer.data(), readBuffer.size(),
                    &readSlot
                 );
            ASSERT( LPF_SUCCESS == rc );
            lpf_memslot_t writeSlot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_global( lpf, 
                    writeBuffer.data(), writeBuffer.size(),
                    &writeSlot
                 );
            ASSERT( LPF_SUCCESS == rc );

        // initializing data, in blocks of double, because that's 
        // somewhat faster. We don't care how it is initialized,
        // as long as it is non-trivial.
        for ( size_t i = 0; i < readBuffer.size(); i += sizeof(double))
        {
            double x = double(pid) + i;
            * reinterpret_cast<double *>(&readBuffer[i]) = x;
        }

        // Now do the real measurements
        sizeHRels[0] = 0;
        sizeHRels[1] = 1;
        sizeHRels[2] = 2;
        sizeHRels[3] = std::max<size_t>( 4, 1ul << range );
        for ( size_t j = 0; j < N; ++j ) 
        {
            for ( size_t i = 0; i < preferredSamples; ++i) 
            {
                rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
                if ( rc != LPF_SUCCESS ) 
                {
                    LOG(2, "Machine probe was interrupted" );
                    return LPF_ERR_FATAL;
                }
                Time t0 = Time::now();

                sendMsgs( lpf, pid, nprocs, sizeHRels[j], 0, 
                          blockSize, readSlot, writeSlot );
                rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );

                if ( rc != LPF_SUCCESS ) 
                {
                    LOG(2, "Machine probe was interrupted" );
                    return LPF_ERR_FATAL;
                }
                Time t1 = Time::now();

                double seconds = (t1-t0).toSeconds();
                // Take the minimum, because it is the measurement with
                // the lowest variance.
                allTimings[j+N*pid] = std::min( allTimings[j+N*pid], seconds);

                // it is very import that for j=0, 1, and 2 the resulting minimum
                // will be very accurate, so we should try to abort during the last
                // measurement round.
                const Time hardDeadline 
                    = Time::fromSeconds( startTime.toSeconds() + hardMaxSeconds );
                const Time softDeadline 
                    = Time::fromSeconds( startTime.toSeconds() + softMaxSeconds );
                Time now = Time::now();
                allStatus[pid] 
                    = hardDeadline < now ? Error :
                      j >= 3 && i > minSamples && softDeadline < now ? Stop :
                      Continue;

                loopExit = allgather( lpf, pid, nprocs, &allStatus[0], allStatusSlot);
                if ( loopExit == Error )
                {
                    LOG(3, "Missed hard deadline during probe. Aborting...");
                    
                    rc = lpf_deregister( lpf, readSlot );
                    ASSERT( LPF_SUCCESS == rc );
                    rc = lpf_deregister( lpf, writeSlot );
                    ASSERT( LPF_SUCCESS == rc );
                    rc = lpf_deregister( lpf, allTimingsSlot );
                    ASSERT( LPF_SUCCESS == rc );
                    rc = lpf_deregister( lpf, allStatusSlot );
                    ASSERT( LPF_SUCCESS == rc );
                    return LPF_PROBE_ERR_TIMEOUT; 
                }
                else if (loopExit == Stop )
                {
                    LOG(3, "Time exhausted during probe. Only took " << i 
                            <<" samples during the round for nMsgs = " << sizeHRels[j] );
                    break;
                }
            }
       }

       // deregister the buffer
       rc = lpf_deregister( lpf, readSlot );
       ASSERT( LPF_SUCCESS == rc );
       rc = lpf_deregister( lpf, writeSlot );
       ASSERT( LPF_SUCCESS == rc );


       // Communicate all measurements over all processes
       for (lpf_pid_t p = 0; p < nprocs; ++p)
       {
           size_t szDbl = sizeof(double);
           size_t size = szDbl * N;
           size_t wOffset = pid * size;
           if (p != pid) {
              rc = lpf_put( lpf, allTimingsSlot, wOffset,
                   p, allTimingsSlot, wOffset, size, 
                   LPF_MSG_DEFAULT );
              ASSERT( LPF_SUCCESS == rc );
           }
       }

       rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );

       if ( LPF_SUCCESS != rc )
       {
           return LPF_ERR_FATAL;
       }

       // take the minimum over all procs for all data points
       // because we want to minimize variance of our measurements.
       for (size_t d = 0; d < N; ++d)
       {
           timeHRels[d] = allTimings[d];
           for (lpf_pid_t p = 1; p < nprocs; ++p )
           {
               double t = allTimings[d + p * N];
               if ( timeHRels[d] > t )
               {
                   timeHRels[d] = t;
               }
           }
       }

       double highHgap = (timeHRels[3]-timeHRels[2])/(sizeHRels[3]-sizeHRels[2])/AllToAllSize;
       double lowHgap = (timeHRels[2]-timeHRels[1])/(sizeHRels[2]-sizeHRels[1])/AllToAllSize;
       double latencyUncapped = timeHRels[1] - lowHgap*sizeHRels[1]*AllToAllSize ;

       double epsilon = std::numeric_limits<double>::min();
       messageGap = std::max( epsilon, highHgap );
       latency = std::max( timeHRels[0], std::min( timeHRels[1], latencyUncapped ) );

       /*if (pid == 0) {
            printf("\n\tbytes\tseconds\n");
            for (size_t d = 0; d < N; ++d)
                printf( STRINGIFY( LPF_CORE_IMPL_ID ) "\t%g\t%g\n", 1.0*sizeHRels[d]*AllToAllSize, timeHRels[d]);
            printf(STRINGIFY( LPF_CORE_IMPL_ID) "\tblocksize: %zd:  latency= %g (%g) \tgap= %g\n", blockSize, latency, latencyUncapped, messageGap );
       }*/

       rc = lpf_deregister( lpf, allTimingsSlot );
       ASSERT( LPF_SUCCESS == rc);
       rc = lpf_deregister( lpf, allStatusSlot );
       ASSERT( LPF_SUCCESS == rc);
        
       return  LPF_SUCCESS;
    }

} } 

}

