
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

#ifndef LPF_CORE_MPI_COMMUNICATION_HPP
#define LPF_CORE_MPI_COMMUNICATION_HPP

#include <cstddef>
#include <vector>
#include <string>

#include "log.hpp"
#include "linkage.hpp"

namespace lpf {

class _LPFLIB_LOCAL Communication
{
public:
    virtual ~Communication() {}

    /// The number of processes in this communicator
    virtual int nprocs() const = 0;

    /// The process ID of the calling process, which is in the range 
    /// of [ 0, nprocs ). 
    virtual int pid() const = 0;

    /// Broadcast a memory range from process \a root to all other processes. 
    virtual void broadcast( 
            void * element, 
            std::size_t elementSize, 
            int root 
        ) const = 0;

    /// Total exchange of a balanced communication
    virtual void allToAll(
            const void * sendArray, void * recvArray,
            int elementSize
            ) const = 0;

    /// Total exchange of potentially unbalanced communication
    virtual void allToAll( 
            const void * sendArray, int * sendOffsets, int * sendCounts,
            void * recvArray, int * recvOffsets, int * recvCounts 
            ) const = 0;

    /// Each process broadcasts an \a element of size \a elementSize. The result
    /// is put in \a array.
    virtual void allgather( 
            const void * element, 
            std::size_t elementSize, 
            void * array
        ) const = 0;

    /// In-place allgather
    virtual void allgather( void * array, std::size_t elementSize) const = 0;

    /// Returns false if and only if all processes call this function with
    /// \a myElement set to \c false.
    virtual bool allreduceOr( bool myElement ) const = 0;

    /// Returns true if and only if all processes call this function with
    /// \a myElement set to \c true.
    virtual bool allreduceAnd( bool myElement ) const = 0;

    /// Returns the sum of myElement on all processes.
    virtual int allreduceSum( int myElement ) const = 0;

    /// Returns the sum of myElement on all processes.
    virtual void allreduceSum( const int * xs, int *result, int n ) const = 0;

    /// Returns the max of myElement on all processes.
    virtual int allreduceMax( int myElement ) const = 0;

    /// Returns the max of myElement on all processes.
    virtual unsigned long allreduceMax( unsigned long myElement ) const = 0;

    /// Wait for all other processes to reach this function
    virtual void barrier() const = 0;

#ifdef LPF_CORE_MPI_USES_mpirma
    /// Reserve a number of memory registration slots
    virtual void reserveMemslots( int maxNumber ) = 0;

    class Memslot {
    public: 
        Memslot() : m_id( -1) {}
        explicit Memslot( int id ) : m_id(id) {}
        int getId() const { return m_id; }
    private:
        int m_id;
    };


    /// Register a memory area and return its ID. Collective call
    virtual Memslot createMemslot( void * base, std::size_t size) = 0;

    /// Deregister an earlier registered memory area. Collective call
    virtual void removeMemslot( Memslot memslot ) = 0;

    /// Put data into a remote memory area. This memory area must have been
    /// registered earlier with attach()
    virtual void put( 
            const void * srcBuf, 
            int dstPid, 
            Memslot dstSlot,
            std::size_t dstOffset,
            std::size_t size
            ) = 0 ; 

    /// Gut data from a remote memory area. This memory area must have been
    /// registered earlier with attach()
    virtual void get( 
            int srcPid, 
            Memslot srcSlot,
            std::size_t srcOffset,
            void * dstBuf, 
            std::size_t size
            ) = 0;

    /// Wait for all put() and get() operations to finish and signal that
    /// the system may expect more put() and get() calls to be issued.
    /// Collective call.
    virtual void fenceAll() = 0 ;
    virtual void fence(Memslot memslot) = 0 ;
#endif

    virtual void reserveMsgs( size_t n ) = 0;

    virtual void isend( const void * srcBuf, size_t bytes, int destPid, 
            int tag) = 0;

    virtual void irecv( void * dstBuf, size_t bytes, int srcPid, 
            int tag) = 0;

    virtual void iwaitall() = 0;

