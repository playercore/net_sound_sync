#ifndef _DSHOW_UTIL_H_
#define _DSHOW_UTIL_H_

#include <streams.h>

class CLockHolder
{
protected:
    CLockHolder() {}
    CCritSec* getLock() { return &m_lock; }

private:
    CCritSec m_lock;
};

#endif  // _DSHOW_UTIL_H_