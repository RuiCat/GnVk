// GN_FormatZip.cpp — ZIP 压缩/解包：cZIP 实现 + libzip 源码纳入（原 GN_Format.cpp 的 zip 区块，按功能拆分）。
#include "GN.h"
#ifdef _GN_FORMAT_H_
/*********************************************** zip 压缩解压摸块 *********************************************************************/
#ifdef GN_USING_UNZIP
#include <libzip/zip.c>
    extern "C" {
#include <libzip/unzip.c>
    }
    int GNd::cZIP::Open(const char *filename_mem, int memsize, int level, int is_add) {
        zFile = zipOpen(filename_mem, memsize, is_add);
        if(zFile) {
            zLevel = level;
            return 0;
        } else return -1;
    }
    int GNd::cZIP::Close() {
        if(!zFile) return -1;
        int ret = zipClose(zFile);
        zFile = 0;
        return ret;
    }
#ifdef GN_DEBUG
#define GT_ZIP_Error(FF)                                                                                                                             \
    if(!zFile) {                                                                                                                                     \
        GN_Log("cZIP::%s() 失败! zip 还没打开! 请先调用 cZIP::Open(...)!\n" /*"cZIP::%s() falie! zip do not Open! please cZIP::Open(...)!\n"*/, FF); \
    }
