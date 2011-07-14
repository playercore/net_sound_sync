#ifndef _LISTENER_H_
#define _LISTENER_H_
#include "net_sock.h"

#include <memory>
#include "chromium/base/thread.h"

class ISocketCallback
{
public:
    virtual void OnAccept(std::shared_ptr<CNetScok> sock) = 0;
};

class CListener
{
public:
    static bool ImplementsThreadSafeReferenceCounting() { return true; }
    CListener(ISocketCallback* callback);
    ~CListener();
    bool Init(int listenPort);
    bool StartListen();
    bool StopListen();
    bool AlreadListen() { return m_listenThread.IsRunning(); }
    int AddRef() { return ++m_referenceCount; }
    int Release() 
    { 
        --m_referenceCount; 
        if (0 == m_referenceCount)
        {
            delete this;
            return 0;
        }
        else
        {
            return m_referenceCount;
        }
    }

protected:
    void listenSocket();

private:
    int m_listenSocket;
    base::Thread m_listenThread;
    bool m_isThreadStopped;
    int m_referenceCount;
    int m_listenPort;
    ISocketCallback* m_callback;
};

#endif