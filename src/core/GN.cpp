/****************************************************************************
Copyright (c) 2018-2025 gncreate Technologies Inc.

 http://www.gncreate.com
 http://www.img-power.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "GN.h"
#ifdef _GN_H_
GN_API GT_sMEMORY_TABLE GT_UsMemTable; //这内存表一定要放在最头定义 否侧用安全指针分配的时会崩

GN_API unsigned int GT_SystemInited = 0;

GN_API char *const GN_GetInterfaceVersion() {
    static const char GN_interface_Version[] = GN_INTERFACE_VERSION;
    return (char *const)GN_interface_Version;
}
GN_API char *const GN_GetVersion() {
    static const char GN_Version[] = GN_VERSION;
    return (char *const)GN_Version;
}
GN_API unsigned int GN_GetModuleConstitute() {
    return 0
#ifdef GN_USING_UTILITY_LIB
           | GN_USING_UTILITY_LIB
#endif
#ifdef GN_USING_2D
           | GN_USING_2D
#endif
#ifdef GN_USING_AUDIO
           | GN_USING_AUDIO
#endif
#ifdef GN_USING_NET
           | GN_USING_NET
#endif
#ifdef GN_USING_UNZIP
           | GN_USING_UNZIP
#endif
        ;
}
#endif