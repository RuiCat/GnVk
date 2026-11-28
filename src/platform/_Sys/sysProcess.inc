// GN_mini 平台实现片段：当前目录获取 + 进程/线程能力探测。
// 2026-11-26 自 GN_Sys.cpp 第 965-1171 行 move-only 拆出（REFACTOR_PLAN §13 B6）。
// 由 GN_Sys.cpp 按原顺序包含（单编译单元，#ifdef 平台分支语义不变）；不是独立头文件。
static void GT_GetCurrentDirectory(char *buf /*[256]*/) {
#ifdef _WIN32_
    GetCurrentDirectory(256, buf);
#elif _WIN32_WCE
#else
    getcwd(buf, 256);
#endif
}
GN_SYS_API void GT_GetAppFileName(char name_out[]) {
#ifdef _WIN32_
    GetModuleFileName(0, name_out, 256);
#elif _WIN32_WCE
#else
    // 契约（B5f）：name_out 缓冲区 ≥256 字节。
    strncpy(name_out, GT_CurAppDir, 255);
    name_out[255] = 0;
#ifdef GT_USING_ANDROID_JNI
#else
#ifdef _GN_INIT_H
    if(GN_ExecArgs) {
        strncpy(name_out + GT_CurAppDirLen, GN_StrGetFileName(GN_ExecArgs[0]), 255 - GT_CurAppDirLen);
        name_out[255] = 0;
    } else
#endif
        strncpy(name_out + GT_CurAppDirLen, "GN_App", 255 - GT_CurAppDirLen);
    name_out[255] = 0;
#endif
#endif
}
#ifdef _WIN32_
#ifndef _SHLOBJ_H_
#include <shlobj.h>
#endif
#endif
GN_SYS_API P<P<char>> GT_EnumSystemDirectory() {
    P<P<char>> dir_names;
#if defined _WIN32_WCE
#elif defined _WIN32_
    char dir2[256];
    GetLogicalDriveStringsA(256, dir2);
    p_dir = dir2;
    unsigned int n = 0;
    while(*p_dir) {
        p_dir += 4;
        ++n;
    } //统计盘符个数
    dir_names.New(4 + n);
    P<char> *_dir_names = dir_names;

    n = 0;
    (_dir_names[0])(GT_CurAppDir, 0);
    _dir_names[0][strlen((char *)_dir_names[0]) - 1] = 0; //应用程序文件当前目录
    ++n;

    SHGetSpecialFolderPathA(0 /*(HWND)GNg::sWINDOW::Hwnd*/, dir, CSIDL_DESKTOPDIRECTORY, 0); //Desktop的目录
    (_dir_names[n])(dir, 0);
    ++n;

    SHGetSpecialFolderPathA(0 /*(HWND)GNg::sWINDOW::Hwnd*/, dir, CSIDL_PERSONAL, 0); //我的文档
    (_dir_names[n])(dir, 0);
    ++n;
    /*
	 GetSystemDirectoryA(dir,256);
     (_dir_names[n])(dir,0); ++n;      //system32
     */
    GetWindowsDirectoryA(dir, 256);
    (_dir_names[n])(dir, 0);
    ++n; //windows

    p_dir = dir2;
    while(*p_dir) {
        (_dir_names[n])(p_dir, 0); //记录各个盘的目录
        _dir_names[n][strlen((char *)_dir_names[n]) - 1] = 0;
        p_dir += 4;
        ++n;
    }
#elif defined GT_USING_IOS
#elif defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
#elif defined _MAC_
    dir_names.New(5);
    P<char> *_dir_names = dir_names;
    (_dir_names[0])(GT_CurAppDir, 0);
    _dir_names[0][strlen((char *)_dir_names[0]) - 1] = 0; //应用程序文件当前目录
    char tmpath[384];
    sprintf(tmpath, "/Users/%s", getlogin());
    (_dir_names[2])(tmpath, 0);
    int remaining = sizeof(tmpath) - strlen(tmpath) - 1;
    strncat(tmpath, "/Desktop", remaining);
    tmpath[sizeof(tmpath) - 1] = 0;
    (_dir_names[1])(tmpath, 0);
    (_dir_names[3])("/usr/lib", 0);
    (_dir_names[4])("/Volumes", 0);
#elif defined _LINUX_
    dir_names.New(2);
    (dir_names[0])(GT_CurAppDir, 0);
    dir_names[0][strlen((char *)dir_names[0]) - 1] = 0; //应用程序文件当前目录
    (dir_names[1])(getenv("HOME"), 0); //用户主目录
#endif
    return dir_names;
}
GN_SYS_API bool GN_SysShowFile(const char *filename) {
#if defined(_WIN32_) || defined(_WIN32_WCE)
    return ((unsigned int)ShellExecute(0, 0, filename, 0, 0, SW_SHOWNORMAL) > 32);
#elif defined(GT_USING_ANDROID_JNI)
    return false;
#elif defined(_MAC_)
    char t_file_name[384];
    return (bool)macSysShowFile(GN_StrToSysCode2(t_file_name, filename), filename[0] == '/' ? 0 : 1);
#else
    return false;
#endif
}
/**************************************************************** 时间API *********************************************************************/
#ifdef _WIN32_WCE
#include <mmsystem.h>
#pragma comment(lib, "mmtimer.lib")
#endif
#ifdef _WIN32_
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

