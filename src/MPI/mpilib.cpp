
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

#include "mpilib.hpp"
#include "config.hpp"
#include "log.hpp"
#include "lpf/mpi.h"
#include "sparseset.hpp"
#include "assert.hpp"
#include "linkage.hpp"
#include "memreg.hpp"

#include <climits>
#include <iostream>

#include <mpi.h>
#include <unistd.h>
#include <stdlib.h>

namespace lpf { namespace mpi {
    
    Exception :: Exception( const std::string & what)
        : std::runtime_error( what )
    { }


    class _LPFLIB_LOCAL Comm :: Impl
    {
    public:
        Impl()
            : m_comm( MPI_COMM_SELF )
            , m_maxMpiMsgSize( Config::instance().getMaxMPIMsgSize() )
            , m_windows()
            , m_usedWindows(0,0)
            , m_emptyRecvs(0)
        {
            int rc = MPI_Comm_dup( MPI_COMM_SELF, &m_comm);

            if ( MPI_SUCCESS != rc)
                throw Exception("Copying MPI communicator failed");
            
            rc = MPI_Comm_rank( m_comm, &m_pid );
            if ( MPI_SUCCESS != rc)
                throw Exception("Unable to get PID from MPI");

            rc = MPI_Comm_size( m_comm, &m_nprocs );
            if ( MPI_SUCCESS != rc)
                throw Exception("Unable to get number of processes from MPI");
        }

        Impl( MPI_Comm comm, int color, int rank)
            : m_comm ( )
            , m_maxMpiMsgSize( Config::instance().getMaxMPIMsgSize() )
            , m_windows()
            , m_usedWindows(0,0)
            , m_emptyRecvs(0)
        {
            int rc = MPI_Comm_split(comm, color, rank, &m_comm);

            if ( MPI_SUCCESS != rc)
            {
                throw Exception("Splitting MPI communicator failed");
            }
            rc = MPI_Comm_rank( m_comm, &m_pid );
            if ( MPI_SUCCESS != rc)
                throw Exception("Unable to get PID from MPI");

            rc = MPI_Comm_size( m_comm, &m_nprocs );
            if ( MPI_SUCCESS != rc)
                throw Exception("Unable to get number of processes from MPI");
        }

        Impl( MPI_Comm comm )
            : m_comm( )
            , m_maxMpiMsgSize( Config::instance().getMaxMPIMsgSize() )
            , m_windows()
            , m_usedWindows(0,0)
            , m_emptyRecvs(0)
        {
            int rc = MPI_Comm_dup(comm, &m_comm);

            if ( MPI_SUCCESS != rc)
            {
                throw Exception("Copying MPI communicator failed");
            }
            rc = MPI_Comm_rank( m_comm, &m_pid );
            if ( MPI_SUCCESS != rc)
                throw Exception("Unable to get PID from MPI");
            rc = MPI_Comm_size( m_comm, &m_nprocs );
            if ( MPI_SUCCESS != rc)
                throw Exception("Unable to get number of processes from MPI");
        }

        ~Impl()
        {
            int finalized = 0;
            int rc = MPI_Finalized(&finalized);
            if ( MPI_SUCCESS != rc )
                LOG( 1, "Unable to determine whether MPI has been finalized");

            if (! finalized )
            {
                typedef SparseSet<WinID>::const_iterator It;
                for (It i = m_usedWindows.begin() //lint !e1551 SparseSet::begin never throws
                    ; i != m_usedWindows.end(); ++i)//lint !e1551 SparseSet::end and ++ never throw
                {
                    ASSERT( MPI_WIN_NULL != m_windows.lookup(*i) ); //lint !e1551 never throws
                    MPI_Win win = m_windows.lookup(*i); //lint !e1551 never throws
                    rc = MPI_Win_free( &win  );
                    // it is illegal to throw an exception here
                    if ( MPI_SUCCESS != rc )
                        LOG( 1, "Unable to free MPI window");
                }
                
                rc = MPI_Comm_free( &m_comm );
                if ( MPI_SUCCESS != rc )
                    LOG( 1, "Unable to free MPI communicator");
            }
        }

