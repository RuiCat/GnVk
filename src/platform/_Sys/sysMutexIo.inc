// GN_mini 平台实现片段：互斥锁 + IO/传感器 stub。
// 2026-11-26 自 GN_Sys.cpp 第 1336-1720 行 move-only 拆出（REFACTOR_PLAN §13 B6）。
// 由 GN_Sys.cpp 按原顺序包含（单编译单元，#ifdef 平台分支语义不变）；不是独立头文件。
void GN_MutexLock(GN_sMUTEX *mut) {
    while(InterlockedCompareExchange(mut, 1, 0));
}
void GN_MutexUnLock(GN_sMUTEX *mut) {
    InterlockedExchange(mut, 0);
}

/***************************************************************** 动态库 **********************************************************************/
#if defined _WIN32_ || defined _WIN32_WCE
GN_SYS_API int *GT_ModuleOpen(const char *module_name) {
    const char *pstr = module_name;
    while(*pstr && *pstr != '/' && *pstr != '\\') ++pstr;
    if(*pstr) {
        char dll_path[1600], *env_path = getenv("PATH");
        unsigned int path_len = strlen(env_path);
        strncpy(dll_path, env_path, path_len);
        dll_path[path_len++] = ';';
        GN_StrGetDirName(module_name, dll_path + path_len);
        SetEnvironmentVariable("PATH", dll_path);
    }
#ifndef _WIN32_WCE
    return (int *)LoadLibrary(module_name);
#else
    return 0;
#endif
}
GN_SYS_API void GT_ModuleClose(int *module) {
    FreeLibrary((HMODULE)module);
}
GN_SYS_API GT_PFUNC_VOID GT_ModuleGetFunction(int *module, const char *func_name) {
    return (GT_PFUNC_VOID)GetProcAddress((HMODULE)module, func_name);
}
#else
#include <dlfcn.h> //依赖库搜索路径指定标记-WI,-rpath=
GN_SYS_API int *GT_ModuleOpen(const char *module_name) {
#if defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
    return 0;
#else
    return (int *)dlopen(module_name, RTLD_GLOBAL | RTLD_NOW);
#endif
}
GN_SYS_API void GT_ModuleClose(int *module) {
    dlclose(module);
}
GN_SYS_API GT_PFUNC_VOID GT_ModuleGetFunction(int *module, const char *func_name) {
    return (GT_PFUNC_VOID)dlsym(module, func_name);
}
#endif
/************************************************************************ 特殊外设IO ******************************************************************************/
#define GT_IO_IN_PORT_NUM 1
#define GT_IO_OUT_PORT_NUM 0
#define GT_MAX_INFV_NUM 3
int InitUsIO() {
    return 0;
}
void UnInitUsIO() {}
void ProcUsIO() {}
GN_SYS_API void GNio::EnableSensors(unsigned int use_flags) {}
GN_SYS_API unsigned int GNio::GetSensorCaps() {
    return 0;
}
GN_SYS_API void GNio::OutPut(int IO_port, int status, int turn_num, int turn_time) {}
GN_SYS_API void GNio::GetInCount(int IO_port, int *nCounter) {}
GN_SYS_API bool GN::StartRockeyPass(const char *gameID) {
    return 1;
}
GN_SYS_API bool GN::RockeyPass() {
    return 1;
}

