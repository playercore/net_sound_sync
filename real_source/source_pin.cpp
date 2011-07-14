#include "source_pin.h"

#include "real_source_filter.h"
#include "../common/debug_util.h"
#include "../common/intrusive_ptr_helper.h"

#include "types.h"

#include <boost/intrusive_ptr.hpp>

#include "../chromium/base/basictypes.h"

using boost::shared_ptr;
using boost::shared_array;
using boost::intrusive_ptr;

CSourcePin::CSourcePin(HRESULT* hr, CRealSourceFilter* sourceFilter, 
                       shared_array<char> buffer,
                       shared_ptr<TReceiveFilterStateInfo> stateInfo)
    : CSourceStream(L"source pin", hr, sourceFilter, L"source pin")
    , m_sourceWaveFormat()
    , m_sourceAllocProperties()
    , m_maxSampleSize(0)
    , m_buffer(buffer)
    , m_stateInfo(stateInfo)
    , m_ownerFilter(sourceFilter)
    , m_nextSampleBeginTime(0)
{

}

HRESULT CSourcePin::FillBuffer(IMediaSample* sample)
{
    CAutoLock lock(m_pLock);  
    HRESULT hr = S_OK;
    if (!m_ownerFilter->IsListenerThreadStarted())
    {
        hr = fillNormalDataInSample(sample);
        if (S_FALSE == hr)
            m_ownerFilter->StartListenerThread();
    }
    else
    { 
        hr = fillZeroDataInSample(sample);            
    }
    return hr;
}

HRESULT CSourcePin::CheckMediaType(const CMediaType* mediaType)
{
    if ((MEDIATYPE_Audio == mediaType->majortype) && 
        (FORMAT_WaveFormatEx == mediaType->formattype))
        return S_OK;

    return S_FALSE;
}

HRESULT CSourcePin::GetMediaType(int position, CMediaType* mediaType)
{
    if (position < 0)
        return E_INVALIDARG;

    if (position > 0)
        return VFW_S_NO_MORE_ITEMS;

    mediaType->majortype = MEDIATYPE_Audio;
    mediaType->subtype = MEDIASUBTYPE_PCM;
    mediaType->formattype = FORMAT_WaveFormatEx;
    WAVEFORMATEX* wfe = reinterpret_cast<WAVEFORMATEX*>(
        mediaType->AllocFormatBuffer(sizeof(WAVEFORMATEX)));
    CopyMemory(wfe, &m_sourceWaveFormat, sizeof(WAVEFORMATEX));
    mediaType->lSampleSize = m_maxSampleSize;
    return S_OK;
}

HRESULT CSourcePin::DecideBufferSize(IMemAllocator* alloc, 
                                     ALLOCATOR_PROPERTIES* propInputRequest)
{
    CopyMemory(propInputRequest, &m_sourceAllocProperties, 
               sizeof(ALLOCATOR_PROPERTIES));
    ALLOCATOR_PROPERTIES actualProp;
    return alloc->SetProperties(propInputRequest, &actualProp);
}

HRESULT CSourcePin::CheckConnect(IPin* pin)
{
    {
        CAutoLock lock(m_pLock);
        char* p1 = reinterpret_cast<char*>(m_buffer.get());
        int typeLen = sizeof(WAVEFORMATEX) + sizeof(ALLOCATOR_PROPERTIES);
        bool haveConnectInfo = *reinterpret_cast<bool*>(p1 + typeLen);
        if (!haveConnectInfo)
            return E_FAIL;

        CopyMemory(&m_sourceWaveFormat, p1, sizeof(WAVEFORMATEX));
        p1 += sizeof(m_sourceWaveFormat);
        CopyMemory(&m_sourceAllocProperties, p1, sizeof(ALLOCATOR_PROPERTIES));
        p1 += sizeof(ALLOCATOR_PROPERTIES);
        m_maxSampleSize = m_sourceAllocProperties.cbBuffer * 2;
    }
//     m_sourceWaveFormat.wFormatTag = 0x0001;
//     m_sourceWaveFormat.nChannels = 0x0002;
//     m_sourceWaveFormat.nSamplesPerSec = 0x0000ac44;
//     m_sourceWaveFormat.nAvgBytesPerSec = 0x0002b110;
//     m_sourceWaveFormat.nBlockAlign = 0x0004;
//     m_sourceWaveFormat.wBitsPerSample = 0x0010;
//     m_sourceWaveFormat.cbSize = 0x0000;
//     m_sourceAllocProperties.cbAlign = 1;
//     m_sourceAllocProperties.cbBuffer = 115200;
//     m_sourceAllocProperties.cBuffers = 4;
//     m_sourceAllocProperties.cbPrefix = 0;
//     m_maxSampleSize = 4096;
    return CSourceStream::CheckConnect(pin);
}