        MPI_Comm comm()
        { return m_comm; }

        size_t getMaxMsgSize() const { return m_maxMpiMsgSize; }

#ifdef LPF_CORE_MPI_USES_mpirma

        MPI_Win window(int id)
        { return m_windows.lookup(id); }

        void reserveWindows( int maxNumber )
        {
#ifdef MPICH
#ifdef I_MPI_VERSION
            // Intel MPI 5 can't handle 16384 windows or more
            if (maxNumber >= 16384)
                throw std::bad_alloc();
#else
            // MPICH 3.1 can't handle 2048 windows or more
            if (maxNumber >= 2048)
                throw std::bad_alloc();
#endif
#endif

            m_windows.reserve( maxNumber );
            m_usedWindows.resize( m_windows.capacity() );
        }

        int createWindow( void * ptr, std::size_t size )
        {
            MPI_Win newWindow = MPI_WIN_NULL;

            // create a window
            int rc = MPI_Win_create( ptr, size, 1, 
                    MPI_INFO_NULL, m_comm, &newWindow );
            if (MPI_SUCCESS != rc )
                 throw Exception("Cannot create MPI Window");

             // check that MPI allows overlapping windows
            int * memory_model = 0;
            int flag = 0;
            rc = MPI_Win_get_attr( newWindow,
                   static_cast<int>( MPI_WIN_MODEL ),
                    & memory_model, &flag);
            if (MPI_SUCCESS != rc )
                 throw Exception("Cannot query MPI Window attributes");
            if ( !flag || static_cast<int>(MPI_WIN_UNIFIED) != *memory_model )
                throw Exception("MPI implementation does not support "
                                "overlapping windows");

            WinID id = m_windows.add( newWindow );
            m_usedWindows.insert( id );
            return id;
        }

        void freeWindow( int id )
        {
            MPI_Win win = m_windows.lookup( id );
            int rc = MPI_Win_free( &win );
            if (MPI_SUCCESS != rc )
                throw Exception("Cannot free MPI Window");
            m_windows.remove( id );
            m_usedWindows.erase( id );
        }

        void fenceAll()
        {
            for (SparseSet<WinID>::const_iterator i = m_usedWindows.begin()
                ; i != m_usedWindows.end(); ++i)
            {
                fence(*i); 
            }
        }

        void fence(int memslot)
        {
            ASSERT( MPI_WIN_NULL != m_windows.lookup(memslot) );
            int memslotOnRoot=memslot;
            ASSERT( ( MPI_Bcast( &memslotOnRoot, 1, MPI_INT, 0, m_comm ),
                      memslotOnRoot == memslot 
                  ) );
#if defined OMPI_MAJOR_VERSION
#if OMPI_MAJOR_VERSION == 2 && OMPI_MINOR_VERSION <=1 
            // OpenMPI 2.1.1 seems to have a buggy MPI_Win_fence
#error OpenMPI 2.1.1 has a buggy MPI_Win_fence
#endif
#endif

            int rc = MPI_Win_fence(0, m_windows.lookup(memslot) );

            if (MPI_SUCCESS != rc )
                throw Exception("Cannot fence MPI Window");
        }
#endif

        void reserveMsgs( unsigned maxNumber )
        {
            const unsigned maxInt
               = static_cast<unsigned>( std::numeric_limits<int>::max() );
            if ( maxNumber > maxInt ) // prevent overflows of messages tags
                throw std::bad_alloc();

            m_imsgs.reserve( maxNumber );
            m_irecvs.reserve( maxNumber );
            m_largeSends.reserve( maxNumber );
            m_largeRequests.reserve( maxNumber );
            if (m_sendIndices.size() < size_t(maxNumber) ) {
                m_sendIndices.resize( maxNumber );
            }
        }

