
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

#include "barrier.hpp"
#include <assert.hpp>
#include <log.hpp>
#include <time.hpp>

#include <algorithm> // fill_n, next_permutation
#include <stdexcept> // bad_alloc
#include <vector>

#include <unistd.h> // usleep, sysconf
#include <stdlib.h> // posix_memalign
#include <pthread.h> // pthread functions
#include <errno.h> // ETIMEDOUT

#include <cstdio>
#include <climits>
#include <cstring> // memcpy

#if __cplusplus >= 201103L    
#include <atomic>
#endif

#ifdef __SSE2__
#include <xmmintrin.h>  // _mm_pause
#endif

#ifdef __SSE3__
#include <cpuid.h>
#include <pmmintrin.h>
#endif


#ifdef LPF_ON_MACOS
// MacOS has no implementation of pthread_barrier. For that reason, the
// autotune function won't work
#define FLAT_BARRIER 1
#else
#define BLOCK_TREE_BARRIER 1
#endif

namespace lpf {


namespace { 
    inline void fence() 
    {
#if __cplusplus >= 201103L
        std::atomic_thread_fence( std::memory_order_seq_cst );
#else 
#ifdef __GNUC__
        __atomic_thread_fence( __ATOMIC_SEQ_CST );
#endif          
#endif
    }


    template < Barrier::SpinMode MODE>
    struct MonitorWait
    {   void set(volatile void * p)
        {
            (void) p;
        
        }
        void pause() 
        {
#ifdef VALGRIND_MEMCHECK        
#ifdef LPF_ON_MACOS
            pthread_yield_np();
#else
            pthread_yield(); // allow other processes to progress
#endif
#endif
        }
    };

#ifdef __SSE2__
    template <>
    struct MonitorWait< Barrier::SPIN_PAUSE >
    {   void set(volatile void * p) { (void) p;  }
        void pause() { _mm_pause(); }
    };
#endif


#if 0
    // DISABLED since it's not portable to call Monitor/mwait from user space
    template <>
    struct MonitorWait< Barrier::SPIN_HT >
    {   
        MonitorWait()
            : m_available( queryAvailability() )
            , m_range( queryRange() )
        {}

        static bool queryAvailability()
        {
            unsigned level = 0x01; // query availability monitor/mwait
            unsigned eax, ebx, ecx, edx;
            __get_cpuid( level, &eax, &ebx, &ecx, &edx);
            bool available = ecx & (1 << 3 );
 
            LOG(3, "Intel MONITOR/MWAIT " << (available?" *IS* ": " *IS NOT* ")
                    << "available. " );

            return available;
        }

        static unsigned queryRange() {
            unsigned level = 0x05; // query monitor/mwait leaf
            unsigned eax, ebx, ecx, edx;
            __get_cpuid( level, &eax, &ebx, &ecx, &edx);
            
            LOG(3, "Intel MONITOR/MWAIT minimum address range is " << eax 
                    << "; maximum address range is " << ebx );

            return eax;
        }

        
        void set(volatile void * p) 
        {
            if (m_available) 
                _mm_monitor( const_cast<const void *>(p), 0, 0);
        }

        void pause() 
        {
            if (m_available )
                _mm_mwait(0, 0);
            else 
                pthread_yield();
        }

        bool m_available;
        size_t m_range;
    };

#else

    template <>
    struct MonitorWait< Barrier::SPIN_YIELD >
    {   void set(volatile void * p) { (void) p;  }
        void pause() 
        { 
#ifdef LPF_ON_MACOS
            pthread_yield_np();
#else
            if (pthread_yield()) {
                LOG(2, "While waiting, the Posix thread library failed to "
                       "yield the CPU to the OS" );
            }
#endif
        }
    };
#endif

    template <class MonitorWait >
    class FlatBarrier
    {
    public:
        // allocate space for a number of independent barriers for
        // nprocs number of threads
        FlatBarrier( unsigned nprocs, unsigned number = 1 )
            : m_monitorWait()
            , m_memory(NULL)
            , m_nprocs(nprocs)
            , m_stride(0)
        {
            size_t pagesize = static_cast< size_t> (sysconf( _SC_PAGESIZE ));
            size_t size = std::max< size_t >( nprocs * sizeof(unsigned), pagesize );

            void * ptr = NULL;
            
            if ( posix_memalign( &ptr, pagesize, number * size ))
                throw std::bad_alloc();

            m_memory = static_cast<unsigned *>(ptr);
            m_stride = size / sizeof(unsigned);
        }

