// GN_mini 平台实现片段：REDEF__A_SUBDIR 目录查找操作。
// 2026-11-26 自 GN_Sys.cpp 第 221-355 行 move-only 拆出（REFACTOR_PLAN §13 B6）。
// 由 GN_Sys.cpp 按原顺序包含（单编译单元，#ifdef 平台分支语义不变）；不是独立头文件。
//标准查找目录操作 -------------------------------------------------------------------------------------------------------
#ifdef REDEF__A_SUBDIR
struct GT_sDIR {
    DIR *dir;
    char sch_str[384];
};
GN_SYS_API intptr_t _findfirst(const char *_Filename, _finddata_t *_fd, char *proc_buf) {
    GT_sDIR *dir0 = 0;
    if(!proc_buf) {
        dir0 = new GT_sDIR;
        proc_buf = dir0->sch_str;
    }
    char *filename = proc_buf + 128;
    int len = strlen(_Filename);
    strncpy(filename, _Filename, len);
    filename[len--] = 0;
    char *dirpath = (char *)"/", *_filename = 0;

    while(len >= 0 && filename[len] != '/' && filename[len] != '\\') {
        if(filename[len] == '.') _filename = filename + len;
        --len;
    }
    if(len >= 0) { //有 '/' 符
        if(len) {
            filename[len] = 0;
            dirpath = filename;
        }
    } else dirpath = (char *)".";
    ++len;

    DIR *dir;
    if(_filename) {
        if(_filename > filename) {
            if(*(_filename - 1) == '*') {
                if(*(_filename + 1) == '*') _filename = 0;
            } else {
                if(*(_filename + 1) == '*') *(_filename + 1) = 0;
                _filename = filename + len;
            }
        } else _filename = 0;
        dir = opendir(dirpath);
    } else {
        _filename = filename + len;
        dir = opendir(dirpath);
        if(!dir && len > 1) {
            filename[len - 1] = '/';
            len += strlen(_filename) + 1;
            dirpath = filename;
            dir = opendir(dirpath);
        }
    }
    if(_filename) {
        strncpy(proc_buf, _filename, 383);
        proc_buf[383] = 0;
    } else proc_buf[0] = 0;

    if(dir) {
        if(dirpath == filename) filename[len - 1] = '/';
        else {
            filename[0] = dirpath[0];
            if(dirpath[0] == '.') {
                filename[1] = '/';
                len = 2;
            } else len = 1;
        }
        ((uint8 *)(filename - 1))[0] = (uint8)len;
        for(;;) {
            struct dirent *node = readdir(dir);
            if(node) {
#define GT_TMP_STAT_TO_FINDDATA(CT, AT)                           \
    if(proc_buf[0] && !strstr(node->d_name, proc_buf)) continue;  \
    if(_fd->attrib == 0xff) {                                     \
        strncpy(_fd->name, node->d_name, sizeof(_fd->name) - 1);  \
        _fd->name[sizeof(_fd->name) - 1] = 0;                     \
    } else GT_StrUTF8ToGB2312(_fd->name, node->d_name);           \
    strncpy(filename + len, node->d_name, 384 - 128 - len - 1);   \
    proc_buf[384 - 1] = 0;                                        \
    struct stat st_buf;                                           \
    if(!stat(filename, &st_buf)) {                                \
        _fd->time_create = st_buf.CT;                             \
        _fd->time_write = st_buf.st_mtime;                        \
        _fd->time_access = st_buf.AT;                             \
        _fd->size = st_buf.st_size;                               \
        _fd->attrib = (st_buf.st_mode & S_IFDIR) ? _A_SUBDIR : 0; \
        if(!(st_buf.st_mode & S_IWUSR)) _fd->attrib |= _A_RDONLY; \
    }
#ifdef _MAC_
                GT_TMP_STAT_TO_FINDDATA(st_ctime, st_atime)
#else
                GT_TMP_STAT_TO_FINDDATA(st_atime, st_ctime)
#endif
                if(dir0) {
                    dir0->dir = dir;
                    return (intptr_t)dir0;
                } else return (intptr_t)dir;
            } else {
                closedir(dir);
                break;
            }
        }
    }
    if(dir0) delete dir0;
    return -1;
}
GN_SYS_API int _findnext(intptr_t _FindHandle, _finddata_t *_fd, char *proc_buf) {
    DIR *dir = (DIR *)_FindHandle;
    if(!proc_buf) {
        dir = ((GT_sDIR *)_FindHandle)->dir;
        proc_buf = ((GT_sDIR *)_FindHandle)->sch_str;
    }
    char *filename = proc_buf + 128;
    int len = (int)(((uint8 *)(filename - 1))[0]);
    for(;;) {
        struct dirent *node = readdir(dir);
        if(node) {
#ifdef _MAC_
            GT_TMP_STAT_TO_FINDDATA(st_ctime, st_atime)
#else
            GT_TMP_STAT_TO_FINDDATA(st_atime, st_ctime)
#endif
            return 0;
        } else break;
    }
    return -1;
}
GN_SYS_API int _findclose(intptr_t _FindHandle, char *proc_buf) {
    if(proc_buf) return closedir((DIR *)_FindHandle);
    else if(_FindHandle) {
        DIR *_dir = ((GT_sDIR *)_FindHandle)->dir;
        delete((GT_sDIR *)_FindHandle);
        return closedir(_dir);
    }
    return -1;
}
#endif
