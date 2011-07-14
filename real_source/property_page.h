#ifndef _PROPERTY_PAGE_
#define _PROPERTY_PAGE_

#include "streams.h"

#include <boost/intrusive_ptr.hpp>
#include "chromium/base/basictypes.h"
#include "chromium/base/thread.h"

#include "../common/intrusive_ptr_helper.h"
#include <string>

struct TReceiveFilterStateInfo;

class CPropertyPage: public CBasePropertyPage
{
public:
    static bool ImplementsThreadSafeReferenceCounting() { return true; }
    static CUnknown* __stdcall CreateInstance(IUnknown* unk, HRESULT* hr);
    CPropertyPage(IUnknown* unk);
    ~CPropertyPage(){}
    virtual HRESULT OnConnect(IUnknown* unknown);
    virtual HRESULT OnDisconnect();
    virtual HRESULT OnApplyChanges();
    virtual INT_PTR OnReceiveMessage(HWND handle, UINT message, WPARAM wparam, 
                                     LPARAM lparam);
    virtual HRESULT OnActivate();

private:

    boost::intrusive_ptr<IBaseFilter> m_ownerFilter;
    std::wstring m_ipAddress;
    int m_port;
    base::Thread m_refetchState;
    bool m_threadStopped;

    bool getEditText(const int editID, std::wstring* value);
    void displayFilterState(TReceiveFilterStateInfo* filterInfo);
    void setViewControlText(const int controlID, const std::wstring* value);
    void setDirty();
    HRESULT fetchFilterStateAndDisplay();
    void displayStateOnLoop();
};

#endif