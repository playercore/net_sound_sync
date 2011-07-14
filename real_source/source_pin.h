#ifndef _SOURCE_PIN_
#define _SOURCE_PIN_

#include "streams.h"

#include <string>
#include "boost/shared_array.hpp"
#include <boost/shared_ptr.hpp>

class CRealSourceFilter;
struct TReceiveFilterStateInfo;

class CSourcePin : public CSourceStream
{
public:
    CSourcePin(HRESULT* hr, CRealSourceFilter* sourceFilter, 
               boost::shared_array<char> buffer, 
               boost::shared_ptr<TReceiveFilterStateInfo> stateInfo);
    ~CSourcePin() {};
    virtual HRESULT FillBuffer(IMediaSample* sample);
    virtual HRESULT CheckMediaType(const CMediaType* mediaType);
    virtual HRESULT GetMediaType(int position, CMediaType* mediaType);
    virtual HRESULT DecideBufferSize(IMemAllocator* alloc, 
                                     ALLOCATOR_PROPERTIES* propInputRequest);
    virtual HRESULT CheckConnect(IPin* pin);

private:
    WAVEFORMATEX m_sourceWaveFormat;
    ALLOCATOR_PROPERTIES m_sourceAllocProperties;
    int m_maxSampleSize;
    boost::shared_array<char> m_buffer;
    boost::shared_ptr<TReceiveFilterStateInfo> m_stateInfo;
    CRealSourceFilter* m_ownerFilter;
};

#endif