        void isend( const void *buf, size_t bytes, int destPid, int tag)
        {
            MPI_Request req = MPI_REQUEST_NULL;
            size_t size = std::min<size_t>( m_maxMpiMsgSize, bytes);

            if (size > 0)
            {
                ASSERT( tag >= 0 );
                int rc = MPI_Isend( 
#ifdef PLATFORM_MPI
                        const_cast<void *>(buf),
#else
                        buf,
#endif
                        size, MPI_BYTE, destPid,
                        tag, m_comm, &req );
                LOG( 4, "Sending first " << size << " of "
                        << bytes << " bytes of message from address " 
                        << buf << " with tag " << tag << " from process "
                        << m_pid << " to " << destPid );

                if (MPI_SUCCESS != rc )
                    throw Exception("Cannot Isend msg");
            }

            if ( size < bytes )
            {
                ASSERT( bytes > m_maxMpiMsgSize );
                Send send;
                send.addr = buf;
                send.size = bytes;
                send.tag = tag;
                send.dstPid = destPid;
                m_largeSends.push_back( send );
                m_largeRequests.push_back( req );
            }
            else
            {
                m_imsgs.push_back( req );
            }
        }

        void irecv( void *buf, size_t bytes, int srcPid, int tag)
        {
            (void) srcPid;
            ASSERT( tag >= 0 );
            ASSERT( unsigned(tag) == m_irecvs.size() );
            m_irecvs.push_back( std::make_pair( buf, bytes) );

            if ( bytes == 0 )
                m_emptyRecvs++;
        }

        void iwaitall()
        {
            // Note. Some MPI's, as for example Intel MPI 5.0.1, MPICH3, OpenMPI.
            // MVAPICH, IBM Platform MPI, have an MPI_Waitall that require
            // O(n^2) time, where n is the number of messages. It seems that,
            // internally, Intel MPI tries to  match the tag of a newly
            // received messages with a linear search through the list of
            // active messages. Because in this case the tags are indices into
            // the communication buffer, no such sorting is required. Hence,
            // the following workaround:
            int rc = MPI_SUCCESS;
            size_t nMsgs = m_irecvs.size() - m_emptyRecvs;
            while ( nMsgs > 0 || !m_largeSends.empty() ) {

                /* handle continuations of large messages */
                int sendReady = 0;
                ASSERT( m_largeRequests.size() <= m_sendIndices.size() );
                rc = MPI_Testsome( m_largeRequests.size(), m_largeRequests.data(),
                        & sendReady, m_sendIndices.data() , MPI_STATUSES_IGNORE );
                if (MPI_SUCCESS != rc)
                    throw Exception("Cannot test for completion of MPI messages");
                for (int i = 0; i < sendReady; ++i)
                {
                    int sendIndex = m_sendIndices[i];
                    ASSERT( unsigned(sendIndex) < m_largeSends.size() );
                    Send & send = m_largeSends[sendIndex];
                    const char * addr =
                        static_cast<const char *>(send.addr) + m_maxMpiMsgSize;
                    ASSERT( send.size > m_maxMpiMsgSize );
                    size_t size = send.size - m_maxMpiMsgSize;
                    int dstPid = send.dstPid;
                    int tag = send.tag;
                    send.addr = addr;
                    send.size = size;

                    MPI_Request * req = &m_largeRequests[sendIndex];
                    if (send.size <= m_maxMpiMsgSize ) { // or enqueue as normal send
                        m_imsgs.push_back( MPI_Request() );
                        req = & m_imsgs.back();
                    }

                    ASSERT( tag >= 0 );
                    ASSERT( size > 0 );
                    rc = MPI_Isend(
#ifdef PLATFORM_MPI
                            const_cast<char *>(addr),
#else
                            addr,
#endif
                            std::min(m_maxMpiMsgSize, size),
                            MPI_BYTE, dstPid, tag, m_comm, req );
                    LOG( 4, "Sending next " << std::min(m_maxMpiMsgSize, size) << " of "
                        << size << " bytes of message from address " 
                        << addr << " with tag " << tag << " from process " << m_pid << " to "
                        << dstPid );

                    if (MPI_SUCCESS != rc )
                        throw Exception("Cannot Isend msg");
                }

                // dequeue large messages 
                for (size_t i = m_largeRequests.size(); i > 0; --i)
                {
                    ASSERT( i <= m_largeRequests.size() );
                    if ( m_largeRequests[ i-1] == MPI_REQUEST_NULL) {
                        using std::swap;
                        swap( m_largeRequests[ i-1 ], m_largeRequests.back() );
                        swap( m_largeSends[ i-1 ], m_largeSends.back() );
                        m_largeRequests.pop_back();
                        m_largeSends.pop_back();
                    }
                }

                /* handle reception of messages in any order */

                MPI_Status status;
                int recvReady = 0;
                rc = MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, m_comm, 
                        &recvReady, &status);
                if (MPI_SUCCESS != rc )
                    throw Exception("Cannot Probe msg");

                if (recvReady) {
                    int srcPid = status.MPI_SOURCE;
                    int tag = status.MPI_TAG;
                    ASSERT( unsigned(tag) < m_irecvs.size() );
                    void * buf = m_irecvs[tag].first;
                    size_t size = m_irecvs[tag].second;
                    ASSERT( size > 0 );
                    LOG( 4, "Receiving next " << std::min(m_maxMpiMsgSize, size) << " of "
                        << size << " bytes of message with tag " << tag << " from process "
                        << srcPid << " to " << m_pid );
                    rc = MPI_Recv( buf, std::min(m_maxMpiMsgSize, size), MPI_BYTE,
                                srcPid, tag, m_comm, MPI_STATUS_IGNORE );

                    if (MPI_SUCCESS != rc )
                        throw Exception("Cannot Recv msg");

                    if ( size > m_maxMpiMsgSize )
                    {
                        char * pos = static_cast<char *>(buf);
                        m_irecvs[tag].first = pos + m_maxMpiMsgSize;
                        m_irecvs[tag].second = size - m_maxMpiMsgSize;
                    }
                    else
                    {
                        nMsgs -= 1;
                    }
                }
            }
            ASSERT( m_largeSends.empty() );
            ASSERT( m_largeRequests.empty() );
            m_irecvs.clear();
            m_emptyRecvs = 0;

