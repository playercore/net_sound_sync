#include "Winsock2.h"
#include "listener.h"

#include "chromium/base/basictypes.h"


using std::shared_ptr;

CListener::CListener(ISocketCallback* callback)
    : m_listenSocket(0)
    , m_listenThread("listen thread")
    , m_isThreadStopped(false)
    , m_callback(callback)
    , m_referenceCount(1)
{

}

bool CListener::Init(int listenPort)
{
    if ((listenPort < 0) || (listenPort > 0xffff))
        return false;

    WSADATA data;
    uint16 version = MAKEWORD(2, 2);
    bool result = WSAStartup(version, &data) == 0;
    if (!result)
        return result;

    m_listenSocket = static_cast<int>(socket(AF_INET, SOCK_STREAM, 
                                             IPPROTO_TCP));
    if (INVALID_SOCKET == m_listenSocket) 
        return false;

    SOCKADDR_IN  addr;
    memset(&addr, 0, sizeof(SOCKADDR_IN));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(listenPort);
    result = bind(m_listenSocket, reinterpret_cast<struct sockaddr*>(&addr), 
                  sizeof(addr)) != SOCKET_ERROR;
    if (!result)
        return result;

    result = listen(m_listenSocket, SOMAXCONN) != SOCKET_ERROR;
    if (!result)
        return result;

    m_listenPort = listenPort;
    return true;
}

bool CListener::StartListen()
{
    m_listenThread.Start();
    m_listenThread.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &CListener::listenSocket));
    return true;
}

CListener::~CListener()
{
    if (m_listenThread.IsRunning())
        m_listenThread.Stop();

    if (m_listenSocket != 0)
        closesocket(m_listenSocket);

    WSACleanup();
}

void CListener::listenSocket()
{
    if (m_isThreadStopped)
        return;

    sockaddr_storage ss;
    int len = sizeof(ss);
    int sock = static_cast<int>(
        accept(m_listenSocket, reinterpret_cast<struct sockaddr*>(&ss), &len));
    if ((INVALID_SOCKET == sock) || (!m_callback))
    {
        int errorCode = WSAGetLastError();
        Sleep(10);
        m_listenThread.message_loop()->PostTask(
            FROM_HERE, NewRunnableMethod(this, &CListener::listenSocket));
        return;
    }

    shared_ptr<CNetScok> acceptSocket(new CNetScok(sock));
    m_callback->OnAccept(acceptSocket);
}

bool CListener::StopListen()
{
    m_isThreadStopped = true;
    m_listenThread.Stop();
    return true;
}
