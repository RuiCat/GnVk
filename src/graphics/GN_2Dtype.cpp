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
#ifdef _GN_2DTYPE_H_
/********************************************************** 引擎在不同方式读取文件时的一些跳转处理 ******************************************************************/
GN_API char *GT_ImageFileNameNumberPosGet(const char *filename) {
    char *pstr = (char *)filename + strlen(filename) - 1;
    while(pstr >= filename) {
        if(*pstr >= '0' && *pstr <= '9') break;
        --pstr;
    }
    if(pstr > filename) {
        uint8 n = 1;
        --pstr;
        while(pstr >= filename) {
            if(*pstr >= '0' && *pstr <= '9') {
                ++n;
                if(n == 4) return pstr;
            } else return 0;
            --pstr;
        }
    }
    return 0;
}
GN_API GNg::PF_LOAD_IMG_FILE_DATA GT_ImageFileNameSet(const char *filename, int *fname_len_out, bool *Img_add_0000_out, char **Img_fileName_out) {
    GNg::PF_LOAD_IMG_FILE_DATA pf_load = 0;
    int fn_len = strlen(filename);
    *Img_add_0000_out = true;
    if(filename[fn_len - 4] == '.') {
        pf_load = GNg::FileImage->CheckFmtSupportFromExtName(filename + (fn_len - 3));
        if(!pf_load) return (GNg::PF_LOAD_IMG_FILE_DATA)1;
        fn_len -= 4;
        *Img_add_0000_out = false; /*考虑平台兼容性有扩展名就去掉它*/
        *fname_len_out = 0;
    } else *fname_len_out = fn_len;
    *Img_fileName_out = new char[fn_len + 1];
    strncpy(*Img_fileName_out, filename, fn_len);
    (*Img_fileName_out)[fn_len] = 0;
    return pf_load;
}
char *GT_ImageLoadLastTypeName = (char *)GNg::ImgFileTypeNames[GNg::eIMG_FILE_PNG];

GN_API uint8 *GT_ImageFileLoadToData(const char *filename, int fname_len, GNg::PF_LOAD_IMG_FILE_DATA load_func, int i, GNg::COLOR_FORMAT *format_out, int *w_out, int *h_out) {
    if(fname_len) {
        char tmp_flienmae[384];
        strncpy(tmp_flienmae, filename, sizeof(tmp_flienmae) - 1);
        tmp_flienmae[sizeof(tmp_flienmae) - 1] = 0;
        char *ext_name = tmp_flienmae + fname_len;
        sprintf(ext_name, "%04d.", i); /*有动画序列就接上序好来读取*/
        ext_name += 5;
        ext_name[3] = 0; //先设好结束符
        char *types = GT_ImageLoadLastTypeName;
        bool can_ext_name = true;
        uint8 *t_data;
        if(GNf::sFILE::DefaultH) {
            void *src_host = GNf::sFILE::DefaultH;
            int mem_size;

#define GT_TMP_LOOP_LOAD_ALL_IMG_FMT_BEGIN                                \
    for(;;) { /*历遍所有支持的格式*/                                      \
        strncpy(ext_name, types, 3);                                      \
        load_func = GNg::FileImage->CheckFmtSupportFromExtName(ext_name); \
        if(load_func) {

#define GT_TMP_LOOP_LOAD_ALL_IMG_FMT_END                                                                                 \
    if(t_data) {                                                                                                         \
        GT_ImageLoadLastTypeName = types;                                                                                \
        return t_data;                                                                                                   \
    }                                                                                                                    \
    }                                                                                                                    \
    for(;;) {                                                                                                            \
        if(types[3]) types += 4; /*下个扩展名*/                                                                          \
        else {                                                                                                           \
            if(can_ext_name) {                                                                                           \
                can_ext_name = false;                                                                                    \
                types = GNg::GetSupportImgFmtFileExtName();                                                              \
            } else {                                                                                                     \
                GT_ImageLoadLastTypeName = types;                                                                        \
                return 0;                                                                                                \
            }                                                                                                            \
        }                                                                                                                \
        if(types[0] > 'Z' && (types[0] != 'h' || types[0] != 'd' || types[0] != 'r')) break; /*忽略大写的扩展名和 hdr */ \
    }                                                                                                                    \
    }
            GT_TMP_LOOP_LOAD_ALL_IMG_FMT_BEGIN
            char *buffer = GNf::FileFullLoadToMemory(tmp_flienmae, &mem_size);
            if(buffer) { /*模版代码:把网上图片一次读到内存才加载*/
                GNf::sFILE::DefaultH = 0;
                t_data = load_func(buffer, format_out, w_out, h_out, false, mem_size);
                GNf::sFILE::DefaultH = src_host;
                delete[] buffer;
            } else t_data = 0;
            GT_TMP_LOOP_LOAD_ALL_IMG_FMT_END
        } else {
            GT_TMP_LOOP_LOAD_ALL_IMG_FMT_BEGIN
            t_data = load_func(tmp_flienmae, format_out, w_out, h_out, false, GNf::sFILE::DefaultType);
            GT_TMP_LOOP_LOAD_ALL_IMG_FMT_END
        }
    } else {
        if(GNf::sFILE::DefaultH) {
            int mem_size;
            char *buffer = GNf::FileFullLoadToMemory((char *)filename, &mem_size);
            if(buffer) { /*模版代码:把网上图片一次读到内存才加载*/
                void *src_host = GNf::sFILE::DefaultH;
                GNf::sFILE::DefaultH = 0;
                uint8 *t_data = load_func(buffer, format_out, w_out, h_out, false, mem_size);
                GNf::sFILE::DefaultH = src_host;
                delete[] buffer;
                return t_data;
            } else return 0;

        } else return load_func((char *)filename, format_out, w_out, h_out, false, GNf::sFILE::DefaultType);
    }
}
GN_API uint8 *GNg::ImageLoadFileToData(const char *filename, int i, COLOR_FORMAT *format_out, int *w_out, int *h_out) {
    int fname_len = strlen(filename);
    if(i >= 0) {
        return GT_ImageFileLoadToData(filename, fname_len, 0, i, format_out, w_out, h_out);
    } else {
        char tmp_flienmae[384];
        strncpy(tmp_flienmae, filename, sizeof(tmp_flienmae) - 1);
        tmp_flienmae[sizeof(tmp_flienmae) - 1] = 0;
        char *ext_name = tmp_flienmae + fname_len;
        *ext_name = '.';
        ++ext_name;
        ext_name[3] = 0; //先设好结束符
        uint8 *t_data;
        char *types = GT_ImageLoadLastTypeName;
        bool can_ext_name = true;
        GNg::PF_LOAD_IMG_FILE_DATA load_func;
        GT_TMP_LOOP_LOAD_ALL_IMG_FMT_BEGIN
        t_data = GT_ImageFileLoadToData(tmp_flienmae, 0, load_func, i, format_out, w_out, h_out);
        GT_TMP_LOOP_LOAD_ALL_IMG_FMT_END
    }
}
GN_API char *const GNg::ImageLoadLastTypeNameGet() {
    return GT_ImageLoadLastTypeName;
}
bool GNg::sWINDOW::FullScreen = false;
bool GNg::sWINDOW::WindowZoom = true;
bool GNg::sWINDOW::CursorShow = true;
int *GNg::sWINDOW::Hwnd = 0;
int *GNg::sWINDOW::HwndRoot = 0;
int *GNg::sWINDOW::Hinst = 0;
short GNg::sWINDOW::FBufferColorBit = 32;

#endif //endif _GN_2DTYPE_H_