            /* finish all the small sends */
            rc = MPI_Waitall( m_imsgs.size(), m_imsgs.data(), 
                        MPI_STATUSES_IGNORE);
            if (MPI_SUCCESS != rc )
                throw Exception("A nonblocking message failed");

            m_imsgs.clear();

            /* do a barrier in order to not confuse with any other sends or receives */ 
            rc = MPI_Barrier( m_comm );
            if (MPI_SUCCESS != rc )
                throw Exception("An MPI Barrier failed");
        }


    private:
        MPI_Comm m_comm;
        int m_pid, m_nprocs;
        const size_t m_maxMpiMsgSize;

        typedef MemoryRegister< MPI_Win > Windows;
        typedef Windows::Slot WinID;

        Windows          m_windows;
        SparseSet<WinID> m_usedWindows;
        std::vector<MPI_Request > m_imsgs;
        std::vector< std::pair< void *, size_t > > m_irecvs;
        size_t m_emptyRecvs;

        struct Send {
            const void * addr;
            size_t size;
            int dstPid;
            int tag;
        };
        std::vector< Send > m_largeSends;
        std::vector<MPI_Request > m_largeRequests;
        std::vector<int> m_sendIndices;
    };

    Comm :: Comm() 
        : m_impl() 
    {}

    Comm :: ~Comm()
    {}

    Comm :: Comm( MPI_Comm * mpiCommunicator )
        : m_impl( new Comm::Impl( *mpiCommunicator ) )
    {}

    Comm Comm :: split( int color, int rank) const
    {
        Comm result;
        result.m_impl.reset( new Impl( this->m_impl->comm(), color, rank) );
        return result;
    }

    Comm Comm :: clone() const
    {
        Comm clone;
        clone.m_impl.reset( new Impl( this->m_impl->comm() ));
        return clone;
    }

    MPI_Comm Comm :: comm() const
    {
        return this->m_impl->comm(); 
    }

