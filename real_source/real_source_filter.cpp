#include "real_source_filter.h"

#include <boost/intrusive_ptr.hpp>

#include "types.h"
#include "chromium/base/at_exit.h"

#include "../common/debug_util.h"
#include "property_page.h"

using std::string;
using boost::intrusive_ptr;
// {FA1A2C01-D373-42B0-A3C7-BFD0139ABE91}
static const GUID CLSID_RealSource = 
{ 0xfa1a2c01, 0xd373, 0x42b0, { 0xa3, 0xc7, 0xbf, 0xd0, 0x13, 0x9a, 0xbe, 0x91 } };

// {431F19F8-B6DB-4F40-8854-FAB84EED7BF8}
static const GUID CLSID_SocketConfigure = 
{ 0x431f19f8, 0xb6db, 0x4f40, { 0x88, 0x54, 0xfa, 0xb8, 0x4e, 0xed, 0x7b, 0xf8 } };
 

const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
    &MEDIATYPE_Audio,           // Major type
    &MEDIASUBTYPE_NULL          // Minor type
};

const AMOVIESETUP_PIN sudPins  =
{
    L"source pin",              // Pin string name
    FALSE,                      // Is it rendered
    FALSE,                      // Is it an output
    FALSE,                      // Allowed zero pins
    TRUE,                       // Allowed many
    &CLSID_NULL,                // Connects to filter
    L"Output",                  // Connects to pin
    1,                          // Number of pins types
    &sudPinTypes } ;            // Pin information

const AMOVIESETUP_FILTER sudFilter =
{
    &CLSID_RealSource,          // Filter CLSID
    L"real source",             // String name
    MERIT_DO_NOT_USE,           // Filter merit
    1,                          // Number pins
    &sudPins                    // Pin details
};

