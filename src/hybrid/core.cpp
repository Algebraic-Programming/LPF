
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

#include "dispatch.hpp"
#include "state.hpp"
#include "nodememreg.hpp"
#include "log.hpp"
#include "machineparams.hpp"
#include "linkage.hpp"

#include <limits>
#include <vector>
#include <cstring>
#include <cstdint>
#include <climits>

#if __cplusplus >= 201103L    
  #include <memory>
#else
  #include <tr1/memory>
#endif



extern "C" {

_LPFLIB_VAR const lpf_err_t LPF_SUCCESS = 0;
_LPFLIB_VAR const lpf_err_t LPF_ERR_OUT_OF_MEMORY = 1;
_LPFLIB_VAR const lpf_err_t LPF_ERR_FATAL = 2;

_LPFLIB_VAR const lpf_args_t LPF_NO_ARGS = { NULL, 0, NULL, 0, NULL, 0 };

_LPFLIB_VAR const lpf_sync_attr_t LPF_SYNC_DEFAULT = 0;

_LPFLIB_VAR const lpf_msg_attr_t LPF_MSG_DEFAULT = 0;

_LPFLIB_VAR const lpf_pid_t LPF_MAX_P = UINT_MAX;

_LPFLIB_VAR const lpf_memslot_t LPF_INVALID_MEMSLOT = SIZE_MAX;

_LPFLIB_VAR const lpf_t LPF_NONE = NULL;

_LPFLIB_VAR const lpf_init_t LPF_INIT_NONE = NULL;

_LPFLIB_VAR const lpf_t LPF_ROOT = static_cast<void*>(const_cast<char *>("LPF_ROOT")) ; 

_LPFLIB_VAR const lpf_machine_t LPF_INVALID_MACHINE = { 0, 0, NULL, NULL };

namespace {

    using lpf::hybrid::LPF_CORE_IMPL_CONFIG::MachineParams;

    struct Init {
    
        lpf::hybrid::Thread m_thread;
        lpf::hybrid::MPI    m_mpi;
        lpf_pid_t m_threadId, m_nThreads;
        lpf_pid_t m_nodeId, m_nNodes;
        MachineParams m_machineParams;
    };

    void hookProbe(lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
    {
        *static_cast<MachineParams *>(args.output)
            = MachineParams::hookProbe( ctx, pid, nprocs );
    }


    lpf::hybrid::ThreadState * realContext( lpf_t ctx )
    { 
        lpf_t c;
        if (ctx == LPF_ROOT)
            c = &lpf::hybrid::ThreadState::root(); 
        else
            c = ctx;
        return static_cast< lpf::hybrid::ThreadState *>(c);
    }
}

_LPFLIB_API lpf_err_t lpf_hybrid_intialize( USE_THREAD(_t) thread, USE_MPI(_t) mpi, 
        lpf_pid_t threadId, lpf_pid_t nThreads, 
        lpf_pid_t nodeId, lpf_pid_t nNodes, lpf_init_t * init )
{
    using namespace lpf::hybrid;
    Init obj = { Thread(thread, threadId, nThreads)
               , MPI(mpi, nodeId, nNodes)
               , threadId, nThreads, nodeId, nNodes
               , MachineParams()
               };

    try {
        Init * winit = new Init(obj);
        *reinterpret_cast<Init **>(init) = winit;

        lpf_args_t args;
        args.input = NULL;
        args.input_size = 0;
        args.output = &winit->m_machineParams;
        args.output_size = sizeof(winit->m_machineParams);
        args.f_symbols = NULL;
        args.f_size = 0;

        lpf_hook( *init, hookProbe, args );
    }
    catch(std::bad_alloc & e)
    {
        return LPF_ERR_OUT_OF_MEMORY;
    }
    return LPF_SUCCESS;
}

_LPFLIB_API lpf_err_t lpf_hybrid_finalize(lpf_init_t context )
{
    delete static_cast<Init *>(context);
    return LPF_SUCCESS;
}


_LPFLIB_API lpf_err_t lpf_hook( lpf_init_t init, lpf_spmd_t spmd, lpf_args_t args)
{
    using namespace lpf::hybrid;
    Init * params = static_cast<Init *>(init);

#if __cplusplus >= 201103L    
    std::shared_ptr<NodeState> nodeState;
#else
    std::tr1::shared_ptr<NodeState> nodeState;
#endif
        
    NodeState * nodeStatePtr = NULL;
    if (params->m_threadId == 0)
    {
        MPI::err_t nrc = MPI::SUCCESS;
        std::vector<lpf_pid_t> threadCounts;

        nrc = gather( params->m_mpi, 0, params->m_nThreads, threadCounts);
        if (nrc != MPI::SUCCESS) return LPF_ERR_FATAL;

        nrc = broadcast( params->m_mpi, 0, threadCounts );
        if (nrc != MPI::SUCCESS) return LPF_ERR_FATAL;

        nodeStatePtr = new NodeState( threadCounts, params->m_mpi );
        nodeState.reset( nodeStatePtr );

        NodeState::machineParams() = params->m_machineParams ;
    }

    Thread::err_t trc = broadcast( params->m_thread, 0, nodeStatePtr );
    if ( trc != Thread::SUCCESS ) return LPF_ERR_FATAL;

    bool failure = false;
    try {
        ThreadState threadState( nodeStatePtr, params->m_thread );
        spmd( &threadState, threadState.pid(), threadState.nprocs(), args );
    }
    catch(std::bad_alloc & e )
    {
        LOG(1, "Not enough memory to run SPMD function on thread " 
                << params->m_threadId << " of node " 
                << nodeStatePtr->nodeId() );
        failure = true;
    }
    catch(...)
    {
        LOG(1, "SPMD function of thread " 
                << params->m_threadId << " of node " 
                << nodeStatePtr->nodeId() << " threw an unexpected exception");
        failure = true;
    }

    trc = reduceOr( params->m_thread, 0, failure);
    if ( trc != Thread::SUCCESS ) return LPF_ERR_FATAL;

    if ( params->m_threadId == 0) 
    {
        MPI::err_t nrc = MPI::SUCCESS;
        nrc = reduceOr( params->m_mpi, 0, failure);
        if (nrc != MPI::SUCCESS) return LPF_ERR_FATAL;
        nrc = broadcast( params->m_mpi, 0, failure);
        if (nrc != MPI::SUCCESS) return LPF_ERR_FATAL;
    }
    trc = broadcast( params->m_thread, 0, failure );
    if ( trc != Thread::SUCCESS ) return LPF_ERR_FATAL;
    
    return failure?LPF_ERR_FATAL:LPF_SUCCESS;
}

_LPFLIB_API lpf_err_t lpf_rehook( lpf_t ctx, lpf_spmd_t spmd, lpf_args_t args)
{
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) {
        (*spmd)(ctx, 0, 1, args);
        return LPF_SUCCESS;
    }

    ThreadState * threadState = realContext(ctx);

    Init obj = { threadState->thread()
               , threadState->nodeState().mpi()
               , threadState->threadId()
               , threadState->nRealThreads()
               , threadState->nodeState().nodeId()
               , threadState->nodeState().nNodes()
               , NodeState::machineParams()
               };
    return lpf_hook( &obj, spmd, args );
}

_LPFLIB_API lpf_err_t lpf_exec( lpf_t ctx, lpf_pid_t P, lpf_spmd_t spmd, lpf_args_t args);


_LPFLIB_API lpf_err_t lpf_register_global(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
)
{
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) {
        char * null = NULL;
        *memslot = static_cast<char *>(pointer) - null;
        return LPF_SUCCESS;
    }

    ThreadState * t = realContext(ctx);
    if (!t->error())
        *memslot = t-> register_global(pointer, size);
    return LPF_SUCCESS;
}

