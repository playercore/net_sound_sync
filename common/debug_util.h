#ifndef _KUGOO_KGPLAYER_PLUGINS_COMMON_DEBUG_UTIL_H_
#define _KUGOO_KGPLAYER_PLUGINS_COMMON_DEBUG_UTIL_H_

#include "chromium/base/basictypes.h"

namespace kugou 
{

#if defined(_DEBUG)
class CTrace
{
public:
    explicit CTrace(bool exclusive) : m_exclusive(exclusive) {}
    void operator ()(const wchar_t* formatted, ...);

private:
    bool m_exclusive;
};

#define TRACE kugou::CTrace(false)

#endif

}

#endif  // _KUGOO_KGPLAYER_PLUGINS_COMMON_DEBUG_UTIL_H_