    //// TEMPLATES 
    /// Broadcast a value from process \a root to all other processes
    template <class T>
    void broadcast( T & mem, int root) const
    { broadcast( &mem, sizeof(T), root); }

    template <class T>
    void broadcast( std::vector<T> & array, int root) const
    {   
        size_t size = array.size();
        broadcast( &size, sizeof(size), root);
        // resize the receive array
        try {
            array.resize( size );
        }
        catch( std::bad_alloc & e)
        {
            // signal the other processes of the failure
            (void) allreduceOr( true );
            LOG(2, "Process '" << pid() << "' has insufficient memory to participate in broadcast" );
            throw;
        }
        if (allreduceOr(false))
        {
            // if one of the processes fail, abort with a bad_alloc
            LOG(2, "Some other process has insufficient memory to participate in broadcast" );
            throw std::bad_alloc();
        }
        broadcast( &array[0], sizeof(T) * size, root);
    }

    template <class T>
    void broadcast( std::basic_string<T> & str, int root) const
    {   
        size_t size = str.size();
        broadcast( size, root );

	if( size == 0 ) {
		str.clear();
		return;
	}

        std::vector< T > array;
        try {
            array.insert( array.end(), str.begin(), str.end() );
        }
        catch( std::bad_alloc & e)
        {
            (void) allreduceOr(true);
            LOG(2, "Process '" << pid() << "' has insufficient memory to participate in broadcast" );
            throw;
        }
        if (allreduceOr(false))
        {
            // if one of the processes fail, abort with a bad_alloc
            LOG(2, "Some other process has insufficient memory to participate in broadcast" );
            throw std::bad_alloc();
        }

        // broadcast the vector
        broadcast( array, root );

        try {
            (void) str.assign( array.begin(), array.end() );
        }
        catch( std::bad_alloc & e)
        {
            (void) allreduceOr( true );
            LOG(2, "Process '" << pid() << "' has insufficient memory to participate in broadcast" );
            throw;
        }
        if (allreduceOr(false))
        {
            // if one of the processes fail, abort with a bad_alloc
            LOG(2, "Some other process has insufficient memory to participate in broadcast" );
            throw std::bad_alloc();
        }
    }
 
    template <class T>
    void allToAll(
            const T * sendArray, T * recvArray
            ) const 
    {
        this->allToAll( sendArray, recvArray, sizeof(T) );
    }

    template <class T>
    void allToAll( 
            const T * sendArray, int * sendOffsets, int * sendCounts,
            T * recvArray, int * recvOffsets, int * recvCounts 
            ) const
    {
        const int nprocs = this->nprocs();
        const void * send = static_cast<const void *>(sendArray);
        void * recv= static_cast<void *>(recvArray);
        for (int p = 0; p < nprocs; ++p)
        {
            sendOffsets[p] *= sizeof(T);
            sendCounts[p] *= sizeof(T);
            recvOffsets[p] *= sizeof(T);
            recvCounts[p] *= sizeof(T);
        }
        this->allToAll( send, sendOffsets, sendCounts, 
                recv, recvOffsets, recvCounts );

        for (int p = 0; p < nprocs; ++p)
        {
            sendOffsets[p] /= sizeof(T);
            sendCounts[p] /= sizeof(T);
            recvOffsets[p] /= sizeof(T);
            recvCounts[p] /= sizeof(T);
        }
    }

    /// Gather a value from all processes
    template <class T>
    void allgather( T x, T * xs ) const
    { allgather( &x, sizeof(T), xs ); }

    /// Gather all values from all processes from an array in-place
    template <class T>
    void allgather( T * xs ) const
    { allgather( xs, sizeof(T) ); }

    /// Put a value into a remote memory area. This memory area must have
    /// been registered with attach()
    template <class T>
    void put( const T & value, int dstPid, T * dst )
    { put( &value, dstPid, dst, sizeof(T) ); }

    /// Get a value from a remote memory area. This memory area must have
    /// been registered with attach()
    template <class T>
    void get( int srcPid, const T * src, T & dst)
    { get( srcPid, src, &dst, sizeof(T) ); }


};


}

#endif
