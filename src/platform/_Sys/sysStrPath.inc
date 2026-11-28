// GN_mini 平台实现片段：模块扩展名/系统名/字符串转系统码/路径转换（Win/Android/Mac 分支）。
// 2026-11-26 自 GN_Sys.cpp 第 356-697 行 move-only 拆出（REFACTOR_PLAN §13 B6）。
// 由 GN_Sys.cpp 按原顺序包含（单编译单元，#ifdef 平台分支语义不变）；不是独立头文件。
/*************************************************** Sys模块必须实现的东西 ************************************************************/

GN_SYS_API char * GN_StrGetModuleExtName(GN_eMODULE_TYPE type) {
#if defined _WIN32_ || defined _WIN32_WCE
    static const char strEXE[] = ".exe";
    static const char strDLL[] = ".dll";
#elif defined _MAC_ || defined GT_USING_IOS
    static const char strEXE[] = "\0";
    static const char strDLL[] = ".dylib";
#else
    static const char strEXE[] = "\0";
    static const char strDLL[] = ".so";
#endif
    switch(type) {
        case GN_eMODULE_EXE: return (char *)strEXE;
        case GN_eMODULE_DLL: return (char *)strDLL;
        default: return (char *)"\0";
    }
}
GN_SYS_API char * GN_StrGetSysName() {
#ifdef _WIN32_
#ifdef __LP64__
    static const char sysName[] = "win64";
#else
    static const char sysName[] = "win32";
#endif
#elif defined _WIN32_WCE
    static const char sysName[] = "wince";
#elif defined _MAC_
#ifdef __LP64__
    static const char sysName[] = "mac64";
#else
    static const char sysName[] = "mac";
#endif
#elif defined GT_USING_IOS
    static const char sysName[] = "ios";
#elif defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
#ifdef __LP64__
    static const char sysName[] = "android64";
#else
    static const char sysName[] = "android";
#endif
#elif defined _LINUX_
    static const char sysName[] = "linux";
#endif
    return (char *)sysName;
}
GN_SYS_API char *GN_StrToSysCode2(char *buf /*[384]*/, const char *in) {
#if defined _WIN32_ || defined _WIN32_WCE
    return (char *)in;
#else
    GT_StrToSysCode(buf, in);
    return buf;
#endif
}
GN_SYS_API void GN_StrToSysCode(char *out, const char *in) {
#ifdef _ICONV_T
    if(out == in) {
        char buf0[8192];
        int len = strlen(in);
        char *in2 = len >= 8192 ? (char *)malloc(len + 1) : buf0;
        strncpy(in2, in, len);
        in2[len] = 0;
        GT_StrToSysCode(out, in2);
        if(in2 != (char *)buf0) free(in2);
    } else
#endif
    {
        GT_StrToSysCode(out, in);
    }
}
GN_SYS_API char *GN_StrToSysFilePath2(char *buf /*[384]*/, const char *src, const char *open_mode, bool is_g_dir) {
    if(is_g_dir) return GN_StrToSysCode2(buf, src); //全局绝对路径就直接转码就可

#ifdef _WIN32_WCE
#elif defined _LINUX_ || defined _MAC_ || defined GT_USING_IOS
    uint8 i = 0;
#ifdef _ICONV_T
    char PSTRBUF[384];
#else
#define PSTRBUF buf
#endif
    while(*src) {
        if(*src == '\\') PSTRBUF[i] = '/'; //非windows下还要把路径分割符转换一下
        else PSTRBUF[i] = *src;
        ++src;
        ++i;
    }
    PSTRBUF[i] = 0;
//gb2312 char to UTF8 string
#if defined GT_USING_IOS || defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
#else
    GT_StrToSysCode(buf, PSTRBUF);
#endif
    //GN_Log("%s\n",buf);

#else /*_WIN32_*/ //-------------------------------------------------------------------
    return (char *)src;
#endif
    return buf;
}
GN_SYS_API void GN_StrToSysFilePath(char des[], const char *src, const char *open_mode, bool is_g_dir) {
    char *ret = GN_StrToSysFilePath2(des, src, open_mode, is_g_dir);
    if(ret == src) {
        strncpy(des, ret, 383);
        des[383] = 0;
    } // 契约（B5f）：调用方缓冲区 ≥384 字节。
}
#if defined(_WIN32_) || defined(_WIN32_WCE)
#include <windows.h>
GN_SYS_API int GN_StrWCharToChar(const void *p_wchar, void *p_char, int char_buf_len) {
    return WideCharToMultiByte(CP_ACP, 0, (WCHAR *)p_wchar, -1, (char *)p_char, char_buf_len, 0, 0);
}
GN_SYS_API int GN_StrCharToWChar(const void *p_char, void *p_wchar, int wchar_buf_len) {
    return MultiByteToWideChar(CP_ACP, 0, (char *)p_char, -1, (WCHAR *)p_wchar, wchar_buf_len);
}
static int GT_StrCharConvert(unsigned short src_fmt, unsigned short dst_fmt, const void *src, void *dst, int char_buf_len) {
    WCHAR tmpstr[2048], *_tmpstr;
    int tlen;
    if(char_buf_len) tlen = char_buf_len;
    else tlen = MultiByteToWideChar(src_fmt, 0, (char *)src, -1, 0, 0);
    _tmpstr = tlen <= 2048 ? tmpstr : new WCHAR[tlen];
    tlen = MultiByteToWideChar(src_fmt, 0, (char *)src, -1, _tmpstr, tlen);

    if(!char_buf_len) {
        char_buf_len = WideCharToMultiByte(dst_fmt, 0, _tmpstr, tlen, 0, 0, 0, 0);
        *((char **)dst) = new char[char_buf_len];
        WideCharToMultiByte(dst_fmt, 0, _tmpstr, tlen, *((char **)dst), char_buf_len, 0, 0);
    } else char_buf_len = WideCharToMultiByte(dst_fmt, 0, _tmpstr, tlen, (char *)dst, char_buf_len, 0, 0);

    if(_tmpstr != tmpstr) delete[] _tmpstr;
    return char_buf_len;
}
GN_SYS_API int GN_StrUTF8ToGB2312(const void *utf8, void *gb, int char_buf_len) {
    return GT_StrCharConvert(CP_UTF8, CP_ACP, utf8, gb, char_buf_len);
}
GN_SYS_API int GN_StrGB2312ToUTF8(const void *gb, void *utf8, int char_buf_len) {
    return GT_StrCharConvert(CP_ACP, CP_UTF8, gb, utf8, char_buf_len);
}
GN_SYS_API int GT_ExeclSub(const char *exec_filename) {
    if(spawnl(P_NOWAIT, exec_filename, "7", 0) == -1) return -1;
    else return 0;
}
#include <tlhelp32.h>
GN_SYS_API int GT_ExitProc(const char *proc_filename) {
    int rc = -1;
    HANDLE hSysSnapshot = NULL;
    hSysSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSysSnapshot == (HANDLE)-1) return -1;
    PROCESSENTRY32 proc;
    proc.dwSize = sizeof(proc);
    if(Process32First(hSysSnapshot, &proc)) {
        do {
            if(!stricmp(proc.szExeFile, proc_filename)) {
                HANDLE Proc_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc.th32ProcessID);
                if(Proc_handle == NULL || !TerminateProcess(Proc_handle, 0)) rc = -1;
                else rc = 0;
            }
        } while(Process32Next(hSysSnapshot, &proc));
    }
    CloseHandle(hSysSnapshot);
    return rc;
}
#else
GN_SYS_API int GN_StrWCharToChar(const void *p_wchar, void *p_char, int char_buf_len) {
    unsigned short *pwstr = (unsigned short *)p_wchar;
    if(char_buf_len) {
#ifdef GT_USING_ANDROID_JNI
        return 0;
#else
        size_t inlen = strlen((char *)pwstr) + 1;
        size_t outlen = char_buf_len;
        iconv_t cd = iconv_open("gb2312", "utf-16be");
        if(cd == 0) return 0;
        iconv(cd, (char **)&pwstr, &inlen, (char **)&p_char, &outlen);
        iconv_close(cd);
        return char_buf_len - outlen;
#endif
    } else {
        while(*pwstr) {
            if(*pwstr > 127) char_buf_len += 2;
            else ++char_buf_len;
            ++pwstr;
        }
        return char_buf_len + 1;
    }
}
GN_SYS_API int GN_StrCharToWChar(const void *p_char, void *p_wchar, int wchar_buf_len) {
    char *pstr = (char *)p_char;
    if(wchar_buf_len) {
#ifdef GT_USING_ANDROID_JNI
        return 0;
#else
        size_t inlen = strlen(pstr) + 1;
        size_t outlen = wchar_buf_len << 1;
        iconv_t cd = iconv_open("utf-16be", "gb2312");
        if(cd == 0) return 0;
        iconv(cd, (char **)&pstr, &inlen, (char **)&p_wchar, &outlen);
        iconv_close(cd);
        return wchar_buf_len - (outlen >> 1);
#endif
    } else {
        while(*pstr) {
            if(*pstr & 128) pstr += 2;
            else ++pstr;
            ++wchar_buf_len;
        }
        return wchar_buf_len + 1;
    }
}
#ifdef GT_USING_ANDROID_JNI
#else
GN_SYS_API int GN_StrUTF8ToGB2312(const void *gb, void *utf8, int char_buf_len) {
#define GT_TEMP_STR_CHAR_CONVERT(STR_LEN, STR_CPY, ADDPROC)                            \
    if(char_buf_len) {                                                                 \
        ADDPROC                                                                        \
        return char_buf_len - STR_CPY((char *)utf8, (char *)gb, (size_t)char_buf_len); \
    } else {                                                                           \
        char_buf_len = STR_LEN((char *)gb) + 1;                                        \
        *((char **)utf8) = new char[char_buf_len];                                     \
        STR_CPY(*((char **)utf8), (char *)gb, (size_t)char_buf_len);                   \
        return char_buf_len;                                                           \
    }
    GT_TEMP_STR_CHAR_CONVERT(GN_StrLenUTF8ToGB2312, iconv_UTF8toGB2312, )
}
GN_SYS_API int GN_StrGB2312ToUTF8(const void *gb, void *utf8, int char_buf_len) {
#define GT_TEMP_STR_CHAR_COPY_CONVERT_                                                       \
    if((void *)gb == utf8) {                                                                 \
        char buf0[8192];                                                                     \
        int len = strlen((char *)gb);                                                        \
        char *in2 = len >= 8192 ? (char *)malloc(len + 1) : buf0;                            \
        strncpy(in2, (char *)gb, len);                                                       \
        in2[len] = 0;                                                                        \
        char_buf_len -= iconv_GB2312toUTF8((char *)utf8, (char *)in2, (size_t)char_buf_len); \
        if(in2 != (char *)buf0) free(in2);                                                   \
        return char_buf_len;                                                                 \
    }
    GT_TEMP_STR_CHAR_CONVERT(GN_StrLenGB2312ToUTF8, iconv_GB2312toUTF8, GT_TEMP_STR_CHAR_COPY_CONVERT_)
}
#endif
#ifdef _MAC_
#include <signal.h>
#include <GL/mac_ext.h>
#define GT_TMP_FIND_PROCS_INFOS(PROC_FUNC)                                                           \
    int err;                                                                                         \
    kinfo_proc *result = NULL;                                                                       \
    bool done = false;                                                                               \
    static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};                               \
    size_t length;                                                                                   \
    do {                                                                                             \
        /*assert(result == NULL);*/                                                                  \
        length = 0;                                                                                  \
        err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1, NULL, &length, NULL, 0);       \
        if(err == -1) err = errno;                                                                   \
                                                                                                     \
        if(err == 0) {                                                                               \
            result = (kinfo_proc *)malloc(length);                                                   \
            if(result == NULL) {                                                                     \
                err = ENOMEM;                                                                        \
            }                                                                                        \
        }                                                                                            \
        if(err == 0) {                                                                               \
            err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1, result, &length, NULL, 0); \
            if(err == -1) {                                                                          \
                err = errno;                                                                         \
            }                                                                                        \
            if(err == 0) {                                                                           \
                size_t pi = 0, p_count = length / sizeof(kinfo_proc);                                \
                for(; pi < p_count; pi++) {                                                          \
                    PROC_FUNC                                                                        \
                }                                                                                    \
                done = true;                                                                         \
            } else if(err == ENOMEM) {                                                               \
                /*assert(result != NULL);*/                                                          \
                free(result);                                                                        \
                result = NULL;                                                                       \
                err = 0;                                                                             \
            }                                                                                        \
        }                                                                                            \
    } while(err == 0 && !done);                                                                      \
    if(result != NULL) {                                                                             \
        free(result);                                                                                \
        result = NULL;                                                                               \
    }