        ~FlatBarrier()
        {
            free( const_cast<unsigned *>(m_memory) );
        }

        void init( unsigned index  )
        {
            std::fill_n( m_memory + index*m_stride, m_stride, 0);
        }
    
        void execute( volatile bool * abort, unsigned pid, unsigned nprocs, unsigned index )
        {
            // a barrier with sense-reversal 
            // and spinlocking on local data
            //
            ASSERT( nprocs <= m_nprocs );
            ASSERT( pid < nprocs );
            fence();
            volatile unsigned * mem = m_memory + m_stride * index;
            const unsigned waiting = mem[ pid ];
            const unsigned done = waiting + 1, next = waiting+2;

            mem[ pid ] = done;
            fence();

            while ( !(*abort)  ) {
                for (pid_t p = 0; p < nprocs; ++p) {
                    unsigned sample = mem[ p ];
                    if ( sample != done && sample != next) {
                        m_monitorWait.set( mem + p );
                        goto wait;
                    }
                }
                goto finished;

        wait:   m_monitorWait.pause(); //lint !e523 !e525 Does nothing for default MonitorWait 
            }
        finished:
            return;
        }

    private:
        FlatBarrier( const FlatBarrier & ); // prohibit copying
        FlatBarrier & operator=(const FlatBarrier & ); // prohibit assignment

        MonitorWait m_monitorWait;
        volatile unsigned * m_memory;
        unsigned m_nprocs;
        size_t   m_stride;
    };

    
    template <class MonitorWait >
    class TreeBarrier
    { 
    public: 
        // given a list of nomials per tree level
        // the nomials must be an increasing sequence where each
        // successor is a multiple of the previous. the last item
        // must be the number of processes (nprocs)
        // e.g.: 7, 14, 56
        TreeBarrier( unsigned nprocs, unsigned * nomials, unsigned depth)
            : m_nprocs( nprocs )
            , m_topology( 1 + depth)
            , m_barrier( nprocs, 1 + 2*countBarriers(nprocs, nomials, depth) )
        {
            ASSERT( nprocs == 1 || m_topology.size() >= 2);
            ASSERT( nomials[depth-1] == nprocs );
            m_topology[0] = 1;
            std::copy( nomials, nomials + depth, m_topology.data()+1);
            // initialize one flat barrier
            m_barrier.init( 0 );
        }

