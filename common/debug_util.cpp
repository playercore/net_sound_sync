#include "debug_util.h"

#include <cstdarg>
#include <algorithm>
#include <vector>
#include <memory>

#include <windows.h>
#include <dbghelp.h>
#include <boost/intrusive_ptr.hpp>

using std::unique_ptr;
using boost::intrusive_ptr;

namespace kugou 
{

#if defined(_DEBUG)

void CTrace::operator ()(const wchar_t* formatted, ...)
{
    const int debugMessageSize = 1024;
    unique_ptr<wchar_t[]> buf(new wchar_t[debugMessageSize]);
    buf[0] = L'\0';
    va_list ap;
    va_start(ap, formatted);
    _vsnwprintf_s(buf.get(), debugMessageSize, debugMessageSize - 1, formatted,
                  ap);
    OutputDebugString(buf.get());
    va_end(ap);
}

#endif  // defined(_DEBUG)

}
