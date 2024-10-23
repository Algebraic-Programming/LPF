
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

#include <lpf/core.h>
#include <lpf/pthread.h>

#include "threadlocaldata.hpp"
#include "machineparams.hpp"
#include <log.hpp>

#include <vector>
#include <cassert>
#include <limits>
#include <climits>
#include <cstdint>
#include <cstdlib>

#if __cplusplus >= 201103L
#include <memory>
#else
#include <tr1/memory>
#endif

#include <pthread.h> // for pthreads

const lpf_err_t LPF_SUCCESS = 0;
const lpf_err_t LPF_ERR_OUT_OF_MEMORY = 1;
const lpf_err_t LPF_ERR_FATAL = 2;

const lpf_init_t LPF_INIT_NONE = NULL;

const lpf_args_t LPF_NO_ARGS = { NULL, 0, NULL, 0, NULL, 0 };

const lpf_sync_attr_t LPF_SYNC_DEFAULT = 0;

const lpf_msg_attr_t LPF_MSG_DEFAULT = 0;

const lpf_pid_t LPF_MAX_P = UINT_MAX;

const lpf_memslot_t LPF_INVALID_MEMSLOT = SIZE_MAX;

const lpf_t LPF_NONE = NULL;

const lpf_t LPF_ROOT = static_cast<void*>(const_cast<char *>("LPF_ROOT")) ;

const lpf_machine_t LPF_INVALID_MACHINE = { 0, 0, NULL, NULL };

namespace {
#if defined LPF_CORE_IMPL_ID && defined LPF_CORE_IMPL_CONFIG
    using namespace lpf :: LPF_CORE_IMPL_ID :: LPF_CORE_IMPL_CONFIG;
#else
    using namespace lpf ;
#endif
    MachineParams & getMachineParams() {
        static MachineParams mp
            = MachineParams::execProbe(&lpf::ThreadLocalData::root(), LPF_MAX_P);
        return mp;
    }

    lpf::ThreadLocalData * realCtx(lpf_t ctx) {
        if (LPF_ROOT == ctx) {
            (void) getMachineParams();  // ensure that machine is probed
            return & lpf::ThreadLocalData::root();
        }
        else {
            return static_cast<lpf::ThreadLocalData *>(ctx);
        }
    }
}

struct _LPFLIB_LOCAL LpfPthreadInit
{
    lpf_pid_t pid, nprocs;
#if __cplusplus >= 201103L
    std::shared_ptr< lpf::GlobalState > state;
#else
    std::tr1::shared_ptr< lpf::GlobalState > state;
#endif
};

lpf_err_t lpf_pthread_initialize(
    lpf_pid_t pid,
    lpf_pid_t nprocs,
    lpf_init_t * _context
) {
    static pthread_mutex_t lpf_hook_mutex  = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t  lpf_hook_cond   = PTHREAD_COND_INITIALIZER;
    static lpf_pid_t lpf_hook_sync_counter = 0;
#if __cplusplus >= 201103L
    static std::shared_ptr< lpf::GlobalState > lpf_hook_state;
#else
    static std::tr1::shared_ptr< lpf::GlobalState > lpf_hook_state;
#endif
    static bool failure = false;

    LpfPthreadInit * newContext = 0;

    // begin critical section
    int prc = pthread_mutex_lock( &lpf_hook_mutex ); ASSERT(!prc);

    try {
        // The first thread to enter this section creates the global state
        if( lpf_hook_sync_counter == 0 ) {
            failure = false;
            lpf_hook_state.reset( new lpf::GlobalState( nprocs ) );
        }
    }
    catch (std::bad_alloc & ) {
        LOG( 2, "Unable to allocate the shared parallel context");

        failure = true;

        if (lpf_hook_state)
        {
            lpf_hook_state->abort();
        }
    }

    if (! failure )
    {
        try {
            // All threads store a reference to the global state in a shared_ptr.
            LpfPthreadInit c = { pid, nprocs, lpf_hook_state };
            newContext = new LpfPthreadInit( c );
        }
        catch (std::bad_alloc & ) {
           LOG( 2, "Unable to allocate thread local data");
            failure = true;
            lpf_hook_state->abort();
        }
    }

    //update sync count
    ++lpf_hook_sync_counter;

    // wait until all processes executed previous code
    if( lpf_hook_sync_counter == nprocs ) {
        lpf_hook_sync_counter = 0;
        lpf_hook_state.reset();
        prc = pthread_cond_broadcast( &lpf_hook_cond ); ASSERT(!prc);
    } else {
        //if not, sleep
        while ( 0 == lpf_hook_sync_counter)
        {
            prc = pthread_cond_wait( &lpf_hook_cond, &lpf_hook_mutex ); ASSERT(!prc);
        }
    }

    // when everyone succeeded, assign the new context to 
    if (!failure)
    {
        *_context = newContext;
    }
    else
    {
        if (newContext)
        {
            delete newContext;
        }
    }
    
    // end critical section
    prc = pthread_mutex_unlock( &lpf_hook_mutex ); ASSERT(!prc);

    return failure ? LPF_ERR_FATAL : LPF_SUCCESS;
}

lpf_err_t lpf_pthread_finalize( lpf_init_t _context ) {
    delete static_cast< LpfPthreadInit * >(_context );
    return LPF_SUCCESS;
}