    int Comm :: nprocs() const
    {
        int result = -1;
        int rc = MPI_Comm_size( this->m_impl->comm(), & result );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not determine size of MPI communicator");
        }
        return result;
    }

    int Comm :: pid() const
    {
        int result = -1;
        int rc = MPI_Comm_rank( this->m_impl->comm(), & result );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not determine rank in MPI communicator");
        }
        return result;
    }

    void Comm :: broadcast( void * mem, size_t size, int root) const
    {
        if ( size > (size_t) INT_MAX )
        {
            throw Exception("Broadcast size is too large");
        }
        int rc = MPI_Bcast( mem, size, MPI_CHAR, root, this->m_impl->comm() );

        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Broadcasting value failed");
        }
    }

    void Comm :: allToAll(
                const void * sendArray, void * recvArray,
                int elementSize
                ) const
    {
    int rc = MPI_Alltoall(
#ifdef PLATFORM_MPI
            const_cast<void *>(sendArray),
#else
            sendArray,
#endif
            elementSize, MPI_BYTE,
                recvArray, elementSize, MPI_BYTE,
                this->m_impl->comm() );

        if ( MPI_SUCCESS != rc )
        {
            throw Exception("All-to-all failed");
        }
    }

    /// Total exchange of potentially unbalanced communication
    void Comm :: allToAll( 
            const void * sendArray, int * sendOffsets, int * sendCounts,
            void * recvArray, int * recvOffsets, int * recvCounts 
            )  const
    {
        int rc = MPI_Alltoallv(
#ifdef PLATFORM_MPI
                const_cast<void *>(sendArray),
#else
                sendArray,
#endif
                sendCounts, sendOffsets, MPI_BYTE,
                recvArray, recvCounts, recvOffsets, MPI_BYTE,
                this->m_impl->comm() );

        if ( MPI_SUCCESS != rc )
        {
            throw Exception("All-to-all-v failed");
        }
    }