HRESULT CSourcePin::fillNormalDataInSample(IMediaSample* sample)
{
    if (m_stateInfo->currentNeedSendPackets <= 0)
        return S_FALSE;

    unsigned char* sampleDataPointer = NULL;
    HRESULT hr = sample->GetPointer(&sampleDataPointer);
    if (FAILED(hr))
        return hr;

    char* dataBeginPtr = m_buffer.get() + sizeof(WAVEFORMATEX) + 
        sizeof(ALLOCATOR_PROPERTIES) + 1;
    char* p1 = dataBeginPtr;

    TDataPacket* dataPacket = reinterpret_cast<TDataPacket*>(p1);        
    p1 += sizeof(TDataPacket);
    CopyMemory(sampleDataPointer, p1, dataPacket->size);
    sample->SetTime(&dataPacket->beginTime, &dataPacket->endTime);
    sample->SetActualDataLength(dataPacket->size);
    sample->SetPreroll(false);
    sample->SetDiscontinuity(true);
    sample->SetSyncPoint(true);
    sample->SetMediaTime(NULL, NULL);
    TRACE(L"beginTime %I64d, endTime: %I64d, size: %d \n", 
        dataPacket->beginTime, dataPacket->endTime, dataPacket->size); 

    int packetSize = dataPacket->size + sizeof(TDataPacket);
    char* pp = dataBeginPtr;

    MoveMemory(pp, pp + packetSize, 1024 * 1024 * 40 - packetSize);

    ++m_stateInfo->sendPackets;
    m_stateInfo->sendDatas += dataPacket->size;
    --m_stateInfo->currentBufferPackets;
    m_stateInfo->currentBufferSize -= packetSize;
    m_stateInfo->currentNeedSendBufferSize = m_stateInfo->currentBufferSize;
    m_stateInfo->currentNeedSendPackets = m_stateInfo->currentBufferPackets;
    m_nextSampleBeginTime = dataPacket->endTime;
    return S_OK;
}

HRESULT CSourcePin::fillZeroDataInSample(IMediaSample* sample)
{
    uint8* p = NULL;
    HRESULT hr = sample->GetPointer(&p);
    if (FAILED(hr))
        return hr;

    memset(p, 0, m_sourceAllocProperties.cbBuffer);
    int64 durationTime = m_sourceAllocProperties.cbBuffer * UNITS / 
        (m_sourceWaveFormat.wBitsPerSample >> 3) / m_sourceWaveFormat.nChannels 
        / m_sourceWaveFormat.nSamplesPerSec;
    int64 endTime = m_nextSampleBeginTime + durationTime;
    sample->SetActualDataLength(m_sourceAllocProperties.cbBuffer);
    sample->SetTime(&m_nextSampleBeginTime, &endTime);
    sample->SetPreroll(false);
    sample->SetDiscontinuity(true);
    sample->SetSyncPoint(true);
    sample->SetMediaTime(NULL, NULL);
    m_nextSampleBeginTime = endTime;
    return S_OK;
}

HRESULT CSourcePin::Run()
{
    intrusive_ptr<IPin> inputPin;
    HRESULT hr = m_pInputPin->QueryInterface(
        IID_IPin, reinterpret_cast<void**>(&inputPin));

    if (FAILED(hr))
        return hr;

    hr = inputPin->BeginFlush();
    if (FAILED(hr))
        return hr;

    hr = inputPin->EndFlush();
    if (FAILED(hr))
        return hr;

    return CSourceStream::Run();
}
