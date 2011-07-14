#ifndef _REAL_SOURCE_FILTER_
#define _REAL_SOURCE_FILTER_

#include "../common/net_sock.h"
#include "streams.h"

#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>

#include "source_pin.h"
#include "chromium/base/thread.h"
#include "chromium/base/basictypes.h"

// {498879A9-20DF-44A2-BF00-33E5E4B82F30}
static const GUID IID_ISocketConnect = 
{0x498879a9, 0x20df, 0x44a2, 0xbf, 0x0, 0x33, 0xe5, 0xe4, 0xb8, 0x2f, 0x30};

// {E7245BF3-EB05-4E9A-8965-D1621460E94C}
static const GUID IID_IRunTimeStateFetcher = 
{0xe7245bf3, 0xeb05, 0x4e9a, 0x89, 0x65, 0xd1, 0x62, 0x14, 0x60, 0xe9, 0x4c};


typedef enum ConnectState
{ 
    SOCKET_CONNECTED, 
    SOCKET_DISCONNECTED 
} KConnectState;

struct TReceiveFilterStateInfo
{
    int receivePackets;             
    int currentBufferPackets;
    int64 receiveDatas;       
    int64 currentBufferSize;
    int sendPackets;
    int64 sendDatas;
    int currentNeedSendPackets;
    int64 currentNeedSendBufferSize;
} ;

MIDL_INTERFACE("498879A9-20DF-44A2-BF00-33E5E4B82F30") ISocketConnect 
    : public IUnknown
{
    virtual HRESULT __stdcall SetIPAddress(const wchar_t* ipAddress);
    virtual HRESULT __stdcall GetIPAddress(wchar_t* ipAddress);
    virtual HRESULT __stdcall SetPort(const int port);
    virtual HRESULT __stdcall GetPort(int* port);
    virtual HRESULT __stdcall Connect();
    virtual HRESULT __stdcall GetState(KConnectState* state);
};

MIDL_INTERFACE("E7245BF3-EB05-4E9A-8965-D1621460E94C") IRunTimeStateFetcher
    : public IUnknown
{
    virtual HRESULT __stdcall GetReceivePackets(int* packets);
    virtual HRESULT __stdcall GetReceiveDataNumbers(int64* numbers);
    virtual HRESULT __stdcall GetSendPackets(int* packets);
    virtual HRESULT __stdcall GetSendDataNumbers(int64* numbers);
    virtual HRESULT __stdcall GetCurrentBufferPackets(int* packets);
    virtual HRESULT __stdcall GetCurrentBufferDataNumbers(int64* numbers);
    virtual HRESULT __stdcall GetCurrentNeedSendPackets(int* packets);
    virtual HRESULT __stdcall GetCurrentNeedSendDataNumbers(int64* numbers);
};

class CRealSourceFilter : public CSource , public ISocketConnect , 
                          public IRunTimeStateFetcher, 
                          public ISpecifyPropertyPages
{
public:
    static bool ImplementsThreadSafeReferenceCounting() { return true; }
    DECLARE_IUNKNOWN
    
    CRealSourceFilter(IUnknown* unk, HRESULT* hr);
    ~CRealSourceFilter();
    static CUnknown* __stdcall CreateInstance(IUnknown* unk, HRESULT* hr);
    virtual HRESULT __stdcall NonDelegatingQueryInterface(const GUID& iid, 
                                                          void** v);
    //ISocketConnect interface
    virtual HRESULT __stdcall SetIPAddress(const wchar_t* ipAddress);
    virtual HRESULT __stdcall GetIPAddress(wchar_t* ipAddress);
    virtual HRESULT __stdcall SetPort(const int port);
    virtual HRESULT __stdcall GetPort(int* port);
    virtual HRESULT __stdcall Connect();
    virtual HRESULT __stdcall GetState(KConnectState* state);

    //IRunTimeStateFetcher interface
    virtual HRESULT __stdcall GetReceivePackets(int* packets);
    virtual HRESULT __stdcall GetReceiveDataNumbers(int64* numbers);
    virtual HRESULT __stdcall GetSendPackets(int* packets);
    virtual HRESULT __stdcall GetSendDataNumbers(int64* numbers);
    virtual HRESULT __stdcall GetCurrentBufferPackets(int* packets);
    virtual HRESULT __stdcall GetCurrentBufferDataNumbers(int64* numbers);
    virtual HRESULT __stdcall GetCurrentNeedSendPackets(int* packets);
    virtual HRESULT __stdcall GetCurrentNeedSendDataNumbers(int64* numbers);

    //ISpecifyPropertyPages interface
    virtual HRESULT __stdcall GetPages(CAUUID* pages);

    bool StartListenerThread();

private:
    const static int m_bufferSize = 1024 * 1024 * 40;
    bool m_isReceiveThreadStopped;
    bool m_isListenerThreadStopped;
    boost::shared_array<char> m_buffer;
    boost::shared_ptr<TReceiveFilterStateInfo> m_stateInfo;
    base::Thread m_receiveThread;  
    base::Thread m_listenerThread;
    CNetScok m_socket;
    CSourcePin m_sourcePin;
    std::wstring m_ipAddress;
    int m_port;
    KConnectState m_connectState;    
    void receiveNetData();
    void checkBufferState();
};

#endif