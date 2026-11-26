// GN_mini 平台实现片段：seek 重定义 + GB2312/UTF8 编码转换。
// 2026-11-26 自 GN_Sys.cpp 第 28-220 行 move-only 拆出（REFACTOR_PLAN §13 B6）。
// 由 GN_Sys.cpp 按原顺序包含（单编译单元，#ifdef 平台分支语义不变）；不是独立头文件。
/* 引擎所使用到标准库的一些内容 * 如果相关平台没有这些标准库函数则在这里有它的相关处理实现以确保应用程序总能顺利调用到这些内容*********/
//标准文件操作 -----------------------------------------------------------------------------------------------------------
#ifdef REDEF_SEEK_SET
GN_SYS_API FILE *fopen(const char *_Filename, const char *_Mode) {
    return 0;
}
GN_SYS_API int fclose(FILE *_File) {
    return -1;
}
GN_SYS_API size_t fread(void *_DstBuf, size_t _ElementSize, size_t _Count, FILE *_File) {
    return 0;
}
GN_SYS_API size_t fwrite(const void *_Str, size_t _Size, size_t _Count, FILE *_File) {
    return 0;
}
GN_SYS_API int fscanf(FILE *_File, const char *_Format, ...) {
    return -1;
}
GN_SYS_API int fprintf(FILE *_File, const char *_Format, ...) {
    return -1;
}
GN_SYS_API int fgetc(FILE *_File) {
    return -1;
}
GN_SYS_API char *fgets(char *_Buf, int _MaxCount, FILE *_File) {
    return 0;
}
GN_SYS_API int fseek(FILE *_File, long _Offset, int _Origin) {
    return -1;
}
GN_SYS_API long ftell(FILE *_File) {
    return -1;
}
GN_SYS_API int feof(FILE *_File) {
    return -1;
}
#endif
//其它一些文件管理 -----------------------------------------------------------------------------------------------------------
#if !defined _WIN32_ && !defined _WIN32_WCE
GN_SYS_API int mkdir(const char *_Path) {
    return mkdir(_Path, S_IRWXU);
}
#endif
#if !defined _WIN32_ && !defined _WIN32_WCE
GN_SYS_API char *itoa(int _Val, char *_DstBuf, int _Radix) {
    if(_Radix == 2) {
        bool set = false;
        char *pc = _DstBuf;
        unsigned int i = 1 << 31, v = *((unsigned int *)&_Val);
        for(; i != 1; i >>= 1) {
            if(set) {
                *pc = (v & i) ? '1' : '0';
                ++pc;
            } else if(v & i) {
                *pc = '1';
                set = true;
                ++pc;
            }
        }
        if(set) {
            *pc = (v & i) ? '1' : '0';
            ++pc;
        } else if(v & i) {
            *pc = '1';
            set = true;
            ++pc;
        }
        *pc = 0;
    } else {
        sprintf(_DstBuf, _Radix == 16 ? "%x" : (_Radix == 8 ? "%o" : "%d"), _Val);
    }
    return _DstBuf;
}
GN_SYS_API char *strupr(char *pstr) {
    char *str = pstr;
    while(*str) {
        if(*str >= 'a' && *str <= 'z') *str += 'A' - 'a';
        ++str;
    }
    return pstr;
}
#endif
/*************************************************************************************************************************************************/
char GT_CurAppDir[256] = {0}, GT_CurAppDirW[256] = {0}; //保存当前运行程序文件所在目录如"C:/app/" 注意这里的目录后均包含目录分隔符号'/'
unsigned char GT_CurAppDirLen = 0, GT_CurAppDirLenW = 0; //当前运行程序文件所在目录的字符串长度