        TreeBarrier( unsigned nprocs)
            : m_nprocs( nprocs )
            , m_topology( 1 + s_depth  )
            , m_barrier( nprocs, 
                    std::max(2u, 1+2*countBarriers(nprocs, s_nomials, s_depth))
              )
        {
            m_topology[0] = 1;
            if ( s_depth == 0) {
                m_topology.push_back( nprocs );
            }
            else
            {
                std::copy( s_nomials, s_nomials + s_depth, m_topology.data()+1);
        
                for (unsigned i = 1; i < m_topology.size(); ++i)
                {
                    if ( m_topology[i] >= nprocs)
                    {
                        m_topology[i] = nprocs;
                        m_topology.resize( i+1);
                        break;
                    }
                }
            }
            // initialize one flat barrier
            m_barrier.init( 0 );
        }

#ifndef LPF_ON_MACOS
        static void autoTune( unsigned pid, unsigned nprocs)
        {
            static pthread_mutex_t mutex  = PTHREAD_MUTEX_INITIALIZER;

            if (nprocs == 1) return;

            static unsigned initialized = 0;
            ASSERT( initialized <= 1);
            static pthread_barrier_t pthreadBarrier;
            int prc = pthread_mutex_lock( &mutex ); ASSERT(!prc);
            if ( !initialized )
            {
               prc = pthread_barrier_init( & pthreadBarrier, NULL, nprocs);
               ASSERT(!prc);
               initialized = 1;
            }
            prc = pthread_mutex_unlock( &mutex ); ASSERT(!prc);


            ASSERT( nprocs <= 10000 );
            // primes up to a hundred => which will support SMP machines
            // up to 10000 processors.
            static unsigned primes[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29,
                31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 97, 101};
            static const unsigned nPrimes = sizeof(primes)/sizeof(primes[0]);

            // factorize 'nprocs'
            unsigned factors[14];
            unsigned nFactors = 0;
            for ( unsigned i = 0, product=nprocs; i < nPrimes; ++i)
            {
                const unsigned prime = primes[i];
                while ( product > 0 && product % prime == 0 ) {
                    ASSERT( nFactors < 14 );
                    factors[ nFactors++ ] = prime;
                    product /= prime;
                }

                if (product <= 1)
                    break;
            }

            Time bestTime = Time::fromSeconds(1.0) ;
            // iterator over all permutations
            do
            {
                // iterate over all product combinations
                for (unsigned i = 0; i < (1u<<nFactors); ++i)
                {
                    unsigned nomials[14]; 
                    unsigned depth=0;
                    unsigned product=1;
                    for (unsigned j = 0; j < nFactors; ++j)
                    {
                        product *= factors[j];
                        if ( (j==nFactors-1) || ( i & (1<<j)) ) {
                            nomials[depth++] = product;
                        }
                    }

                    bool abort = false;
                    static volatile TreeBarrier * object = NULL;
                    prc = pthread_barrier_wait( &pthreadBarrier ); 
                    ASSERT(prc == PTHREAD_BARRIER_SERIAL_THREAD || prc == 0);
                    if ( pid == 0)
                    {
                       object = new TreeBarrier(nprocs, nomials, depth);
                    }
                    fence();
                    prc = pthread_barrier_wait( &pthreadBarrier ); 
                    ASSERT(prc == PTHREAD_BARRIER_SERIAL_THREAD || prc == 0);

                    TreeBarrier * test = const_cast<TreeBarrier *>(object);

                    test->init(&abort, pid);

                    Time t0 = Time::now();
                    for (unsigned k = 0; k < 10; ++k)
                        test->execute(&abort, pid);
                    Time t1 = Time::now();
                    

                    prc = pthread_barrier_wait( &pthreadBarrier ); 
                    ASSERT(prc == PTHREAD_BARRIER_SERIAL_THREAD || prc == 0);
                    if ( pid == 0)
                    {
                       delete object;
                       object = NULL;
                    }
                    fence();
                    prc = pthread_barrier_wait( &pthreadBarrier );
                    ASSERT(prc == PTHREAD_BARRIER_SERIAL_THREAD || prc == 0);

                    // save the best result
                    if ( (t1-t0) < bestTime && pid == 0)
                    {
                        std::copy( nomials, nomials+depth, s_nomials);
                        s_depth = depth;
                        bestTime = t1-t0;
                    }
                }
            }while(std::next_permutation( factors, factors+nFactors));

            prc = pthread_mutex_lock( &mutex ); ASSERT(!prc);
            initialized++;
            if ( initialized == nprocs+1 )
            {
               prc = pthread_barrier_destroy( & pthreadBarrier ); ASSERT(!prc);
               initialized = 0;
            }
            prc = pthread_mutex_unlock( &mutex ); ASSERT(!prc);
                    
        }
#endif


        void init( volatile bool * abort, pid_t pid )
        {
            if (m_nprocs == 1) return;

            unsigned n = m_topology.size();
            unsigned b = 1;
            for (unsigned i = 1; i < n; ++i)
            {
                unsigned k = m_topology[i];
                if ( pid % k == 0 )
                    m_barrier.init(b + pid/k);
                
                b += (m_nprocs + k - 1) / k;
            }
            for (unsigned i = n-2; i > 0; --i)
            {
                unsigned k = m_topology[i];
                if ( pid % k == 0 )
                    m_barrier.init(b + pid/k);
                
                b += (m_nprocs + k - 1) / k;
            }
       
            // synchronize all, to make sure all subbarriers have
            // been initialized before continuing
            m_barrier.execute( abort, pid, m_nprocs, 0);
        }

