
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

#ifndef PLATFORM_CORE_SPARSESET_HPP
#define PLATFORM_CORE_SPARSESET_HPP

#include <vector>
#include <utility>
#include <limits>

#include "log.hpp"
#include "assert.hpp"

namespace lpf {

/// A sparse set a la Briggs and Torczon. It provides a set of objects of a
/// specified integral data type, and allows insertion and removal of elements
/// constant time. However, memory footprint is proportional to the numerical
/// range of the domain. 
template <class T>
class SparseSet
{
    typedef std::vector< T > Domain;
public:

    typedef typename Domain::size_type size_type;
    typedef typename Domain::const_iterator iterator;
    typedef typename Domain::const_iterator const_iterator;
    typedef T value_type;

    /// Instantiate an empty set with empty range [1,0].
    SparseSet()
        : m_min( 1 )
        , m_max( 0 )
        , m_size( 0 )
        , m_domain()
        , m_map()
    {}

    /// Allocate a set for the specified range [min, max].
    explicit SparseSet( T min, T max)
        : m_min( 0)
        , m_max( 0)
        , m_size( 0 )
        , m_domain()
        , m_map()
    {
        reset( min, max );
    }

    /// Empty the set and reallocate a set for the specified range [min, max].
    void reset( T min, T max )
    {
        ASSERT( max >= min );
        ASSERT( static_cast<size_type>(max-min) < this->max_size() ); 
        size_type size = max - min + 1; // [min,max] is inclusive range

        size_type oldSize = m_domain.size();
        try
        {
            m_domain.resize( size );
            m_map.resize( size );
        }
        catch ( std::bad_alloc & )
        {
            m_domain.resize( oldSize );
            LOG(2, "Unable to allocate extra memory for a SparseSet" );
            throw;
        }

        for ( size_type i = 0; i < size ; ++i )
        {
            m_domain[i] = i;
            m_map[i] = i;
        }
        m_min = min;
        m_max = max;
        m_size = 0;

        ASSERT( m_domain.size() == static_cast<size_type>(m_max - m_min + 1) );
    }

    /// Extend the range of the set with a new maximum.
    void extend( T newMax )
    {
        ASSERT( newMax >= m_max );
        ASSERT( static_cast<size_type>(newMax-m_min) < this->max_size() ); 

        size_type newSize = newMax - m_min + 1; // [min,max] is inclusive range
        size_type oldSize = m_domain.size();

        ASSERT( m_domain.size() == static_cast<size_type>(m_max - m_min + 1));
        ASSERT( newSize > oldSize );
        try
        {
            m_domain.resize( newSize );
            m_map.resize( newSize );
        }
        catch ( std::bad_alloc & )
        {
            m_domain.resize( oldSize );
            LOG(2, "Unable to allocate extra memory for a SparseSet" );
            throw;
        }

        for ( size_type i = oldSize ; i < newSize; ++i )
        {
            m_domain[i] = i;
            m_map[ i ] = i;
        }
        m_max = newMax;

        ASSERT( m_domain.size() == static_cast<size_type>(m_max - m_min + 1) );
    }

    /// Reduce the maximum of the range so that memory can be released.
    void shrink( T newMax )
    {
        ASSERT( newMax <= m_max );
        ASSERT( m_min <= newMax );

        ASSERT( m_domain.size() == static_cast<size_type>(m_max - m_min + 1) );

        const size_type targetSize = std::max<size_type>( m_size, newMax - m_min + 1 );
        // remove items from the back of the list without breaking any
        // invariants
        while ( m_domain.size() > targetSize && m_domain.back() > newMax )
        {
            m_domain.pop_back();
        }
        m_max = m_domain.size() + m_min - 1;
        m_map.resize( m_domain.size() );
        ASSERT( m_domain.size() >= m_size );
        ASSERT( m_domain.size() == static_cast<size_type>(m_max - m_min + 1) );
        ASSERT( m_max >= m_min );

        try {
            std::vector< T > copy( m_domain );
            m_domain.swap( copy );
        }
        catch( std::bad_alloc & )
        {
            LOG(2, "Unable to allocate temporary space to shrink memory for a SparseSet" );
            /* ignore any bad_allocs, because we only failed to reduce memory
             * footprint */
        }

        try
        {
            std::vector< size_type > copy( m_map );
            m_map.swap( copy );
        }
        catch( std::bad_alloc & )
        {
            /* ignore any bad_allocs, because we only failed to reduce memory
             * footprint */
            LOG(2, "Unable to allocate temporary space to shrink memory for a SparseSet" );
        }

        ASSERT( m_domain.size() >= m_size );
        ASSERT( m_domain.size() == static_cast<size_type>(m_max - m_min + 1) );
        ASSERT( m_map.size() == m_domain.size() );
    }

    /// Change the maximum of the range
    void resize( T newMax )
    {
        if (newMax < m_max )
            shrink( newMax );
        else if (newMax > m_max)
            extend( newMax );
    }

    /// Remove all elements
    void clear() 
    {
        m_size = 0;
    }

    /// Query whether the set is empty
    bool empty() const
    {
        return m_size == 0;
    }
 
    /// Query the number of elements in the set
    size_type size() const
    {
        return m_size;
    }

    std::pair<T, T> range() const
    { return std::make_pair(m_min, m_max); }

    size_type max_size() const
    { return std::min( m_domain.max_size(), m_map.max_size() ); }

    const_iterator begin() const 
    { return m_domain.begin(); }

    const_iterator end() const 
    { return m_domain.begin() + m_size ; }

    const_iterator find( T x ) const
    {
        if (x < m_min)
            return end();
        else if (x > m_max)
            return end();
        else if ( m_map[ x - m_min ] >= m_size )
            return end();
        else
            return m_domain.begin() + m_map[ x - m_min ];
    }

    /// Query whether the set contains the element \a x.
    bool contains( T x ) const  
    {
        return x >= m_min && x <= m_max && m_map[ x - m_min ] < m_size;
    }

    /// Adds \a x to the set, if it was not already there.
    void insert( T x )
    {
        ASSERT( x >= m_min );
        ASSERT( x <= m_max );

        if ( ! contains( x ) )
        {
            using std::swap;
            size_type a = m_map[ x - m_min ];
            size_type b = m_size;

            swap( m_domain[ a ], m_domain[ b ]);
            swap( m_map[ m_domain[a] ], m_map[ m_domain[b] ] );
            ++m_size;
        }
    }

    /// Removes \a x from the set, if it was in there.
    void erase( T x )
    {
        ASSERT( x >= m_min );
        ASSERT( x <= m_max );

        if ( contains( x ) )
        {
            using std::swap;
            --m_size;
            size_type a = m_map[ x - m_min ];
            size_type b = m_size;

            swap( m_domain[ a ], m_domain[ b ]);
            swap( m_map[ m_domain[a] ], m_map[ m_domain[b] ] );
        }
    }


private:
    T m_min, m_max;
    size_type m_size;
    std::vector< T >         m_domain;
    std::vector< size_type > m_map;
};


} // namespace lpf

#endif
