#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <string>

enum KProxyType
{
    PROXY_TYPE_UNKNOWN = 0,
    PROXY_TYPE_HTTP = 1,
    PROXY_TYPE_SOCK4 = 2,
    PROXY_TYPE_SOCK5 = 3
};

class CNetScok
{
public:
    CNetScok(const char* host, int port,const char* proxyHost = "", int proxyHostPort = 0,
             KProxyType proxyType = PROXY_TYPE_UNKNOWN);
    CNetScok(int sock);
    ~CNetScok();
    
    bool Connect(int timeout);
    int Read(void* data, int len);
    int Write(void* data, int len);
    void Close() { close(); };

private:
    
    bool connectWithTimeout(const char* host, int port, int timeout);
    int read(void* data, int len, int timeout);
    void close();
    int write(void* data, int len, int timeout);

    std::string m_host;
    int m_hostPort;
    std::string m_proxyHost;
    int m_proxyHostPort;
    KProxyType m_proxyType;
    int m_sock;
};

#endif /* __NETWORK_H__ */
