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

#include<GN.h>
#ifdef _GN_SYS_H_
/* 引擎所使用到标准库的一些内容 * 如果相关平台没有这些标准库函数则在这里有它的相关处理实现以确保应用程序总能顺利调用到这些内容*********/
//标准文件操作 -----------------------------------------------------------------------------------------------------------
#ifdef REDEF_SEEK_SET
GN_SYS_API FILE *  fopen(const char * _Filename, const char * _Mode){ return 0; }
GN_SYS_API int     fclose(FILE * _File){ return -1; }
GN_SYS_API size_t  fread(void * _DstBuf,  size_t _ElementSize, size_t _Count,  FILE * _File){ return 0; }
GN_SYS_API size_t  fwrite(const void * _Str,  size_t _Size,  size_t _Count,  FILE * _File){ return 0; }
GN_SYS_API int     fscanf(FILE * _File, const char * _Format, ...){ return -1; }
GN_SYS_API int     fprintf(FILE * _File,const char * _Format, ...){ return -1; }
GN_SYS_API int     fgetc(FILE * _File){ return -1; }
GN_SYS_API char *  fgets(char * _Buf,int _MaxCount, FILE * _File){ return 0; }
GN_SYS_API int     fseek(FILE * _File, long _Offset, int _Origin){ return -1; }
GN_SYS_API long    ftell(FILE * _File){ return -1; }
GN_SYS_API int     feof (FILE * _File){ return -1; }
#endif
//其它一些文件管理 -----------------------------------------------------------------------------------------------------------
#if !defined _WIN32_ && !defined _WIN32_WCE
GN_SYS_API int      mkdir(const char * _Path){ return mkdir(_Path , S_IRWXU); }
#endif
#if !defined _WIN32_ && !defined _WIN32_WCE
GN_SYS_API char * itoa(int _Val, char * _DstBuf, int _Radix){
	if(_Radix == 2){
		bool set = false;
		char * pc = _DstBuf;
		unsigned int i = 1 << 31, v = *((unsigned int *)&_Val);
		for(; i != 1; i >>= 1){
			if( set ){ *pc = (v & i)? '1' : '0'; ++pc; }
			else if(v & i){ *pc = '1'; set = true; ++pc; }
		}
		if( set ){ *pc = (v & i)? '1' : '0'; ++pc; }
		else if(v & i){ *pc = '1'; set = true; ++pc; }
        *pc = 0;
	}else{
		sprintf(_DstBuf, _Radix == 16 ? "%x" : (_Radix == 8 ? "%o" : "%d"), _Val);
	}
	return _DstBuf;
}
GN_SYS_API char* strupr(char *pstr){
    char *str = pstr;
    while(*str){
        if(*str >= 'a' && *str <= 'z')*str += 'A' - 'a';
        ++str;
    }
    return pstr;
}
#endif
/*************************************************************************************************************************************************/
char          GT_CurAppDir[256] = {0},  GT_CurAppDirW[256] = {0};  //保存当前运行程序文件所在目录如"C:/app/" 注意这里的目录后均包含目录分隔符号'/'
unsigned char GT_CurAppDirLen   = 0,    GT_CurAppDirLenW   = 0;    //当前运行程序文件所在目录的字符串长度

GN_SYS_API char *const GN_DocDir(const char *path_file){
    strncpy(GT_CurAppDirW+GT_CurAppDirLenW, path_file, sizeof(GT_CurAppDirW)-GT_CurAppDirLenW-1);
    GT_CurAppDirW[sizeof(GT_CurAppDirW)-1] = 0;
    return (char *const)GT_CurAppDirW;
}
//返回GB2312编码字符串对应的UTF8字符串所占字节数(GB2312编码字符串)
int GN_StrLenGB2312ToUTF8(const char *gb){
    char *pstr = (char*)gb;
    int len = 0;
    while(*pstr){
        if(*pstr & 128){ len += 3; pstr += 2; }
        else           { ++len;  ++pstr; }
    }
    return len;
}
//返回UTF8字符串缓存可输入GB2312编码字符串的长度(GB2312编码字符串, UTF8字符串缓冲大小字节数)
int GN_StrGetUTF8BufCanInputGB2312Len(const char *gb, int buf_size){
    char *pstr = (char*)gb;
    int len = 0;
    --buf_size;
    while(*pstr && len < buf_size){
        if(*pstr & 128){ len += 3; pstr += 2; }
        else           { ++len;  ++pstr; }
    }
    return pstr - gb;
}
//返回UTF8编码字符串对应的GB2312字符串所占字节数(UTF8编码字符串)
int GN_StrLenUTF8ToGB2312(const char *utf){
    char *pstr = (char*)utf;
    int len = 0;
    while(*pstr){
        #define GT_TMP_UTF8SIZEtoGB2312SIZE \
        if(*pstr & 128){\
            if(*pstr & 64){\
                if(*pstr & 32)pstr += 3;\
                else pstr += 2;\
            }else ++pstr;\
            len += 2;\
        }else          { ++len;  ++pstr; }

        GT_TMP_UTF8SIZEtoGB2312SIZE
    }
    return len;
}
//返回GB2312字符串缓存可输入UTF8编码字符串的长度(GB2312编码字符串, GB2312字符串缓冲大小字节数)
int GN_StrGetGB2312BufCanInputUTF8Len(const char *utf, int buf_size){
    char *pstr = (char*)utf;
    int len = 0;
    --buf_size;
    while(*pstr && len < buf_size){
        GT_TMP_UTF8SIZEtoGB2312SIZE
    }
    return pstr - utf;
}
#if defined _MAC_ || defined _LINUX_ && !defined GT_USING_GLes //纯LINUX
#define GT_TMP_STR_ENCODE_CONVERT( FUNC , DST , SRC ) \
int FUNC (char *outbuf,char *inbuf, size_t outlen = 409600){\
    size_t inlen = strlen(inbuf)+1;\
    iconv_t cd = iconv_open( DST , SRC );\
    if(cd){\
        iconv(cd,&inbuf,&inlen,&outbuf,&outlen);\
        iconv_close(cd);\
    }\
    return outlen;\
}
GT_TMP_STR_ENCODE_CONVERT( iconv_GB2312toUTF8 , "utf-8" , "gb2312" )
GT_TMP_STR_ENCODE_CONVERT( iconv_UTF8toGB2312 , "gb2312" , "utf-8" )

#define GT_StrToSysCode( out , in ) iconv_GB2312toUTF8(out,(char*)(in))
/*
#elif defined GT_USING_GLUT   && defined _MAC_
 #include<GL/mac_ext.h>
 #define GT_StrToSysCode( out , in ) glut_mac_GB2312toUTF8(out, (char*)(in))

#elif defined GT_USING_GLsdl  && defined _MAC_
 #define GT_StrToSysCode( out , in ) SDL_MAC_GB2312toUTF8(out, in)
*/
#elif defined GT_USING_IOS
 #define GT_StrToSysCode( out , in ) 

#elif defined GT_USING_ANDROID_JNI && defined _GN_MAIN_H_
void GT_StrToSysCode(char* out , const char *in, bool to_utf8=true){
  
}
#define GT_StrUTF8ToGB2312( OUT , IN ) GT_StrToSysCode(OUT, IN, false)
#else
 #define GT_StrToSysCode( out , in ) do { strncpy(out, in, 383); out[383] = 0; } while(0)
