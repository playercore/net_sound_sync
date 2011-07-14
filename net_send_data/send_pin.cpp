#include "send_pin.h"
#include "types.h"
#include "send_filter.h"
#include <fstream>

CSendPin::CSendPin(CSendDataFilter* ownerFilter, CCritSec* lock, HRESULT* hr, 
                   wchar_t* pinName)
    : CBaseInputPin(NAME("send pin"), ownerFilter, lock, hr, pinName)
    , m_ownerFilter(ownerFilter)
    , m_listener(this)
    , m_sendThread("send thread")
    , m_buffer(new byte[m_bufferSize])
    , m_sendSocket(NULL)
    , m_isThreadStopped(false)
    , m_currentAllocatorProp()
    , m_connectWaveFormat()
    , m_remainSize(m_bufferSize)
{
    m_listener.Init(23232);
}

CSendPin::~CSendPin()
{
    m_listener.StopListen();
}

HRESULT CSendPin::CheckMediaType(const CMediaType* mt)
{
    if (!mt)
        return E_POINTER;

    WAVEFORMATEX* wfe = reinterpret_cast<WAVEFORMATEX*>(mt->Format());
    if (wfe == NULL)
        return E_INVALIDARG;

    if (mt->majortype != MEDIATYPE_Audio)
        return E_INVALIDARG;

    if (mt->formattype != FORMAT_WaveFormatEx)
        return E_INVALIDARG;

    if (wfe->wFormatTag != WAVE_FORMAT_PCM)
        return E_INVALIDARG;

    return S_OK;
}

HRESULT __stdcall CSendPin::Receive(IMediaSample* sample)
{
    CAutoLock lock(m_pLock);
    if (m_remainSize - static_cast<int>(sizeof(TDataPacket)) < 
        sample->GetActualDataLength())
        return S_OK;

    byte* p = m_buffer.get() + m_bufferSize - m_remainSize;
    TDataPacket* packet = reinterpret_cast<TDataPacket*>(p);
    packet->size = sample->GetActualDataLength();
    sample->GetTime(&packet->beginTime, &packet->endTime);
//     static int64 lastEndTime = 0;
//     packet->endTime = lastEndTime + packet->endTime - packet->beginTime;
//     packet->beginTime = lastEndTime;
//     lastEndTime = packet->endTime;

    unsigned char* p1 = NULL;
    sample->GetPointer(&p1);
    CopyMemory(p + sizeof(TDataPacket), p1, packet->size);
    m_remainSize -= (sizeof(TDataPacket) + packet->size);
    return CBaseInputPin::Receive(sample);
}

HRESULT __stdcall CSendPin::NotifyAllocator(IMemAllocator* allocator, 
                                            int readOnly)
{
    HRESULT hr = CBaseInputPin::NotifyAllocator(allocator, readOnly);
    if (FAILED(hr))
        return hr;

    IMemAllocator* alloc;
    hr = GetAllocator(&alloc);
    if (FAILED(hr))
        return hr;

    ALLOCATOR_PROPERTIES props;
    hr = alloc->GetProperties(&props);
    alloc->Release();
    if (FAILED(hr))    
        return hr;   

    CopyMemory(&m_currentAllocatorProp, &props, sizeof(ALLOCATOR_PROPERTIES));
    CopyMemory(&m_connectWaveFormat, m_mt.pbFormat, sizeof(WAVEFORMATEX));
    return S_OK;
}

void CSendPin::OnAccept(std::shared_ptr<CNetScok> sock)
{
    m_sendSocket = sock;
    m_sendThread.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &CSendPin::SendData));
}

void CSendPin::SendData()
{
    if (m_isThreadStopped)
        return;

    int writeSize = 0;
    scoped_ptr<char> buf;
    {
        CAutoLock lock(m_pLock);
        static bool isFirstSendData = true;
        //没有数据需要发送，直接返回
        if ((m_remainSize >= m_bufferSize) && (!isFirstSendData))
        {
            Sleep(1000);
            m_sendThread.message_loop()->PostTask(
                FROM_HERE, NewRunnableMethod(this, &CSendPin::SendData));
            return;
        }

        if (isFirstSendData)
        {
            writeSize = sizeof(WAVEFORMATEX) + sizeof(ALLOCATOR_PROPERTIES);
            buf.reset(new char[writeSize]);
            CopyMemory(buf.get(), &m_connectWaveFormat, sizeof(WAVEFORMATEX));
            CopyMemory(buf.get() + sizeof(WAVEFORMATEX), &m_currentAllocatorProp, 
                sizeof(ALLOCATOR_PROPERTIES));
            isFirstSendData = false;
        }
        else
        {
            writeSize = m_bufferSize - m_remainSize;
            buf.reset(new char[writeSize]);
            CopyMemory(buf.get(), m_buffer.get(), writeSize);
            memset(m_buffer.get(), 0, m_bufferSize);
            m_remainSize = m_bufferSize;
        }
    }

    m_sendSocket->Write(&writeSize, 4);
    m_sendSocket->Write(buf.get(), writeSize);

    Sleep(300);
    m_sendThread.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &CSendPin::SendData));
}

HRESULT CSendPin::Active()
{
    if (!m_listener.AlreadListen())
        if (!m_listener.StartListen())
            return E_FAIL;

    m_isThreadStopped = false;
    m_sendThread.Start();
    return CBaseInputPin::Active();    
}

HRESULT CSendPin::Inactive()
{
    m_isThreadStopped = true;
    m_sendThread.Stop();
    return CBaseInputPin::Inactive();
}
