
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

#ifndef LPF_CORE_MPI_MPILIB_HPP
#define LPF_CORE_MPI_MPILIB_HPP

#include <stdexcept>
#include <cstddef>
#include <string>
#include <mpi.h>

#if __cplusplus >= 201103L    
  #include <memory>
#else
  #include <tr1/memory>
#endif

#include "communication.hpp"
#include "linkage.hpp"

namespace lpf { namespace mpi {

    class _LPFLIB_LOCAL Exception : public std::runtime_error
    {
    public:
        Exception(const std::string & operation); 
    };

    /** An MPI Communicator exposing a small selection of MPI communication
     * routines
     */
    class _LPFLIB_LOCAL Comm : public Communication
    {   friend class Lib;
    public:
        Comm();
        ~Comm();

        explicit Comm(MPI_Comm * mpiCommunicator);

        /// The number of processes in this communicator
        int nprocs() const;

        /// The process ID of the calling process, which is in the range 
        /// of [ 0, nprocs ). 
        int pid() const;

        /// Split a communicator into smaller groups. Only the processes that 
        /// called this function with the same color, will end up in the same
        /// communicator
        Comm split( int color, int rank ) const;

        /// Duplicate a communicator to make sure it has no shared state with
        /// the original.
        Comm clone() const;

        /// Get the MPI communicator
        MPI_Comm comm() const;

        /// Broadcast a memory range from process \a root to all other processes. 
        void broadcast( void * mem, std::size_t size, int root) const;
        using Communication::broadcast;

        /// Total exchange of a balanced communication
        virtual void allToAll(
                const void * sendArray, void * recvArray,
                int elementSize
                ) const ;

        /// Total exchange of potentially unbalanced communication
        virtual void allToAll( 
                const void * sendArray, int * sendOffsets, int * sendCounts,
                void * recvArray, int * recvOffsets, int * recvCounts 
                ) const ;

#ifdef MPI_HAS_IBARRIER        
        static void allToAllAllocTempspace( int nSends, std::vector<char> & tempSpace );
        virtual void allToAll( int nSends, 
            const void * sendData, size_t * sendOffsets, size_t * sendCounts, int * sendPids,
            void * recvData, size_t recvSize, size_t * received,
            std::vector<char> & tempSpace ) const ;
#endif

        using Communication::allToAll;


        /// Gather data from a memory range from all processes
        void allgather( const void * x, std::size_t xSize, void * xs) const;
        using Communication::allgather;

        /// In-place gather data from a memory range from all processes
        void allgather( void * xs, std::size_t xSize) const;

        /// Returns false if and only if all processes call this function with
        /// \a myElement set to \c false.
        bool allreduceOr( bool myElement ) const;

        /// Returns true if and only if all processes call this function with
        /// \a myElement set to \c true.
        bool allreduceAnd( bool myElement ) const;

        /// Returns the sum of myElement on all processes.
        int allreduceSum( int myElement ) const ;

        /// Returns the sum of myElement on all processes.
        void allreduceSum( const int * xs, int *result, int n ) const;

        /// Returns the max of myElement on all processes.
        int allreduceMax( int myElement ) const ;

        /// Returns the max of myElement on all processes.
        unsigned long allreduceMax( unsigned long myElement ) const ;

        /// Wait for all other processes to reach this function
        void barrier() const;

#ifdef LPF_CORE_MPI_USES_mpirma
        /// Reserve a number of memory registration slots
        virtual void reserveMemslots( int maxNumber ) ;

        /// Register a memory area and return its ID.
        virtual Memslot createMemslot( void * base, std::size_t size);

        /// Deregister an earlier registered memory area
        virtual void removeMemslot( Memslot memslot ) ;

        /// Put data into a remote memory area. This memory area must have been
        /// registered earlier with attach()
        virtual void put( 
                const void * srcBuf, 
                int dstPid, 
                Memslot dstSlot,
                std::size_t dstOffset,
                std::size_t size
                )  ; 

        /// Gut data from a remote memory area. This memory area must have been
        /// registered earlier with attach()
        virtual void get( 
                int srcPid, 
                Memslot srcSlot,
                std::size_t srcOffset,
                void * dstBuf, 
                std::size_t size
                ) ;

        /// Wait for all put() and get() operations to finish and signal that
        /// the system may expect more put() and get() calls to be issued.
        virtual void fenceAll() ;
        virtual void fence(Memslot slot);

#endif

        virtual void reserveMsgs( size_t n );

        virtual void isend( const void * srcBuf, size_t bytes, int destPid, 
            int tag );

        virtual void irecv( void * dstBuf, size_t bytes, int srcPid, 
                int tag );

        virtual void iwaitall();

    private:
        struct Impl;

#if __cplusplus >= 201103L    
        std::shared_ptr<Impl> m_impl;
#else
        std::tr1::shared_ptr<Impl> m_impl;
#endif
    };

    class _LPFLIB_LOCAL Lib
    {
    public:
        static Lib & instance(int * argc = NULL, char *** argv = NULL );

        void abort() const;

        int total_nprocs() const;

        const Comm & world() const;

        const Comm & self() const
        { return m_self; }

        void setFatalMode()
        {   m_fatalFailure = true; }

        bool supportsThreads() const
        { return m_supportsThreads; }


    private:
        Lib(int * argc, char *** argv);
        ~Lib();

        mutable Comm m_world;
        Comm m_self;
        int m_mpiInitialized; 
        bool m_selfInitialized;
        bool m_fatalFailure;
        bool m_supportsThreads;
    };

} // namespace mpi


} // namespace lpf

#endif
