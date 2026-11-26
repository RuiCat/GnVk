// GN_mini 图像编解码片段：TGA 文件头 + RLE 解包宏 + 加载/保存。
// 2026-11-26 自 GN_FormatImgFile.cpp 第 164-414 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_FormatImgFile.cpp 按原顺序包含（单编译单元）；不是独立头文件。
    struct GT_sTGA_HEADER //TGA文件头
    {
        uint8 imageInfoByteLen; //图像信息长度字节
        uint8 colorTableHas; //是否含有颜色表
        uint8 imageType; //图像类型 2:无压缩 10:压缩
        uint8 colorTableInfo[5]; //颜色表信息
        uint8 imageInfo[10]; //图像信息
    };
    uint8 GT_LoadTgaFileInfo(GT_sTGA_HEADER *_header, GNg::COLOR_FORMAT *format, int *w, int *h, GNf::sFILE *fp) {
        //memset(_header, 0, sizeof(GT_sTGA_HEADER));
        GNf::Read((char *)_header, 1, sizeof(GT_sTGA_HEADER), fp);
        if(_header->imageType != 2 && _header->imageType != 10) {
            GN_Log("读取 tga 文件失败! 遇到未能解析类型为:%d 的数据!\n", (int)_header->imageType);
            return 0;
        }
        *w = (_header->imageInfo[5] << 8) + _header->imageInfo[4];
        *h = (_header->imageInfo[7] << 8) + _header->imageInfo[6];
        uint8 bitn = _header->imageInfo[8];
        *format = GNg::ImgFmtGetFormat(bitn);
        return bitn;
    }
    GN_API uint8 *GNg::LoadTgaFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out, int memsize) {
#define GT_TMP_LOAD_TGA_FILE_INFO                            \
    GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize); \
    if(!fp) return 0;                                        \
    GT_sTGA_HEADER header;                                   \
    uint8 bitn = GT_LoadTgaFileInfo(&header, format, w, h, fp);

        GT_TMP_LOAD_TGA_FILE_INFO
        GNf::Close(fp);
        return 0;
    }
    GN_API uint8 *GNg::LoadTgaFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize, bool unpk) {
        GT_TMP_LOAD_TGA_FILE_INFO
        if(!bitn) {
            GNf::Close(fp);
            return 0;
        }
        /*if(*w <= 0 || *h <= 0){
		GNf::Close(fp);
		return 0;
    }*/
        char info[255] = {'\0'}; //读取图像信息
        GNf::Read(info, 1, header.imageInfoByteLen, fp);

        int pbyte = bitn >> 3;
        int data_size = (*w) * (*h) * pbyte;
        uint8 *data = new uint8[data_size];
        if(header.imageType == 2) { //对于非压缩格式
            GNf::Read((char *)data, 1, data_size, fp);

        } else if(header.imageType == 10) { //对于压缩格式
            int ofs = GNf::Tell(fp);
            GNf::Seek(fp, 0, GNf::eSEEK_END);
            int rsize = GNf::Tell(fp) - ofs;
            GNf::Seek(fp, ofs, GNf::eSEEK_START);
            uint8 *data1 = new uint8[rsize];
            GNf::Read((char *)data1, 1, rsize, fp); //一次性读过来再解析

            int pn = (*w) * (*h), pi = 0;
            uint8 chunk_flag, *_data1 = data1;
            if(bitn == 32) {
#define GT_TMP_TGA_IMG_DATA_UNPAK(TYPE, N)                  \
    TYPE *pd = (TYPE *)data;                                \
    do {                                                    \
        int i = 0;                                          \
        if(_data1[0] & 0x80) {                              \
            int n = (_data1[0] & 0x7f) + 1;                 \
            ++_data1;                                       \
            TYPE cc = *((TYPE *)_data1);                    \
            _data1 += N;                                    \
            for(; i < n && pi < pn; ++i, ++pi) pd[pi] = cc; \
        } else {                                            \
            int n = _data1[0] + 1;                          \
            ++_data1;                                       \
            for(; i < n && pi < pn; ++i, ++pi) {            \
                pd[pi] = *((TYPE *)_data1);                 \
                _data1 += N;                                \
            }                                               \
        }                                                   \
    } while(pi < pn);

                GT_TMP_TGA_IMG_DATA_UNPAK(unsigned int, 4)
            } else if(bitn == 24) {
                GT_TMP_TGA_IMG_DATA_UNPAK(GNg::sCOLOR3, 3)
            } else if(bitn == 16) {
                GT_TMP_TGA_IMG_DATA_UNPAK(unsigned short, 2)
            }
            delete[] data1;
        }
        GNf::Close(fp);
        if(unpk) {
            if(bitn != 16) unpk = false;
        }
        if(col_turn || unpk) {
            uint8 *data2 = new uint8[unpk ? (data_size >> 1) * 3 : data_size];
            if(unpk) { //BGR555 转 RGB888
                unsigned short *pkdata = (unsigned short *)data;
                uint8 *_data2 = data2;
                if(col_turn) {
                    int y = (*h) - 1, i, n = (*w);
                    for(; y >= 0; --y) {
                        pkdata = ((unsigned short *)data) + (n * y);
                        i = 0;
#define GT_TMP_UNPK_BGR555                                  \
    for(; i < n; ++i, _data2 += 3) {                        \
        _data2[0] = ((pkdata[i] & 32767) >> 10) * 255 / 31; \
        _data2[1] = ((pkdata[i] & 1023) >> 5) * 255 / 31;   \
        _data2[2] = (pkdata[i] & 31) * 255 / 31;            \
    }
                        GT_TMP_UNPK_BGR555
                    }
                } else {
                    int i = 0, n = (*w) * (*h);
                    GT_TMP_UNPK_BGR555
                }
                *format = GNg::CR_RGB;
            } else if(col_turn) {
                uint8 *_data1, *_data2 = data2, *_dend;
                int rn = (*w) * pbyte, y = (*h) - 1;
                for(; y >= 0; --y) {
                    _data1 = data + rn * y;
                    _dend = _data1 + rn;
                    for(; _data1 < _dend; _data1 += pbyte, _data2 += pbyte) {
                        _data2[2] = _data1[0];
                        _data2[1] = _data1[1];
                        _data2[0] = _data1[2];
                        if(pbyte > 3) _data2[3] = _data1[3];
                    }
                }
            }
            delete[] data;
            data = data2;
        } else {
            uint8 *_data1 = data, *_dend = data + (*w) * (*h) * pbyte;
            for(; _data1 < _dend; _data1 += pbyte) {
                GN_Swap(uint8, _data1[2], _data1[0]);
            }
        }
        return data;
    }
    GN_API bool GNg::SaveTgaFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn, uint8 comps) {
        GNf::sFILE *fp = GNf::Open(file_name, "wb", GNf::eFILE_STD);
        if(!fp) {
            GN_Log(GT_ERR_FileCreateFail, "tga", file_name);
            return false;
        }
        col_turn = !col_turn;
        GT_sTGA_HEADER header;
        memset(&header, 0, sizeof(GT_sTGA_HEADER));
        header.imageType = comps ? 10 : 2;
        header.imageInfo[4] = w & 255;
        header.imageInfo[5] = w >> 8;
        header.imageInfo[6] = h & 255;
        header.imageInfo[7] = h >> 8;
        short byte_n = ImgFmtGetByteNum(srcformat);
        short byte_n1 = byte_n;
        int srsize = w * byte_n;
        if(byte_n <= 2) {
            if(byte_n == 2) {
                if(srcformat == CR_LUMINANCE_ALPHA) byte_n = 4;
            } else if(srcformat == CR_ALPHA) byte_n = 4;
            else byte_n = 3;
        }
        header.imageInfo[8] = byte_n << 3;
        header.imageInfo[9] = byte_n == 4 ? 8 : 0;
        GNf::Write((char *)&header, 1, sizeof(GT_sTGA_HEADER), fp);
        int rsize = w * byte_n;
        uint8 buf[16384];
        GN::sTBUF_INT tbuf(buf, 16384);
        //一行行转换一行行写
        uint8 *data1 = (uint8 *)tbuf.GetSpace(comps ? rsize + (rsize >> 1) : rsize);
        int y1 = col_turn ? h - 1 : 0, y2 = col_turn ? -1 : h, add = col_turn ? -1 : 1;
        if(comps) {
            --comps;
            uint8 *_data1, *_k, *_data, *__data, *_end, chunk;
            PFUNC_PIX_CV pfunc_pix_cv = PixRGBturnFuncChoose(srcformat);
            for(; y1 != y2; y1 += add) {
                _data = data + y1 * srsize;
                _end = _data + srsize;

#define GT_TMP_WRITE_IMG_REL_DATA(TGA, CMPN, BYTE_SRC, BYTE_DST, PROC, ID, AK, FK) \
    __data = _data + BYTE_SRC;                                                     \
                                                                                   \
    _k = data1;                                                                    \
    chunk = 0;                                                                     \
    _data1 = data1 + 1;                                                            \
    PROC(_data1, _data ID);                                                        \
    _data1 += BYTE_DST;                                                            \
                                                                                   \
    for(; __data < _end; __data += BYTE_SRC) { /*扫描一行像素*/                    \
        if(GN_MemCompareSimilar(_data ID, __data ID, CMPN, comps)) { /*值相异*/    \
            if(chunk & 0x80) {                                                     \
                ++chunk;                                                           \
                if((chunk & 0x7f) == FK) goto GT_GOTO_##TGA##PushColorSeg;         \
            } else if(!chunk) {                                                    \
                GT_GOTO_##TGA##PushEqualColor : chunk = 0x81;                      \
            } else {                                                               \
                *_k = chunk - 1 AK;                                                \
                _data1 -= BYTE_DST;                                                \
                _data = __data - BYTE_SRC;                                         \
                _k = _data1;                                                       \
                ++_data1;                                                          \
                PROC(_data1, _data ID);                                            \
                _data1 += BYTE_DST;                                                \
                goto GT_GOTO_##TGA##PushEqualColor;                                \
            }                                                                      \
        } else { /*值相等*/                                                        \
            if(chunk & 0x80) {                                                     \
                GT_GOTO_##TGA##PushNotEqualColor :                                 \
                        *_k = chunk AK;                                            \
                chunk = 0;                                                         \
                _k = _data1;                                                       \
                ++_data1;                                                          \
                _data = __data;                                                    \
                PROC(_data1, __data ID);                                           \
                _data1 += BYTE_DST;                                                \
            } else {                                                               \
                ++chunk;                                                           \
                PROC(_data1, __data ID);                                           \
                _data1 += BYTE_DST;                                                \
                if(chunk == FK) {                                                  \
                    GT_GOTO_##TGA##PushColorSeg : __data += BYTE_SRC;              \
                    if(__data >= _end) break;                                      \
                    goto GT_GOTO_##TGA##PushNotEqualColor;                         \
                }                                                                  \
            }                                                                      \
        }                                                                          \
    }                                                                              \
    *_k = chunk AK;                                                                \
    GNf::Write((char *)data1, 1, _data1 - data1, fp);

                GT_TMP_WRITE_IMG_REL_DATA(Tga, byte_n1, byte_n1, byte_n, pfunc_pix_cv, , , 0x7f)
            }
        } else {
            for(; y1 != y2; y1 += add) {
                if(srcformat == CR_RGBA || srcformat == CR_RGB) {
                    ImgFmtConvertTs(data1, srcformat, data + y1 * srsize, srcformat, w, true);
                } else if(srcformat == CR_LUMINANCE_ALPHA || srcformat == CR_ALPHA) {
                    ImgFmtConvertTs(data1, CR_RGBA, data + y1 * srsize, srcformat, w, true);
                } else if(srcformat == CR_RGB565) {
                    ImgFmtRGB565toRGB555((unsigned short *)data1, (unsigned short *)(data + y1 * srsize), w, true);
                } else { //其它单通道色
                    ImgFmtConvertTs(data1, CR_RGB, data + y1 * srsize, srcformat, w, true);
                }
                GNf::Write((char *)data1, 1, rsize, fp);
            }
        }
        GNf::Close(fp);
        return true;
    }