#endif
#ifndef GT_StrUTF8ToGB2312
#define GT_StrUTF8ToGB2312 iconv_UTF8toGB2312
#endif
//标准查找目录操作 -------------------------------------------------------------------------------------------------------
#ifdef REDEF__A_SUBDIR
struct GT_sDIR
{
    DIR   *dir;
    char   sch_str[384];
};
GN_SYS_API intptr_t _findfirst(const char * _Filename, _finddata_t * _fd, char *proc_buf){
    GT_sDIR *dir0 = 0;
    if(!proc_buf){
        dir0 = new GT_sDIR;
        proc_buf = dir0->sch_str;
    }
    char *filename = proc_buf+128;
    int len = strlen(_Filename);
    strncpy(filename, _Filename, len); filename[len--] = 0;
    char *dirpath = "/", *_filename = 0;
    
    while(len >= 0 && filename[len] != '/' && filename[len] != '\\'){
        if(filename[len] == '.')_filename = filename + len;
        --len;
    }
    if(len >= 0){ //有 '/' 符
        if(len){
            filename[len] = 0;
            dirpath = filename;
        }
    }else dirpath = ".";
    ++len;
    
    DIR *dir;
    if(_filename){
        if(_filename > filename){
            if(*(_filename-1) == '*'){
                if(*(_filename+1) == '*')_filename = 0;
            }else{
                if(*(_filename+1) == '*')*(_filename+1) = 0;
                _filename = filename + len;
            }
        }else _filename = 0;
        dir = opendir(dirpath);
    }else{
        _filename = filename + len;
        dir = opendir(dirpath);
        if(!dir && len > 1){
            filename[len-1] = '/';
            len += strlen(_filename) + 1;
            dirpath = filename;
            dir = opendir(dirpath);
        }
    }
    if(_filename){ strncpy(proc_buf, _filename, 383); proc_buf[383] = 0; }
    else proc_buf[0] = 0;
    
    if(dir){
        if(dirpath == filename)filename[len-1] = '/';
        else{
            filename[0] = dirpath[0];
            if(dirpath[0] == '.'){
                filename[1] = '/';
                len = 2;
            }else len = 1;
        }
        ((uint8*)(filename-1))[0] = (uint8)len;
        for(;;){
            struct dirent *node = readdir(dir);
            if(node){
               #define GT_TMP_STAT_TO_FINDDATA( CT , AT ) \
                if(proc_buf[0] && !strstr(node->d_name, proc_buf))continue;\
                if(_fd->attrib == 0xff){ strncpy(_fd->name, node->d_name, sizeof(_fd->name)-1); _fd->name[sizeof(_fd->name)-1] = 0; }\
                else GT_StrUTF8ToGB2312(_fd->name, node->d_name);\
                strncpy(filename+len, node->d_name, 384 - 128 - len - 1); proc_buf[384 - 1] = 0;\
                struct stat st_buf;\
                if(!stat(filename, &st_buf)){\
                    _fd->time_create = st_buf. CT ;\
                    _fd->time_write  = st_buf.st_mtime;\
                    _fd->time_access = st_buf. AT ;\
                    _fd->size        = st_buf.st_size;\
                    _fd->attrib      = (st_buf.st_mode & S_IFDIR)? _A_SUBDIR : 0;\
                    if(!(st_buf.st_mode & S_IWUSR))_fd->attrib |= _A_RDONLY;\
                }
               #ifdef _MAC_
                GT_TMP_STAT_TO_FINDDATA( st_ctime , st_atime )
               #else
                GT_TMP_STAT_TO_FINDDATA( st_atime , st_ctime )
               #endif
                if(dir0){
                    dir0->dir = dir;
                    return (intptr_t)dir0;
                }else return (intptr_t)dir;
            }else{
                closedir(dir);
                break;
            }
        }
    }
    if(dir0)delete dir0;
    return -1;
}
GN_SYS_API int _findnext(intptr_t _FindHandle, _finddata_t * _fd, char *proc_buf){
    DIR *dir = (DIR*)_FindHandle;
    if(!proc_buf){
        dir = ((GT_sDIR*)_FindHandle)->dir;
        proc_buf = ((GT_sDIR*)_FindHandle)->sch_str;
    }
    char *filename = proc_buf+128;
    int len = (int)(((uint8*)(filename-1))[0]);
    for(;;){
        struct dirent *node = readdir(dir);
        if(node){
           #ifdef _MAC_
            GT_TMP_STAT_TO_FINDDATA( st_ctime , st_atime )
           #else
            GT_TMP_STAT_TO_FINDDATA( st_atime , st_ctime )
           #endif
            return 0;
        }else break;
    }
    return -1;
}
GN_SYS_API int _findclose(intptr_t _FindHandle, char *proc_buf){
    if(proc_buf)return closedir((DIR*)_FindHandle);
    else if(_FindHandle){
        DIR *_dir = ((GT_sDIR*)_FindHandle)->dir;
        delete ((GT_sDIR*)_FindHandle);
        return closedir(_dir);
    }
    return -1;
}
#endif
/*************************************************** Sys模块必须实现的东西 ************************************************************/

