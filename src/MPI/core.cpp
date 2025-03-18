
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
#include <lpf/mpi.h>
#include <lpf/abort.h>

#include <vector>
#include <limits>
#include <cstdlib>
#include <climits>
#include <cstdint>
#include <iostream>

#include "mpilib.hpp"
#include "interface.hpp"
#include "memorytable.hpp"
#include "process.hpp"
#include "config.hpp"
#include "log.hpp"
#include "dynamichook.hpp"
#include "time.hpp"

#include <mpi.h>

// the value 2 in this implementation indicates support for lpf_abort in a way
// that may deviate from the stdlib abort()
const int LPF_HAS_ABORT = 2;

// Error codes. 
// Note: Some code (e.g. in process::broadcastSymbol) depends on the 
// fact that numbers are assigned in order of severity, where 0 means
// no error and 3 means unrecoverable error. That way the severest error
// status can be replicated through Communication::allreduceMax
const lpf_err_t LPF_SUCCESS = 0;
const lpf_err_t LPF_ERR_OUT_OF_MEMORY = 1;
const lpf_err_t LPF_ERR_FATAL = 2;

const lpf_tag_t LPF_INVALID_TAG = std::numeric_limits< uint32_t >::max();

const lpf_args_t LPF_NO_ARGS = { NULL, 0, NULL, 0, NULL, 0 };

const lpf_sync_attr_t LPF_SYNC_DEFAULT = NULL;

const lpf_msg_attr_t LPF_MSG_DEFAULT = LPF_INVALID_TAG;

const lpf_pid_t LPF_MAX_P = UINT_MAX;

const lpf_memslot_t LPF_INVALID_MEMSLOT = SIZE_MAX;

const lpf_t LPF_NONE = NULL;

const lpf_init_t LPF_INIT_NONE = NULL;

extern "C" const int LPF_MPI_AUTO_INITIALIZE __attribute__((weak)) = 1;

const lpf_t LPF_ROOT = static_cast<void*>(const_cast<char *>("LPF_ROOT")) ; 

const lpf_machine_t LPF_INVALID_MACHINE = { 0, 0, NULL, NULL };

namespace {
    lpf::Interface * realContext( lpf_t ctx )
    { 
        if  ( LPF_ROOT == ctx )
            return lpf::Interface::root();
        else
            return static_cast< lpf::Interface *>(ctx);
    }
}

// MPI extension

lpf_err_t lpf_mpi_initialize_with_mpicomm( MPI_Comm comm, lpf_init_t * init)
{
    lpf_err_t status = LPF_SUCCESS;

    // FIXME: cannot handle out-of-memory situations
    * reinterpret_cast<lpf::mpi::Comm ** >(init)
        = new lpf::mpi::Comm(&comm);

    return status;
}

lpf_err_t lpf_mpi_initialize_over_tcp( 
        const char * server, const char * port, int timeout,
        lpf_pid_t pid, lpf_pid_t nprocs, 
        lpf_init_t * init )
{
    try {
        // make sure MPI_Init is initialized
        (void) lpf::mpi::Lib::instance();

        // Create an MPI communicator
        MPI_Comm comm = lpf::mpi::dynamicHook(
                server, port, pid, nprocs, 
                lpf::Time::fromSeconds( timeout / 1000.0) );

        // wrap it
        lpf::mpi::Comm * wcomm = new lpf::mpi::Comm(&comm);
        // free the original.
        int rc = MPI_Comm_free(&comm);
        if ( MPI_SUCCESS != rc ) {
            LOG(1, "Unable to free MPI communicator");
            return LPF_ERR_FATAL;
        }

        // Measure machine params / do a probe
        lpf::Interface :: doProbe( *wcomm );

        // store a copy in 'init'
        * reinterpret_cast<lpf::mpi::Comm ** >(init) = wcomm;

        return LPF_SUCCESS;
    }
    catch( std::bad_alloc & e )
    {
        LOG(1, "Cannot initialize lpf_init_t, due to lack of memory; " << e.what() );
        return LPF_ERR_OUT_OF_MEMORY;
    }
    catch( std::exception & e )
    {
        lpf::mpi::Lib::instance().setFatalMode();
        LOG(1, "Cannot initialize lpf_init_t, because " << e.what() );
        return LPF_ERR_FATAL;
    }
    catch(...)
    {
        lpf::mpi::Lib::instance().setFatalMode();
        LOG(1, "Cannot initialize lpf_init_t, because of an unknown exception");
        return LPF_ERR_FATAL;
    }
}

