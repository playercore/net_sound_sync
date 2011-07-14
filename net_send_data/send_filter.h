#ifndef _SEND_FILTER_H_
#define _SEND_FILTER_H_

#include "send_pin.h"
#include "streams.h"

#include "../common/dshow_util.h"

// { 35919F40-E904-11ce-8A03-00AA006ECB65 }
static const GUID CLSID_NetSendFilter = 
{0x35919f40, 0xe904, 0x11ce, 0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65};

class CSendDataFilter : public CBaseFilter, public CLockHolder
{
public:
    DECLARE_IUNKNOWN

    CSendDataFilter(IUnknown* unk, HRESULT* hr);
    ~CSendDataFilter() {};

    //CBaseFilter pure function
    int GetPinCount();
    CBasePin* GetPin(int n);

    // This goes in the factory template table to create new instances
    static CUnknown* __stdcall CreateInstance(IUnknown* unk, HRESULT* hr);
    virtual HRESULT __stdcall JoinFilterGraph(IFilterGraph* graph, 
                                              wchar_t* name);

private:
    CSendPin m_sendPin;
};

#endif