GN_SYS_API char *const GN_StrGetModuleExtName(GN_eMODULE_TYPE type){
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
	switch(type){
	case GN_eMODULE_EXE: return (char*const)strEXE;
	case GN_eMODULE_DLL: return (char*const)strDLL;
	default: return "\0";
	}
}
GN_SYS_API char *const GN_StrGetSysName(){
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
	return (char*const)sysName;
}
GN_SYS_API char* GN_StrToSysCode2(char *buf/*[384]*/, const char *in){
    #if defined _WIN32_ || defined _WIN32_WCE
     return (char*)in;
    #else
     GT_StrToSysCode( buf , in );
     return buf;
    #endif
}
GN_SYS_API void GN_StrToSysCode(char *out,const char *in){
   #ifdef _ICONV_T
    if(out == in){
        char buf0[8192];
        int len = strlen(in);
        char *in2 = len >= 8192 ? (char*)malloc(len+1) : buf0;
        strncpy(in2, in, len); in2[len] = 0;
        GT_StrToSysCode( out , in2 );
        if(in2 != (char*)buf0)free(in2);
    }else
   #endif
    { GT_StrToSysCode( out , in ); }
}
GN_SYS_API char* GN_StrToSysFilePath2(char *buf/*[384]*/, const char *src, const char *open_mode, bool is_g_dir){
	if(is_g_dir)return GN_StrToSysCode2(buf, src); //全局绝对路径就直接转码就可

   #ifdef _WIN32_WCE
   #elif defined _LINUX_ || defined _MAC_ || defined GT_USING_IOS
	uint8 i = 0;
    #ifdef _ICONV_T
     char  PSTRBUF[384];
    #else
     #define PSTRBUF buf
    #endif
	uint8 i2 = i;
	while(*src){
	    if(*src == '\\')PSTRBUF[i] = '/';  //非windows下还要把路径分割符转换一下
		else PSTRBUF[i] = *src;
		++src; ++i;
	}
    PSTRBUF[i] = 0;
     //gb2312 char to UTF8 string
	 #if defined GT_USING_IOS || defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
	 #else
	 GT_StrToSysCode(buf, PSTRBUF);
	 #endif
	 //GN_Log("%s\n",buf);

   #else /*_WIN32_*/ //-------------------------------------------------------------------
    return (char*)src;
   #endif
    return buf;
}
GN_SYS_API void GN_StrToSysFilePath(char des[],const char *src,const char *open_mode, bool is_g_dir){
    char *ret = GN_StrToSysFilePath2(des,src,open_mode,is_g_dir);
    if(ret == src){ strncpy(des, ret, 383); des[383] = 0; } // TODO: verify buffer size, assumed 384
}
#if defined(_WIN32_) || defined(_WIN32_WCE)
#include<windows.h>
GN_SYS_API int GN_StrWCharToChar(const void *p_wchar, void *p_char, int char_buf_len){
    return WideCharToMultiByte(CP_ACP,0,(WCHAR*)p_wchar,-1,(char*)p_char,char_buf_len,0,0);
}
GN_SYS_API int GN_StrCharToWChar(const void *p_char, void *p_wchar, int wchar_buf_len){
    return MultiByteToWideChar(CP_ACP,0,(char*)p_char,-1,(WCHAR*)p_wchar,wchar_buf_len);
}
static int GT_StrCharConvert(unsigned short src_fmt, unsigned short dst_fmt, const void *src, void *dst, int char_buf_len){
    WCHAR tmpstr[2048], *_tmpstr;
    int tlen;
	if(char_buf_len)tlen = char_buf_len;
	else tlen = MultiByteToWideChar(src_fmt,0,(char*)src,-1,0,0);
	_tmpstr = tlen <= 2048 ? tmpstr : new WCHAR[tlen];
         tlen = MultiByteToWideChar(src_fmt,0,(char*)src,-1,_tmpstr,tlen);

	if(!char_buf_len){
		char_buf_len = WideCharToMultiByte(dst_fmt,0,_tmpstr,tlen,0,0,0,0);
        *((char**)dst) = new char[char_buf_len];
                       WideCharToMultiByte(dst_fmt,0,_tmpstr,tlen,*((char**)dst),char_buf_len,0,0);
	}else char_buf_len=WideCharToMultiByte(dst_fmt,0,_tmpstr,tlen,  (char *)dst, char_buf_len,0,0);

	if(_tmpstr != tmpstr)delete [] _tmpstr;
	return char_buf_len;
}
GN_SYS_API int GN_StrUTF8ToGB2312(const void *utf8, void *gb, int char_buf_len){
    return GT_StrCharConvert(CP_UTF8, CP_ACP, utf8, gb, char_buf_len);
}
GN_SYS_API int GN_StrGB2312ToUTF8(const void *gb, void *utf8, int char_buf_len){
    return GT_StrCharConvert(CP_ACP, CP_UTF8, gb, utf8, char_buf_len);
}
GN_SYS_API int GT_ExeclSub(const char *exec_filename){
	if(spawnl(P_NOWAIT, exec_filename, "7", 0) == -1)return -1;
	else return 0;
}
#include<tlhelp32.h>
GN_SYS_API int GT_ExitProc(const char *proc_filename){
	int rc = -1;
    HANDLE hSysSnapshot = NULL;
    hSysSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS , 0);
    if(hSysSnapshot == (HANDLE) - 1)return -1;
    PROCESSENTRY32 proc;
    proc.dwSize = sizeof(proc);
    if(Process32First( hSysSnapshot, &proc )){
        do{
			if(!stricmp(proc.szExeFile, proc_filename)){
                HANDLE Proc_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc.th32ProcessID);
                if(Proc_handle==NULL || !TerminateProcess(Proc_handle, 0))rc = -1;
                else rc = 0;
			}
        }while(Process32Next( hSysSnapshot, &proc ));
	}
    CloseHandle( hSysSnapshot );
    return rc;
}
#else
GN_SYS_API int GN_StrWCharToChar(const void *p_wchar, void *p_char, int char_buf_len){
    unsigned short *pwstr = (unsigned short*)p_wchar;
    if(char_buf_len){
       #ifdef GT_USING_ANDROID_JNI
        return 0;
       #else
        size_t inlen = strlen((char*)pwstr)+1;
        size_t outlen = char_buf_len;
        iconv_t cd = iconv_open( "gb2312" , "utf-16be" );
        if (cd == 0)return 0;
        iconv(cd,(char**)&pwstr,&inlen,(char**)&p_char,&outlen);
        iconv_close(cd);
        return char_buf_len - outlen;
       #endif
    }else{
        while(*pwstr){
            if(*pwstr > 127)char_buf_len += 2;
            else ++char_buf_len;
            ++pwstr;
        }
        return char_buf_len + 1;
    }
}
GN_SYS_API int GN_StrCharToWChar(const void *p_char, void *p_wchar, int wchar_buf_len){
    char *pstr = (char*)p_char;
    if(wchar_buf_len){
       #ifdef GT_USING_ANDROID_JNI
        return 0;
       #else
        size_t inlen = strlen(pstr)+1;
        size_t outlen = wchar_buf_len<<1;
        iconv_t cd = iconv_open("utf-16be" , "gb2312");
        if (cd == 0)return 0;
        iconv(cd,(char**)&pstr,&inlen,(char**)&p_wchar,&outlen);
        iconv_close(cd);
        return wchar_buf_len - (outlen>>1);
       #endif
    }else{
        while(*pstr){
            if(*pstr & 128)pstr += 2;
            else ++pstr;
            ++wchar_buf_len;
        }
        return wchar_buf_len + 1;
    }
}
#ifdef GT_USING_ANDROID_JNI
#else
GN_SYS_API int GN_StrUTF8ToGB2312(const void *gb, void *utf8, int char_buf_len){
   #define GT_TEMP_STR_CHAR_CONVERT( STR_LEN , STR_CPY , ADDPROC ) \
    if(char_buf_len){\
        ADDPROC \
        return char_buf_len - STR_CPY((char*)utf8, (char*)gb, (size_t)char_buf_len);\
	}else{\
        char_buf_len = STR_LEN((char*)gb) + 1;\
        *((char**)utf8) = new char[char_buf_len];\
        STR_CPY(*((char**)utf8), (char*)gb, (size_t)char_buf_len);\
		return char_buf_len;\
	}
    GT_TEMP_STR_CHAR_CONVERT( GN_StrLenUTF8ToGB2312 , iconv_UTF8toGB2312 ,  )
}
GN_SYS_API int GN_StrGB2312ToUTF8(const void *gb, void *utf8, int char_buf_len){
   #define GT_TEMP_STR_CHAR_COPY_CONVERT_ \
    if((void*)gb == utf8){\
        char buf0[8192];\
        int len = strlen((char*)gb);\
        char *in2 = len >= 8192 ? (char*)malloc(len+1) : buf0;\
        strncpy(in2, (char*)gb, len); in2[len] = 0;\
        char_buf_len -= iconv_GB2312toUTF8((char*)utf8, (char*)in2, (size_t)char_buf_len);\
        if(in2 != (char*)buf0)free(in2);\
        return char_buf_len;\
    }
    GT_TEMP_STR_CHAR_CONVERT( GN_StrLenGB2312ToUTF8 , iconv_GB2312toUTF8 , GT_TEMP_STR_CHAR_COPY_CONVERT_ )
}
#endif
#ifdef _MAC_
#include <signal.h>
#include <GL/mac_ext.h>
#define GT_TMP_FIND_PROCS_INFOS( PROC_FUNC ) \
    int                 err;\
    kinfo_proc *        result = NULL;\
    bool                done   = false;\
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };\
    size_t              length;\
    do {\
        /*assert(result == NULL);*/\
        length = 0;\
        err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,NULL, &length, NULL, 0);\
        if (err == -1)err = errno;\
            \
            if (err == 0) {\
                result = (kinfo_proc *)malloc(length);\
                if (result == NULL) {\
                    err = ENOMEM;\
                }\
            }\
        if (err == 0) {\
            err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,result, &length,NULL, 0);\
            if (err == -1) {\
                err = errno;\
            }\
            if (err == 0) {\
                size_t pi = 0, p_count = length / sizeof(kinfo_proc);\
                for(; pi < p_count; pi++){\
                    PROC_FUNC \
                }\
                done = true;\
            } else if (err == ENOMEM) {\
                /*assert(result != NULL);*/\
                free(result);\
                result = NULL;\
                err = 0;\
            }\
        }\
    } while (err == 0 && ! done);\
    if(result != NULL){\
        free(result);\
        result = NULL;\
    }