#ifdef MPI_HAS_IBARRIER        
    void Comm :: allToAllAllocTempspace( int nSends, std::vector<char> & tempSpace )
    {
        tempSpace.resize( (sizeof(MPI_Request) + sizeof(int))*nSends );
    }

    void Comm :: allToAll( int nSends, 
            const void * sendData, size_t * sendOffsets, size_t * sendCounts, int * sendPids,
            void * recvData, size_t recvSize, size_t * received,
            std::vector<char> & tempSpace ) const
    { 
      // The NBX algorithm by Hoefler 2010 - Scalable Communication Protocol for Dynamic Sparse Data Exchange
      // (see also page 29 from "Using Advanced MPI")
        MPI_Comm comm = this->m_impl->comm();
        const size_t maxMsgSize = this->m_impl->getMaxMsgSize();
        ASSERT( maxMsgSize <= unsigned( std::numeric_limits<int>::max()) );
        const int tag = 0;

        if ( tempSpace.size() < (sizeof(MPI_Request) + sizeof(int))*nSends )
            throw Exception("Insufficient temporary space");

        MPI_Request * sendRequests = reinterpret_cast<MPI_Request *>(tempSpace.data());
        std::fill( sendRequests, sendRequests + nSends, MPI_REQUEST_NULL);

        int * indices = reinterpret_cast<int *>(tempSpace.data() + sizeof(MPI_Request)*nSends);
        std::fill( indices, indices + nSends, -1);

#ifdef PLATFORM_MPI
        char * s = const_cast<char *>(static_cast<const char *>(sendData));
#else
        const char * s = static_cast<const char *>(sendData);
#endif       

        char * r = static_cast<char *>(recvData);
        *received = 0;

        for (int i = 0; i < nSends; ++i) {
            int n = std::min(maxMsgSize, sendCounts[i]);
            if (n > 0) {
                int rc = MPI_Issend( s + sendOffsets[i], n, MPI_BYTE, 
                                 sendPids[i], tag, comm, &sendRequests[i]);
                if (MPI_SUCCESS != rc)
                    throw Exception("Cannot send non-blocking synchronous MPI message");
            }
            sendOffsets[i] += n;
            sendCounts[i] -= n;
        }

        MPI_Request barrierRequest;
        int barrierDone = 0, barrierActive = 0;
        while (!barrierDone) {

            // receive messages
            int msgArrived = 0;
            MPI_Status msg;
            int rc = MPI_Iprobe( MPI_ANY_SOURCE, tag, comm, &msgArrived, &msg);
            if (MPI_SUCCESS != rc)
                throw Exception("Cannot probe for MPI message");

            if (msgArrived) {
                int n = -1;
                rc = MPI_Get_count( &msg, MPI_BYTE, &n);
                if (MPI_SUCCESS != rc)
                    throw Exception("Unable to determine size of MPI message");
                if (recvSize < unsigned(n)) 
                    throw Exception("Overflow in DSDE all-to-all (NBX algorithm)");
                rc = MPI_Recv( r, n, MPI_BYTE, msg.MPI_SOURCE, tag, comm, &msg);
                if (MPI_SUCCESS != rc)
                    throw Exception("Unable to receive MPI message");
                r += n;
                recvSize -= n;
                *received += n;
            }

            // if we haven't send all messages yet, check on them
            if (!barrierActive) {
                int msgsSent = -1;
                rc = MPI_Testsome( nSends, sendRequests,
                        & msgsSent, indices, MPI_STATUSES_IGNORE);
                if (MPI_SUCCESS != rc)
                    throw Exception("Unable to test for completion of sent MPI messages");
                for (int i = 0; i < msgsSent; ++i) {
                    // any message that is ready, should be continued if necessary
                    int j = indices[i];
                    if ( sendCounts[j] > 0 ) {
                        // continue
                        int n = std::min(maxMsgSize, sendCounts[j]);
                        rc = MPI_Issend( s + sendOffsets[j], n, MPI_BYTE, 
                                sendPids[j], tag, comm, &sendRequests[j]);
                        if (MPI_SUCCESS != rc)
                            throw Exception("Unable to send synchronous non-block MPI message");
                        sendOffsets[j] += n;
                        sendCounts[j] -= n;
                    }
                }
                for (int i = 0; i < nSends; ++i) {
                    // any message that is ready
                    if ( sendCounts[i] == 0 && sendRequests[i] == MPI_REQUEST_NULL ) {
                        // remove from the queue
                        using std::swap;
                        swap( sendOffsets[i], sendOffsets[nSends-1]);
                        swap( sendCounts[i], sendCounts[nSends-1]);
                        swap( sendPids[i], sendPids[nSends-1]);
                        swap( sendRequests[i], sendRequests[nSends-1]);
                        nSends -= 1;
                        i -= 1;
                    }
                }
                if (nSends == 0) {
                    rc = MPI_Ibarrier( comm, &barrierRequest);
                    if (MPI_SUCCESS != rc)
                        throw Exception("Unable to start non-blocking MPI barrier");
                    barrierActive = 1;
                }
            }
            else {
                // participate in the barrier and wait until completion
                rc = MPI_Test( &barrierRequest, &barrierDone, MPI_STATUS_IGNORE);
                if (MPI_SUCCESS != rc)
                    throw Exception("Unable to whether non-blocking MPI barrier has completed");
            }
        }
    }
