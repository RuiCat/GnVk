// GN_mini 图像编解码片段：HDR 文件头 + RGBE 打包/解包 + 加载/保存。
// 2026-11-26 自 GN_FormatImgFile.cpp 第 607-905 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_FormatImgFile.cpp 按原顺序包含（单编译单元）；不是独立头文件。
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
            int w3 = w * 3;
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
        // 判断"源格式不是 RGB/RGBA"要比较 srcformat；旧写法 (CR_RGB || CR_RGBA) 是枚举常量求或，
        // 恒为真 ⇒ 灰度/调色板等 ≤4 字节格式从不转换，PackRGBE0 按 3 字节/像素读 1 字节/像素的行（越界+错色）。
        bool not_rgb_a = !(px_byten <= 4 && (srcformat == GNg::CR_RGB || srcformat == GNg::CR_RGBA));

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
