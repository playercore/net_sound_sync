#include "send_filter.h"
#include "chromium/base/at_exit.h"

CSendDataFilter::CSendDataFilter(IUnknown* unk, HRESULT* hr)
    : CBaseFilter(L"new send filter", unk, getLock(), CLSID_NetSendFilter, hr)
    , m_sendPin(this, getLock(), hr, L"send pin")
{

}

int CSendDataFilter::GetPinCount()
{
    return 1;
}

CBasePin* CSendDataFilter::GetPin(int n)
{
    if (n != 0)
        return NULL;

    return &m_sendPin;
}

CUnknown* __stdcall CSendDataFilter::CreateInstance(IUnknown* unk, HRESULT* hr)
{
    CUnknown* unknown = new CSendDataFilter(unk, hr);
    if (SUCCEEDED(*hr))
        return unknown;

    delete unknown;
    return NULL;
}

HRESULT CSendDataFilter::JoinFilterGraph(IFilterGraph* graph, wchar_t* name)
{
    int i = 0;
    i++;
    return CBaseFilter::JoinFilterGraph(graph, name);
}

const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
    &MEDIATYPE_Audio,           // Major type
    &MEDIASUBTYPE_NULL          // Minor type
};


const AMOVIESETUP_PIN sudPins  =
{
    L"Input",                   // Pin string name
    FALSE,                      // Is it rendered
    FALSE,                      // Is it an output
    FALSE,                      // Allowed zero pins
    FALSE,                      // Allowed many
    &CLSID_NULL,                // Connects to filter
    L"Output",                  // Connects to pin
    1,                          // Number of pins types
    &sudPinTypes 
} ;            // Pin information


const AMOVIESETUP_FILTER sudFilter =
{
    &CLSID_NetSendFilter,       // Filter CLSID
    L"send data filter",        // String name
    MERIT_DO_NOT_USE,           // Filter merit
    1,                          // Number pins
    &sudPins                    // Pin details
};


// List of class IDs and creator functions for class factory

CFactoryTemplate g_Templates []  = {
    { L"send data filter"
    , &CLSID_NetSendFilter
    , CSendDataFilter::CreateInstance
    , NULL
    , &sudFilter}
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);

} 

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);

} 

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
    DWORD  dwReason, 
    LPVOID lpReserved)
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