        void execute( volatile bool * abort, unsigned pid )
        {
            if (m_nprocs == 1) return;

            const pid_t nprocs = m_nprocs;
            unsigned b = 1;

            for (unsigned l = 1; l < m_topology.size(); ++l)
            {
                unsigned k = m_topology[l];
                unsigned kp = m_topology[l-1];
                const pid_t nprocs_l = 
                    (( pid < ((nprocs/k)*k)? k: (nprocs % k ))
                      + kp - 1 )/kp;
                const pid_t pid_l = (pid % k)/kp;

                if (nprocs > kp && (pid % kp) == 0)
                {
                    m_barrier.execute( abort, pid_l, nprocs_l, b + pid/k );
                }

                b += (m_nprocs + k -1)/k;
            }

            for (unsigned l = m_topology.size() - 2; l > 0; --l)
            { 
                unsigned k = m_topology[l];
                unsigned kp = m_topology[l-1];
                const pid_t nprocs_l = 
                    (( pid < ((nprocs/k)*k)? k: (nprocs % k ))
                      + kp - 1 )/kp;
                const pid_t pid_l = (pid % k)/kp;

                if (nprocs > kp && (pid % kp) == 0)
                {
                    m_barrier.execute( abort, pid_l, nprocs_l, b + pid/k );
                }

                b += (m_nprocs + k -1)/k;
            }
        }

    private:
        static unsigned countBarriers( unsigned nprocs, unsigned * topo, unsigned depth )
        {
            unsigned b = 0;
            for (unsigned i = 0; i < depth; ++i) {
                b += (nprocs + topo[i] -1) / topo[i];
            }
            return b;
        }

        // default values obtained by auto-tuning.
        static unsigned s_nomials[14]; 
        static unsigned s_depth;

        unsigned m_nprocs;
        std::vector< unsigned > m_topology;
        FlatBarrier< MonitorWait > m_barrier;
    };

    template <class MonitorWait>
    unsigned TreeBarrier<MonitorWait>::s_nomials[14];

    template <class MonitorWait>
    unsigned TreeBarrier<MonitorWait>::s_depth = 0;
}
        


template <>
class Barrier::Impl< Barrier::SPIN_COND >
{
public:
    Impl( pid_t nprocs )
        : m_counter( 0 )
        , m_phase(0)
        , m_nprocs( nprocs )
        , m_pmutex( )
        , m_pcond( )
    {
        pthread_mutex_init( &m_pmutex, NULL );
        pthread_cond_init( &m_pcond, NULL );
    }

    ~Impl()
    {
        pthread_mutex_destroy( &m_pmutex );
        pthread_cond_destroy( &m_pcond );
    }


    void init( volatile bool * abort, pid_t pid )
    {
        (void) abort;
        (void) pid;
    }

    void execute( volatile bool * abort, pid_t pid )
    {
        (void) pid;

        int rc = 0;
        rc = pthread_mutex_lock( &m_pmutex );
        if (rc)
            throw std::runtime_error("pthread_mutex_lock failed");

        m_counter += 1 ;

        if( *abort || m_counter == m_nprocs ) {
            m_counter = 0;
            m_phase += 1;
            /* all threads are now in 'execute' */
            rc = pthread_cond_broadcast( &m_pcond ); 
            if (rc) 
                throw std::runtime_error("pthread_cond_broadcast failed");
        } else {
            //if not, sleep
            unsigned phase = m_phase;
            while ( !*abort && phase == m_phase )
            {
                rc = pthread_cond_wait( &m_pcond , &m_pmutex ); 
                if (rc != ETIMEDOUT && rc)
                    throw std::runtime_error("pthread_cond_wait failed");
            }
        }

        rc = pthread_mutex_unlock( &m_pmutex );
        if (rc) throw std::runtime_error("pthread_mutex_unlock" );
    }

private:
    Impl( const Impl & ); //copying prohibited
    Impl & operator=(const Impl & ); // assignment prohibited