lpf_err_t lpf_mpi_finalize( lpf_init_t context ) {
 
    lpf_err_t status = LPF_SUCCESS;

    delete static_cast< lpf::mpi::Comm *>(context);

    return status;
}

// tags extension

lpf_err_t lpf_tag_create_mattr(
    lpf_t ctx,
    lpf_msg_attr_t * attr
)
{
    (void) ctx;
    *attr = LPF_MSG_DEFAULT;
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_destroy_mattr(
    lpf_t ctx,
    lpf_msg_attr_t attr
)
{
    (void) ctx;
    (void) attr;
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_create_sattr(
    lpf_t ctx,
    lpf_sync_attr_t * attr
)
{
    lpf_err_t ret = LPF_SUCCESS;
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        try {
            ret = i->createNewSyncAttr(attr);
	} catch (const std::bad_alloc &) {
            LOG(2, "lpf_tag_create_sattr: out of memory (bad_alloc)");
            return LPF_ERR_OUT_OF_MEMORY;
	} catch (const std::exception &e) {
            LOG(1, "lpf_tag_create_sattr fatal error: " << e.what());
            return LPF_ERR_FATAL;
	}
    }
    return ret;
}

lpf_err_t lpf_tag_destroy_sattr(
    lpf_t ctx,
    lpf_sync_attr_t attr
)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        i->destroySyncAttr(attr);
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_get_mattr(
    lpf_t ctx,
    lpf_msg_attr_t attr,
    lpf_tag_t * tag
)
{
    (void) ctx;
    ASSERT( tag != NULL );
    *tag = attr;
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_get_sattr(
    lpf_t ctx,
    lpf_sync_attr_t attr,
    lpf_tag_t * tag
)
{
    ASSERT( tag != NULL );
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        *tag = i->getTagFromSyncAttr(attr);
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_set_sattr(
    lpf_t ctx,
    lpf_tag_t tag,
    lpf_sync_attr_t attr
)
{
    ASSERT( attr != NULL );
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        i->setTagInSyncAttr(tag,attr);
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_set_mattr(
    lpf_t ctx,
    lpf_tag_t tag,
    lpf_msg_attr_t * attr
)
{
    (void) ctx;
    ASSERT( attr != NULL );
    *attr = tag;
    return LPF_SUCCESS;
}

lpf_err_t lpf_zero_create_sattr(
    lpf_t ctx,
    lpf_sync_attr_t * attr
)
{
    return lpf_tag_create_sattr(ctx,attr);
}

lpf_err_t lpf_zero_destroy_sattr(
    lpf_t ctx,
    lpf_sync_attr_t attr
)
{
    return lpf_tag_destroy_sattr(ctx,attr);
}

// zero-cost extension

lpf_err_t lpf_zero_create_mattr(
    lpf_t ctx,
    lpf_msg_attr_t * attr
)
{
    return lpf_tag_create_mattr(ctx,attr);
}

lpf_err_t lpf_zero_destroy_mattr(
    lpf_t ctx,
    lpf_msg_attr_t attr
)
{
    return lpf_tag_destroy_mattr(ctx,attr);
}

lpf_err_t lpf_zero_set_expected(
    lpf_t ctx,
    size_t expected_sent, size_t expected_rcvd,
    lpf_sync_attr_t attr
)
{
    ASSERT( attr != NULL );
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        i->setZCAttr(expected_sent,expected_rcvd,attr);
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_zero_get_expected(
    lpf_t ctx,
    lpf_sync_attr_t attr,
    size_t * expected_sent, size_t * expected_rcvd
)
{
    ASSERT( attr != NULL );
    ASSERT( expected_sent != NULL );
    ASSERT( expected_rcvd != NULL );
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        i->getZCAttr(attr,*expected_sent,*expected_rcvd);
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_zero_get_status(
    lpf_t ctx, lpf_sync_attr_t attr,
    size_t * rcvd, size_t * sent
)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        i->getRcvdMsgCount(rcvd,attr);
	i->getSentMsgCount(sent,attr);
    }
    return LPF_SUCCESS;
}

// non-coherent extension

lpf_err_t lpf_noc_flush_sent( lpf_t ctx)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        i->flushSent();
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_noc_flush_received( lpf_t ctx)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        i->flushReceived();
    }
    return LPF_SUCCESS;
}

// core functionality

lpf_err_t lpf_hook(
    lpf_init_t _init,
    lpf_spmd_t spmd,
    lpf_args_t args
)
{
    lpf::mpi::Comm & init = * static_cast<lpf::mpi::Comm *>(_init);
    return lpf::Interface::hook( init, spmd, args );
}

lpf_err_t lpf_rehook(
    lpf_t ctx,
    lpf_spmd_t spmd,
    lpf_args_t args
)
{
    lpf::Interface * context = realContext(ctx);
    return context->rehook( spmd, args );
}

lpf_err_t lpf_exec(
    lpf_t ctx,
    lpf_pid_t P, 
    lpf_spmd_t spmd,
    lpf_args_t args
)
{
    lpf::Interface * i = realContext(ctx);
    if (i->isAborted())
        return LPF_ERR_FATAL;
    return i->exec( P, spmd, args );
}


lpf_err_t lpf_register_global(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted())
        *memslot = i->registerGlobal(pointer, size);
    return LPF_SUCCESS;
}

lpf_err_t lpf_register_local(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted())
        *memslot = i->registerLocal(pointer, size);
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_create(
    lpf_t ctx,
    bool active,
    lpf_tag_t * tag
)
{
    (void)active;
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        try {
            *tag = i->registerTag();
        } catch (const std::exception & e) {
            LOG(1, "lpf_tag_create fatal error: " << e.what());
            return LPF_ERR_FATAL;
        }
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_deregister(
    lpf_t ctx,
    lpf_memslot_t memslot
)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted())
        i->deregister(memslot);
    return LPF_SUCCESS;
}