#else
#define GT_TMP_FIND_PROCSS \
DIR *dir = opendir("/proc");\
if (dir){\
    char filename[384];\
    char name[256];\
    FILE * status = 0;\
    struct dirent *next;\
    while ((next = readdir(dir)) != NULL){\
        if (!strcmp(next->d_name, "..") || !isdigit(*next->d_name) )continue; /*返回目录或不是数字开头的目录忽略*/\
        \
        sprintf(filename, "/proc/%s/status", next->d_name);\
        status = fopen(filename, "r");\
        if (status){\
            /*读取该文件头一行, 通常格式为 Name: GNapp*/\
            if(fscanf(status, "%s %s", filename , name) == 2){
                
#endif
#include <sys/wait.h>
GN_SYS_API int GT_ExeclSub(const char *exec_filename){
    signal(SIGCHLD, SIG_IGN); //忽略子进程结束的信号
    pid_t pid = fork(); //创建一个子进程
    if (pid == -1)return -1;
    if (pid == 0) {
        execl(exec_filename, exec_filename, NULL);
        _exit(-1); //如果执行失败，退出子进程
    }
    int status = 0;
    waitpid(pid, &status, WNOHANG); //父进程，不等待子进程结束，立即返回子进程的状态
    return WIFEXITED(status) ? 0 : -1;
}
GN_SYS_API int GT_ExitProc(const char *proc_filename){
    pid_t pid = -1;
   #ifdef _MAC_
    GT_TMP_FIND_PROCS_INFOS( \
        if(!strcmp(result[pi].kp_proc.p_comm, proc_filename)){\
            pid = result[pi].kp_proc.p_pid;\
            break;\
        }\
    )
   #else
    GT_TMP_FIND_PROCSS
                    if(!strcmp(proc_filename, name)){
                        pid = (unsigned int )atol(next->d_name);
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
	if(pid == -1)return -1;
    return (!kill(pid, SIGTERM))? 0 : -1;
}
#endif
GN_SYS_API P<GT_sPROC_INFO> GT_GetSysRunProcAll(){
	GN::sLIST<GT_sPROC_INFO> proc_list;
   #if defined(_WIN32_) || defined(_WIN32_WCE)
    HANDLE hSysSnapshot = NULL;
    hSysSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS , 0);
	if (hSysSnapshot != (HANDLE) - 1){
        PROCESSENTRY32 proc;
        proc.dwSize = sizeof(proc);
        if(Process32First( hSysSnapshot, &proc )){
            GT_sPROC_INFO *cur_proc;
			do{
				cur_proc = proc_list.add(GT_sPROC_INFO());
                cur_proc->name(proc.szExeFile, 0);
				cur_proc->pid = proc.th32ProcessID;

            }while(Process32Next( hSysSnapshot, &proc ));
		}
        CloseHandle( hSysSnapshot );
	}
   #elif defined _MAC_
    GT_sPROC_INFO *cur_proc;
    GT_TMP_FIND_PROCS_INFOS( \
        cur_proc = proc_list.add(GT_sPROC_INFO());\
        cur_proc->name(result[pi].kp_proc.p_comm,0);\
        cur_proc->pid = (unsigned int )result[pi].kp_proc.p_pid;\
    )
   #elif defined _LINUX_/* && !defined GT_USING_GLes*/
    GT_TMP_FIND_PROCSS
		            GT_sPROC_INFO *cur_proc = proc_list.add(GT_sPROC_INFO());
                    cur_proc->name(name,0);
					cur_proc->pid = (unsigned int )atol(next->d_name);
                }
                fclose(status);
			}
        }
        closedir(dir);
	}
   #endif
	if(proc_list.empty())return P_();
	else return P<GT_sPROC_INFO>(proc_list);
}
GN_SYS_API unsigned int GT_GetSysCoreCount(){
   #if defined( _WIN32_) || defined(_WIN32_WCE)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (unsigned int)si.dwNumberOfProcessors;
   #else /*if defined (_LINUX_)*/
    return sysconf(_SC_NPROCESSORS_CONF);
   #endif
}
char *GT_StrPasteBuffer =  0;
#if defined( _WIN32_) || defined(_WIN32_WCE)
GN_SYS_API char const* GN_StrGetPasteText(){
	GN_DeleteArray(GT_StrPasteBuffer);
	if(IsClipboardFormatAvailable(CF_TEXT)){ //确定剪贴板是否含有CF_TEXT格式的数据
        OpenClipboard(0 /*(HWND)GNg::sWINDOW::Hwnd*/);		 //打开剪贴板
        HGLOBAL hGlobal = GetClipboardData(CF_TEXT);		 //获得引用文本的内存块句柄
        size_t clipSize = GlobalSize(hGlobal);
        GT_StrPasteBuffer = new char[clipSize]; //分配一块与剪贴板数据块大小相同的内存块
        LPVOID pGlobal = GlobalLock(hGlobal);
        strncpy(GT_StrPasteBuffer, (char*)pGlobal, clipSize - 1);
        GT_StrPasteBuffer[clipSize - 1] = 0;
        GlobalUnlock(hGlobal);
        CloseClipboard();
	}
	return GT_StrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text){
	OpenClipboard(0);						//打开剪贴板
    EmptyClipboard();						//清空剪贴板
	if(text){
		unsigned int len =  strlen(text);
        HGLOBAL hGlobal = GlobalAlloc(GHND|GMEM_SHARE, (SIZE_T)(len+1));
        LPVOID pGlobal = GlobalLock(hGlobal);
        strncpy((char*)pGlobal, text, len); ((char*)pGlobal)[len] = 0;
        GlobalUnlock(hGlobal);
        SetClipboardData(CF_TEXT, hGlobal); //把内存句柄交给剪贴板
	}
	CloseClipboard();
}
#elif defined(GT_USING_ANDROID_JNI)
GN_SYS_API char const* GN_StrGetPasteText(){
	return 0;
}
GN_SYS_API void GN_StrSetPasteText(const char *text){

}
#elif defined _MAC_
GN_SYS_API char const* GN_StrGetPasteText(){
	char const* text = macGetPasteText();
    GN_StrcpyData( GT_StrPasteBuffer , text );
    return GT_StrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text){
	macSetPasteText(text?text:"");
}
#elif defined(_LINUX_)
#ifdef GN_HAS_X11_CLIPBOARD
#include <X11/Xlib.h>
#include <X11/Xatom.h>
struct GT_ClipboardX11 {
    Display *disp; Window win; Atom clip, utf8, targets, sel_prop; bool init;
    GT_ClipboardX11() : disp(0), win(0), init(false) {}
    ~GT_ClipboardX11() { if (win) XDestroyWindow(disp, win); if (disp) XCloseDisplay(disp); }
    void Setup() {
        if (init) return; disp = XOpenDisplay(NULL); if (!disp) return;
        int scr = DefaultScreen(disp);
        win = XCreateSimpleWindow(disp, RootWindow(disp, scr), 0,0,1,1,0,0,0);
        clip = XInternAtom(disp, "CLIPBOARD", False);
        utf8 = XInternAtom(disp, "UTF8_STRING", False);
        targets = XInternAtom(disp, "TARGETS", False);
        sel_prop = XInternAtom(disp, "GN_SEL_PROP", False); init = true;
    }
    char* ReadSelection(Atom selection) {
        Setup(); if (!disp) return 0;
        XConvertSelection(disp, selection, utf8, sel_prop, win, CurrentTime);
        XFlush(disp);
        XEvent ev;
        for (int retry = 0; retry < 40; ++retry) {
            if (XCheckTypedWindowEvent(disp, win, SelectionNotify, &ev)) {
                if (ev.xselection.property != None) {
                    Atom type; int fmt; unsigned long nitems, after; unsigned char *data = 0;
                    XGetWindowProperty(disp, win, sel_prop, 0, 65536/4, True, AnyPropertyType, &type, &fmt, &nitems, &after, &data);
                    if (data && type == utf8 && fmt == 8) {
                        char *out = new char[nitems + 1]; memcpy(out, data, nitems); out[nitems] = 0;
                        XFree(data); return out;
                    }
                    if (data) XFree(data);
                }
                break;
            }
            usleep(5000);
        }
        return 0;
    }
};
static GT_ClipboardX11 GT_X11Clip;
GN_SYS_API char const* GN_StrGetPasteText(){
    GN_DeleteArray(GT_StrPasteBuffer);
    char *text = GT_X11Clip.ReadSelection(GT_X11Clip.clip);
    if (!text) text = GT_X11Clip.ReadSelection(XA_PRIMARY);
    if (text) { GT_StrPasteBuffer = text; return GT_StrPasteBuffer; }
    FILE *fp = popen("wl-paste -t text 2>/dev/null", "r");
    if (fp) { char buf[4096] = {0}; if (fread(buf, 1, sizeof(buf)-1, fp) > 0) GN_StrcpyData(GT_StrPasteBuffer, buf); pclose(fp); }
    return GT_StrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text){
    GN_StrcpyData(GT_StrPasteBuffer, text ? text : "");
    if (GT_X11Clip.Setup(), GT_X11Clip.disp) {
        XSetSelectionOwner(GT_X11Clip.disp, XA_PRIMARY, GT_X11Clip.win, CurrentTime);
        XSetSelectionOwner(GT_X11Clip.disp, GT_X11Clip.clip, GT_X11Clip.win, CurrentTime);
        XEvent ev;
        while (XCheckTypedWindowEvent(GT_X11Clip.disp, GT_X11Clip.win, SelectionRequest, &ev)) {
            XSelectionEvent sev = {0};
            sev.type = SelectionNotify; sev.display = ev.xselectionrequest.display;
            sev.requestor = ev.xselectionrequest.requestor;
            sev.selection = ev.xselectionrequest.selection;
            sev.target = ev.xselectionrequest.target;
            sev.property = ev.xselectionrequest.property;
            sev.time = ev.xselectionrequest.time;
            if (ev.xselectionrequest.target == GT_X11Clip.targets) {
                Atom types[] = {GT_X11Clip.utf8, XA_STRING};
                XChangeProperty(GT_X11Clip.disp, ev.xselectionrequest.requestor,
                    ev.xselectionrequest.property, XA_ATOM, 32, PropModeReplace, (unsigned char*)types, 2);
            } else {
                XChangeProperty(GT_X11Clip.disp, ev.xselectionrequest.requestor,
                    ev.xselectionrequest.property, ev.xselectionrequest.target, 8, PropModeReplace,
                    (unsigned char*)GT_StrPasteBuffer, strlen(GT_StrPasteBuffer));
            }
            XSendEvent(GT_X11Clip.disp, ev.xselectionrequest.requestor, False, 0, (XEvent*)&sev);
        }
        XFlush(GT_X11Clip.disp);
    }
    if (text && text[0]) { FILE *fp = popen("wl-copy 2>/dev/null", "w"); if (fp) { fputs(text, fp); pclose(fp); } }
}
#else
GN_SYS_API char const* GN_StrGetPasteText(){
    GN_DeleteArray(GT_StrPasteBuffer);
    FILE *fp = popen("wl-paste -t text 2>/dev/null", "r");
    if (fp) { char buf[4096] = {0}; if (fread(buf, 1, sizeof(buf)-1, fp) > 0) GN_StrcpyData(GT_StrPasteBuffer, buf); pclose(fp); }
    return GT_StrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text){
    GN_StrcpyData(GT_StrPasteBuffer, text ? text : "");
    if (text && text[0]) { FILE *fp = popen("wl-copy 2>/dev/null", "w"); if (fp) { fputs(text, fp); pclose(fp); } }
}
#endif
#else
GN_SYS_API char const* GN_StrGetPasteText(){
	return GT_StrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text){
	GN_StrcpyData( GT_StrPasteBuffer , text );
}
#endif

