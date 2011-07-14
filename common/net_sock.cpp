#include "net_sock.h"

#include <Ws2tcpip.h>
#include <memory>
#include <boost/lexical_cast.hpp>


using std::string;
using boost::lexical_cast;
using std::shared_ptr;

// CNetScok* CNetScok::Accept(int waitSock, struct sockaddr_storage* ss)
// {
//     socklen_t sslen = sizeof(struct sockaddr_storage);
//     int sock = static_cast<int>(accept(
//         waitSock, reinterpret_cast<struct sockaddr*>(ss), &sslen));
//     if(sock == -1) 
//         return NULL;
// 
//     return new CNetScok(sock);
// }

CNetScok::CNetScok(const char* host, int port,const char* proxyHost, 
                   int proxyHostPort, KProxyType proxyType)
    : m_host(host)
    , m_hostPort(port)
    , m_proxyHost(proxyHost)
    , m_proxyHostPort(proxyHostPort)
    , m_proxyType(proxyType)
    , m_sock(0)
{
    short version = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(version, &data);
}

CNetScok::CNetScok(int sock)
    : m_host()
    , m_hostPort(0)
    , m_proxyHost()
    , m_proxyHostPort(0)
    , m_proxyType(PROXY_TYPE_UNKNOWN)
    , m_sock(sock)
{
}

CNetScok::~CNetScok()
{
    WSACleanup();
    close();
}

void CNetScok::close()
{
    if (m_sock > 0)
        closesocket(m_sock);

    m_sock = 0;
}

int CNetScok::Read(void* data, int len)
{
    int readLen = 0;
    int byteRead = 0;
    while (readLen < len)
    {
        byteRead = read(reinterpret_cast<char*>(data) + readLen, len - readLen,
                        INFINITE);
        if (byteRead <= 0)
            break;

        readLen += byteRead;
    }
    return readLen;
}

int CNetScok::read(void* data, int len, int timeout)
{
    if (m_sock <= 0 || !data || !len)
        return -1;

    fd_set  fdRead  = { 0 };
    TIMEVAL stTime;
    TIMEVAL* pstTime = NULL;

    if (INFINITE != timeout) 
    {
        stTime.tv_sec = timeout/1000;
        stTime.tv_usec = (timeout % 1000) * 1000;
        pstTime = &stTime;
    }

    // Set Descriptor
    FD_SET(m_sock, &fdRead);

    // Select function set read timeout
    int res = select(m_sock, &fdRead, NULL, NULL, pstTime);
    if ( res > 0)
    {
        // TCP
        res = recv(m_sock, (LPSTR) data, len, 0);
    }

    if (res == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        SetLastError( err );
    }

    return res >= 0 ? res : -1;
}

int CNetScok::Write(void* data, int len)
{
    int writeLen = 0;
    int byteWrite = 0;
    while (writeLen < len)
    {
        byteWrite = write(reinterpret_cast<char*>(data) + writeLen, len - writeLen,
                          INFINITE);
        if (byteWrite <= 0)
            break;

        writeLen += byteWrite;
    }
    return writeLen;
}

int CNetScok::write(void* data, int len, int timeout)
{
    if (m_sock <= 0 || !data || !len)
        return -1;

    fd_set  fdWrite  = { 0 };
    TIMEVAL stTime;
    TIMEVAL* pstTime = NULL;

    if (INFINITE != timeout) 
    {
        stTime.tv_sec = timeout/1000;
        stTime.tv_usec = (timeout % 1000) * 1000;
        pstTime = &stTime;
    }

    // Set Descriptor
    FD_SET(m_sock, &fdWrite);

    // Select function set read timeout
    int res = select(m_sock, NULL, &fdWrite, NULL, pstTime);
    if ( res > 0)
    {
        // TCP
        res = send(m_sock, (LPSTR) data, len, 0);
    }

    if (res == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        SetLastError( err );
    }

    return res >= 0 ? res : -1;
}

bool CNetScok::Connect(int timeout)
{
    // 暂时不支持代理
    return connectWithTimeout(m_host.c_str(), m_hostPort, timeout);
}

bool CNetScok::connectWithTimeout(const char* host, int port, int timeout)
{
    close();
    if(port < 0 || port > 0xffff)
        return false;

    string portstr = lexical_cast<string, int>(port);
    struct addrinfo hints;
    struct addrinfo* result = NULL;

    ZeroMemory(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int errorCode = getaddrinfo(host, portstr.c_str(), &hints, &result);
    if (errorCode)
        return false;

    // 函数结束后释放result
    shared_ptr<void> r(result, freeaddrinfo);

    //为了能打断正在阻塞的连接,需要先保存sock
    m_sock = static_cast<int>(socket(result->ai_family, result->ai_socktype,
                                     result->ai_protocol));

    if(m_sock < 0) 
    {
        m_sock = 0;
        return false;
    }

    unsigned long ul = 1;
    if (ioctlsocket(m_sock, FIONBIO, reinterpret_cast<unsigned long*>(&ul))
        == SOCKET_ERROR)
    {
        close();
        return false;
    }

    if(connect(m_sock, result->ai_addr, static_cast<int>(result->ai_addrlen))
       == -1)
    {
        /* failed not because it was non-blocking */
        int err;
        SetLastError(err = WSAGetLastError());
        if (WSAEWOULDBLOCK != err)
        {
            close();
            return false;
        }
    }

    fd_set set;
    struct timeval tv;    
    for(int tryCount = 0; tryCount <= 5; ++tryCount) 
    { 
        if (tryCount >= 5)
        {
            close();
            return false;
        }

        tv.tv_sec  = timeout / 5000;
        tv.tv_usec = (timeout % 1000) * 200;

        FD_ZERO(&set);
        FD_SET(m_sock, &set);

        int ret = select(m_sock + 1, NULL, &set, NULL, &tv);
        errorCode = WSAGetLastError();
        if(ret < 0) 
        { 
            close();
            return false;
        }
        else if(ret > 0) 
        {
            break;
        }
    }

    /* Turn the socket to blocking, as we don't need it. */
    ul = 0;
    if (ioctlsocket(m_sock, FIONBIO, (unsigned long*)&ul) == SOCKET_ERROR)
    {
        close();
        return false;
    }

    return true;
}