#endif

    void Comm :: allgather( const void * x, std::size_t xSize, void * xs) const
    {
        int rc = MPI_Allgather( 
#ifdef PLATFORM_MPI
                const_cast<void *>(x),
#else
                x,
#endif
                xSize, MPI_BYTE, xs, xSize, MPI_BYTE,
                m_impl->comm() );

        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Allgather failed");
        }
    }

    void Comm :: allgather( void * xs, std::size_t xSize ) const
    {
        int rc = MPI_Allgather( MPI_IN_PLACE, xSize, MPI_BYTE, xs, xSize, 
                MPI_BYTE, m_impl->comm() );

        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Allgather failed");
        }
    }

    bool Comm :: allreduceOr( bool myelement) const
    {
        int myvalue = myelement;
        int result = 0;
        int rc = MPI_Allreduce( & myvalue, & result, 1, MPI_INT, MPI_LOR, 
                this->m_impl->comm() 
                );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not compute global 'or' with MPI_Allreduce");
        }
        return result;
    }

    bool Comm :: allreduceAnd( bool myelement) const
    {
        int myvalue = myelement;
        int result = 0;
        int rc = MPI_Allreduce( & myvalue, & result, 1, MPI_INT, MPI_LAND, 
                this->m_impl->comm() 
                );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not compute global 'and' with MPI_Allreduce");
        }
        return result;
    }

    int Comm :: allreduceSum( int myElement ) const 
    {
        int result = 0;
        int rc = MPI_Allreduce( & myElement, & result, 1, MPI_INT, MPI_SUM, 
                this->m_impl->comm() 
                );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not compute global 'sum' with MPI_Allreduce");
        }
        return result;
    }

    void Comm :: allreduceSum( const int * xs, int *result, int n) const 
    {
        int rc = MPI_Allreduce( 
#ifdef PLATFORM_MPI
                const_cast<int *>(xs),
#else
                xs,
#endif               
                result, n, MPI_INT, MPI_SUM, 
                this->m_impl->comm() 
                );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not compute global 'sum' with MPI_Allreduce");
        }
    }

    int Comm :: allreduceMax( int myElement ) const 
    {
        int result = 0;
        int rc = MPI_Allreduce( & myElement, & result, 1, MPI_INT, MPI_MAX, 
                this->m_impl->comm() 
                );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not compute global 'max' with MPI_Allreduce");
        }
        return result;
    }

    unsigned long Comm :: allreduceMax( unsigned long myElement ) const 
    {
        unsigned long result = 0;
        int rc = MPI_Allreduce( & myElement, & result, 1, MPI_UNSIGNED_LONG, MPI_MAX, 
                this->m_impl->comm() 
                );
        if ( MPI_SUCCESS != rc )
        {
            throw Exception("Could not compute global 'max' with MPI_Allreduce");
        }
        return result;
    }

    void Comm :: barrier() const
    {
        int rc = MPI_Barrier( m_impl->comm() );
        if (MPI_SUCCESS != rc )
        {
            throw Exception("MPI barrier failed");
        }
    }

#ifdef LPF_CORE_MPI_USES_mpirma

    void Comm
        :: reserveMemslots( int maxNumber ) 
    { m_impl->reserveWindows( maxNumber ); }

    Communication::Memslot Comm
        :: createMemslot( void * base, std::size_t size)
    {
        return Memslot( m_impl->createWindow(base, size) );
    }

    void Comm 
        :: removeMemslot( Memslot memslot ) 
    { m_impl->freeWindow( memslot.getId() ); }

    void Comm
        :: fenceAll() 
    { 
        m_impl->fenceAll();
    }

    void Comm
        :: fence(Memslot slot) 
    { 
        m_impl->fence(slot.getId());
    }

    void Comm 
        :: put( const void * srcBuf, int dstPid, Memslot dstSlot,
                std::size_t dstOffset , std::size_t size)
    {
        const size_t maxMsgSize = m_impl->getMaxMsgSize();
        const size_t maxBlockSize = std::min( maxMsgSize, size );

        for (size_t offset = 0; offset < size; offset += maxBlockSize ) 
        {
            const char * src = static_cast<const char *>(srcBuf);
            size_t block= std::min( maxMsgSize, size - offset );

            int rc = MPI_Put( src + offset, block, MPI_BYTE, 
                    dstPid, dstOffset + offset, block, MPI_BYTE, 
                    m_impl->window(dstSlot.getId())
                    );

            if ( MPI_SUCCESS != rc )
            {
                throw Exception("MPI Put failed");
            }
        }
    }


    void Comm 
        :: get( int srcPid, Memslot srcSlot, std::size_t srcOffset,
                void * dstBuf, std::size_t size)
    {
        const size_t maxMsgSize = m_impl->getMaxMsgSize();
        const size_t maxBlockSize = std::min( maxMsgSize, size );

        for (size_t offset = 0; offset < size; offset += maxBlockSize ) 
        {
            char * dst = static_cast<char *>(dstBuf);
            size_t block= std::min( maxMsgSize, size - offset );

            int rc = MPI_Get( dst + offset, block, MPI_BYTE, 
                    srcPid, srcOffset + offset, block, MPI_BYTE, 
                    m_impl->window(srcSlot.getId())
                    );

            if ( MPI_SUCCESS != rc )
            {
                throw Exception("MPI Get failed");
            }
        }
    }