lpf_err_t lpf_hook(
    lpf_init_t _init,
    lpf_spmd_t spmd,
    lpf_args_t args
)
{
    LpfPthreadInit * init = 
        static_cast< LpfPthreadInit * >( _init );

    if (init)
        return lpf::ThreadLocalData::hook( 
            &*init->state, init->pid, init->nprocs, spmd, args
            );
    return LPF_ERR_FATAL;
}

lpf_err_t lpf_rehook(
        lpf_t ctx,
        lpf_spmd_t spmd,
        lpf_args_t args)
{
    lpf_init_t init = NULL;
    lpf::ThreadLocalData * tld = realCtx(ctx);

    lpf_err_t rc 
        = lpf_pthread_initialize( tld->getPid(), tld->getNprocs(), &init);

    if ( LPF_SUCCESS == rc) {
        rc = lpf_hook(init, spmd, args);

        if (LPF_SUCCESS == rc ) {
            rc = lpf_pthread_finalize( init );
        }
    }

    return rc;
}


lpf_err_t lpf_exec(
    lpf_t ctx,
    lpf_pid_t P, 
    lpf_spmd_t spmd,
    lpf_args_t args
)
{
    lpf::ThreadLocalData * t = realCtx(ctx);
    lpf_err_t status = LPF_ERR_FATAL;
    if (!t->isAborted())
        status = t->exec( P, spmd, args );
    return status;
}


lpf_err_t lpf_register_global(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
)
{
    (void) size; // ignore parameter size, since the information is not required on shared memory.
    lpf::ThreadLocalData * thread = realCtx(ctx);
    
    if (!thread->isAborted())
       *memslot = thread ->registerGlobal(pointer, size);

    return LPF_SUCCESS;
}

lpf_err_t lpf_register_local(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
)
{
    (void) size; // ignore parameter size, since the information is not required on shared memory.
    lpf::ThreadLocalData * thread = realCtx(ctx);
    if (!thread->isAborted())
        *memslot = thread ->registerLocal(pointer, size);

    return LPF_SUCCESS;
}

lpf_err_t lpf_deregister(
    lpf_t ctx,
    lpf_memslot_t memslot
)
{
    lpf::ThreadLocalData * thread = realCtx(ctx);
    if (!thread->isAborted())
        thread->deregister(memslot);
    return LPF_SUCCESS;
}

lpf_err_t lpf_put(
    lpf_t ctx,
    lpf_memslot_t src_slot, 
    size_t src_offset,
    lpf_pid_t dst_pid, 
    lpf_memslot_t dst_slot, 
    size_t dst_offset, 
    size_t size, 
    lpf_msg_attr_t attr
)
{
    (void) attr; // ignore parameter 'msg' since this implementation only 
                 // implements core functionality
    lpf::ThreadLocalData * thread = realCtx(ctx);

    if (!thread->isAborted())
        thread->put( src_slot, src_offset, dst_pid, dst_slot, dst_offset, size);

    return LPF_SUCCESS;
}


lpf_err_t lpf_get(
    lpf_t ctx, 
    lpf_pid_t pid, 
    lpf_memslot_t src, 
    size_t src_offset, 
    lpf_memslot_t dst, 
    lpf_memslot_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
)
{
    (void) attr; // ignore parameter 'msg' since this implementation only 
                 // implements core functionality
    lpf::ThreadLocalData * thread = realCtx(ctx);

    if (!thread->isAborted())
        thread->get( pid, src, src_offset, dst, dst_offset, size );

    return LPF_SUCCESS;
}

lpf_err_t lpf_sync( lpf_t ctx, lpf_sync_attr_t attr )
{
    (void) attr; // ignore attr parameter since this implementation only
                 // implements core functionality
    return realCtx(ctx)->sync();
}

namespace {
    double messageGap( lpf_pid_t p, 
            size_t min_msg_size, 
            lpf_sync_attr_t attr 
            ) 
    {
        (void)p;
        (void)attr;

        return getMachineParams().getMessageGap( min_msg_size );
    }

    double latency( lpf_pid_t p, size_t min_msg_size, lpf_sync_attr_t attr ) {
        (void)p;
        (void)attr;
        return getMachineParams().getLatency( min_msg_size );
    }
}

lpf_err_t lpf_probe( lpf_t ctx, lpf_machine_t * params )
{
    lpf::ThreadLocalData * t = realCtx(ctx);
    
    if (!t->isAborted()) {
        params->p = t->getNoAllProcs();
        params->free_p = t->getNoSubProcs();
        params->g = &messageGap;
        params->l = &latency;
    }
    return LPF_SUCCESS ;
}

lpf_err_t lpf_resize_message_queue( lpf_t ctx, size_t max_msgs )
{
    lpf::ThreadLocalData * t = realCtx(ctx);
    if (t->isAborted())
        return LPF_SUCCESS;
    return t->resizeMesgQueue(max_msgs);
}

lpf_err_t lpf_resize_memory_register( lpf_t ctx, size_t max_regs )
{
    lpf::ThreadLocalData * t = realCtx(ctx);
    if (t->isAborted())
        return LPF_SUCCESS;
    return t->resizeMemreg(max_regs);
}

lpf_err_t lpf_abort(lpf_t ctx) {
    (void) ctx;
    // Using std::abort is not portable
    // SIGABRT code 6 is often coverted to code 134.
    // Therefore, use std::quick_exit(6) instead
    // The reason we do not use std::exit is that
    // it implies calling destructors, and this leads to 
    // segmentation faults for pthread backend and abnormal
    // programs. std::quick_exit does not call destructors
    std::quick_exit(6);
    return LPF_SUCCESS;
}