_LPFLIB_API lpf_err_t lpf_register_local(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
)
{
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) {
        char * null = NULL;
        *memslot = static_cast<char *>(pointer) - null;
        return LPF_SUCCESS;
    }

    ThreadState * t = realContext(ctx);
    if (!t->error())
        *memslot = t->register_local(pointer, size);
    return LPF_SUCCESS;
}

_LPFLIB_API lpf_err_t lpf_deregister(
    lpf_t ctx,
    lpf_memslot_t memslot
)
{
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) return LPF_SUCCESS;

    ThreadState * t = realContext(ctx);
    if (!t->error())
        t->deregister(memslot);
    return LPF_SUCCESS;
}

_LPFLIB_API lpf_err_t lpf_put( lpf_t ctx,
                       lpf_memslot_t src_slot, 
                       size_t src_offset,
                       lpf_pid_t dst_pid, 
                       lpf_memslot_t dst_slot, 
                       size_t dst_offset, 
                       size_t size, 
                       lpf_msg_attr_t attr
)
{
    (void) attr;
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) {
        char * null = NULL;
        char * src_ptr = null + src_slot;
        char * dst_ptr = null + dst_slot;
        std::memcpy( dst_ptr + dst_offset, src_ptr + src_offset, size );
        return LPF_SUCCESS;
    }

    ThreadState * t = realContext(ctx);
    if (!t->error())
        t->put( src_slot, src_offset, dst_pid, dst_slot, dst_offset, size );
    return LPF_SUCCESS;
}


