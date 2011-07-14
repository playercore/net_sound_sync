#ifndef _SEND_PIN_H_
#define _SEND_PIN_H_

#include "../common/net_sock.h"
#include "../common/listener.h"

#include <memory>
#include <boost/scoped_array.hpp>

#include "../chromium/base/thread.h"
#include "streams.h"

class CSendDataFilter;

class CSendPin : public CBaseInputPin, public ISocketCallback
{
public:
    static bool ImplementsThreadSafeReferenceCounting() { return true; }
    CSendPin(CSendDataFilter* ownerFilter, CCritSec* lock, HRESULT* hr, 
             wchar_t* pinName);
    ~CSendPin();

    HRESULT CheckMediaType(const CMediaType* mt);
    virtual HRESULT __stdcall Receive(IMediaSample* sample);
    virtual HRESULT __stdcall NotifyAllocator(IMemAllocator* allocator, 
                                              int readOnly);
    virtual HRESULT Active();
    virtual HRESULT Inactive();

private:
    const static int m_bufferSize = 1024 * 1024 * 40;
    CSendDataFilter* m_ownerFilter;         
    CListener m_listener;
    std::shared_ptr<CNetScok> m_sendSocket;
    base::Thread m_sendThread;
    boost::scoped_array<byte> m_buffer;
    bool m_isThreadStopped;
    WAVEFORMATEX m_connectWaveFormat;
    ALLOCATOR_PROPERTIES m_currentAllocatorProp;
    int m_remainSize;

    void OnAccept(std::shared_ptr<CNetScok> sock);
    void SendData();
};

#endif