#define GT_TEMP_CUR_APP_DIR_ADD_SPLIT( DIR , LEN ) \
	LEN = (unsigned char)strlen( DIR );\
    DIR [ LEN ] = '/';\
	LEN ++;\
    DIR [ LEN ] = 0

GN_SYS_API void GF_AppCurPathChange(char *app_path, char *doc_path){
    strncpy(GT_CurAppDir, app_path, sizeof(GT_CurAppDir)-1); GT_CurAppDir[sizeof(GT_CurAppDir)-1] = 0;
	GT_TEMP_CUR_APP_DIR_ADD_SPLIT( GT_CurAppDir , GT_CurAppDirLen );
	if(doc_path){
		strncpy(GT_CurAppDirW, doc_path, sizeof(GT_CurAppDirW)-1); GT_CurAppDirW[sizeof(GT_CurAppDirW)-1] = 0;
		GT_TEMP_CUR_APP_DIR_ADD_SPLIT( GT_CurAppDirW , GT_CurAppDirLenW );
	}
}
GN_SYS_API int GT_GetAppCurPath(char dir_out[]){
	if(GT_CurAppDir[0]){
        // TODO: verify buffer size - assumed 256
        strncpy(dir_out, GT_CurAppDir, 255); dir_out[255] = 0;
	    dir_out[GT_CurAppDirLen - 1] = 0;
		return (int)(GT_CurAppDirLen - 1);
	}else{
	    dir_out[0] = 0;
	    return 0;
	}
}
static void GT_GetCurrentDirectory(char * buf/*[256]*/){
    #ifdef _WIN32_
     GetCurrentDirectory(256,buf);
    #elif _WIN32_WCE
    #else
     getcwd(buf,256);
    #endif
}
GN_SYS_API void GT_GetAppFileName(char name_out[]){
	#ifdef _WIN32_
     GetModuleFileName(0,name_out,256);
    #elif _WIN32_WCE
    #else
     // TODO: verify buffer size - assumed 256
     strncpy(name_out, GT_CurAppDir, 255); name_out[255] = 0;
     #ifdef GT_USING_ANDROID_JNI
     #else
      #ifdef _GN_INIT_H
       if(GN_ExecArgs){
          strncpy(name_out+GT_CurAppDirLen, GN_StrGetFileName(GN_ExecArgs[0]), 255 - GT_CurAppDirLen);
          name_out[255] = 0;
       }else
      #endif
          strncpy(name_out+GT_CurAppDirLen, "GN_App", 255 - GT_CurAppDirLen);
          name_out[255] = 0;
     #endif
    #endif
}
#ifdef _WIN32_
 #ifndef _SHLOBJ_H_
  #include <shlobj.h>
 #endif
#endif
GN_SYS_API P< P<char> > GT_EnumSystemDirectory(){
	P< P<char> > dir_names;
    char dir[256], *p_dir;
    #if defined _WIN32_WCE
	#elif defined _WIN32_
	 char dir2[256];
	 GetLogicalDriveStringsA(256, dir2);
     p_dir = dir2;
	 unsigned int n = 0;
	 while(*p_dir){ p_dir += 4; ++n; }	//统计盘符个数
     dir_names.New(4+n);
	 P<char> *_dir_names = dir_names;

	 n = 0;
	 (_dir_names[0])(GT_CurAppDir,0); _dir_names[0][strlen((char*)_dir_names[0])-1] = 0;   //应用程序文件当前目录
     ++n;

	 SHGetSpecialFolderPathA(0/*(HWND)GNg::sWINDOW::Hwnd*/,dir,CSIDL_DESKTOPDIRECTORY,0); //Desktop的目录
	 (_dir_names[n])(dir,0); ++n;

	 SHGetSpecialFolderPathA(0/*(HWND)GNg::sWINDOW::Hwnd*/,dir,CSIDL_PERSONAL,0);      //我的文档
	 (_dir_names[n])(dir,0); ++n;
     /*
	 GetSystemDirectoryA(dir,256);
     (_dir_names[n])(dir,0); ++n;      //system32
     */
	 GetWindowsDirectoryA(dir,256);
	 (_dir_names[n])(dir,0); ++n;      //windows

	 p_dir = dir2;
	 while(*p_dir){
		 (_dir_names[n])(p_dir,0); //记录各个盘的目录
		  _dir_names[n][strlen((char*)_dir_names[n])-1] = 0;
		  p_dir += 4; ++n;
	 }
	#elif defined GT_USING_IOS
	#elif defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
	#elif defined _MAC_
     dir_names.New(5);
     P<char> *_dir_names = dir_names;
     (_dir_names[0])(GT_CurAppDir,0); _dir_names[0][strlen((char*)_dir_names[0])-1] = 0; //应用程序文件当前目录
     char tmpath[384];
     sprintf(tmpath, "/Users/%s", getlogin());
     (_dir_names[2])(tmpath,0);
     int remaining = sizeof(tmpath) - strlen(tmpath) - 1;
     strncat(tmpath, "/Desktop", remaining); tmpath[sizeof(tmpath)-1] = 0;
     (_dir_names[1])(tmpath,0);
     (_dir_names[3])("/usr/lib",0);
     (_dir_names[4])("/Volumes",0);
	#elif defined _LINUX_
     dir_names.New(2);
	 (dir_names[0])(GT_CurAppDir,0); dir_names[0][strlen((char*)dir_names[0])-1] = 0;   //应用程序文件当前目录
	 (dir_names[1])(getenv("HOME"),0);                                                  //用户主目录
    #endif
    return dir_names;
}
GN_SYS_API bool GN_SysShowFile(const char *filename){
   #if defined( _WIN32_) || defined(_WIN32_WCE)
	return ((unsigned int)ShellExecute(0, 0, filename, 0, 0, SW_SHOWNORMAL) > 32);
   #elif defined(GT_USING_ANDROID_JNI)
	return false;
   #elif defined(_MAC_)
    char t_file_name[384];
    return (bool)macSysShowFile(GN_StrToSysCode2(t_file_name,filename), filename[0]=='/'?0:1);
   #else
    return false;
   #endif
}
/**************************************************************** 时间API *********************************************************************/
#ifdef _WIN32_WCE
#include<mmsystem.h>
#pragma comment(lib,"mmtimer.lib")
#endif
#ifdef _WIN32_
#include<mmsystem.h>
#pragma comment(lib,"winmm.lib")

LARGE_INTEGER GT_TimeFreq;

