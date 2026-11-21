// GN_FormatImgFile.cpp — 图像文件读写：BMP/TGA/PNG/HDR 保存与加载、RGBE 打包、图像格式判定（原 GN_Format.cpp 拆分）。
#include "GN.h"
#include "GN_FormatInternal.h"
#ifdef _GN_FORMAT_H_
#if defined _WIN32_ || defined _WIN32_WCE
#ifndef DONT_INCLUDE_IMAGELIBS
#define DONT_INCLUDE_IMAGELIBS
#endif
#if _MSC_VER < 1600
#pragma comment(lib, "png.lib")
#else
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "png.lib")
#if _MSC_VER >= 1900
extern "C" FILE *__cdecl __iob_func(unsigned i) {
    return __acrt_iob_func(i);
}
#endif
#endif
#endif
#include <libpng/png.h>

#pragma pack(push, 2) //设置编译器编译结构体两字节对齐
    struct GT_sBMP_HEADER //bmp文件头
    {
        unsigned short type; //类型表示
        unsigned int size; //文件字节大小
        unsigned short reserved1; //保留占位
        unsigned short reserved2; //保留占位
        unsigned int offBits; //图象数据地址相对文件开始地址的偏移字节
    };
#pragma pack(pop) //取消刚才编译结构体对齐设置
    struct GT_sBMP_INFO //bmp文件信息
    {
        unsigned int size; //信息结构的大小字节数
        int w; //图象宽
        int h; //图象高
        unsigned short planes; //图象帧数
        unsigned short bit; //颜色位数
        unsigned int compression; //压缩方式 0为无压缩 也是bmp数据常态
        unsigned int sizeImage; //图象数据大小字节
        int xPelsPerMeter; //水平分辨率
        int yPelsPerMeter; //垂直分辨率
        unsigned int clrUsed; //调色板颜色数
        unsigned int clrImportant; //保留占位
    };
    //	uint8	rgbBlue,rgbGreen,rgbRed,rgbReserved; //GT_sBMP_INFO 之后如有调色板 接下来的均是调色板各个颜色 BGRX

    const char *GT_ERR_FileCreateFail = "写入 %s 文件 %s 失败! 文件创建失败!\n";

    //读取BMP图片文件信息(bmp文件信息, 图片文件名或内存地址,使用内存模式指定大小, 读完是否一并关闭文件 否则就把文件指针跳转到数据区)返回正在打开的文件 返回0表示读取失败
    GNf::sFILE *GT_LoadBmpFileInfo(char *filename_mem, GNg::COLOR_FORMAT *format, int *w, int *h, int *fdsize, bool close, int memsize) {
        GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize); //打开文件 filename_mem 和 memsize 可以是磁盘文件 或压缩包 或 内存数据块 具体开 memsize 怎么表示
        if(!fp) return 0; //文件打开失败
        GT_sBMP_HEADER bmp_head; //定义bmp文件头结构
        char *type_str = (char *)(&bmp_head);
        GNf::Read(type_str, sizeof(GT_sBMP_HEADER), 1, fp); //读取文件头
        if(type_str[0] != 'B' || type_str[1] != 'M') { //首两个字节如不是 BM 即不是bmp文件
            GNf::Close(fp); //关闭文件
            return 0; //返回效验失败
        }
        GT_sBMP_INFO bmp_info; //定义bmp文件信息块结构
        GNf::Read((char *)(&bmp_info), sizeof(GT_sBMP_INFO), 1, fp); //读取文件信息块
        if(bmp_info.h < 0) bmp_info.h = -bmp_info.h; //图象高有些软件输出可能为负数就反转为正数
        *w = bmp_info.w;
        *h = bmp_info.h; //返回图片的宽高
        *format = GNg::ImgFmtGetFormat(bmp_info.bit); //把颜色位数转为引擎表示的颜色格式
        *fdsize = bmp_info.sizeImage; //返回图像文件数据大小

        if(close) GNf::Close(fp); //要求关闭就关闭文件
        else GNf::Seek(fp, bmp_head.offBits, GNf::eSEEK_START); //否则使文件指针跳转到数据区 以便接下来读取
        return fp; //返回文件
    }
    GN_API uint8 *GNg::LoadBmpFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out, int memsize) {
        int fdsize;
        GT_LoadBmpFileInfo(filename_mem, format, w, h, &fdsize, true, memsize); //读取文件信息后就要求顺便关闭文件
        return 0; //暂时不处理缩略图返回0
    }
    GN_API uint8 *GNg::LoadBmpFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        int fdsize;
        GNf::sFILE *fp = GT_LoadBmpFileInfo(filename_mem, format, w, h, &fdsize, false, memsize); //读取bmp文件信息并获得正在打开的文件
        if(!fp) return 0;
        int tw = *w, th = *h; //宽高接下来频繁访问 把它们传到局部变量处理更高效
        unsigned int px_byte = ImgFmtGetByteNum(*format); //获取指定颜色格式的单个像素占字节数
        unsigned int pitch = px_byte * (unsigned int)tw; //计算到图像一行数据所占字节数
        int size = th * pitch; //计算到图像数据总大小
        uint8 *tmp_data = new uint8[size]; //分配一块数据以用来保存图像
        unsigned int sk = 0;
        if(fdsize > size /* + 2*/) sk = (fdsize - (size /* + 2*/)) / th; //计算出图像文件数据里一行多出的字节数 bmp文件数据通常每行以4字节的倍数对齐
        //GN_Log("sizeImage=%d, sk=%d\n", bmp_info.sizeImage, sk);	//打印图像文件数据大小和一行多出的字节数到 RunLog.txt 文件里

        int i;
        if(col_turn && sk) { //当要求翻转列象素且有行多出的字节
            for(i = th - 1; i >= 0; --i) { //接收数据从最末行开始历遍到首行
                GNf::Read((char *)(tmp_data + (i * pitch)), 1, pitch, fp); //读出一行行数据
                GNf::Seek(fp, sk, GNf::eSEEK_CUR); //跳过多余的对齐字节
            }
        } else if(col_turn) { //当要求翻转列象素
            for(i = th - 1; i >= 0; --i) //接收数据从最末行开始历遍到首行
                GNf::Read((char *)(tmp_data + (i * pitch)), 1, pitch, fp); //读出一行行数据

        } else if(sk) { //仅有行多出字节
            for(i = 0; i < th; ++i) { //接收数据从首行开始历遍到最末行
                GNf::Read((char *)(tmp_data + (i * pitch)), 1, pitch, fp); //读出一行行数据
                GNf::Seek(fp, sk, GNf::eSEEK_CUR); //跳过多余的对齐字节
            }
        } else { //没反转也没行多出的字节
            if(GNf::Read((char *)tmp_data, 1, size, fp) <= 0) { //整片数据一次过读取
                GN_DeleteArray(tmp_data); //当读取失败时删除刚分配的数据 并使 tmp_data 为0 以便待会返回0
                GN_Log("读取 bmp 文件数据时有错!\n"); //打印图像读取失败信息到 RunLog.txt 文件里
                GNf::Close(fp); //退出前先关闭文件
                return 0; //失败退出
            }
        }
        uint8 *_data = tmp_data, *endd = tmp_data + size;
        for(; _data < endd; _data += px_byte) GN_Swap(uint8, _data[0], _data[2]); //历遍每个像素数据 使红蓝分量值调换一下 即把bmp颜色数据默认的BGR转为引擎用的RGB !您也可以调用GNg::ImgFmtConvertTs函数来转换 但该函数输入输出参数不能为同一指针

        GNf::Close(fp); //完成就关闭文件
        return tmp_data; //返回图像数据
    }
    GN_API bool GNg::SaveBmpFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn) {
        GNf::sFILE *fp = GNf::Open(file_name, "wb", GNf::eFILE_STD); //打开创建文件(路径文件名, 以写入二进制方式, 创建于标准本地磁盘文件)
        if(!fp) {
            GN_Log(GT_ERR_FileCreateFail, "bmp", file_name);
            return false;
        } //文件创建失败就打印失败原因到 RunLog.txt 文件里
        GT_sBMP_HEADER bmp_head; //定义bmp文件头结构
        GT_sBMP_INFO bmp_info; //定义bmp文件信息结构
        int sbh = sizeof(GT_sBMP_HEADER);
        int sbi = sizeof(GT_sBMP_INFO);
        memset(&bmp_head, 0, sbh); //使文件头结构里的所有属性先设为0
        memset(&bmp_info, 0, sbi); //使文件信息结构里的所有属性先设为0
        int pixel_byte = (int)ImgFmtGetByteNum(srcformat); //获取指定颜色格式的单个像素占字节数
        bmp_info.bit = (unsigned short)pixel_byte << 3; //颜色位数=像素占字节数乘8   <<3相当于乘8 但性能更好
        bmp_info.w = w;
        bmp_info.h = h; //设置图片宽高
        bmp_info.planes = 1; //只有1帧图像
        bmp_info.size = 40; //文件信息结构
        bmp_info.xPelsPerMeter = 2874; //常规水平分辨率
        bmp_info.yPelsPerMeter = 2874; //常规水平分辨率
        unsigned int data_size = (unsigned int)pixel_byte * (unsigned int)w * (unsigned int)h; //计算到图像数据总大小
        bmp_info.sizeImage = data_size; //指定图像数据大小
        //unsigned int   v = data_size % 4;
        //if(v)bmpI.sizeImage += 4 - v;
        bmp_head.type = 0x4d42; //文件效验标签 "BM" 的16进制
        bmp_head.offBits = 54; //无调色板的数据区在文件开始的偏移字节位置
        bmp_head.size = 54 + bmp_info.sizeImage; //文件总大小字节
        GNf::Write((char *)&bmp_head, sbh, 1, fp); //写入文件头信息
        GNf::Write((char *)&bmp_info, sbi, 1, fp); //写入文件信息
        uint8 *data2 = new uint8[data_size]; //分配一块图像数据大小的内存
        GNg::ImgFmtConvertTs(data2, srcformat, data, srcformat, w * h, true); //使红蓝分量值调换一下 即把引擎用的颜色数据RGB转为bmp默认的BGR

        if(col_turn) { //当要求翻转列象素
            unsigned int pitch = pixel_byte * (unsigned int)w; //计算到图像一行数据所占字节数
            int i = h - 1; //写入数据从最末行开始历遍到首行
            for(; i >= 0; --i) GNf::Write((char *)(data2 + (i * pitch)), 1, pitch, fp); //要翻转图象就要一行行写

        } else GNf::Write((char *)data2, 1, data_size, fp); //一次过写入整片图像数据

        GNf::Close(fp); //写完关闭文件
        delete[] data2; //删除临时数据
        return true; //返回成功
    }
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
    struct GT_sRPNG_FILE //PNG文件数据读取器
    {
        GNf::sFILE *fp; //文件
        uint8 zPngReadBuf[16]; //用来交换gim宽和高参数用
        unsigned int zPngReadN; //用来作跨函数传递临时参数用, 读取gim文件时用到
    };
    inline void GT_PngReadCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
        GT_sRPNG_FILE *rpf = (GT_sRPNG_FILE *)(png_ptr->io_ptr);
        GNf::Read((char *)data, 1, length, rpf->fp);
        rpf->zPngReadN++;
        if(rpf->zPngReadN <= 3) {
            uint8 ihdr[] = {0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52}; //增补PNG第二个头标识 "IHDR"
            switch(rpf->zPngReadN) {
                case 1: memcpy(data, ihdr, length); break;
                case 2: memcpy(data, ihdr + 4, length); break;
                case 3: memcpy(data, rpf->zPngReadBuf, length); break;
            }
        }
    }
    GN_API uint8 *GNg::LoadPngFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out, int memsize) {
        *w = GN_INT_MIN;
        *h = 2147483637 /*GN_INT_MAX-10*/; //用于提示仅获取信息即可
        return LoadPngFileData(filename_mem, format, w, h, 0, memsize);
    }
    GN_API uint8 *GNg::LoadPngFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize);
        if(!fp) {
            //GN_Log("读取 png 文件失败! 不能打开该文件或该文件不存在!\n""read_png_file File  could not be opened for reading\n");
            return 0;
        }
        uint8 header[16];
        GNf::Read((char *)header, 1, 4, fp);
        GNf::Seek(fp, 0, GNf::eSEEK_START);
        GT_sRPNG_FILE rpf;
        rpf.fp = fp;
        rpf.zPngReadN = 1000000;
        GNf::Read((char *)header, 1, 16, fp);
        if(png_sig_cmp((png_bytep)header, 0, 8) || strncmp((char *)(header + 12), "IHDR", 4)) {
            GN_Log("读取 png 文件失败! 文件不是有效的PNG格式文件\n");
            GNf::Close(fp);
            return 0;
        }
        GNf::Seek(fp, 8, GNf::eSEEK_START);

        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); //获取图像头数据
        if(!png_ptr) {
            GN_Log("读取 png 文件失败! 创建读取头失败!\n");
            GNf::Close(fp);
            return 0;
        }
        png_infop info_ptr = png_create_info_struct(png_ptr); //获取图像信息头
        if(!info_ptr) {
            GN_Log("读取 png 文件失败! 创建信息头失败!\n");
            png_destroy_read_struct(&png_ptr, &info_ptr, 0);
            GNf::Close(fp);
            return 0;
        }
        png_set_read_fn(png_ptr, &rpf, GT_PngReadCallback); //设置读数据的回调函数
        png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);
        if(info_ptr->bit_depth != 8) {
            GN_Log("读取 png 文件失败! 该文件使用了非 8位/通道 的格式!\n");
            png_destroy_read_struct(&png_ptr, &info_ptr, 0);
            GNf::Close(fp);
            return 0;
        }
        int th = info_ptr->height;
        switch(info_ptr->color_type) {
            case PNG_COLOR_TYPE_RGB_ALPHA: *format = GNg::CR_RGBA; break;
            case PNG_COLOR_TYPE_RGB: *format = GNg::CR_RGB; break;
            case PNG_COLOR_TYPE_GRAY_ALPHA: *format = GNg::CR_ALPHA; break;
            case PNG_COLOR_TYPE_GRAY: *format = GNg::CR_LUMINANCE; break;
            case PNG_COLOR_TYPE_PALETTE: *format = GNg::CR_PALETTE; break;
        }
        uint8 *tmp_data = 0;
        if(*w == GN_INT_MIN && *h == 2147483637) {
            ;
        } else {
            uint8 loc_buf[65536]; //64K
            GN::sTBUF_INT temp_buf(loc_buf, 65536);
            int row_bytes = info_ptr->rowbytes;
            tmp_data = new uint8[th * row_bytes]; //分配数据资源
            uint8 **row_ptrs = (uint8 **)temp_buf.GetSpace(sizeof(uint8 *) * th), *data1;
            int i = 0, th1 = th - 1;
            if(col_turn)
                for(data1 = tmp_data + (th1 * row_bytes); i < th; ++i, data1 -= row_bytes) row_ptrs[i] = data1;
            else /*----*/
                for(data1 = tmp_data; i < th; ++i, data1 += row_bytes) row_ptrs[i] = data1;
            png_read_image(png_ptr, row_ptrs); //读取图像数据
            //png_read_end(png_ptr, info_ptr);  //读取PNG结束字串
        }
        *w = info_ptr->width;
        *h = th;
        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        GNf::Close(fp);
        return tmp_data;
    }
    inline void GT_PngWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
        GNf::Write((char *)data, 1, length, (GNf::sFILE *)png_ptr->io_ptr);
    }
    GN_API bool GNg::SavePngFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn, unsigned int dateID) {
        GNf::sFILE *fp = GNf::Open(file_name, "wb", GNf::eFILE_STD);
        if(!fp) {
            GN_Log(GT_ERR_FileCreateFail, "png", file_name);
            return false;
        }
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if(!png_ptr) {
            GN_Log("写入 png 文件失败! 创建写入头失败!\n");
            GNf::Close(fp);
            return false;
        }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if(!info_ptr) {
            GN_Log("写入 png 文件失败! 创建信息头失败!\n");
            png_destroy_write_struct(&png_ptr, &info_ptr);
            GNf::Close(fp);
            return false;
        }
        //png_init_io(png_ptr, (FILE*)fp->handle_buf);
        png_set_write_fn(png_ptr, fp, GT_PngWriteCallback, 0); /*设置写数据的回调函数*/
        if(setjmp(png_jmpbuf(png_ptr))) {
            GN_Log("写入 png 文件失败! 写入信息头失败!\n");
            png_destroy_write_struct(&png_ptr, &info_ptr);
            GNf::Close(fp);
            return false;
        }
        int row_byte;
        png_byte color_type;
        uint8 *tmp_data = 0;
        if(srcformat == CR_RGB565 || srcformat == CR_LUMINANCE_ALPHA) { //转换为PNG读写规格并转换图象数据
            COLOR_FORMAT d_format;
            if(srcformat == CR_RGB565) {
                d_format = CR_RGB;
                color_type = PNG_COLOR_TYPE_RGB;
            } else {
                d_format = CR_RGBA;
                color_type = PNG_COLOR_TYPE_RGB_ALPHA;
            }
            row_byte = w * ImgFmtGetByteNum(d_format);
            tmp_data = new uint8[h * row_byte];
            ImgFmtConvertTs(tmp_data, d_format, data, srcformat, h * w);
            data = tmp_data;
        } else { //转换为PNG读写规格
            switch(srcformat) { //各种颜色类型的处理
                case CR_RGBA: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
                case CR_RGB: color_type = PNG_COLOR_TYPE_RGB; break;
                case CR_ALPHA: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
                case CR_LUMINANCE:
                    color_type = PNG_COLOR_TYPE_GRAY;
                    break;
                    //case CR_RGBA4444: break;
                    //case CR_RGBA5551: break;
            }
            row_byte = w * ImgFmtGetByteNum(srcformat);
        }
        uint8 loc_buf[65536]; //64K
        GN::sTBUF_INT temp_buf(loc_buf, 65536);
        uint8 **row_ptrs = (uint8 **)temp_buf.GetSpace(sizeof(uint8 *) * h);
        int i, h1 = h - 1;
        if(col_turn)
            for(i = 0; i < h; ++i) row_ptrs[i] = data + ((h1 - i) * row_byte);
        else /*----*/
            for(i = 0; i < h; ++i) row_ptrs[i] = data + (i * row_byte);

        png_set_IHDR(png_ptr, info_ptr, w, h,
                     8, color_type, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        bool ret = true;
        png_write_info(png_ptr, info_ptr);
        /*if(setjmp(png_jmpbuf(png_ptr))){
	    ret = false;
		GN_Log("写入 png 文件失败! 写入数据时有错!\n");
		goto GT_GOTO_ENDwritePNG;
	}*/
        png_write_image(png_ptr, row_ptrs); //写数据

        if(setjmp(png_jmpbuf(png_ptr))) {
            ret = false;
            GN_Log("写入 png 文件失败! 写入结束标记时有错!\n");
        }
        //结束并释放写缓冲
        png_write_end(png_ptr, info_ptr);
    GT_GOTO_ENDwritePNG:
        png_destroy_write_struct(&png_ptr, &info_ptr);

        //清除缓存数据
        if(tmp_data) delete[] tmp_data;
        GNf::Close(fp);
        return ret;
    }
    struct GT_sHDR_HEADER {
        char programtype[16];
        float gamma;
        float exposure;
        int width;
        int height;
    };
    static int GT_LoadHdrFileInfo(GNf::sFILE *fp, GT_sHDR_HEADER *info) {
        char buf[128];
        if(!GNf::Gets(buf, 128, fp)) {
        GT_GOTO_LoadHdrErr:
            GN_Log("读取 hdr 文件失败! 文件格式有误或损坏!\n");
            return -1;
        }
        if((buf[0] != '#') || (buf[1] != '?')) {
            /* if you want to require the magic token then uncomment the next line */
            /*return -2; */
        }
        info->gamma = 1.0f;
        info->exposure = 1.0f;
        info->programtype[0] = 0;
        int i = 0, i2 = 2, pn = sizeof(info->programtype) - 1;
        for(; i < pn; ++i, ++i2) {
            if((buf[i2] == 0) || isspace(buf[i2])) break;
            info->programtype[i] = buf[i2];
        }
        info->programtype[i] = 0;
        if(!GNf::Gets(buf, 128, fp)) goto GT_GOTO_LoadHdrErr;

        float tempf;
        for(;;) {
            if((buf[0] == 0) || (buf[0] == '\n')) goto GT_GOTO_LoadHdrErr;
            else if(!strcmp(buf, "FORMAT=32-bit_rle_rgbe\n")) break;
            else if(sscanf(buf, "GAMMA=%g", &tempf) == 1) info->gamma = tempf;
            else if(sscanf(buf, "EXPOSURE=%g", &tempf) == 1) info->exposure = tempf;
            if(!GNf::Gets(buf, 128, fp)) goto GT_GOTO_LoadHdrErr;
        }
        //if(!GNf::Gets(buf,128,fp))goto GT_GOTO_LoadHdrErr;
        //if(strcmp(buf,"\n"))goto GT_GOTO_LoadHdrErr;
        for(;;) {
            if(!GNf::Gets(buf, 128, fp)) goto GT_GOTO_LoadHdrErr;
            if(sscanf(buf, "-Y %d +X %d", &(info->height), &(info->width)) == 2) break;
        }
        return 0;
    }
    GN_API void GNg::PackRGBE0(uint8 *rgb, uint8 *rgbe /*[4]*/) {
        rgbe[0] = rgb[0];
        rgbe[1] = rgb[1];
        rgbe[2] = rgb[2];
        rgbe[3] = 128;
    }
    GN_API void GNg::PackRGBE(/*GNg::sVEC3* */ uint8 *rgb, uint8 *rgbe /*[4]*/) {
        float *_c = (float *)rgb;
        int8 e = 0;
        float max_c = _c[0] > _c[1] ? _c[0] : _c[1], exp = 1.0f;
        if(_c[2] > max_c) max_c = _c[2];
        if(max_c > 1.0f) {
            ++e;
            int8 e2 = (int8)max_c;
            if((float)e2 < max_c) ++e2;
            while((1 << e) < e2) ++e;
            exp = (float)(1 << e);
        }
        exp = 255.0f / exp;
        rgbe[0] = (uint8)(_c[0] * exp);
        rgbe[1] = (uint8)(_c[1] * exp);
        rgbe[2] = (uint8)(_c[2] * exp);
        rgbe[3] = e + 128;
    }
    inline void GT_UnpkRGBE(/*GNg::sVEC3* */ float *rgbf, uint8 *rgbe /*[4]*/) {
        if(rgbe[3]) {
            float f = (float)ldexp(1.0, rgbe[3] - (int)(128 + 8));
            rgbf[0] = rgbe[0] * f;
            rgbf[1] = rgbe[1] * f;
            rgbf[2] = rgbe[2] * f;
        } else {
            rgbf[0] = 0.0f;
            rgbf[1] = 0.0f;
            rgbf[2] = 0.0f;
        }
    }
    GN_API void GNg::UnpkRGBE(/*GNg::sVEC3* */ float *rgbf, uint8 *rgbe /*[4]*/) {
        GT_UnpkRGBE(rgbf, rgbe);
    }
    static int GT_ReadImgPixelsRGBE(GNf::sFILE *fp, float *data, int w, int h, bool col_turn) {
        uint8 rgbe[4];
        if(col_turn) {
            int w3 = w * 3, i;
            float *_data, *_end;
            while(h-- > 0) {
                _data = data + w3 * h;
                _end = _data + w3;
                for(; _data < _end; _data += 3) {
                    if(GNf::Read((char *)rgbe, 4, 1, fp) < 1) return -1;
                    GT_UnpkRGBE(_data, rgbe);
                }
            }
        } else {
            int wh = w * h;
            while(wh-- > 0) {
                if(GNf::Read((char *)rgbe, 4, 1, fp) < 1) return -1;
                GT_UnpkRGBE(data, rgbe);
                data += 3;
            }
        }
        return 0;
    }
    static int GT_ReadImgPixelsRGBE_REL(GNf::sFILE *fp, float *data, int w, int h, bool col_turn) {
        uint8 rgbe[4], *scanline_buffer = 0, *ptr, *ptr_end;
        int i, count, w3 = w * 3;
        uint8 buf[2];

        if((w < 8) || (w > 0x7fff)) return GT_ReadImgPixelsRGBE(fp, data, w, h, col_turn);
        float *_data = data;
        while(h > 0) {
            if(col_turn) _data = data + (h - 1) * w3;
            if(GNf::Read((char *)rgbe, 4, 1, fp) < 1) {
                GN_MemFree(scanline_buffer);
                return -1;
            }
            if((rgbe[0] != 2) || (rgbe[1] != 2) || (rgbe[2] & 0x80)) { //非压缩格式
                GT_UnpkRGBE(_data, rgbe);
                _data += 3;
                GN_MemFree(scanline_buffer);
                if(GT_ReadImgPixelsRGBE(fp, _data, w - 1, 1, col_turn)) return -1;
                return GT_ReadImgPixelsRGBE(fp, data, w, (h - 1), col_turn);
            }
            if((((int)rgbe[2]) << 8 | rgbe[3]) != w) {
                GN_MemFree(scanline_buffer);
                return -1;
            }
            if(!scanline_buffer) scanline_buffer = (uint8 *)malloc(4 * w);
            if(!scanline_buffer) return -2;
            ptr = scanline_buffer;
            for(i = 0; i < 4; ++i) { //GT_TMP_TGA_IMG_DATA_UNPAK
                ptr_end = scanline_buffer + (i + 1) * w;
                while(ptr < ptr_end) {
                    GNf::Read((char *)buf, 2, 1, fp);
                    if(buf[0] > 128) {
                        count = buf[0] - 128;
                        if((count == 0) || (count > ptr_end - ptr)) {
                            GN_MemFree(scanline_buffer);
                            return -1;
                        }
                        while(count-- > 0) *ptr++ = buf[1];
                    } else {
                        count = buf[0];
                        if((count == 0) || (count > ptr_end - ptr)) {
                            GN_MemFree(scanline_buffer);
                            return -1;
                        }
                        *ptr++ = buf[1];
                        if(--count > 0) {
                            if(GNf::Read((char *)ptr, count, 1, fp) < 1) {
                                GN_MemFree(scanline_buffer);
                                return -1;
                            }
                            ptr += count;
                        }
                    }
                }
            }
            //把间开的通道值转换并复制到连续的
            for(i = 0; i < w; ++i) {
                rgbe[0] = scanline_buffer[i];
                rgbe[1] = scanline_buffer[i + w];
                rgbe[2] = scanline_buffer[i + 2 * w];
                rgbe[3] = scanline_buffer[i + 3 * w];
                GT_UnpkRGBE(_data, rgbe);
                _data += 3;
            }
            --h;
        }
        GN_MemFree(scanline_buffer);
        return 0;
    }
    GN_API uint8 *GNg::LoadHdrFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out, int memsize) {
#define GT_TMP_LOAD_HDR_FILE_INFO                            \
    GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize); \
    if(!fp) return 0;                                        \
    GT_sHDR_HEADER hinfo;                                    \
    if(GT_LoadHdrFileInfo(fp, &hinfo)) {                     \
        GNf::Close(fp);                                      \
        return 0;                                            \
    }                                                        \
    *format = CR_RGB16;                                      \
    *w = hinfo.width;                                        \
    *h = hinfo.height;

        GT_TMP_LOAD_HDR_FILE_INFO
        return 0;
    }
    GN_API uint8 *GNg::LoadHdrFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize, float *max_ins) {
        GT_TMP_LOAD_HDR_FILE_INFO
        int size = hinfo.width * hinfo.height * 3;
        uint8 *data = (uint8 *)new float[size];

        if(GT_ReadImgPixelsRGBE_REL(fp, (float *)data, hinfo.width, hinfo.height, !col_turn)) {
            delete[] data;
            data = 0;
            GN_Log("读取 hdr 文件失败! 解释数据时遇到错误!\n");
        }
        GNf::Close(fp);
        if(max_ins) {
            *format = CR_RGB;
            if(*max_ins < 0.0f) {
                uint8 *data_lr = ImgDataHdrToLinearData((float *)data, *w, *h, 3, -(*max_ins));
                delete[] data;
                data = data_lr;
            } else {
                float *dataf = (float *)data, maxins = 0.0f;
                int i = 0;
                for(; i < size; ++i) {
                    if(dataf[i] > maxins) maxins = dataf[i];
                }
                if(maxins > 3.0f) maxins = 3.0f;
                *max_ins = maxins;
                float scl = maxins > 1.0f ? 255.0f / maxins : 255.0f;
                for(i = 0; i < size; ++i) data[i] = dataf[i] < maxins ? (uint8)(dataf[i] * scl) : 255;
            }
        }
        return data;
    }
    GN_API bool GNg::SaveHdrFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn, uint8 comps_lv) {
        GNf::sFILE *fp = GNf::Open(file_name, "wb", GNf::eFILE_STD);
        if(!fp) {
            GN_Log(GT_ERR_FileCreateFail, "hdr", file_name);
            return false;
        }
        col_turn = !col_turn;
        if(!comps_lv) comps_lv = 1;
        int px_byten = GNg::ImgFmtGetByteNum(srcformat);
        bool not_rgb_a = !(px_byten <= 4 && (GNg::CR_RGB || GNg::CR_RGBA));

        uint8 locbuf[16000];
        GN::sTBUF_INT tbuf(locbuf, 16000);
        int line_size = (w << 2);
        uint8 *wbuf = (uint8 *)tbuf.GetSpace(384 >= line_size ? 384 : line_size);
        int head_size = sprintf((char *)wbuf, "#?RADIANCE\n# Made with GN\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", h, w);
        GNf::Write((char *)wbuf, 1, head_size, fp);

        uint8 *lbuf = 0, *_data, *_end, *_wpos;
        int srsize = w * px_byten;
        int px_byten1 = px_byten;
        if(not_rgb_a) {
            px_byten1 = 3;
            lbuf = (uint8 *)tbuf.GetSpace(w * 3);
        }
        int srsize1 = w * px_byten1;
        int y1 = col_turn ? h - 1 : 0, y2 = col_turn ? -1 : h, add = col_turn ? -1 : 1;
        if(comps_lv) {
            uint8 comps = comps_lv - 1;
            uint8 line_header[4] = {2, 2, (uint8)(w >> 8), (uint8)(w & 0xff)}; //压缩格式行标记
            uint8 *data1 = (uint8 *)tbuf.GetSpace(line_size >> 1);
            uint8 *_data1, *_k, *__data, chunk;
            int i;
            for(; y1 != y2; y1 += add) {
                GNf::Write((char *)line_header, 4, 1, fp);

#define GT_TMP_IMG_FMT_CONVERT_RGBE                                                           \
    _data = data + y1 * srsize;                                                               \
    _end = _data + srsize;                                                                    \
    _wpos = wbuf;                                                                             \
    if(px_byten > 4) { /*浮点格式*/                                                           \
        for(; _data < _end; _data += px_byten, _wpos += 4) GNg::PackRGBE(_data, _wpos);       \
    } else {                                                                                  \
        if(not_rgb_a) {                                                                       \
            ImgFmtConvertTs(lbuf, CR_RGB, _data, srcformat, w, false);                        \
            _data = lbuf;                                                                     \
            _end = _data + srsize1;                                                           \
            for(; _data < _end; _data += px_byten1, _wpos += 4) GNg::PackRGBE0(_data, _wpos); \
        } else {                                                                              \
            for(; _data < _end; _data += px_byten, _wpos += 4) GNg::PackRGBE0(_data, _wpos);  \
        }                                                                                     \
    }
                GT_TMP_IMG_FMT_CONVERT_RGBE //转换一行写一行

#define GT_TMP_SET_PVALUE(DST, SRC) *(DST) = *(SRC)
                    for(i = 0; i < 4; ++i) {
                    _data = wbuf;
                    _end = _data + line_size;
                    GT_TMP_WRITE_IMG_REL_DATA(Hdr, 1, 4, 1, GT_TMP_SET_PVALUE, +i, +1, 0x7e)
                }
            }
        } /*else{
		for(; y1 != y2; y1 += add){
			GT_TMP_IMG_FMT_CONVERT_RGBE
			GNf::Write((char*)wbuf, 1, line_size, fp);
		}
	}*/
        GNf::Close(fp);
        return true;
    }
    GN_API bool GNg::IsHdrFile(char *filename) {
        char *ext_name = GN_StrGetExtName(filename);
        return (ext_name && (!strcmp(ext_name, ext_name[0] > 'Z' ? "hdr" : "HDR")));
    }
    //计算像素周围3x3平均颜色(返回像素平均颜色, 图形数据, 像素位置X, Y, 图像宽, 高, 特定格式每像素占字节数)
    static void GT_PixelAvgColor3x3(uint8 *px_out, /*const*/ uint8 *img_data, int x, int y, int w, int h, int pix_byte_n) {
        uint8 *_pd;
        unsigned int buf[12];
        memset(buf, 0, sizeof(unsigned int) * pix_byte_n);
        int x2 = x < w - 1 ? x + 1 : x;
        int y2 = y < h - 1 ? y + 1 : y;
        int x1 = x > 1 ? x - 1 : 0;
        int y1 = y > 1 ? y - 1 : 0;
        int count = (x2 - x1 + 1) * (y2 - y1 + 1), rn;
        switch(pix_byte_n) {
            case 3:
#define GT_TMP_LOOP_ADJ_PIXEL(PXPROC) \
    for(; y1 <= y2; ++y1) {           \
        rn = y1 * w;                  \
        for(x = x1; x <= x2; ++x) {   \
            PXPROC;                   \
        }                             \
    }
                GT_TMP_LOOP_ADJ_PIXEL(_pd = img_data + (rn + x) * 3; buf[0] += _pd[0]; buf[1] += _pd[1]; buf[2] += _pd[2])
                px_out[0] = buf[0] / count;
                px_out[1] = buf[1] / count;
                px_out[2] = buf[2] / count;
                break;
            case 4:
                GT_TMP_LOOP_ADJ_PIXEL(_pd = img_data + (rn + x) * 4; buf[0] += _pd[0]; buf[1] += _pd[1]; buf[2] += _pd[2]; buf[3] += _pd[3])
                px_out[0] = buf[0] / count;
                px_out[1] = buf[1] / count;
                px_out[2] = buf[2] / count;
                px_out[3] = buf[3] / count;
                break;
            case 1:
                GT_TMP_LOOP_ADJ_PIXEL(_pd = img_data + (rn + x); buf[0] += _pd[0])
                px_out[0] = buf[0] / count;
                break;
        }
    }
    //像素颜色累积(颜色源, 累计目标颜色, 特定格式每像素占字节数)
    static void GT_PixelColorAccum(uint8 *src, uint8 *dst, int pix_byte_n) {
        if(dst[0] == 0xff && !dst[1] && dst[2] == 0xff) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        } else {
            dst[0] = ((unsigned short)dst[0] + (unsigned short)src[0]) >> 1;
            dst[1] = ((unsigned short)dst[1] + (unsigned short)src[1]) >> 1;
            dst[2] = ((unsigned short)dst[2] + (unsigned short)src[2]) >> 1;
        }
    }
    GN_API uint8 *GNg::ImgDataHdrToLinearData(const float *data, int w, int h, int chn, float ha, float gamma) {
#define GT_TMP_HDR_TO_LINEAR(C) (((C * (0.15f * C + 0.05f) + 0.004f) / (C * (0.15f * C + 0.5f) + 0.06f)) - 0.066667f)
        //float ha1 = ha > 1.0f ? ha : 1.0f;	/*模板代码:HDR to LINEAR*/
        //if(ha1 > hdrlmt)ha1 = hdrlmt;
        float ha1 = 1.0f / GT_TMP_HDR_TO_LINEAR(11.2f);
        uint8 *ldata = new uint8[w * h * chn];
        uint8 *_ldata = ldata;
        float *_data = (float *)data, *edata = (float *)data + w * h * chn, c;
        if(chn < 4) {
            for(; _data < edata; ++_data, ++_ldata) {
                c = *_data * ha;
                c = c ? pow(GT_TMP_HDR_TO_LINEAR(c) * ha1, gamma) * 255.0f : 0.0f;
                *_ldata = c < 255.0f ? (uint8)c : 255;
            }
        } else {
            float *edata1;
            for(; _data < edata; ++_data, ++_ldata) {
                edata1 = _data + 3;
                for(; _data < edata1; ++_data, ++_ldata) {
                    c = *_data * ha;
                    c = c ? pow(GT_TMP_HDR_TO_LINEAR(c) * ha1, gamma) * 255.0f : 0.0f;
                    *_ldata = c < 255.0f ? (uint8)c : 255;
                }
                *_ldata = (*_data) < 1.0f ? (uint8)((*_data) * 255.0f) : 255;
            }
        }
        return ldata;
    }
    GN_API uint8 *GNg::LoadImgFileData(eIMG_FILE_TYPE file_type, char *filename_mem, COLOR_FORMAT *format, int *w, int *h, int memsize) {
        uint8 *data = 0;
        switch(file_type) {
            case eIMG_FILE_PNG:
            case eIMG_FILE_GIM: data = LoadPngFileData(filename_mem, format, w, h, false, memsize); break;
            case eIMG_FILE_BMP: data = LoadBmpFileData(filename_mem, format, w, h, true, memsize); break;
            case eIMG_FILE_TGA: data = LoadTgaFileData(filename_mem, format, w, h, true, memsize); break;
            case eIMG_FILE_HDR: data = LoadHdrFileData(filename_mem, format, w, h, true, memsize); break;
            case eIMG_FILE_UNKNOWN: {
                *format = (COLOR_FORMAT)GNf::GetExtNamePos(filename_mem, memsize);
                data = FileImage->LoadData(filename_mem, format, w, h, false, memsize);
            } break;
        }
        return data;
    }
    GN_API bool GNg::SaveImgFile(char *file_name, unsigned char *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn, uint8 comps_lv) {
        return FileImage->Save(file_name, data, srcformat, w, h, col_turn, comps_lv);
    }
    GN_API bool GNg::ImgFileScale(const char *dst_file_name, int dst_w, int dst_h, char *filename_mem, eIMG_FILE_TYPE file_type, int memsize, uint8 comps_lv) {
        COLOR_FORMAT color_fmt;
        int w, h;
        uint8 *data = LoadImgFileData(file_type, filename_mem, &color_fmt, &w, &h, memsize);
        if(!data) return false; //读取不到源文件

        uint8 *data_tg = new uint8[dst_w * dst_h * (int)ImgFmtGetByteNum(color_fmt)];
        ImgScale(data_tg, dst_w, dst_h, data, w, h, ImgFmtGetByteNum(color_fmt));

        bool save_ok = FileImage->Save((char *)dst_file_name, data_tg, color_fmt, dst_w, dst_h, false, comps_lv);
        delete[] data_tg;
        delete[] data;
        return save_ok;
    }
    GN_API GNg::eIMG_FILE_TYPE GNg::GetImgFmtTypeFromFileName(const char *filename) {
        char *ext_name = GN_StrGetExtName(filename), ext_name2[8];
        if(ext_name) {
            char ofs = 0;
        GT_GOTO_CheckImgFmtSupport:
            if(!strcmp(ext_name, "png")) return eIMG_FILE_PNG;
            else if(!strcmp(ext_name, "gim")) return eIMG_FILE_GIM;
            else if(!strcmp(ext_name, "bmp")) return eIMG_FILE_BMP;
            else if(!strcmp(ext_name, "tga")) return eIMG_FILE_TGA;
            else if(!strcmp(ext_name, "hdr")) return eIMG_FILE_HDR;
            if(!ofs && ext_name[0] <= 'Z' && ext_name[1] <= 'Z') {
                // 加固（STRCPY 审计）：扩展名任意长，ext_name2 仅 8 字节，按容量截断。
                strncpy(ext_name2, ext_name, sizeof(ext_name2) - 1);
                ext_name2[sizeof(ext_name2) - 1] = 0;
                ext_name = ext_name2;
                char ofs = 'a' - 'A';
                int i = 0;
                while(ext_name[i]) {
                    ext_name[i] += ofs;
                    ++i;
                } //大写名转小写再检测
                goto GT_GOTO_CheckImgFmtSupport;
            }
        }
        return eIMG_FILE_UNKNOWN;
    }
#endif //_GN_FORMAT_H_
