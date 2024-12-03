#include "mesgqueue.hpp"
#include "mesgqueueNoc.hpp"

namespace lpf 
{

    MessageQueueNoc :: MessageQueueNoc( Communication & comm ) :
        m_ibverbs( comm ),
        MessageQueue(comm)
    {}

memslot_t MessageQueueNoc :: addNocReg( void * mem, std::size_t size )
{

#if defined LPF_CORE_MPI_USES_zero
    return m_ibverbs.regNoc(mem, size);
#endif
    return LPF_INVALID_MEMSLOT;

}

void MessageQueueNoc :: removeNocReg( memslot_t slot )
{

#if defined LPF_CORE_MPI_USES_zero
    m_ibverbs.dereg(slot);
#endif

}

void MessageQueueNoc :: nocGet( pid_t srcPid, memslot_t srcSlot, size_t srcOffset,
        memslot_t dstSlot, size_t dstOffset, size_t size )
{

#if defined LPF_CORE_MPI_USES_zero
    m_ibverbs.getNoc(srcPid,
            m_memreg.getVerbID( srcSlot),
            srcOffset,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size );
#endif
}

void MessageQueueNoc :: nocPut( memslot_t srcSlot, size_t srcOffset,
        pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size )
{
#if defined LPF_CORE_MPI_USES_zero
    m_ibverbs.putNoc( m_memreg.getVerbID( srcSlot),
            srcOffset,
            dstPid,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size);
#endif
}

err_t MessageQueueNoc :: nocResizeMemreg( size_t nRegs )
{
#if defined LPF_CORE_MPI_USES_zero
    return m_ibverbs.resizeMemreg(nRegs);
#endif
}

} // namespace lpf