#else
#define GT_ZIP_Error(FF)
#endif
    // ZipCurProcFileName 在类里是外部声明（char[] 不完整类型），容量只能在此同源登记。
    static const int GT_ZIP_PATH_BUF_SIZE = 384;
    int GNd::cZIP::AddDir(const char *in_zip_name, const char *g_name, bool add_all_sub__) {
        GT_ZIP_Error("AddDir") if(g_name) {
            GN::sLIST<P<GNf::sFILE_INFO>> f_list, d_list;
            if(GNf::FindFileDirInfoData(g_name, true, &f_list, &d_list) == -1 || d_list.empty()) {
                GN_Log("cZIP::AddDir 不存在目录: %s!\n" /*"cZIP::AddDir do not directory: %s!\n"*/, g_name);
                return -1;
            }
            return AddDir(in_zip_name, d_list.front(), add_all_sub__);
        }
        else {
#define GT_TMP_ZIP_ADD_DIR_NAME_MAKE                                 \
    char *_dir = ZipCurProcFileName;                                 \
    int len = strlen(in_zip_name);                                   \
    /* 加固（STRCPY 审计）：ZipCurProcFileName 固定 384 字节，装不下就不拼。 */ \
    if(len + 2 > (int)GT_ZIP_PATH_BUF_SIZE) return -1;    \
    if(_dir != (char *)in_zip_name) strncpy(_dir, in_zip_name, len); \
    memcpy(_dir + len, "/", sizeof("/"))

            GT_TMP_ZIP_ADD_DIR_NAME_MAKE;
            zip_fileinfo zf;
            zipMakeFileInfo(&zf, _A_SUBDIR);
            if(zipOpenNewFileInZip(zFile, _dir, &zf, 0, 0, 0, 0, 0, zLevel ? Z_DEFLATED : 0, zLevel) >= 0) {
                zipCloseFileInZip(zFile);
                return 0;
            } else return -1;
        }
    }
    int GNd::cZIP::AddFile(const char *in_zip_name, const char *g_name_mem, int memsize) {
#ifdef GN_DEBUG
        GT_ZIP_Error("AddFile") if(!g_name_mem) GN_Log("cZIP::AddFile() 失败! 参数:g_name_mem 不能为空的!\n" /*"cZIP::AddFile() falie! parame: g_name_mem is empty!\n"*/);
#endif
        if(memsize < GNf::eFILE_MEM) {
            GN::sLIST<P<GNf::sFILE_INFO>> f_list, d_list;
            if(GNf::FindFileDirInfoData(g_name_mem, true, &f_list, &d_list) == -1 || f_list.empty()) {
                GN_Log("cZIP::AddFile 不存在文件: %s!\n" /*"cZIP::AddFile do not file: %s!\n"*/, g_name_mem);
                return -1;
            }
            return AddFile(in_zip_name, f_list.front());
        } else { //直接加内存文件
            // 加固（STRCPY 审计）：ZipCurProcFileName 固定 384 字节，超长名直接失败。
            if(strlen(in_zip_name) >= GT_ZIP_PATH_BUF_SIZE) return -1;
            if((char *)in_zip_name != (char *)ZipCurProcFileName) memcpy(ZipCurProcFileName, in_zip_name, strlen(in_zip_name) + 1);
            zip_fileinfo zf;
            zipMakeFileInfo(&zf, _A_NORMAL);
            int ret = 0;
            if(zipOpenNewFileInZip(zFile, in_zip_name, &zf, 0, 0, 0, 0, 0, zLevel ? Z_DEFLATED : 0, zLevel) >= 0) {
                float *_porg = &ZipCurProcFileProgress;
                *_porg = 0.0f;
                if(zipWriteInFileInZip(zFile, (voidp)g_name_mem, memsize) < 0) {
                    GN_Log("cZIP::AddFile 压缩文件数据时有错!\n" /*"cZIP::AddFile zip file data error!\n"*/);
                    ret = -1;
                }
                *_porg = 1.0f;
                zipCloseFileInZip(zFile);
            } else ret = -1;
            return ret;
        }
    }
    int GNd::cZIP::AddDir(const char *in_zip_name, const GNf::sFILE_INFO *f_info, bool add_all_sub__) {
        GT_TMP_ZIP_ADD_DIR_NAME_MAKE;
        zip_fileinfo zf;
        zipGetFileInfo(&zf, f_info);
        int ret = 0;
        if(zipOpenNewFileInZip(zFile, _dir, &zf, 0, 0, 0, 0, 0, zLevel ? Z_DEFLATED : 0, zLevel) >= 0) {
            zipCloseFileInZip(zFile);
            if(add_all_sub__) {
                ++len;
                char find_str[384];
                int gdir_len = strlen(f_info->global_filename);
                GN::sLIST<P<GNf::sFILE_INFO>> f_list, d_list;
                // 加固（STRCPY 审计）：装得下 "/*.*"+NUL 才拼，否则记录失败。
                if(gdir_len + 5 <= (int)sizeof(find_str)) {
                    strncpy(find_str, f_info->global_filename, gdir_len);
                    memcpy(find_str + gdir_len, "/*.*", sizeof("/*.*"));
                    GNf::FindFileDirInfoData(find_str, true, &f_list, &d_list);
                } else ret = -1;
                GN::sLIST<P<GNf::sFILE_INFO>>::iterator itf, etf;
                GNf::sFILE_INFO *p_info;

                if(f_list.size()) {
                    itf = f_list.begin(), etf = f_list.end();
                    for(; itf != etf; ++itf) {
                        p_info = (*itf);
                        // 加固（STRCPY 审计）：路径总长必须装进 ZipCurProcFileName[384]。
                        if(len + (int)strlen(p_info->name) + 1 > (int)GT_ZIP_PATH_BUF_SIZE) { ret = -1; continue; }
                        memcpy(_dir + len, p_info->name, strlen(p_info->name) + 1);
                        if(AddFile(_dir, p_info) < 0) {
                            GN_ErrorLast.New(384);
                            snprintf(GN_ErrorLast, 384, "压缩目录里的文件: %s 中途有错!", p_info->global_filename);
                            ret = -1;
                        }
                    }
                }
                if(d_list.size()) {
                    char *_name;
                    itf = d_list.begin(), etf = d_list.end();
                    for(; itf != etf; ++itf) {
                        p_info = (*itf);
                        _name = p_info->name;
                        if(_name[0] == '.' && ((_name[1] == '.' && _name[2] == 0) || _name[1] == 0)) {
                            ;
                        } else {
                            if(len + (int)strlen(_name) + 1 > (int)GT_ZIP_PATH_BUF_SIZE) { ret = -1; continue; }
                            memcpy(_dir + len, _name, strlen(_name) + 1);
                            if(AddDir(_dir, p_info, add_all_sub__) < 0) ret = -1;
                        }
                    }
                }
            }
        } else ret = -1;
        return ret;
    }
    int GNd::cZIP::AddFile(const char *in_zip_name, const GNf::sFILE_INFO *f_info) {
        // 加固（STRCPY 审计）：ZipCurProcFileName 固定 384 字节，超长名直接失败。
        if(strlen(in_zip_name) >= GT_ZIP_PATH_BUF_SIZE) return -1;
        if((char *)in_zip_name != (char *)ZipCurProcFileName) memcpy(ZipCurProcFileName, in_zip_name, strlen(in_zip_name) + 1);
        zip_fileinfo zf;
        zipGetFileInfo(&zf, f_info);
        int ret = 0;
        if(zipOpenNewFileInZip(zFile, in_zip_name, &zf, 0, 0, 0, 0, 0, zLevel ? Z_DEFLATED : 0, zLevel) >= 0) {
            bool eg = GNf::sFILE::EnableGlobalPath;
            GNf::sFILE::EnableGlobalPath = true;
            GNf::sFILE *fp = GNf::Open((char *)f_info->global_filename, "rb", GNf::eFILE_STD);
            GNf::sFILE::EnableGlobalPath = eg;
            if(fp) {
                int buf_size = GN_USING_FILE_BUF << 3;
                int f_size = GNf::GetSize(fp), _size = 0, _r_size;
                char *buf = new char[(f_size > buf_size) ? buf_size : f_size];
                float &porg = ZipCurProcFileProgress, porg_ratio = 1.0f / (float)f_size;
                porg = 0.0f;
                while(_size < f_size) {
                    _r_size = GNf::Read(buf, 1, ((f_size - _size) > buf_size) ? buf_size : (f_size - _size), fp);
                    if(_r_size > 0) {
                        if(zipWriteInFileInZip(zFile, buf, _r_size) < 0) {
                            GN_Log("cZIP::AddFile 压缩 %s 文件数据时有错!\n" /*"cZIP::AddFile zip %s file data error!\n"*/, f_info->global_filename);
                            ret = -1;
                            break;
                        } else {
                            _size += _r_size;
                            porg = _size * porg_ratio;
                        }
                    } else {
                        GN_Log("cZIP::AddFile 读取 %s 文件数据时有错!\n" /*"cZIP::AddFile read %s file data error!\n"*/, f_info->global_filename);
                        ret = -1;
                        break;
                    }
                }
                delete[] buf;
                GNf::Close(fp);
            } else ret = -1;
            zipCloseFileInZip(zFile);
        } else ret = -1;
        return ret;
    }
    GN::sNUMBER<uint8> GT_ZipIDs;
    unzFile GT_ZipPacks[32];
    GN_API int GNd::ZipPackOpen(const char *zip_pak_name_mem, int memsize) {
        unzFile uf = unzOpen(zip_pak_name_mem, memsize);
        if(uf) {
            if(GT_ZipIDs.ProbeID() < 32) {
                int zid = (int)GT_ZipIDs.GetID();
                GT_ZipPacks[zid] = uf;
                return -(zid + 1);
            }
        }
        return 0;
    }
    GN_API void GNd::ZipPackClose(int zp_id) {
        if(zp_id < 0) {
            int zid = -(zp_id + 1);
            unzClose(GT_ZipPacks[zid]);
            GT_ZipIDs.ReSetIDToData((uint8)zid);
        }
    }
    GN_API uint8 *GNd::UnzipLoadFileData(const char *zip_pak_name_mem, const char *inzipfile, unsigned int *size, int memsize) {
#ifdef GN_DEBUG
        if(!zip_pak_name_mem || !inzipfile) { GN_Log("UnzipLoadFileData 没有指定好压缩包文件名或须解压的文件名!\n" /*"UnzipLoadFileData do not assigned file name or in zip file!\n"*/); }
#endif
        unzFile uf = unzOpen(zip_pak_name_mem, memsize);
        if(unzLocateFile(uf, inzipfile, 0) != UNZ_OK) {
            GN_Log("文件 %s 不存在于 %s 压缩包里!\n" /*"file %s not found in the %s zipfile\n"*/, inzipfile, (memsize < GNf::eFILE_MEM) ? zip_pak_name_mem : "\0");
            unzClose(uf);
            return 0;
        }
        static const char *unzErrorStr = "错误 %d 在于解压调用 %s\n"; //"error %d with zipfile in %s\n"
        unz_file_info file_info;
        char filename_inzip[384];
        int err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, 384, 0, 0, 0, 0);
        if(err != UNZ_OK) {
            GN_Log(unzErrorStr, err, "unzGetCurrentFileInfo");
            unzClose(uf);
            return 0;
        }
        err = unzOpenCurrentFile(uf);
        if(err != UNZ_OK) {
            GN_Log(unzErrorStr, err, "unzOpenCurrentFile");
            unzClose(uf);
            return 0;
        }
        *size = file_info.uncompressed_size;
        uint8 *buf = new uint8[file_info.uncompressed_size];

        unzReadCurrentFile(uf, buf, file_info.uncompressed_size); //一次过读取整个文件
        unzCloseCurrentFile(uf);
        unzClose(uf);
        return buf;
    }
    GN_API int GNd::Unzip(const char *zip_pak_name_mem, const char *out_path, int memsize, bool remove_pak) {
#ifdef GN_DEBUG
        if(!zip_pak_name_mem) { GN_Log("解压缩! 没有指定好压缩包文件名或数据!\n" /*"Unzip pack do not assigned zip file name!\n"*/); }
#endif
        unzFile unzf = unzOpen(zip_pak_name_mem, memsize);
        unsigned int name_len = 384;
        char *str, *read_name = ZipCurProcFileName;
        if(out_path && out_path[0]) {
            if(GNf::Access(out_path, 0)) {
                if(GNf::Mkdir(out_path) == -1) { //创建输出目录
                    unzClose(unzf);
                    GN_ErrorLast("创建输出目录失败!" /*"create output dir faile!"*/, 0);
                    return -1;
                }
            }
            unsigned int p_len = strlen(out_path);
            strncpy(read_name, out_path, p_len);
            read_name[p_len++] = '/';
            read_name[p_len] = 0;
            name_len -= p_len;
            str = (char *)(read_name + p_len);
        } else str = read_name;

        bool (*on_unzip_file_begin)(const char *, const void **, int) = OnUnZipFileBeginCallBack;
        void (*on_unzip_file_end)(const char *) = OnUnZipFileEndCallBack;
        unz_file_info file_info;
        GNf::sFILE *outfile = 0;
        float porg_ratio, &porg = ZipCurProcFileProgress;
        int f_size, _size, r_size, _w_size, buf_size = GN_USING_FILE_BUF << 3;
        char *buf = new char[buf_size], *fname, err_msg[384];
        do {
            if(unzGetCurrentFileInfo(unzf, &file_info, str, name_len, 0, 0, 0, 0) != UNZ_OK) {
#define GT_TMP_UNZ_RETURN(VAL) \
    delete[] buf;              \
    unzClose(unzf);            \
    return VAL

                GN_ErrorLast("解压时获得其中一个文件信息失败!" /*"unzGetCurrentFileInfo faile!"*/, 0);
                GT_TMP_UNZ_RETURN(-2);
            }
            f_size = file_info.uncompressed_size;
            if(!f_size && str[file_info.size_filename - 1] == '/') {
                if(GNf::Access(read_name, 0)) {
                    if(GNf::Mkdir(read_name) == -1) { //是目录和该目录不存在时就创建它
                        GN_ErrorLast("创建压缩包里的目录失败!" /*"create in pack dir faile!"*/, 0);
                        GT_TMP_UNZ_RETURN(-3);
                    }
                }
            } else if(strcmp(str + (file_info.size_filename - 3), ".db")) { //过滤掉.db文件
                if(on_unzip_file_begin) {
                    if(!on_unzip_file_begin(read_name, (const void **)&unzf, f_size)) continue;
                }
                if(unzOpenCurrentFile(unzf) != UNZ_OK) {
                    snprintf(err_msg, sizeof(err_msg), "解压缩文件定位打开文件: %s 失败!" /*"unzipfile in open file: %s faile!"*/, read_name);
                    GN_ErrorLast(err_msg, 0);
                    GT_TMP_UNZ_RETURN(-4);
                }
                _size = 0;
                outfile = GNf::Open(read_name, "wb", GNf::eFILE_STD);
                if(!outfile) { //文件解压目标位置的目录可能还未创建就创建它
                    fname = GN_StrRemoveFileName(read_name);
                    GNf::Mkdir(read_name);
                    *(fname - 1) = '/';
                    outfile = GNf::Open(read_name, "wb", GNf::eFILE_STD);
                    if(!outfile) goto GT_GOTO_UnZipOnceFileEnd;
                }
                porg_ratio = 1.0f / (float)f_size;
                porg = 0.0f;
                while(_size < f_size) {
                    r_size = ((f_size - _size) > buf_size) ? buf_size : (f_size - _size);
                    if(unzReadCurrentFile(unzf, buf, r_size) >= 0) {
                        _w_size = GNf::Write(buf, 1, r_size, outfile);
                        if(_w_size != r_size) {
                            snprintf(err_msg, sizeof(err_msg), "解压缩文件 %s 写入解压好的数据时失败!" /*"unzipfile: %s write data faile!"*/, read_name);
                            GN_ErrorLast(err_msg, 0);
                            break;
                        }
                    } else {
                        snprintf(err_msg, sizeof(err_msg), "解压缩文件 %s 的数据时失败!" /*"unzReadCurrentFile: %s faile!"*/, read_name);
                        GN_ErrorLast(err_msg, 0);
                        break;
                    }
                    _size += _w_size;
                    porg = _size * porg_ratio;
                }
                GNf::Close(outfile);
            GT_GOTO_UnZipOnceFileEnd:
                unzCloseCurrentFile(unzf);
                if(on_unzip_file_end) on_unzip_file_end(read_name);
                if(_size < f_size) {
                    snprintf(err_msg, sizeof(err_msg), "解压文件: %s 时遇到错误,解压缩被中断!", read_name);
                    GN_ErrorLast(err_msg, 0);
                    GT_TMP_UNZ_RETURN(-5);
                }
            }
        } while(unzGoToNextFile(unzf) != UNZ_END_OF_LIST_OF_FILE);

        delete[] buf;
        unzClose(unzf);
        if(remove_pak && memsize == GNf::eFILE_STD) GNf::Del(zip_pak_name_mem);
        return 0;
    }
    GN_API int GNd_UnzOpenCurrentFile(unzFile file) {
        return unzOpenCurrentFile(file);
    }
    GN_API int GNd_UnzReadCurrentFile(unzFile file, voidp buf, unsigned len) {
        return unzReadCurrentFile(file, buf, len);
    }
    GN_API int GNd_UnzCloseCurrentFile(unzFile file) {
        return unzCloseCurrentFile(file);
    }

    GN_API char GNd::ZipCurProcFileName[GT_ZIP_PATH_BUF_SIZE] = {0};
    GN_API float GNd::ZipCurProcFileProgress = 1.0f;
    GN_API bool (*GNd::OnUnZipFileBeginCallBack)(const char *, const void **, int) = 0;
    GN_API void (*GNd::OnUnZipFileEndCallBack)(const char *) = 0;
#endif

#endif //_GN_FORMAT_H_