_LPFLIB_API lpf_err_t lpf_get(
    lpf_t ctx, 
    lpf_pid_t src_pid, 
    lpf_memslot_t src_slot, 
    size_t src_offset, 
    lpf_memslot_t dst_slot, 
    lpf_memslot_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
)
{
    (void) attr;
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) {
        char * null = NULL;
        char * src_ptr = null + src_slot;
        char * dst_ptr = null + dst_slot;
        std::memcpy( dst_ptr + dst_offset, src_ptr + src_offset, size );
        return LPF_SUCCESS;
    }

    ThreadState * t = realContext(ctx);
    if (!t->error())
        t->get( src_pid, src_slot, src_offset, dst_slot, dst_offset, size );
    return LPF_SUCCESS;
}

_LPFLIB_API lpf_err_t lpf_sync( lpf_t ctx, lpf_sync_attr_t attr )
{
    (void) attr;
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) 
        return LPF_SUCCESS;
    return realContext(ctx)->sync();
}


_LPFLIB_API lpf_err_t lpf_probe( lpf_t ctx, lpf_machine_t * params )
{
    using namespace lpf::hybrid;

    if (ctx == LPF_ROOT && !NodeState::machineParams().isSet())
        NodeState::machineParams() = MachineParams::execProbe( ctx, LPF_MAX_P);

    if ( ctx == LPF_SINGLE_PROCESS || !realContext(ctx)->error() ) {
        params->p = NodeState::machineParams().getTotalProcs();
        params->free_p = ctx == LPF_ROOT ? params->p : 1;
        params->g = &NodeState::messageGap;
        params->l = &NodeState::latency;
    }

    return LPF_SUCCESS ;
}

_LPFLIB_API lpf_err_t lpf_resize_message_queue( lpf_t ctx, size_t max_msgs )
{
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) 
       return LPF_SUCCESS;

    ThreadState * t = realContext(ctx);
    if (!t->error())
        return t->resizeMesgQueue( max_msgs );
    else
        return LPF_SUCCESS;
}

_LPFLIB_API lpf_err_t lpf_resize_memory_register( lpf_t ctx, size_t max_regs )
{
    using namespace lpf::hybrid;
    if (ctx == LPF_SINGLE_PROCESS) 
       return LPF_SUCCESS;

    ThreadState * t = realContext(ctx);
    if (!t->error())
        return t->resizeMemreg( max_regs);
    else
        return LPF_SUCCESS;
}

} // extern "C"