GN_SYS_API void GNio::TicketsNumOut(int count) {}
GN_SYS_API int GNio::GetOweTickets() {
    return 0;
}
GN_SYS_API bool GNio::IsNoTickets() {
    return false;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
GN_SYS_API float GNio::InFv[GT_MAX_INFV_NUM] = {0.0f};
GN_SYS_API bool GNio::InPort[GT_IO_IN_PORT_NUM] = {0};

GN_SYS_API uint8 GNio::GetInFvNum() {
    return GT_MAX_INFV_NUM;
}
GN_SYS_API uint8 GNio::GetInPortNum() {
    return GT_IO_IN_PORT_NUM;
}
GN_SYS_API uint8 GNio::GetOutPutNum() {
    return GT_IO_OUT_PORT_NUM;
}

/*****************************************************************************************************************************************************************/
GN_SYS_API void GN_InitSys() {
    if(GT_SystemInited & (1 << 5)) return;
#ifdef _WIN32_
    //检查超级计时器是否可用
    int _sTimeReady = QueryPerformanceFrequency(&GT_TimeFreq);
    if(_sTimeReady) GN_TimeGetCount = GT_TimeGetCountS; /*检查决定用边种计时器*/
#endif
    GN_ThreadStart(GT_CheckMultiThreadCaps, 0);
    GN_ThreadSupport = false;

    if(!GT_CurAppDir[0]) { //如果系统入口之前没设置过当前目录就在这里获取一下
        GT_GetCurrentDirectory(GT_CurAppDir);
        GT_TEMP_CUR_APP_DIR_ADD_SPLIT(GT_CurAppDir, GT_CurAppDirLen);
    }
    InitUsIO();
    GT_SystemInited |= 1 << 5;
}
GN_SYS_API void GN_UnInitSys() {
    GN_DeleteArray(GNg::GN_ClipboardState.aStrPasteBuffer); //删除剪贴板内容
    UnInitUsIO();
    if(!GNg::GN_UserThreadState.aUsThreadTable.empty()) {
#ifdef GN_DEBUG
        GN_Log("还有线程 %d 个没有退出!!\n", GNg::GN_UserThreadState.aUsThreadTable.size());
#endif
        GNg::GN_UserThreadState.aUsThreadTable.clear();
    }
    GT_SystemInited -= 1 << 5;
}
GN_SYS_API void GN_SysProc(unsigned int nTime) {
    ProcUsIO();
}
#if defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID

#elif defined(_LINUX_)
#include <sys/utsname.h>
#endif
GN_SYS_API char * GN_GetInerfaceInfoSys() {
#ifdef _WIN32_WCE
    static const char sysInfo[] = "WinCE 6";
#elif defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
    static const char sysInfo[64] = "Android 10";
#elif defined GT_USING_IOS
    static const char sysInfo[] = "IOS 9";
#elif defined _WIN32_
    static char sysInfo[64] = "Windows ";
    char *sv = sysInfo + 8;
    if(!sv[0]) {
        sysInfo[63] = 0; // ensure null termination for strncpy
        OSVERSIONINFOEX os;
        os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        if(GetVersionEx((OSVERSIONINFO *)&os)) {
            //下面根据版本信息判断操作系统名称
            switch(os.dwMajorVersion) { //判断主版本号
                case 4:
                    switch(os.dwMinorVersion) { //判断次版本号
                        case 0:
                            if(os.dwPlatformId == VER_PLATFORM_WIN32_NT) strncpy(sv, "NT 4.0", 55);
                            else if(os.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) strncpy(sv, "95", 55);
                            break;
                        case 10: strncpy(sv, "98", 55); break;
                        case 90: strncpy(sv, "Me", 55); break;
                    }
                    break;
                case 5:
                    switch(os.dwMinorVersion) { //再比较dwMinorVersion的值
                        case 0: strncpy(sv, "2000", 55); break;
                        case 1: strncpy(sv, "XP", 55); break;
                        case 2:
#define SM_SERVERR2 89
                            if(GetSystemMetrics(SM_SERVERR2) == 0) strncpy(sv, "2003", 55);
                            else strncpy(sv, "2003 R2", 55);
                            break;
                    }
                    break;
                case 6:
                    switch(os.dwMinorVersion) {
                        case 0:
                            if(os.wProductType == VER_NT_WORKSTATION) strncpy(sv, "Vista", 55);
                            else strncpy(sv, "Server 2008", 55);
                            break;
                        case 1:
                            if(os.wProductType == VER_NT_WORKSTATION) strncpy(sv, "7", 55);
                            else strncpy(sv, "Server 2008 R2", 55);
                            break;
                        case 2:
                            if(os.wProductType == VER_NT_WORKSTATION) strncpy(sv, "8", 55);
                            else strncpy(sv, "Server 2012", 55);
                            break;
                        case 3:
                            strncpy(sv, "8.1", 55);
                            break;
                    }
                    break;
                default: {
                    HMODULE hModNtdll = 0;
                    if(hModNtdll = ::LoadLibraryW(L"ntdll.dll")) {
                        typedef void(WINAPI * pfRTLGETNTVERSIONNUMBERS)(DWORD *, DWORD *, DWORD *);
                        pfRTLGETNTVERSIONNUMBERS pfGetVersionNum;
                        pfGetVersionNum = (pfRTLGETNTVERSIONNUMBERS)::GetProcAddress(hModNtdll, "RtlGetNtVersionNumbers");
                        if(pfGetVersionNum) {
                            DWORD dwMajorVer, dwMinorVer, dwBuildNum;
                            pfGetVersionNum(&dwMajorVer, &dwMinorVer, &dwBuildNum);
                            dwBuildNum &= 0x0ffff;
                            snprintf(sv, (int)(sizeof(sysInfo) - 8), "%d.%d.%d", dwMajorVer, dwMinorVer, dwBuildNum);
                            ::FreeLibrary(hModNtdll);
                            break;
                        }
                        ::FreeLibrary(hModNtdll);
                    }
                    strncpy(sv, "X", 55);
                } break;
            }
        } else strncpy(sv, "?", 55);
    }
#elif defined _MAC_
    static char sysInfo[64] = "";
    if(!sysInfo[0]) {
        strncpy(sysInfo, "Mac OS X 10.5", sizeof(sysInfo) - 1);
        sysInfo[sizeof(sysInfo) - 1] = 0;
        FILE *fp = fopen("/System/Library/CoreServices/SystemVersion.plist", "r");
        if(fp) {
            char info[384], *lable;
            while(fgets(info, 384, fp)) {
                if(strstr(info, "ProductVersion")) {
                    fgets(info, 384, fp);
                    lable = strstr(info, "<string>");
                    if(lable) sscanf(lable + 8, "%[^<]", sysInfo + 9);
                    break;
                }
            }
            fclose(fp);
        }
    }
#elif defined _LINUX_
    static char sysInfo[64] = ""; // B9-1: was const but written via casts; align with _MAC_ branch
    if(!sysInfo[0]) {
        struct utsname uinfos;
        if(uname(&uinfos)) {
            strncpy(sysInfo, "Linux ?", 63);
            sysInfo[63] = 0;
        } else snprintf(sysInfo, sizeof(sysInfo), "%s %s", uinfos.sysname, uinfos.release);
    }
#endif
    return (char *)sysInfo;
}
#ifdef _WIN64
#include <intrin.h>
#endif
GN_SYS_API void GN_GetDeviceInfoCpu(char *info) {
#ifdef _WIN64
    int cpuInfo[4] = {-1};
    __cpuid(cpuInfo, 0x80000002);
    memcpy(info, cpuInfo, 16);
    info += 16;
    __cpuid(cpuInfo, 0x80000003);
    memcpy(info, cpuInfo, 16);
    info += 16;
    __cpuid(cpuInfo, 0x80000004);
    memcpy(info, cpuInfo, 16);
    info += 16;
    *info = 0;
#elif defined(_WIN32_)
#ifdef _MSC_VER
    unsigned int *pinfo = (unsigned int *)info;
    unsigned int deax;
    unsigned int debx;
    unsigned int decx;
    unsigned int dedx;
    unsigned int veax = 0x80000002;
    for(; veax <= 0x80000004; ++veax) { //检测 0x80000002 到 0x80000004
        __asm
            {
			mov eax, veax
			cpuid
			mov deax, eax
			mov debx, ebx
			mov decx, ecx
			mov dedx, edx
            }
        *pinfo = deax;
        ++pinfo;
        *pinfo = debx;
        ++pinfo;
        *pinfo = decx;
        ++pinfo;
        *pinfo = dedx;
        ++pinfo;
    }
    *((char *)pinfo) = 0;
#else
    asm("movl $0, %%eax\n\t"
        "cpuid\n\t"
        "movl %%ebx, (%0)\n\t"
        "movl %%edx, 4(%0)\n\t"
        "movl %%ecx, 8(%0)\n\t"
        :
        : "D"(info)
        : "eax", "ebx", "ecx", "edx");
#endif
#elif defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
    // 契约（B5f）：info 缓冲区 ≥256 字节（char* 参数，容量由调用方保证）。
    strncpy(info, "?.??GHz", 255);
    info[255] = 0;
#elif defined _MAC_ || defined GT_USING_IOS
    size_t len = 256;
    sysctlbyname("machdep.cpu.brand_string", info, &len, NULL, 0);
#else
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if(fp) {
        int bsize = 64, len;
        while(fgets(info, bsize, fp)) {
            len = strlen(info);
            info[len++] = ' ';
            info += len;
            bsize -= len;
        }
        if(bsize == 64) *info = 0;
        else *(info - 1) = 0;
        fclose(fp);
    }
#endif
}
GN_SYS_API void GN_GetDeviceInfoRam(char *info) {
#define GBYTES 1073741824
#define MBYTES 1048576
#define KBYTESD 1024.0
    unsigned int long tot_ram = 1024, avl_ram = 1024;
#if defined _WIN32_ || defined _WIN32_WCE
    MEMORYSTATUSEX statusex;
    statusex.dwLength = sizeof(statusex);
    if(GlobalMemoryStatusEx(&statusex)) {
        tot_ram = statusex.ullTotalPhys;
        avl_ram = statusex.ullAvailPhys;
    }
#elif defined _MAC_ || defined GT_USING_IOS
    size_t len = sizeof(unsigned int long);
    sysctlbyname("hw.memsize", &tot_ram, &len, NULL, 0);
#else
    FILE *fp = fopen("/proc/meminfo", "r");
    if(fp) {
        tot_ram = 0;
        char rbuf[256], *pstr;
        uint8 mk = 0;
        unsigned int long size;
        while(fgets(rbuf, 256, fp)) {
            pstr = strstr(rbuf, "Mem");
            if(pstr) {
                if(strstr(pstr + 3, "Total:")) {
                    sscanf(pstr + 10, "%lu", &size);
                    tot_ram += size;
                    mk |= 1;
                } else if(strstr(pstr + 3, "Free:")) {
                    sscanf(pstr + 9, "%lu", &size);
                    tot_ram += size;
                    mk |= 2;
                } else if(strstr(pstr + 3, "Available:")) {
                    sscanf(pstr + 14, "%lu", &avl_ram);
                    mk |= 4;
                }
                if(mk == 7) break;
            }
        }
        fclose(fp);
        tot_ram *= 1024;
        avl_ram *= 1024;
    }
#endif
    double decimal_total = 0.0;
    unsigned int long remain_total = tot_ram % GBYTES /*1G字节*/;
    unsigned int long total = tot_ram / GBYTES;
    //unsigned int  long avl = avl_ram / GBYTES;
    //unsigned int  long remain_avl = avl_ram % GBYTES;

    if(remain_total) decimal_total = (remain_total / MBYTES) / KBYTESD;
    //if(remain_avl)decimal_avl = (remain_avl / MBYTES) / KBYTESD;
    decimal_total += (double)total;
    //decimal_avl += (double)avl;
    //sprintf(info, "TOTAL RAM: %.2fGB (%.2f GB available)", decimal_total, decimal_avl);
    sprintf(info, "Total RAM: %.2fGB", decimal_total);
}

/************************************************************************************************************************/
#ifdef _MAC_
#undef va_start
#undef va_end
extern "C" {
void va_start(va_list vv, const char *fmtstr) { /*__builtin_va_start( vv , fmtstr);*/ }
void va_end(va_list vv) { /* __builtin_va_end( vv );*/ }
void Status_SetNewStatusByKey(char *str) {}
float gDesiredBitRate = 1.0f;
}
#endif
