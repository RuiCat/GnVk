// GN_mini 图像编解码片段：像素工具（GT_PixelAvgColor3x3/GT_PixelColorAccum，全仓无调用，待删）。
// 2026-11-26 自 GN_FormatImgFile.cpp 第 906-1040 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_FormatImgFile.cpp 按原顺序包含（单编译单元）；不是独立头文件。
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