double GT_TimeGetCountS(void){
    LARGE_INTEGER time_count;
    QueryPerformanceCounter(&time_count);
	return time_count.QuadPart / (GT_TimeFreq.QuadPart / 1000.0);
}
#endif

#ifdef GT_USING_GLsdl
GN_SYS_API void GN_TimeSleep(unsigned int sleep_time){ SDL_Delay(sleep_time); }
GN_SYS_API double GT_TimeGetCountN(void){ return (double)SDL_GetTicks(); }

#elif defined _WIN32_ || defined _WIN32_WCE
GN_SYS_API void GN_TimeSleep(unsigned int sleep_time){ Sleep(sleep_time); }
GN_SYS_API double GT_TimeGetCountN(void){ return (double)timeGetTime(); }

#else //其他LINUX 或 UINX
#if HAVE_NANOSLEEP || HAVE_CLOCK_GETTIME
#include <time.h>
#endif
#include <chrono>
GN_SYS_API void GN_TimeSleep(unsigned int sleep_time){
	int was_error;
   #if HAVE_NANOSLEEP /*-------------------------------------*/
	struct timespec elapsed, tv;
	elapsed.tv_sec = sleep_time/1000;
	elapsed.tv_nsec = (sleep_time%1000)*1000000;
	do {
		errno = 0;
        tv.tv_sec = elapsed.tv_sec;
		tv.tv_nsec = elapsed.tv_nsec;
		was_error = nanosleep(&tv, &elapsed);

	} while ( was_error && (errno == EINTR) );
   #else /*--------------------------------------------------*/
	struct timeval tv;
	unsigned int then, now, elapsed;
	then = (unsigned int)GT_TimeGetCountN();
	do {
		errno = 0;
		now = (unsigned int)GT_TimeGetCountN();
		elapsed = (now-then);
		then = now;
		if (elapsed >= sleep_time)break;
		sleep_time -= elapsed;
		tv.tv_sec  = sleep_time/1000;
		tv.tv_usec = (sleep_time%1000)*1000;
		was_error = select(0, NULL, NULL, NULL, &tv);
	} while ( was_error && (errno == EINTR) );
   #endif /*-------------------------------------------------*/
}
GN_SYS_API double GT_TimeGetCountN(void){
    static auto start_run_time = std::chrono::high_resolution_clock::now();
	return (double)(std::chrono::duration_cast< std::chrono::nanoseconds >(std::chrono::high_resolution_clock::now()- start_run_time).count()) * 0.000001;
}
#endif //end LINUX
GN_SYS_API GT_P_DOUBLE_VOID_FUNC GN_TimeGetCount = GT_TimeGetCountN;

/******************************************************************** 多线程 *****************************************************************/
struct GT_sTHREAD_ARG{
    void  * arg;
	void  * arg2;
    void (*fun)(P<GN_sTHREAD> , void *, void *);
	P<GN_sTHREAD>  tr;

	GT_sTHREAD_ARG(){}
	GT_sTHREAD_ARG(P<GN_sTHREAD> &_tr, void *_arg, void *_arg2, void (*_fun)(P<GN_sTHREAD> , void *,void *) ):tr(_tr), arg(_arg), arg2(_arg2), fun(_fun){}
};
GN::sLIST< GT_sTHREAD_ARG >  GT_UsThreadTable;

GN_SYS_API bool  GN_ThreadSupport = false;

static void GT_CheckMultiThreadCaps(P<GN_sTHREAD> this_thread, void *arg, void *arg2){
	GN_TimeSleep(10);
    GN_ThreadSupport = true;
}
GN_sTHREAD::GN_sTHREAD():id(0),us_state(1){}

void  GN_sTHREAD::UsFlagsAdd(uint8 fg){ us_state |= fg; GN_MemBarrier(); }
void  GN_sTHREAD::UsFlagsDel(uint8 fg){ us_state &=~fg; GN_MemBarrier(); }
bool  GN_sTHREAD::UsFlagsHas(uint8 fg){ return us_state & fg; }
void  GN_sTHREAD::UsStateAnd(uint8 mk){ us_state &= mk; GN_MemBarrier(); }
void  GN_sTHREAD::UsStateSet(uint8 st){ us_state = st;  GN_MemBarrier(); }
uint8 GN_sTHREAD::UsStateGet()        { return us_state; }

#if defined _WIN32_ || defined _WIN32_WCE
GN_sTHREAD::~GN_sTHREAD(){ 
	if(id){ CloseHandle((HANDLE)id); id = 0; } 
}
unsigned WINAPI GT_ThreadStartPoint( void * arg ){
    GT_sTHREAD_ARG * del_tr = (GT_sTHREAD_ARG *)arg;
	del_tr->fun(del_tr->tr, del_tr->arg, del_tr->arg2);
    GT_UsThreadTable.del(del_tr);
	return 0;
}
GN_SYS_API P<GN_sTHREAD> GN_ThreadStart( void (*proc)(P<GN_sTHREAD> this_thread, void *arg, void *arg2) , void *_arg , void *_arg2){
	P<GN_sTHREAD> p_tr(1);
    GT_sTHREAD_ARG *del_tr = GT_UsThreadTable.add( GT_sTHREAD_ARG(p_tr, _arg, _arg2, proc));
	unsigned pid;
    HANDLE porc = (HANDLE)::_beginthreadex(0, 0, GT_ThreadStartPoint, del_tr , 0/*CREATE_SUSPENDED*/, &pid);
	if(porc){
		//SetThreadPriority(porc, THREAD_PRIORITY_TIME_CRITICAL);	//设为高优先级
		//ResumeThread(porc);
		p_tr->id = porc;
		return p_tr;
	}else{
		GT_UsThreadTable.del(del_tr);
		return P_();
	}
}
GN_SYS_API bool GN_ThreadEndWait(const P<GN_sTHREAD> &pt){
    #ifdef GN_DEBUG
	if(!pt){ GN_Log("GN_ThreadEndWait parame is empty!\n"); }
    #endif
	if(::WaitForSingleObject((HANDLE)(pt->id),INFINITE) == WAIT_FAILED)return false;
	else return true;
}
GN_SYS_API void GT_ThreadExit(){ ::ExitThread(0); }

GN_SYS_API void *GN_ThreadCondCreate(){
    return CreateEvent(0, 1, 1, "prl");
}
GN_SYS_API void  GN_ThreadCondRemove(void *cond){
    CloseHandle(cond);
}
GN_SYS_API void  GN_ThreadCondWait(volatile void *cond){
    WaitForSingleObject((HANDLE)cond, INFINITE);
}
GN_SYS_API void  GN_ThreadCondCtrl(void *cond, uint8 state){
    if(state)SetEvent(cond);
    else   ResetEvent(cond);
}
#ifdef __LP64__
GN_SYS_API void GN_MemBarrier(){
	MemoryBarrier();
}
#endif
#else
GN_sTHREAD::~GN_sTHREAD(){  id = 0; }

void* GT_ThreadStartPoint( void * arg ){
    GT_sTHREAD_ARG * del_thread = (GT_sTHREAD_ARG *)arg;
	del_thread->fun(del_thread->tr, del_thread->arg, del_thread->arg2);
    #ifdef GT_USING_ANDROID_JNI
    if(GT_JEnv){ GT_Jvm->DetachCurrentThread(); GT_JEnv = 0; }
    #endif
    GT_UsThreadTable.del(del_thread);
	return 0;
}
GN_SYS_API P<GN_sTHREAD> GN_ThreadStart( void (*proc)(P<GN_sTHREAD> this_thread, void *arg, void *arg2) , void *_arg, void *_arg2){
	void * proc_id;
    P<GN_sTHREAD> p_thread(1);
    GT_sTHREAD_ARG * del_thread = GT_UsThreadTable.add( GT_sTHREAD_ARG(p_thread, _arg, _arg2, proc));
	int ret = pthread_create((pthread_t*)&proc_id, 0, GT_ThreadStartPoint, del_thread);
	if(ret != -1){
		/*存在问题:还未设为高优先级*/
		p_thread->id = proc_id;
		return p_thread;
	}else{
        GT_UsThreadTable.del(del_thread);
		return P_();
	}
}
GN_SYS_API bool GN_ThreadEndWait(const P<GN_sTHREAD> &pt){
	#ifdef GN_DEBUG
	if(!pt){ GN_Log("GN_ThreadEndWait parame is empty!\n"); }
    #endif
	if(pthread_join(*((pthread_t*)&(pt->id)), 0))return false;
	else return true;
}
GN_SYS_API void GT_ThreadExit(){ pthread_exit(0); }