#else
#define GT_TMP_FIND_PROCSS                                                                                            \
    DIR *dir = opendir("/proc");                                                                                      \
    if(dir) {                                                                                                         \
        char filename[384];                                                                                           \
        char name[256];                                                                                               \
        FILE *status = 0;                                                                                             \
        struct dirent *next;                                                                                          \
        while((next = readdir(dir)) != NULL) {                                                                        \
            if(!strcmp(next->d_name, "..") || !isdigit(*next->d_name)) continue; /*返回目录或不是数字开头的目录忽略*/ \
                                                                                                                      \
            snprintf(filename, sizeof(filename), "/proc/%s/status", next->d_name);                                                       \
            status = fopen(filename, "r");                                                                            \
            if(status) {                                                                                              \
                /*读取该文件头一行, 通常格式为 Name: GNapp*/                                                          \
                if(fscanf(status, "%s %s", filename, name) == 2) {

#endif
#include <sys/wait.h>
GN_SYS_API int GT_ExeclSub(const char *exec_filename) {
    signal(SIGCHLD, SIG_IGN); //忽略子进程结束的信号
    pid_t pid = fork(); //创建一个子进程
    if(pid == -1) return -1;
    if(pid == 0) {
        execl(exec_filename, exec_filename, NULL);
        _exit(-1); //如果执行失败，退出子进程
    }
    int status = 0;
    waitpid(pid, &status, WNOHANG); //父进程，不等待子进程结束，立即返回子进程的状态
    return WIFEXITED(status) ? 0 : -1;
}
GN_SYS_API int GT_ExitProc(const char *proc_filename) {
    pid_t pid = -1;
#ifdef _MAC_
    GT_TMP_FIND_PROCS_INFOS(
        if(!strcmp(result[pi].kp_proc.p_comm, proc_filename)) {
            pid = result[pi].kp_proc.p_pid;
            break;
        })
#else
    GT_TMP_FIND_PROCSS
    if(!strcmp(proc_filename, name)) {
        pid = (unsigned int)atol(next->d_name);
        fclose(status);
        break;
    }
}
fclose(status);
}
}
closedir(dir);
}
#endif
    if(pid == -1) return -1;
    return (!kill(pid, SIGTERM)) ? 0 : -1;
}
#endif
