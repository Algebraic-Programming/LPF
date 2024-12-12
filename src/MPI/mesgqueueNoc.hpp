#pragma once

#include "lpf/core.h"
#include "mesgqueue.hpp"
#include "ibverbsNoc.hpp"

#ifdef  LPF_CORE_MPI_USES_zero
namespace lpf 
{
class _LPFLIB_LOCAL MessageQueueNoc : public MessageQueue
{

public:
    explicit MessageQueueNoc( Communication & comm );
    /*
     * begin NOC extension
     */
    memslot_t addNocReg( void * mem, std::size_t size );
    void      removeNocReg( memslot_t slot );
    void nocGet( pid_t srcPid, memslot_t srcSlot, size_t srcOffset,
            memslot_t dstSlot, size_t dstOffset, size_t size );
    void nocPut( memslot_t srcSlot, size_t srcOffset,
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size );
    err_t nocResizeMemreg( size_t nRegs );
    /*
     * end NOC extension
     */
//protected:
    //mpi::IBVerbsNoc m_ibverbs;
};

} // namespace lpf
#endif