CFactoryTemplate g_Templates []  = {
    {
        L"real source", 
        &CLSID_RealSource, 
        CRealSourceFilter::CreateInstance, 
        NULL, 
        &sudFilter
    },
    {
        L"real source property page",
        &CLSID_SocketConfigure,
        CPropertyPage::CreateInstance,
        NULL,
        NULL
    }
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


CUnknown* CRealSourceFilter::CreateInstance(IUnknown* unk, HRESULT* hr)
{
    CUnknown* result = new CRealSourceFilter(unk, hr);
    if (FAILED(*hr))
    {
        delete result;
        return NULL;
    }
    return result;
}

CRealSourceFilter::CRealSourceFilter(IUnknown* unk, HRESULT* hr)
    : CSource(L"real source filter", unk, CLSID_RealSource, hr)
    , m_buffer(new char[m_bufferSize])
    , m_receiveThread("receive thread")
    , m_sourcePin(hr, this, m_buffer, m_stateInfo)
    , m_socket("127.0.0.1", 23232, "", 0, PROXY_TYPE_UNKNOWN)
    , m_isReceiveThreadStopped(false)
    , m_ipAddress(L"127.0.0.1")
    , m_port(23232)
    , m_connectState(SOCKET_DISCONNECTED)
    , m_stateInfo(new TReceiveFilterStateInfo())
    , m_listenerThread("listener thread")
    , m_isListenerThreadStopped(false)
{
    memset(m_buffer.get(), 0, m_bufferSize);
    memset(m_stateInfo.get(), 0, sizeof(TReceiveFilterStateInfo));
}

CRealSourceFilter::~CRealSourceFilter()
{

}

void CRealSourceFilter::receiveNetData()
{
    if (m_isReceiveThreadStopped)
        return;

    int readBufferSize = 0;
    int realSize = m_socket.Read(&readBufferSize, 4);
    if (-1 == readBufferSize)
    {
        m_isReceiveThreadStopped = true;
        return;
    }
    const static int typeLen = sizeof(WAVEFORMATEX) + 
        sizeof(ALLOCATOR_PROPERTIES);
    scoped_ptr<char> buf(new char[readBufferSize]);
    realSize = m_socket.Read(buf.get(), readBufferSize);
    {
        CAutoLock lock(m_pLock);
        static bool isFirstReceiveData = true;
        if (isFirstReceiveData)
        {
            CopyMemory(m_buffer.get(), buf.get(), realSize); 
            *reinterpret_cast<bool*>(m_buffer.get() + typeLen) = true; 
            ++realSize;  //占用一个空位用来作为标识媒体类型格式是否已经收到
            isFirstReceiveData = false;
        }
        else
        {
            /*TRACE(L"realsize: %d\n", realSize);*/
            char* p = m_buffer.get() + m_stateInfo->currentBufferSize;
            CopyMemory(p, buf.get(), realSize);
            
            int size = 0;
            do {
                TDataPacket* p = reinterpret_cast<TDataPacket*>(
                    buf.get() + size);
                ++m_stateInfo->receivePackets;
                ++m_stateInfo->currentBufferPackets;
                TRACE(L"beginTime:%I64d, endTime:%I64d, size:%d \n", 
                    p->beginTime, p->endTime, p->size);
                size += (p->size + sizeof(TDataPacket));
                assert(p->size != 0);
            } while (size < realSize);
            
            m_stateInfo->currentNeedSendPackets = 
                m_stateInfo->currentBufferPackets;
            
        }
        m_stateInfo->receiveDatas += realSize;
        m_stateInfo->currentBufferSize += realSize;     
        m_stateInfo->currentNeedSendBufferSize = 
            m_stateInfo->currentBufferSize - typeLen - 1;
    }
    Sleep(100);
    m_receiveThread.message_loop()->PostTask(
        FROM_HERE, 
        NewRunnableMethod(this, &CRealSourceFilter::receiveNetData));  
}

HRESULT CRealSourceFilter::NonDelegatingQueryInterface(const GUID& iid, 
                                                       void** v)
{
    if (IID_ISpecifyPropertyPages == iid)
        return GetInterface(static_cast<ISpecifyPropertyPages*>(this), v);
    else if (IID_ISocketConnect == iid)
        return GetInterface(static_cast<ISocketConnect*>(this), v);
    else if (IID_IRunTimeStateFetcher == iid)
        return GetInterface(static_cast<IRunTimeStateFetcher*>(this), v);
    else return CSource::NonDelegatingQueryInterface(iid, v);
}

HRESULT CRealSourceFilter::GetPages(CAUUID* pages)
{
    if (!pages)
        return E_POINTER;

    pages->cElems = 1;
    pages->pElems = reinterpret_cast<GUID*>(CoTaskMemAlloc(sizeof(GUID)));
    pages->pElems[0] = CLSID_SocketConfigure;
    return S_OK;
}

HRESULT CRealSourceFilter::SetIPAddress(const wchar_t* ipAddress)
{
    if (!ipAddress)
        return S_FALSE;

    m_ipAddress = ipAddress;
    return S_OK;
}

HRESULT CRealSourceFilter::GetIPAddress(wchar_t* ipAddress)
{
    if (!ipAddress)
        return S_FALSE;

    CopyMemory(ipAddress, m_ipAddress.c_str(), m_ipAddress.size() * 2);
    ipAddress[m_ipAddress.size()] = L'\0';
    return S_OK;
}

HRESULT CRealSourceFilter::SetPort(const int port)
{
    m_port = port;
    return S_OK;
}

HRESULT CRealSourceFilter::GetPort(int* port)
{
    *port = m_port;
    return S_OK;
}

HRESULT CRealSourceFilter::Connect()
{
    if (SOCKET_CONNECTED == m_connectState)
        return S_OK;

    for (int retryTimes = 0; retryTimes < 5; ++retryTimes)
        if (m_socket.Connect(15))
        {
            m_connectState = SOCKET_CONNECTED;
            m_receiveThread.Start();
            m_receiveThread.message_loop()->PostTask(
                FROM_HERE, 
                NewRunnableMethod(this, &CRealSourceFilter::receiveNetData));
            return S_OK;
        }

    return E_FAIL;
}

HRESULT CRealSourceFilter::GetState(KConnectState* state)
{
    *state = m_connectState;
    return S_OK;
}

HRESULT CRealSourceFilter::GetReceivePackets(int* packets)
{
    *packets = m_stateInfo->receivePackets;
    return S_OK;
}

HRESULT CRealSourceFilter::GetReceiveDataNumbers(int64* numbers)
{
    *numbers = m_stateInfo->receiveDatas;
    return S_OK;
}

HRESULT CRealSourceFilter::GetSendPackets(int* packets)
{
    *packets = m_stateInfo->sendPackets;
    return S_OK;
}

HRESULT CRealSourceFilter::GetSendDataNumbers(int64* numbers)
{
    *numbers = m_stateInfo->sendDatas;
    return S_OK;
}

HRESULT CRealSourceFilter::GetCurrentBufferPackets(int* packets)
{
    *packets = m_stateInfo->currentBufferPackets;
    return S_OK;
}

HRESULT CRealSourceFilter::GetCurrentNeedSendPackets(int* packets)
{
    *packets = m_stateInfo->currentNeedSendPackets;
    return S_OK;
}

HRESULT CRealSourceFilter::GetCurrentBufferDataNumbers(int64* numbers)
{
    *numbers = m_stateInfo->currentBufferSize;
    return S_OK;
}

HRESULT CRealSourceFilter::GetCurrentNeedSendDataNumbers(int64* numbers)
{
    *numbers = m_stateInfo->currentNeedSendBufferSize;
    return S_OK;
}

bool CRealSourceFilter::StartListenerThread()
{
    if (!m_listenerThread.IsRunning())
    {
        m_isListenerThreadStopped = false;
        m_listenerThread.Start();
    }

    m_listenerThread.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, 
                                     &CRealSourceFilter::checkBufferState));
    return true;
}

