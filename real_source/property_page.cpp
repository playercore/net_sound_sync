#include "property_page.h"

#include <boost/scoped_array.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include "real_source_filter.h"
#include "resource.h"

using boost::intrusive_ptr;
using std::wstring;
using boost::lexical_cast;

CPropertyPage::CPropertyPage(IUnknown* unk)
    : CBasePropertyPage(L"real source", unk, IDD_SOCKET_CONFIGURE, NULL)
    , m_ownerFilter(NULL)
    , m_ipAddress(L"")
    , m_port(0)
    , m_refetchState("get filter state")
    , m_threadStopped(false)
{

}

CUnknown* CPropertyPage::CreateInstance(IUnknown* unk, HRESULT* hr)
{
    CUnknown* result = new CPropertyPage(unk);
    if (NULL == result)
        *hr = E_OUTOFMEMORY;

    return result;
}

HRESULT CPropertyPage::OnConnect(IUnknown* unknown)
{
    HRESULT hr = CBasePropertyPage::OnConnect(unknown);
    if (FAILED(hr))
        return hr;

    return unknown->QueryInterface(IID_IBaseFilter, 
                                   reinterpret_cast<void**>(&m_ownerFilter));    

}

HRESULT CPropertyPage::OnDisconnect()
{
    m_ownerFilter.reset();
    m_threadStopped = true;
    m_refetchState.Stop();
    return CBasePropertyPage::OnDisconnect();
}

HRESULT CPropertyPage::OnApplyChanges()
{
    if (!m_ownerFilter)
        return E_POINTER;

    intrusive_ptr<ISocketConnect> socketConnect;
    HRESULT hr = m_ownerFilter->QueryInterface(
        IID_ISocketConnect, reinterpret_cast<void**>(&socketConnect));
    if (FAILED(hr))
        return hr;

    hr = socketConnect->SetIPAddress(m_ipAddress.c_str());    
    if (FAILED(hr))
        return hr;

    hr = socketConnect->SetPort(m_port);
    if (FAILED(hr))
        return hr;

    KConnectState state;
    socketConnect->GetState(&state);
    if (SOCKET_DISCONNECTED == state)
    {
        hr = socketConnect->Connect();
        if (FAILED(hr))
            return hr;
    }
    return CBasePropertyPage::OnApplyChanges();
}

INT_PTR CPropertyPage::OnReceiveMessage(HWND handle, UINT message, 
                                        WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_COMMAND:
        {
            if ((LOWORD(wparam) == IDC_ADDRESS) && 
                (HIWORD(wparam) == EN_CHANGE))
            {
                wstring s;
                if (getEditText(IDC_ADDRESS, &s))
                {
                    m_ipAddress = s;
                    setDirty();
                }                
            }
            else if ((LOWORD(wparam) == IDC_PORT) && 
                     (HIWORD(wparam) == EN_CHANGE))
            {
                wstring s;
                if (getEditText(IDC_PORT, &s))
                {
                    m_port = lexical_cast<int, wstring>(s);
                    setDirty();
                }                
            }
        }
        default:
        {

        }
    }
    return CBasePropertyPage::OnReceiveMessage(handle, message, wparam, lparam);
}

void CPropertyPage::setDirty()
{
    m_bDirty = TRUE;
    if (m_pPageSite)
    {
        m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
    }
}

bool CPropertyPage::getEditText(const int editID, std::wstring* value)
{
    if (!value)
        return false;

    int len = SendDlgItemMessage(m_Dlg, editID, EM_LINELENGTH, 0, 0);
    scoped_array<wchar_t> v(new wchar_t[len + 1]);
    int p = reinterpret_cast<int>(const_cast<wchar_t*>(v.get()));
    SendDlgItemMessage(m_Dlg, editID, EM_GETLINE, 0, p);
    //假如没有结束符，赋值到字符串对象时候会一直解释到出现结束符为止
    v[len] = L'\0';
    *value = v.get();
    return true;
}