struct GT_sTRCOND{
    pthread_mutex_t     mutex;
    pthread_cond_t      condine;
    volatile uint8      state;

    GT_sTRCOND():mutex(PTHREAD_MUTEX_INITIALIZER),condine(PTHREAD_COND_INITIALIZER),state(0){}
};
GN_SYS_API void *GN_ThreadCondCreate(){
    return new GT_sTRCOND;
}
GN_SYS_API void  GN_ThreadCondRemove(void *cond){
    delete ((GT_sTRCOND*)cond);
}
GN_SYS_API void  GN_ThreadCondWait(volatile void *cond){
    GT_sTRCOND *_cond = (GT_sTRCOND*)cond;
    pthread_mutex_lock(&(_cond->mutex));
    while(!_cond->state){
        pthread_cond_wait(&(_cond->condine), &(_cond->mutex));
    }
    pthread_mutex_unlock(&(_cond->mutex));
}
GN_SYS_API void  GN_ThreadCondCtrl(void *cond, uint8 state){
    ((GT_sTRCOND*)cond)->state = state; GN_MemBarrier();
    pthread_cond_broadcast(&(((GT_sTRCOND*)cond)->condine));
}
#if (defined _LINUX_ && defined GT_USING_GLes) || defined(_MAC_)
GN_SYS_API void GN_MemBarrier(){
    #if __cplusplus >= 201103L
    std::atomic_thread_fence(std::memory_order_seq_cst);
    #else
    __sync_synchronize();
    #endif
}
#endif
#endif
void GN_MutexLock(  GN_sMUTEX *mut){ while(InterlockedCompareExchange(mut,1,0)); }
void GN_MutexUnLock(GN_sMUTEX *mut){ InterlockedExchange(mut,0); }

/***************************************************************** 动态库 **********************************************************************/
#if defined _WIN32_ || defined _WIN32_WCE
GN_SYS_API int *GT_ModuleOpen(const char *module_name){
	const char *pstr = module_name;
	while(*pstr && *pstr != '/' && *pstr != '\\')++pstr;
	if(*pstr){
		char dll_path [1600], *env_path = getenv("PATH");
		unsigned int path_len = strlen(env_path);
		strncpy(dll_path, env_path, path_len);
		dll_path[path_len++] = ';';
		GN_StrGetDirName(module_name, dll_path + path_len);
		SetEnvironmentVariable("PATH", dll_path);
	}
   #ifndef _WIN32_WCE
	return (int*)LoadLibrary(module_name);
   #else
    return 0;
   #endif
}
GN_SYS_API void GT_ModuleClose(int *module){
    FreeLibrary((HMODULE)module);
}
GN_SYS_API GT_PFUNC_VOID GT_ModuleGetFunction(int *module, const char *func_name){
    return (GT_PFUNC_VOID)GetProcAddress((HMODULE)module, func_name);
}
#else
#include <dlfcn.h>  //依赖库搜索路径指定标记-WI,-rpath=
GN_SYS_API int *GT_ModuleOpen(const char *module_name){
   #if defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
    return 0;
   #else
    return (int*)dlopen(module_name,  RTLD_GLOBAL|RTLD_NOW);
   #endif
}
GN_SYS_API void  GT_ModuleClose(int *module){
    dlclose(module);
}
GN_SYS_API GT_PFUNC_VOID GT_ModuleGetFunction(int *module, const char *func_name){
    return (GT_PFUNC_VOID)dlsym(module, func_name);
}
#endif
/************************************************************************ 特殊外设IO ******************************************************************************/
#define GT_IO_IN_PORT_NUM 1
#define GT_IO_OUT_PORT_NUM 0
#define GT_MAX_INFV_NUM 3
int InitUsIO(){ return 0; }
void UnInitUsIO(){}
void ProcUsIO(){}
GN_SYS_API void GNio::EnableSensors(unsigned int use_flags){}
GN_SYS_API unsigned int GNio::GetSensorCaps(){ return 0; }
GN_SYS_API void GNio::OutPut(int IO_port, int status, int turn_num, int turn_time){}
GN_SYS_API void GNio::GetInCount(int IO_port, int *nCounter){}
GN_SYS_API bool GN::StartRockeyPass(const char *gameID){ return 1; }
GN_SYS_API bool GN::RockeyPass(){ return 1; }

GN_SYS_API void GNio::TicketsNumOut(int count){ }
GN_SYS_API int  GNio::GetOweTickets(){ return 0; }
GN_SYS_API bool GNio::IsNoTickets(){ return false; }

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
GN_SYS_API float  GNio::InFv[GT_MAX_INFV_NUM]     = { 0.0f };
GN_SYS_API bool   GNio::InPort[GT_IO_IN_PORT_NUM] = { 0 };

GN_SYS_API uint8 GNio::GetInFvNum(){ return GT_MAX_INFV_NUM; }
GN_SYS_API uint8 GNio::GetInPortNum(){ return GT_IO_IN_PORT_NUM; }
GN_SYS_API uint8 GNio::GetOutPutNum(){ return GT_IO_OUT_PORT_NUM; }

/*****************************************************************************************************************************************************************/
GN_SYS_API void GN_InitSys(){
	if(GT_SystemInited & (1<<5))return;
   #ifdef _WIN32_
    //检查超级计时器是否可用
	int _sTimeReady = QueryPerformanceFrequency(&GT_TimeFreq);
    if( _sTimeReady)GN_TimeGetCount = GT_TimeGetCountS;/*检查决定用边种计时器*/
   #endif
	GN_ThreadStart(GT_CheckMultiThreadCaps, 0);
	GN_ThreadSupport = false;

	if(!GT_CurAppDir[0]){ //如果系统入口之前没设置过当前目录就在这里获取一下
        GT_GetCurrentDirectory(GT_CurAppDir);
		GT_TEMP_CUR_APP_DIR_ADD_SPLIT( GT_CurAppDir , GT_CurAppDirLen );
	}
	InitUsIO();
	GT_SystemInited |= 1<<5;
}
GN_SYS_API void GN_UnInitSys(){
	GN_DeleteArray(GT_StrPasteBuffer);   //删除剪贴板内容
	UnInitUsIO();
    if(!GT_UsThreadTable.empty()){
       #ifdef GN_DEBUG
		GN_Log("还有线程 %d 个没有退出!!\n", GT_UsThreadTable.size());
       #endif
        GT_UsThreadTable.clear();
	}
	GT_SystemInited -= 1<<5;
}
GN_SYS_API void GN_SysProc(unsigned int nTime){
    ProcUsIO();
}
#if defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID

