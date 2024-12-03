#include "mesgqueueNoc.hpp"

#if defined LPF_CORE_MPI_USES_zero

namespace lpf 
{

    MessageQueueNoc :: MessageQueueNoc( Communication & comm ) :
        m_ibverbs( comm ),
        MessageQueue(comm)
    {}

memslot_t MessageQueueNoc :: addNocReg( void * mem, std::size_t size )
{

    return m_ibverbs.regNoc(mem, size);

}

void MessageQueueNoc :: removeNocReg( memslot_t slot )
{

    m_ibverbs.dereg(slot);

}

void MessageQueueNoc :: nocGet( pid_t srcPid, memslot_t srcSlot, size_t srcOffset,
        memslot_t dstSlot, size_t dstOffset, size_t size )
{

    m_ibverbs.get(srcPid,
            m_memreg.getVerbID( srcSlot),
            srcOffset,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size );
}

void MessageQueueNoc :: nocPut( memslot_t srcSlot, size_t srcOffset,
        pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size )
{
    m_ibverbs.put( m_memreg.getVerbID( srcSlot),
            srcOffset,
            dstPid,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size);
}

err_t MessageQueueNoc :: nocResizeMemreg( size_t nRegs )
{
    m_ibverbs.resizeMemreg(nRegs);
    return LPF_SUCCESS;
}

} // namespace lpf
  //
#endif