    volatile pid_t m_counter;
    volatile unsigned m_phase;
    pid_t m_nprocs;
    pthread_mutex_t m_pmutex;
    pthread_cond_t m_pcond;
};



#ifdef FLAT_BARRIER

template <Barrier::SpinMode mode>
struct Barrier::Impl : public FlatBarrier< MonitorWait< mode> > 
{
   Impl( unsigned nprocs, unsigned number)
       :FlatBarrier<MonitorWait< mode> >(nprocs, number)
   {}
};

Barrier::Barrier( pid_t nprocs, SpinMode mode ) 
    : m_nprocs( nprocs )
    , m_spinfast( mode==SPIN_FAST? new Impl< SPIN_FAST >(nprocs, 1) : NULL )
    , m_spinnice( mode==SPIN_PAUSE? new Impl< SPIN_PAUSE >(nprocs, 1) : NULL )
    , m_spinht( mode==SPIN_YIELD? new Impl< SPIN_YIELD >(nprocs, 1) : NULL )
    , m_spincond( mode==SPIN_COND? new Impl< SPIN_COND>(nprocs) : NULL )
    , m_abort( false )
{
    if (m_spinfast) m_spinfast->init(0);
    if (m_spinnice) m_spinnice->init(0);
    if (m_spinht)   m_spinht->init(0);
}

Barrier::~Barrier()
{
    delete m_spinfast;
    delete m_spinnice;
    delete m_spinht;
    delete m_spincond;
}

void Barrier::init( pid_t pid)
{
    (void) pid;
}

bool Barrier::execute(pid_t myId) 
{
    if (m_spinfast) m_spinfast->execute(&m_abort, myId, m_nprocs, 0);
    if (m_spinnice) m_spinnice->execute(&m_abort, myId, m_nprocs, 0);
    if (m_spinht)   m_spinht->execute(&m_abort, myId, m_nprocs, 0);
    if (m_spincond) m_spincond->execute(&m_abort, myId);
    return m_abort;
}

void Barrier::autoTune(pid_t pid, pid_t nprocs, SpinMode mode)
{
    (void) pid;
    (void) nprocs;
    (void) mode;
    /* there is nothing to autotune */
}

#endif




#ifdef BLOCK_TREE_BARRIER

template <Barrier::SpinMode mode>
struct Barrier::Impl : public TreeBarrier< MonitorWait<mode> > 
{
   Impl( unsigned nprocs, unsigned * nomials, unsigned depth )
       :TreeBarrier<MonitorWait<mode> >(nprocs, nomials, depth)
   {}

   Impl( unsigned nprocs )
       :TreeBarrier<MonitorWait<mode> >(nprocs)
   {}
};
 

Barrier::Barrier( pid_t nprocs, SpinMode mode ) 
    : m_nprocs( nprocs )
    , m_spinfast(NULL)
    , m_spinnice(NULL)
    , m_spinht(NULL)
    , m_spincond(NULL)
    , m_abort( false )
{
    if (mode==SPIN_FAST)
        m_spinfast = new Impl<SPIN_FAST>( nprocs);

    if (mode==SPIN_PAUSE)
        m_spinnice = new Impl<SPIN_PAUSE>( nprocs);

    if (mode==SPIN_YIELD)
        m_spinht = new Impl<SPIN_YIELD>( nprocs);

    if (mode==SPIN_COND)
        m_spincond = new Impl<SPIN_COND>( nprocs);
}
   

Barrier::~Barrier()
{
    delete m_spinfast;
    delete m_spinnice;
    delete m_spinht;
    delete m_spincond;
}

void Barrier::init( pid_t pid )
{
    if (m_spinfast) m_spinfast->init(&m_abort, pid);
    if (m_spinnice) m_spinnice->init(&m_abort, pid);
    if (m_spinht) m_spinht->init(&m_abort, pid);
    if (m_spincond) m_spincond->init(&m_abort, pid);
}

bool Barrier::execute( pid_t pid )
{
    if (m_spinfast) m_spinfast->execute(&m_abort, pid);
    if (m_spinnice) m_spinnice->execute(&m_abort, pid);
    if (m_spinht) m_spinht->execute(&m_abort, pid);
    if (m_spincond) m_spincond->execute(&m_abort, pid);
    return m_abort;
}

void Barrier::autoTune(pid_t pid, pid_t nprocs, SpinMode mode)
{
    switch (mode) {
        case SPIN_FAST:   Impl<SPIN_FAST>::autoTune( pid, nprocs ); break;
        case SPIN_PAUSE:  Impl<SPIN_PAUSE>::autoTune( pid, nprocs ); break;
        case SPIN_YIELD:  Impl<SPIN_YIELD>::autoTune( pid, nprocs ); break;
        case SPIN_COND:  /* no tuning necessary */ break;
    }
}

#endif
} // namespace lpf