#elif defined(_LINUX_)
#include <sys/utsname.h>
#endif
GN_SYS_API char *const GN_GetInerfaceInfoSys(){
   #ifdef _WIN32_WCE
    static const char sysInfo[] = "WinCE 6";
   #elif defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
    static const char sysInfo[64] = "Android 10";
   #elif defined GT_USING_IOS
	static const char sysInfo[] = "IOS 9";
   #elif defined _WIN32_
	static char sysInfo[64] = "Windows ";
	char *sv  = sysInfo + 8;
	if(!sv[0]){
        sysInfo[63] = 0; // ensure null termination for strncpy
        OSVERSIONINFOEX os;
        os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        if(GetVersionEx((OSVERSIONINFO *)&os)){
            //下面根据版本信息判断操作系统名称
            switch(os.dwMajorVersion){ //判断主版本号
            case 4:
                switch(os.dwMinorVersion){//判断次版本号
                case 0:
                if     (os.dwPlatformId==VER_PLATFORM_WIN32_NT)     strncpy(sv, "NT 4.0", 55);
                else if(os.dwPlatformId==VER_PLATFORM_WIN32_WINDOWS)strncpy(sv, "95", 55);
                break;
            case 10: strncpy(sv, "98", 55); break;
            case 90: strncpy(sv, "Me", 55); break;
            }
            break;
        case 5:
            switch(os.dwMinorVersion){ //再比较dwMinorVersion的值
            case 0: strncpy(sv, "2000", 55); break;
            case 1: strncpy(sv, "XP", 55);   break;
            case 2:
				#define SM_SERVERR2 89
                if(GetSystemMetrics( SM_SERVERR2 )==0)strncpy(sv, "2003", 55);
                else                                  strncpy(sv, "2003 R2", 55);
                    break;
                }
                break;
            case 6:
                switch(os.dwMinorVersion){
                case 0:
                    if(os.wProductType==VER_NT_WORKSTATION)strncpy(sv, "Vista", 55);
                    else                                   strncpy(sv, "Server 2008", 55);
                    break;
                case 1:
                    if(os.wProductType==VER_NT_WORKSTATION)strncpy(sv, "7", 55);
                    else                                   strncpy(sv, "Server 2008 R2", 55);
                    break;
                case 2:
                    if(os.wProductType==VER_NT_WORKSTATION)strncpy(sv, "8", 55);
                    else                                   strncpy(sv, "Server 2012", 55);
                    break;
                case 3:
                    strncpy(sv, "8.1", 55);
                    break;
				}
                break;
            default:{
                HMODULE hModNtdll = 0;
                if (hModNtdll = ::LoadLibraryW(L"ntdll.dll")) {
                    typedef void (WINAPI *pfRTLGETNTVERSIONNUMBERS)(DWORD*, DWORD*, DWORD*);
                    pfRTLGETNTVERSIONNUMBERS pfGetVersionNum;
                    pfGetVersionNum = (pfRTLGETNTVERSIONNUMBERS)::GetProcAddress(hModNtdll, "RtlGetNtVersionNumbers");
                    if(pfGetVersionNum) {
                        DWORD dwMajorVer, dwMinorVer, dwBuildNum;
                        pfGetVersionNum(&dwMajorVer, &dwMinorVer, &dwBuildNum);
                        dwBuildNum &= 0x0ffff;
                        sprintf(sv, "%d.%d.%d", dwMajorVer, dwMinorVer, dwBuildNum);
                        ::FreeLibrary(hModNtdll);
                        break;
                    }
                    ::FreeLibrary(hModNtdll);
                }
                strncpy(sv , "X", 55);
                }break;
            }
		}else strncpy(sv , "?", 55);
	}
   #elif defined _MAC_
	static char sysInfo[64] = "";
    if(!sysInfo[0]){
        strncpy(sysInfo, "Mac OS X 10.5", sizeof(sysInfo)-1); sysInfo[sizeof(sysInfo)-1] = 0;
        FILE *fp = fopen("/System/Library/CoreServices/SystemVersion.plist", "r");
        if(fp){
            char info[384], *lable;
            while(fgets(info, 384, fp)){
                if(strstr(info,"ProductVersion")){
                    fgets(info, 384, fp);
                    lable = strstr(info, "<string>");
                    if(lable)sscanf(lable+8, "%[^<]", sysInfo+9);
                    break;
                }
            }
            fclose(fp);
        }
    }
   #elif defined _LINUX_
	static const char sysInfo[64] = "";
	if(!sysInfo[0]){
        struct utsname uinfos;
        if(uname(&uinfos)){ strncpy((char*)sysInfo, "Linux ?", 63); ((char*)sysInfo)[63] = 0; }
        else sprintf(sysInfo, "%s %s", uinfos.sysname, uinfos.release);
	}
   #endif
	return (char *const)sysInfo;
}
#ifdef _WIN64
#include <intrin.h>
#endif
GN_SYS_API void GN_GetDeviceInfoCpu(char *info){
   #ifdef _WIN64
	int cpuInfo[4] = { -1 };
	__cpuid(cpuInfo, 0x80000002);
	memcpy(info, cpuInfo, 16); info += 16;
	__cpuid(cpuInfo, 0x80000003);
	memcpy(info, cpuInfo, 16); info += 16;
	__cpuid(cpuInfo, 0x80000004);
	memcpy(info, cpuInfo, 16); info += 16;
	*info = 0;
   #elif defined(_WIN32_)
    #ifdef _MSC_VER
	unsigned int  *pinfo = (unsigned int  *)info;
	unsigned int  deax;
	unsigned int  debx;
	unsigned int  decx;
	unsigned int  dedx;
	unsigned int  veax = 0x80000002;
	for (; veax <= 0x80000004; ++veax){ //检测 0x80000002 到 0x80000004
		__asm
		{
			mov eax, veax
			cpuid
			mov deax, eax
			mov debx, ebx
			mov decx, ecx
			mov dedx, edx
		}
		*pinfo = deax; ++pinfo;
		*pinfo = debx; ++pinfo;
		*pinfo = decx; ++pinfo;
		*pinfo = dedx; ++pinfo;
	}
	*((char*)pinfo) = 0;
    #else
      asm( "movl $0, %%eax\n\t"
       "cpuid\n\t"
       "movl %%ebx, (%0)\n\t"
       "movl %%edx, 4(%0)\n\t"
       "movl %%ecx, 8(%0)\n\t"
       :
       :"D" (info)
       :"eax" , "ebx" , "ecx" , "edx"
       );
    #endif
   #elif defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
    // TODO: verify buffer size - info is char* parameter
    strncpy(info, "?.??GHz", 255); info[255] = 0;
    #elif defined _MAC_ || defined GT_USING_IOS
    size_t len = 256;
    sysctlbyname("machdep.cpu.brand_string", info, &len, NULL, 0);
   #else
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if(fp){
        int bsize = 64, len;
        while(fgets(info, bsize, fp)){
            len = strlen(info);
            info[len++] = ' ';
            info += len;
            bsize -= len;
        }
        if(bsize == 64)*info = 0;
        else *(info-1) = 0;
        fclose(fp);
    }
   #endif
}
GN_SYS_API void GN_GetDeviceInfoRam(char *info){
	#define  GBYTES  1073741824
	#define  MBYTES  1048576
	#define  KBYTESD 1024.0
	unsigned int  long tot_ram = 1024, avl_ram = 1024;
   #if defined _WIN32_ || defined _WIN32_WCE
	MEMORYSTATUSEX statusex;
	statusex.dwLength = sizeof(statusex);
	if(GlobalMemoryStatusEx(&statusex)){
		tot_ram = statusex.ullTotalPhys;
		avl_ram = statusex.ullAvailPhys;
	}
   #elif defined _MAC_ || defined GT_USING_IOS
    size_t len = sizeof(unsigned int  long);
    sysctlbyname("hw.memsize", &tot_ram, &len, NULL, 0);
   #else
    FILE *fp = fopen("/proc/meminfo", "r");
    if(fp){
        tot_ram = 0;
        char rbuf[256], *pstr;
        uint8 mk = 0;
        unsigned int  long size;
        while(fgets(rbuf, 256, fp) ){
            pstr = strstr(rbuf,"Mem");
            if(pstr){
                if(strstr(pstr+3,"Total:")){
                    sscanf(pstr+10, "%lld", &size);
                    tot_ram += size;
                    mk |= 1;
                }else if(strstr(pstr+3,"Free:")){
                    sscanf(pstr+9, "%lld", &size);
                    tot_ram += size;
                    mk |= 2;
                }else if(strstr(pstr+3,"Available:")){
                    sscanf(pstr+14, "%lld", &avl_ram);
                    mk |= 4;
                }
                if(mk == 7)break;
            }
        }
        fclose(fp);
        tot_ram *= 1024;
        avl_ram *= 1024;
    }
   #endif
	double decimal_total = 0.0, decimal_avl = 0.0;
	unsigned int  long remain_total = tot_ram % GBYTES/*1G字节*/;
	unsigned int  long total = tot_ram / GBYTES;
	//unsigned int  long avl = avl_ram / GBYTES;
	//unsigned int  long remain_avl = avl_ram % GBYTES;

	if(remain_total)decimal_total = (remain_total / MBYTES) / KBYTESD;
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
	void va_start(va_list vv, const char *fmtstr){  /*__builtin_va_start( vv , fmtstr);*/ }
	void va_end(va_list vv){ /* __builtin_va_end( vv );*/  }
	void Status_SetNewStatusByKey(char *str){ }
	float gDesiredBitRate = 1.0f;
}
#endif
#endif //endif _GN_SYS_H_
