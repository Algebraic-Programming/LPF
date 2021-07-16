
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

#ifndef LPFLIB_HYBRID_BLOB_HPP
#define LPFLIB_HYBRID_BLOB_HPP

#include <cstring>
#include <vector>

#include "linkage.hpp"

class _LPFLIB_LOCAL BufBlob { 
public:

    template <class T> void push( const T & x)
    {   size_t i = m_buffer.size();
        m_buffer.resize( i + sizeof(x));
        std::memcpy( m_buffer.data() + i, &x, sizeof(x));
    }

    template <class T> void push( const std::vector< T > & xs )
    {   size_t i = m_buffer.size();
        m_buffer.resize( i + xs.size() * sizeof(T) );
        std::memcpy( m_buffer.data() + i, xs.data(), xs.size()*sizeof(T));
    }

    void push( const void * data, size_t size)
    {   size_t i = m_buffer.size();
        m_buffer.resize( i + size );
        std::memcpy( m_buffer.data() + i, data, size );
    }

    template <class T> void pop( T & x)
    {   ASSERT( m_buffer.size() >= sizeof(x));
        size_t i = m_buffer.size() - sizeof(x);
        std::memcpy( &x, m_buffer.data() + i, sizeof(x));
        m_buffer.resize( i );
    }

    template <class T> void pop( std::vector< T > & xs )
    {   ASSERT( m_buffer.size() >= xs.size() * sizeof(T));
        size_t i = m_buffer.size() - xs.size() * sizeof(T);
        std::memcpy( xs.data(), m_buffer.data() + i, xs.size()*sizeof(T));
        m_buffer.resize(i);
    }

    void pop( void * data, size_t size)
    {   ASSERT( m_buffer.size() >= size );
        size_t i = m_buffer.size() - size;
        std::memcpy( data, m_buffer.data() + i , size);
        m_buffer.resize(i);
    }

    char * data() 
    { return m_buffer.data(); }

    size_t size() const
    { return m_buffer.size(); }

    size_t maxsize() const
    { return m_buffer.max_size(); }

private:
    std::vector< char > m_buffer;
};

class _LPFLIB_LOCAL UnbufBlob { 
public:
    UnbufBlob( void * data, size_t size, size_t maxsize)
        : m_data( static_cast<char*>(data)), m_size(size), m_maxsize(maxsize) 
    {}

    template <class T> void push( const T & x)
    {   ASSERT( m_size + sizeof(x) <= m_maxsize );
        std::memcpy( m_data + m_size, &x, sizeof(x));
        m_size += sizeof(x);
    }

    template <class T> void push( const std::vector< T > & xs )
    {   size_t bytes = sizeof(T)*xs.size();
        ASSERT( m_size + bytes <= m_maxsize );
        std::memcpy( m_data + m_size, xs.data(), bytes );
        m_size += bytes;
    }

    void push( const void * data, size_t size)
    {   ASSERT( m_size + size <= m_maxsize);
        std::memcpy( m_data + m_size, data, size );
        m_size += size;
    }

    template <class T> void pop( T & x)
    {   ASSERT( m_size >= sizeof(x));
        m_size -= sizeof(x);
        std::memcpy( &x, m_data + m_size, sizeof(x));
    }

    template <class T> void pop( std::vector< T > & xs )
    {    const size_t bytes = xs.size()*sizeof(T);
         ASSERT( m_size >= bytes);
         m_size -= bytes;
         std::memcpy( xs.data(), m_data + m_size, bytes );
    }

    void pop( void * data, size_t size)
    {   ASSERT( m_size >= size );
        m_size -= size;
        std::memcpy( data, m_data + m_size, size);
    }

    char * data() 
    { return m_data; }

    size_t size() const
    { return m_size; }

    size_t maxsize() const
    { return m_maxsize; }

private:
    char * m_data;
    size_t m_size;
    size_t m_maxsize;
};

#endif