HRESULT CPropertyPage::OnActivate()
{
    if (!m_ownerFilter)
        return E_POINTER;

    intrusive_ptr<ISocketConnect> socketConnect;
    HRESULT hr = m_ownerFilter->QueryInterface(
        IID_ISocketConnect, reinterpret_cast<void**>(&socketConnect));
    if (FAILED(hr))
        return hr;

    wchar_t v[255];
    socketConnect->GetIPAddress(v);
    m_ipAddress = v;
    socketConnect->GetPort(&m_port);
    setViewControlText(IDC_ADDRESS, &m_ipAddress);
    setViewControlText(IDC_PORT, &lexical_cast<wstring, int>(m_port));
    
    m_refetchState.Start();
    m_refetchState.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &CPropertyPage::displayStateOnLoop));

    return CBasePropertyPage::OnActivate();
}

void CPropertyPage::displayFilterState(TReceiveFilterStateInfo* filterInfo)
{
    
    setViewControlText(
        IDC_CURRENT_BUFFER_PACKETS, 
        &lexical_cast<wstring, int>(filterInfo->currentBufferPackets));
    setViewControlText(
        IDC_CURRENT_BUFFER_SIZE, 
        &lexical_cast<wstring, int64>(filterInfo->currentBufferSize));
    setViewControlText(IDC_RECEIVE_PACKETS, 
                       &lexical_cast<wstring, int>(filterInfo->receivePackets));
    setViewControlText(IDC_RECEIVE_DATAS, 
                       &lexical_cast<wstring, int64>(filterInfo->receiveDatas));
    setViewControlText(IDC_SEND_PACKETS,
                       &lexical_cast<wstring, int>(filterInfo->sendPackets));
    setViewControlText(IDC_SEND_DATAS, 
                       &lexical_cast<wstring, int64>(filterInfo->sendDatas));
    setViewControlText(
        IDC_CURRENT_NEED_SEND_PACKETS,
        &lexical_cast<wstring, int>(filterInfo->currentNeedSendPackets));
    setViewControlText(
        IDC_CURRENT_NEED_SEND_BUFFER_SIZE,
        &lexical_cast<wstring, int64>(filterInfo->currentNeedSendBufferSize));
}

void CPropertyPage::setViewControlText(const int controlID, 
                                       const wstring* value)
{
    LPARAM lp = reinterpret_cast<LPARAM>(value->c_str());
    SendDlgItemMessage(m_Dlg, controlID, WM_SETTEXT, 0, lp);
}

HRESULT CPropertyPage::fetchFilterStateAndDisplay()
{
    intrusive_ptr<IRunTimeStateFetcher> stateFetcher;
    HRESULT hr = m_ownerFilter->QueryInterface(
        IID_IRunTimeStateFetcher, reinterpret_cast<void**>(&stateFetcher));
    if (FAILED(hr))
        return hr;
    TReceiveFilterStateInfo state;
    memset(&state, 0, sizeof(state));
    stateFetcher->GetCurrentBufferDataNumbers(&state.currentBufferSize);
    stateFetcher->GetReceivePackets(&state.receivePackets);
    stateFetcher->GetReceiveDataNumbers(&state.receiveDatas);
    stateFetcher->GetSendPackets(&state.sendPackets);
    stateFetcher->GetSendDataNumbers(&state.sendDatas);
    stateFetcher->GetCurrentBufferPackets(&state.currentBufferPackets);
    stateFetcher->GetCurrentNeedSendPackets(&state.currentNeedSendPackets);
    stateFetcher->GetCurrentNeedSendDataNumbers(
        &state.currentNeedSendBufferSize);
    displayFilterState(&state);
    return S_OK;
}

void CPropertyPage::displayStateOnLoop()
{
    if (m_threadStopped)
        return;

    fetchFilterStateAndDisplay();
    Sleep(1000);
    m_refetchState.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &CPropertyPage::displayStateOnLoop));
}