LARGE_INTEGER GT_TimeFreq;

double GT_TimeGetCountS(void) {
    LARGE_INTEGER time_count;
    QueryPerformanceCounter(&time_count);
    return time_count.QuadPart / (GT_TimeFreq.QuadPart / 1000.0);
}
#endif

#ifdef GT_USING_GLsdl
GN_SYS_API void GN_TimeSleep(unsigned int sleep_time) {
    SDL_Delay(sleep_time);
}
GN_SYS_API double GT_TimeGetCountN(void) {
    return (double)SDL_GetTicks();
}

#elif defined _WIN32_ || defined _WIN32_WCE
GN_SYS_API void GN_TimeSleep(unsigned int sleep_time) {
    Sleep(sleep_time);
}
GN_SYS_API double GT_TimeGetCountN(void) {
    return (double)timeGetTime();
}

#else //其他LINUX 或 UINX
#if HAVE_NANOSLEEP || HAVE_CLOCK_GETTIME
#include <time.h>
#endif
#include <chrono>
GN_SYS_API void GN_TimeSleep(unsigned int sleep_time) {
    int was_error;
#if HAVE_NANOSLEEP /*-------------------------------------*/
    struct timespec elapsed, tv;
    elapsed.tv_sec = sleep_time / 1000;
    elapsed.tv_nsec = (sleep_time % 1000) * 1000000;
    do {
        errno = 0;
        tv.tv_sec = elapsed.tv_sec;
        tv.tv_nsec = elapsed.tv_nsec;
        was_error = nanosleep(&tv, &elapsed);

    } while(was_error && (errno == EINTR));
#else /*--------------------------------------------------*/
    struct timeval tv;
    unsigned int then, now, elapsed;
    then = (unsigned int)GT_TimeGetCountN();
    do {
        errno = 0;
        now = (unsigned int)GT_TimeGetCountN();
        elapsed = (now - then);
        then = now;
        if(elapsed >= sleep_time) break;
        sleep_time -= elapsed;
        tv.tv_sec = sleep_time / 1000;
        tv.tv_usec = (sleep_time % 1000) * 1000;
        was_error = select(0, NULL, NULL, NULL, &tv);
    } while(was_error && (errno == EINTR));
#endif /*-------------------------------------------------*/
}
GN_SYS_API double GT_TimeGetCountN(void) {
    static auto start_run_time = std::chrono::high_resolution_clock::now();
    return (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start_run_time).count()) * 0.000001;
}
#endif //end LINUX
GN_SYS_API GT_P_DOUBLE_VOID_FUNC GN_TimeGetCount = GT_TimeGetCountN;

/******************************************************************** 多线程 *****************************************************************/
struct GT_sTHREAD_ARG {
    void *arg;
    void *arg2;
    void (*fun)(P<GN_sTHREAD>, void *, void *);
    P<GN_sTHREAD> tr;

    GT_sTHREAD_ARG() {}
    GT_sTHREAD_ARG(P<GN_sTHREAD> &_tr, void *_arg, void *_arg2, void (*_fun)(P<GN_sTHREAD>, void *, void *)) : arg(_arg), arg2(_arg2), fun(_fun), tr(_tr) {}
};
// DA-M2 批 3：用户线程登记表收敛进文件内上下文。
struct sUserThreadContext {
    GN::sLIST<GT_sTHREAD_ARG> aUsThreadTable;
};
static sUserThreadContext gUserThreadContext;

GN_SYS_API bool GN_ThreadSupport = false;