GN_SYS_API char *const GN_DocDir(const char *path_file) {
    strncpy(GT_CurAppDirW + GT_CurAppDirLenW, path_file, sizeof(GT_CurAppDirW) - GT_CurAppDirLenW - 1);
    GT_CurAppDirW[sizeof(GT_CurAppDirW) - 1] = 0;
    return (char *const)GT_CurAppDirW;
}
//返回GB2312编码字符串对应的UTF8字符串所占字节数(GB2312编码字符串)
int GN_StrLenGB2312ToUTF8(const char *gb) {
    char *pstr = (char *)gb;
    int len = 0;
    while(*pstr) {
        if(*pstr & 128) {
            len += 3;
            pstr += 2;
        } else {
            ++len;
            ++pstr;
        }
    }
    return len;
}
//返回UTF8字符串缓存可输入GB2312编码字符串的长度(GB2312编码字符串, UTF8字符串缓冲大小字节数)
int GN_StrGetUTF8BufCanInputGB2312Len(const char *gb, int buf_size) {
    char *pstr = (char *)gb;
    int len = 0;
    --buf_size;
    while(*pstr && len < buf_size) {
        if(*pstr & 128) {
            len += 3;
            pstr += 2;
        } else {
            ++len;
            ++pstr;
        }
    }
    return pstr - gb;
}
//返回UTF8编码字符串对应的GB2312字符串所占字节数(UTF8编码字符串)
int GN_StrLenUTF8ToGB2312(const char *utf) {
    char *pstr = (char *)utf;
    int len = 0;
    while(*pstr) {
#define GT_TMP_UTF8SIZEtoGB2312SIZE   \
    if(*pstr & 128) {                 \
        if(*pstr & 64) {              \
            if(*pstr & 32) pstr += 3; \
            else pstr += 2;           \
        } else ++pstr;                \
        len += 2;                     \
    } else {                          \
        ++len;                        \
        ++pstr;                       \
    }

        GT_TMP_UTF8SIZEtoGB2312SIZE
    }
    return len;
}
//返回GB2312字符串缓存可输入UTF8编码字符串的长度(GB2312编码字符串, GB2312字符串缓冲大小字节数)
int GN_StrGetGB2312BufCanInputUTF8Len(const char *utf, int buf_size) {
    char *pstr = (char *)utf;
    int len = 0;
    --buf_size;
    while(*pstr && len < buf_size) {
        GT_TMP_UTF8SIZEtoGB2312SIZE
    }
    return pstr - utf;
}
#if defined _MAC_ || defined _LINUX_ && !defined GT_USING_GLes //纯LINUX
#define GT_TMP_STR_ENCODE_CONVERT(FUNC, DST, SRC)                 \
    int FUNC(char *outbuf, char *inbuf, size_t outlen = 409600) { \
        size_t inlen = strlen(inbuf) + 1;                         \
        iconv_t cd = iconv_open(DST, SRC);                        \
        if(cd) {                                                  \
            iconv(cd, &inbuf, &inlen, &outbuf, &outlen);          \
            iconv_close(cd);                                      \
        }                                                         \
        return outlen;                                            \
    }
GT_TMP_STR_ENCODE_CONVERT(iconv_GB2312toUTF8, "utf-8", "gb2312")
GT_TMP_STR_ENCODE_CONVERT(iconv_UTF8toGB2312, "gb2312", "utf-8")

#define GT_StrToSysCode(out, in) iconv_GB2312toUTF8(out, (char *)(in))
/*
#elif defined GT_USING_GLUT   && defined _MAC_
 #include<GL/mac_ext.h>
 #define GT_StrToSysCode( out , in ) glut_mac_GB2312toUTF8(out, (char*)(in))

#elif defined GT_USING_GLsdl  && defined _MAC_
 #define GT_StrToSysCode( out , in ) SDL_MAC_GB2312toUTF8(out, in)
*/
#elif defined GT_USING_IOS
#define GT_StrToSysCode(out, in)

#elif defined GT_USING_ANDROID_JNI && defined _GN_MAIN_H_
void GT_StrToSysCode(char *out, const char *in, bool to_utf8 = true) {
}
#define GT_StrUTF8ToGB2312(OUT, IN) GT_StrToSysCode(OUT, IN, false)
#else
#define GT_StrToSysCode(out, in) \
    do {                         \
        strncpy(out, in, 383);   \
        out[383] = 0;            \
    } while(0)
#endif
#ifndef GT_StrUTF8ToGB2312
#define GT_StrUTF8ToGB2312 iconv_UTF8toGB2312
#endif