void CRealSourceFilter::checkBufferState()
{
    if (m_isListenerThreadStopped)
        return ;

    intrusive_ptr<IMediaControl> mediaControl;
    HRESULT hr = m_pGraph->QueryInterface(
        IID_IMediaControl, reinterpret_cast<void**>(&mediaControl));
    if (FAILED(hr))
        return ;

    CAutoLock lock(m_pLock);
    OAFilterState filterState;
    mediaControl->GetState(INFINITE , &filterState);
    if ((m_stateInfo->currentNeedSendPackets > 0) && 
        (m_stateInfo->currentBufferPackets < 200))
    {
        if (State_Paused != filterState)
            mediaControl->Pause();

        Sleep(1000);
        m_listenerThread.message_loop()->PostTask(
            FROM_HERE, 
            NewRunnableMethod(this, &CRealSourceFilter::checkBufferState));
    }
    else if (m_stateInfo->currentBufferPackets > 200)
    {
        intrusive_ptr<IReferenceClock> referenceClock;
        HRESULT hr = m_pGraph->QueryInterface(
            IID_IReferenceClock, reinterpret_cast<void**>(&referenceClock));
        if (SUCCEEDED(hr))
        {
            int64 refTime = 0;
            referenceClock->GetTime(&refTime);
            char* p = m_buffer.get() + sizeof(WAVEFORMATEX) + 
                sizeof(ALLOCATOR_PROPERTIES) + 1;
            TDataPacket* packet = reinterpret_cast<TDataPacket*>(p);
            int chuckDataSize = 0;
            int chuckDataNumber = 0;
            while (packet->beginTime < refTime)
            {
                chuckDataSize += sizeof(TDataPacket) + packet->size;
                char* ptr = reinterpret_cast<char*>(packet);
                ptr += sizeof(TDataPacket) + packet->size;
                packet = reinterpret_cast<TDataPacket*>(ptr);
                ++chuckDataNumber;
            }
            if (chuckDataSize > 0)
            {
                int copySize = 1024 * 1024 * 40 - chuckDataSize - 
                    (sizeof(WAVEFORMATEX) + sizeof(ALLOCATOR_PROPERTIES) + 1);
                CopyMemory(p, p + chuckDataSize, copySize);
                //TODO:增加丢弃的数据包和数据大小
                m_stateInfo->currentBufferPackets -= chuckDataNumber;
                m_stateInfo->currentBufferSize -= chuckDataSize;
                Sleep(1000);
                m_listenerThread.message_loop()->PostTask(
                    FROM_HERE, 
                    NewRunnableMethod(this, 
                                      &CRealSourceFilter::checkBufferState));
            }           
            else
            {
                mediaControl->Run();
            }
        }
    }
    else
    {        
        Sleep(1000);
        m_listenerThread.message_loop()->PostTask(
            FROM_HERE, 
            NewRunnableMethod(this, &CRealSourceFilter::checkBufferState));
    }
        
}

HRESULT CRealSourceFilter::Run(int64 start)
{
    if (m_listenerThread.IsRunning())
    {
        m_isListenerThreadStopped = true;
        m_listenerThread.Stop();
    }

    return CSource::Run(start);
}

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);

}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);

} 

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
    static base::AtExitManager* atExit = NULL;
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        atExit = new base::AtExitManager;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        delete atExit;
        break;
    }
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