lpf_err_t lpf_tag_destroy(
    lpf_t ctx,
    lpf_tag_t tag
)
{
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted()) {
        try {
            i->destroyTag(tag);
        } catch (const std::exception & e) {
            LOG(1, "lpf_tag_destroy fatal error: " << e.what());
            return LPF_ERR_FATAL;
        }
    }
    return LPF_SUCCESS;
}

lpf_err_t lpf_put( lpf_t ctx,
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
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted())
        i->put( src_slot, src_offset, dst_pid, dst_slot, dst_offset, size );
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
    lpf::Interface * i = realContext(ctx);
    if (!i->isAborted())
        i->get( pid, src, src_offset, dst, dst_offset, size );
    return LPF_SUCCESS;
}

lpf_err_t lpf_sync( lpf_t ctx, lpf_sync_attr_t attr )
{
    return realContext(ctx)->sync(attr);
}

lpf_err_t lpf_probe( lpf_t ctx, lpf_machine_t * params )
{
    lpf::Interface * i = realContext(ctx);

    if (!i->isAborted())
        i->probe(*params);

    return LPF_SUCCESS ;
}

lpf_err_t lpf_resize_memory_register( lpf_t ctx, size_t max_regs )
{
    lpf::Interface * i = realContext(ctx);
    if (i->isAborted())
        return LPF_SUCCESS;

    return i->resizeMemreg(max_regs);
}

lpf_err_t lpf_resize_message_queue( lpf_t ctx, size_t max_msgs )
{
    lpf::Interface * i = realContext(ctx);
    if (i->isAborted())
        return LPF_SUCCESS;

    return i->resizeMesgQueue(max_msgs);
}

lpf_err_t lpf_resize_tag_register(
    lpf_t ctx,
    size_t max_tags
)
{
    lpf::Interface * i = realContext(ctx);
    if (i->isAborted())
        return LPF_SUCCESS;

    try {
        return i->resizeTagRegister(max_tags);
    } catch (const std::exception & e) {
        LOG(1, "lpf_resize_tag_register fatal error: " << e.what());
	return LPF_ERR_FATAL;
    }
}

lpf_err_t lpf_abort( lpf_t ctx ) {
    (void) ctx;
    MPI_Abort(MPI_COMM_WORLD, 6);
    return LPF_SUCCESS;
}