#endif

    void Comm :: reserveMsgs( size_t n )
    {
        m_impl->reserveMsgs(n);
    }

    void Comm 
        :: isend( const void * srcBuf, size_t bytes, int destPid, int tag)
    {
        m_impl->isend( srcBuf, bytes, destPid, tag);
    }

    void Comm 
        :: irecv( void * dstBuf, size_t bytes, int srcPid, int tag )
    {
        m_impl->irecv( dstBuf, bytes, srcPid, tag );
    }

    void Comm :: iwaitall()
    {
        m_impl->iwaitall();
    }


    Lib :: Lib(int * argc, char ***argv)
        : m_world()
        , m_self()
        , m_mpiInitialized( false )
        , m_selfInitialized( false )
        , m_fatalFailure(false)
        , m_supportsThreads(false)
    {
        int rc = MPI_Initialized( &m_mpiInitialized );
        if ( MPI_SUCCESS != rc )
        {
           throw Exception("Could not determine whether MPI was already initialized"); 
        }
        
        if ( ! m_mpiInitialized && !LPF_MPI_AUTO_INITIALIZE )
        {
            throw Exception("The application did not call MPI_Init()");
        }

        if ( ! m_mpiInitialized )
        {
            int provided = MPI_THREAD_SINGLE;
            rc = MPI_Init_thread( argc, argv, MPI_THREAD_FUNNELED, &provided  );
            m_selfInitialized = true;
            
            if ( MPI_SUCCESS != rc )
            {
               throw Exception("Initialization MPI failed"); 
            }

            if ( provided >= MPI_THREAD_FUNNELED )
            {
                m_supportsThreads = true;
            }
        }
        else
        {
            int provided = MPI_THREAD_SINGLE;

            rc = MPI_Query_thread( &provided );
            if ( MPI_SUCCESS != rc )
            {
               throw Exception("MPI thread support query failed"); 
            }
            if ( provided >= MPI_THREAD_FUNNELED )
            {
                m_supportsThreads = true;
            }
        }

        m_self.m_impl.reset( new Comm::Impl( MPI_COMM_SELF ) );
    }

    int Lib :: total_nprocs() const
    {
        int nprocs;
        int rc = MPI_Comm_size( MPI_COMM_WORLD, &nprocs );
        if ( MPI_SUCCESS != rc )
            throw Exception("Cannot determine number of MPI processes");
        return nprocs;
    }

    const Comm & Lib :: world() const 
    {
        if (!m_world.m_impl) 
            m_world.m_impl.reset( new Comm::Impl( MPI_COMM_WORLD ) );
        
        return m_world;
    }

    Lib :: ~Lib()
    {
        if (m_fatalFailure)
        {
            LOG(1, "LPF forced _exit() before call to MPI_Finalize()"
                   "because library has experienced unrecoverable failure." );
            _exit(EXIT_FAILURE);
        }

        m_world.m_impl.reset(); //lint !e1551 shared_ptr::reset never throws
        m_self.m_impl.reset();  //lint !e1551

        int finalized = 0;
        int rc = MPI_Finalized(&finalized);
        if( MPI_SUCCESS != rc )
            LOG(1, "Unable to determine whether MPI program has been finalized");
        
        if ( m_selfInitialized && !finalized )
        {
            rc = MPI_Finalize();
            if( MPI_SUCCESS != rc )
                LOG(1, "Unable to finalize MPI program");
        }
    }

    void Lib :: abort() const
    {
        std::cerr << "Aborting program" << std::endl;
        (void) MPI_Abort( MPI_COMM_WORLD, 134); // SIGABORT + 128 => exit code in bash
    }

    Lib & Lib :: instance(int * argc, char *** argv )
    {
        static Lib lib(argc, argv);
        return lib;
    }


} } // namespace mpi, namespace lpf
