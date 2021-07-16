
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

#ifndef LPF_CORE_MPI_SPALL2ALL_HPP
#define LPF_CORE_MPI_SPALL2ALL_HPP

#include "spall2all.h"
#include "mpilib.hpp"
#include "assert.hpp"
#include "vall2all.hpp"
#include "linkage.hpp"
#include "config.hpp"

#include <stdexcept>
#include <limits>

namespace lpf { namespace mpi {


class _LPFLIB_LOCAL SparseAllToAll : public VirtualAllToAll
{
public:
    typedef std::size_t size_type;

    static const uint64_t RNGSEED = 0;

    SparseAllToAll(int pid, int nprocs, int nvotes=0)
        : m_pid( pid )
        , m_nprocs( nprocs )
        , m_ballot( nvotes )
        , m_spa()
    {   
        sparse_all_to_all_create( & m_spa, pid, nprocs, RNGSEED, nvotes, 
               Config::instance().getMaxMPIMsgSize() );
        if (sparse_all_to_all_reserve( &m_spa, 0, 0 ))
            throw std::bad_alloc();
    }

    ~SparseAllToAll()
    {
        sparse_all_to_all_destroy( & m_spa );
    }

    size_t max_size( size_t maxMsgSize ) const
    {
        return std::numeric_limits<size_t>::max() / maxMsgSize;
    }


    void reserve( size_type number, size_t maxMsgSize )
    {
        if ( number > sparse_all_to_all_get_buf_capacity( &m_spa, maxMsgSize ) )
        {
            size_t buf = maxMsgSize * number;
            size_t msgs = number;

            int error = sparse_all_to_all_reserve( &this->m_spa, buf, msgs );
            if (error)
                throw std::bad_alloc();
        }
    }

    void clear()
    { 
        sparse_all_to_all_clear( & m_spa ); 
    }

    int exchange( const mpi::Comm & comm, bool prerandomize,
           int * vote, int retry = 5)
    { 
        int error = 1;
        for (int r = 0; r < retry && error; ++r) {
            error = sparse_all_to_all( comm.comm(), &m_spa, prerandomize,
                   vote, m_ballot.data() );
            if (error) prerandomize = true;
        }
        if (error < 0)
            return -1;

        std::copy( m_ballot.begin(), m_ballot.end(), vote );
        return 0;
    }

    void send( pid_t pid, const void * msg, size_t size )
    { 
        int error = sparse_all_to_all_send( &m_spa, pid, 
                reinterpret_cast<const char *>(msg), size );

        if (error)
            throw std::out_of_range("Not enough space reserved");
    }

    size_t recv( void * buf, size_t maxSize )
    {
        size_t size = maxSize;
        int error = sparse_all_to_all_recv( &m_spa, 
                reinterpret_cast<char *>(buf), &size);
        if (error && size == 0)
            return 0;
        else return size;
    }

    bool empty() const
    { return sparse_all_to_all_is_empty(&m_spa); }

private:
    SparseAllToAll( const SparseAllToAll & other );
    SparseAllToAll & operator=( const SparseAllToAll & other );

    int m_pid;
    int m_nprocs;
    std::vector< int > m_ballot;
    sparse_all_to_all_t m_spa;
};


} } // namespaces lpf, mpi

#endif
