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
#include "GN_FormatInternal.h"
#ifdef _GN_FORMAT_H_

GN_API bool GN_AddUpperFmtName =
#if defined _WIN32_ || defined _WIN32_WCE
    false;
#else
    true;
#endif
/********************************************************* 颜色格式 **********************************************************/

GN_API const char *GNg::ColorFmtNamesEn[] = {"AUTO", "RGBA", "RGB", "RGB565", "RGBA4444", "RGBA5551", "LA", "A", "L", "R", "G", "B", "INDEX", "RGBA16", "RGB16"};

GN_API bool GNg::ImgFmtHasAlpha(COLOR_FORMAT format) {
    switch(format) {
        case GNg::CR_RGBA:
            /*case GNg::CR_RGBA4444:*/
            /*case GNg::CR_RGBA5551:*/
        case GNg::CR_LUMINANCE_ALPHA:
        case GNg::CR_ALPHA:
        case GNg::CR_RGBA16:
            return true;
            default: break; // B9-3: 未列举枚举值兜底（保行为）
}
    return false;
}
GN_API bool GNg::ImgFmtIsLuminance(COLOR_FORMAT format) {
    return (format == CR_LUMINANCE_ALPHA || format == CR_LUMINANCE);
}
GN_API GNg::COLOR_FORMAT GNg::ImgFmtGetFormat(short bit_num) {
    COLOR_FORMAT fmt;
    switch(bit_num) {
        case 32: fmt = CR_RGBA; break;
        case 24: fmt = CR_RGB; break;
        case 16: fmt = CR_RGB565; break;
        case 8: fmt = CR_PALETTE; break;
        case 48: fmt = CR_RGB16; break;
        case 64: fmt = CR_RGBA16; break;
        default: fmt = CR_DEFAULT_FORMAT; break;
    }
    return fmt;
}
GN_API short GNg::ImgFmtGetByteNum(COLOR_FORMAT format) {
    if(format == CR_RGBA || format == CR_DEFAULT_FORMAT) return 4;
    else if(format == CR_RGB) return 3;
    else if(format == CR_RGB565 /*|| format == CR_RGBA4444 || format == CR_RGBA5551*/ || format == CR_LUMINANCE_ALPHA) return 2;
    else if(format >= CR_ALPHA && format <= CR_PALETTE) return 1;
    else if(format == CR_RGB16) return 12;
    else if(format == CR_RGBA16) return 16;
    else return 0;
}
GN_API void GNg::ImgFmtConvertTs(uint8 *pdesc_data, COLOR_FORMAT descFormat, const uint8 *psrc_data, COLOR_FORMAT srcFormat, int img_w, bool RGB_turn) {
    int ctli, sdid = 0, rid = 0, bid = 0;
    if(descFormat == CR_RGBA) {
        int corw = 4, lw = img_w * corw; /******转换成RGBA****************/
        if(srcFormat == CR_RGBA) { /*------ RGBA <- RGBA ---------------------------------------*/
            if(RGB_turn) {
                for(ctli = 0; ctli < lw; ctli += corw) {
                    pdesc_data[ctli + 2] = psrc_data[sdid++]; //R
                    pdesc_data[ctli + 1] = psrc_data[sdid++]; //G
                    pdesc_data[ctli] = psrc_data[sdid++]; //B
                    pdesc_data[ctli + 3] = psrc_data[sdid++]; //A
                }
            } else memcpy(pdesc_data, psrc_data, lw);

        } else if(srcFormat == CR_RGB) { /*------ RGBA <- RGB  ---------------------------------------*/
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = psrc_data[sdid++]; //R
                pdesc_data[ctli + 1] = psrc_data[sdid++]; //G
                pdesc_data[ctli + bid] = psrc_data[sdid++]; //B
                pdesc_data[ctli + 3] = 255; //A
            }
        } else if(srcFormat == CR_RGB565) { /*------ RGBA <- RGB565  --------------------------------*/
            unsigned short *tcolor = (unsigned short *)psrc_data;
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = (tcolor[sdid] & 31) * 255 / 31; //R
                pdesc_data[ctli + 1] = ((tcolor[sdid] & 2016) >> 5) * 255 / 63; //G
                pdesc_data[ctli + bid] = ((tcolor[sdid] & 63488) >> 11) * 255 / 31; //B
                pdesc_data[ctli + 3] = 255; //A
                ++sdid;
            }
        } else if(srcFormat == CR_LUMINANCE_ALPHA) { /*------ RGBA <- LA  -----------------------------*/
            uint8 lum;
            for(ctli = 0; ctli < lw; ctli += corw) {
                lum = psrc_data[sdid++];
                ;
                pdesc_data[ctli] = lum;
                pdesc_data[ctli + 1] = lum;
                pdesc_data[ctli + 2] = lum;
                pdesc_data[ctli + 3] = psrc_data[sdid++]; //A
            }
        } else if(srcFormat == CR_ALPHA) { /*------ RGBA <- A  --------------------------------------*/
            memset(pdesc_data, 0xff, lw);
            for(ctli = 3; ctli < lw; ctli += corw)
                pdesc_data[ctli] = psrc_data[sdid++]; //A

        } else if(srcFormat == CR_LUMINANCE) { /*------ RGBA <- L  ---------------------------------*/
            uint8 lum;
            for(ctli = 0; ctli < lw; ctli += corw) {
                lum = psrc_data[sdid++];
                pdesc_data[ctli] = lum;
                pdesc_data[ctli + 1] = lum;
                pdesc_data[ctli + 2] = lum;
                pdesc_data[ctli + 3] = 255;
            }
        } else if(srcFormat == CR_RED) { /*------ RGBA <- R  ---------------------------------------*/
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = psrc_data[sdid++]; //R
                pdesc_data[ctli + 1] = 0;
                pdesc_data[ctli + bid] = 0;
                pdesc_data[ctli + 3] = 255;
            }
        } else if(srcFormat == CR_GREEN) { /*------ RGBA <- G  ------------------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = 0;
                pdesc_data[ctli + 1] = psrc_data[sdid++]; //G
                pdesc_data[ctli + 2] = 0;
                pdesc_data[ctli + 3] = 255;
            }
        } else if(srcFormat == CR_BLUE) { /*------ RGBA <- B  -------------------------------------*/
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = 0;
                pdesc_data[ctli + 1] = 0;
                pdesc_data[ctli + bid] = psrc_data[sdid++]; //B
                pdesc_data[ctli + 3] = 255;
            }
        }
    } else if(descFormat == CR_RGB) {
        int corw = 3, lw = img_w * corw; /******转换成RGB****************/
        if(srcFormat == CR_RGBA) { /*------ RGB <- RGBA  -----------------------------------*/
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = psrc_data[sdid++]; //R
                pdesc_data[ctli + 1] = psrc_data[sdid++]; //G
                pdesc_data[ctli + bid] = psrc_data[sdid++]; //B
                ++sdid; //A
            }
        } else if(srcFormat == CR_RGB) { /*------ RGB <- RGB  ------------------------------------*/
            if(RGB_turn) {
                for(ctli = 0; ctli < lw; ctli += corw) {
                    pdesc_data[ctli + 2] = psrc_data[sdid++]; //R
                    pdesc_data[ctli + 1] = psrc_data[sdid++]; //G
                    pdesc_data[ctli] = psrc_data[sdid++]; //B
                }
            } else memcpy(pdesc_data, psrc_data, lw);

        } else if(srcFormat == CR_RGB565) { /*------ RGB <- RGB565  ------------------------------*/
            unsigned short *tcolor = (unsigned short *)psrc_data;
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = (tcolor[sdid] & 31) * 255 / 31; //R
                pdesc_data[ctli + 1] = ((tcolor[sdid] & 2016) >> 5) * 255 / 63; //G
                pdesc_data[ctli + bid] = ((tcolor[sdid] & 63488) >> 11) * 255 / 31; //B
                ++sdid;
            }
        } else if(srcFormat == CR_LUMINANCE_ALPHA) { /*------ RGB <- LA  -------------------------*/
            uint8 lum;
            for(ctli = 0; ctli < lw; ctli += corw) {
                lum = psrc_data[sdid++];
                pdesc_data[ctli] = lum;
                pdesc_data[ctli + 1] = lum;
                pdesc_data[ctli + 2] = lum;
                ++sdid; //A
            }
        } else if(srcFormat == CR_ALPHA || srcFormat == CR_LUMINANCE) { /*------ RGB <- A,L-----*/
            uint8 lum;
            for(ctli = 0; ctli < lw; ctli += corw) {
                lum = psrc_data[sdid++];
                pdesc_data[ctli] = lum;
                pdesc_data[ctli + 1] = lum;
                pdesc_data[ctli + 2] = lum; //A | L
            }
        } else if(srcFormat == CR_RED) { /*------ RGB <- R  --------------------------------------*/
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = psrc_data[sdid++]; //R
                pdesc_data[ctli + 1] = 0;
                pdesc_data[ctli + bid] = 0;
            }
        } else if(srcFormat == CR_GREEN) { /*------ RGB <- G  ------------------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = 0;
                pdesc_data[ctli + 1] = psrc_data[sdid++]; //G
                pdesc_data[ctli + 2] = 0;
            }
        } else if(srcFormat == CR_BLUE) { /*------ RGB <- B --------------------------------------*/
            if(RGB_turn) {
                rid = 2;
                bid = 0;
            } else {
                rid = 0;
                bid = 2;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli + rid] = 0;
                pdesc_data[ctli + 1] = 0;
                pdesc_data[ctli + bid] = psrc_data[sdid++]; //B
            }
        }
    } else if(descFormat == CR_RGB565) {
        int corw = 1, lw = img_w * corw; /******转换成压宿RGB****************/
        unsigned short *tcolor = (unsigned short *)pdesc_data;
        if(srcFormat == CR_RGBA) { /*------ RGB565 <- RGBA --------------------------------*/
            if(RGB_turn) {
                rid = 0;
                bid = 2;
            } else {
                rid = 2;
                bid = 0;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                tcolor[ctli] = psrc_data[sdid + rid] * 31 / 255 << 11 | psrc_data[sdid + 1] * 63 / 255 << 5 | psrc_data[sdid + bid] * 31 / 255;
                sdid += 4;
            }
        } else if(srcFormat == CR_RGB) { /*------ RGB565 <- RGB ----------------------------------*/
            if(RGB_turn) {
                rid = 0;
                bid = 2;
            } else {
                rid = 2;
                bid = 0;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                tcolor[ctli] = psrc_data[sdid + rid] * 31 / 255 << 11 | psrc_data[sdid + 1] * 63 / 255 << 5 | psrc_data[sdid + bid] * 31 / 255;
                sdid += 3;
            }
        } else if(srcFormat == CR_RGB565) { /*------ RGB565 <- RGB565  --------------------------------*/
            if(RGB_turn) {
                unsigned short *scolor = (unsigned short *)psrc_data;
                for(ctli = 0; ctli < lw; ctli += corw) {
                    tcolor[ctli] = ((scolor[sdid] & 31) << 11) | (scolor[sdid] & 2016) | ((scolor[sdid] & 63488) >> 11);
                    ++sdid;
                }
            } else memcpy(pdesc_data, psrc_data, lw);

        } else if(srcFormat == CR_LUMINANCE_ALPHA) { /*------ RGB565 <- LA -----------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                tcolor[ctli] = psrc_data[sdid] * 31 / 255 << 11 | psrc_data[sdid] * 63 / 255 << 5 | psrc_data[sdid] * 31 / 255;
                sdid += 2;
            }
        } else if(srcFormat == CR_ALPHA || srcFormat == CR_LUMINANCE) { /*------ RGB565 <- A,L */
            for(ctli = 0; ctli < lw; ctli += corw) {
                tcolor[ctli] = psrc_data[sdid] * 31 / 255 << 11 | psrc_data[sdid] * 63 / 255 << 5 | psrc_data[sdid] * 31 / 255;
                ++sdid;
            }
        } else if(srcFormat == CR_RED) { /*------ RGB565 <- R -----------------------------------*/
            if(RGB_turn) {
                rid = 11;
            } else {
                rid = 0;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                tcolor[ctli] = psrc_data[sdid] * 31 / 255 << rid;
                ++sdid;
            }
        } else if(srcFormat == CR_GREEN) { /*------ RGB565 <- G -----------------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                tcolor[ctli] = psrc_data[sdid] * 63 / 255 << 5;
                ++sdid;
            }
        } else if(srcFormat == CR_BLUE) { /*------ RGB565 <- B -----------------------------------*/
            if(RGB_turn) {
                bid = 0;
            } else {
                bid = 11;
            }
            for(ctli = 0; ctli < lw; ctli += corw) {
                tcolor[ctli] = psrc_data[sdid] * 31 / 255 << bid;
                ++sdid;
            }
        }
    } else if(descFormat == CR_LUMINANCE_ALPHA) {
        int corw = 2, lw = img_w * corw; /******转换成亮度和透明****************/
        if(srcFormat == CR_RGBA) { /*------ LA <- RGBA -----------------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = (unsigned char)(((int)psrc_data[sdid] + (int)psrc_data[sdid + 1] + (int)psrc_data[sdid + 2]) / 3);
                sdid += 3;
                pdesc_data[ctli + 1] = psrc_data[sdid++]; //A
            }
        } else if(srcFormat == CR_RGB) { /*------ LA <- RGB -------------------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = (unsigned char)(((int)psrc_data[sdid] + (int)psrc_data[sdid + 1] + (int)psrc_data[sdid + 2]) / 3);
                sdid += 3;
                pdesc_data[ctli + 1] = 255; //A
            }
        } else if(srcFormat == CR_RGB565) { /*------ LA <- RGB565  ------------------------------*/
            unsigned short *tcolor = (unsigned short *)psrc_data;
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = (unsigned char)(((tcolor[sdid] & 31) * 255 / 31 + ((tcolor[sdid] & 2016) >> 5) * 255 / 63 + ((tcolor[sdid] & 63488) >> 11) * 255 / 31) / 3);
                ++sdid;
                pdesc_data[ctli + 1] = 255; //A
            }
        } else if(srcFormat == CR_LUMINANCE_ALPHA) { /*------ LA <- LA --------------------------*/
            memcpy(pdesc_data, psrc_data, lw);

        } else if(srcFormat == CR_ALPHA) { /*------ LA <- A -------------------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = 255; //L
                pdesc_data[ctli + 1] = psrc_data[sdid++]; //A
            }
        } else if(srcFormat == CR_LUMINANCE || srcFormat == CR_RED || srcFormat == CR_GREEN || srcFormat == CR_BLUE) { /*------ LA <- L,R,G,B */
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = psrc_data[sdid++]; //L
                pdesc_data[ctli + 1] = 255; //A
            }
        }
    } else if(descFormat == CR_ALPHA || descFormat == CR_LUMINANCE || descFormat == CR_RED || descFormat == CR_GREEN || descFormat == CR_BLUE) {
        int corw = 1, lw = img_w * corw; /******转换成其他单通道格式****************/
        if(srcFormat == CR_RGBA) { /*------ A,L,R,G,B <- RGBA ----------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = (unsigned char)(((int)psrc_data[sdid] + (int)psrc_data[sdid + 1] + (int)psrc_data[sdid + 2]) * (int)psrc_data[sdid + 3] / 765); //A
                sdid += 4;
            }
        } else if(srcFormat == CR_RGB) { /*------ A,L,R,G,B <- RGB -----------------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = (unsigned char)(((int)psrc_data[sdid] + (int)psrc_data[sdid + 1] + (int)psrc_data[sdid + 2]) / 3); //A
                sdid += 3;
            }
        } else if(srcFormat == CR_RGB565) { /*------ A,L,R,G,B <- RGB565  -----------------------*/
            unsigned short *tcolor = (unsigned short *)psrc_data;
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = (unsigned char)(((tcolor[sdid] & 31) * 255 / 31 + ((tcolor[sdid] & 2016) >> 5) * 255 / 63 + ((tcolor[sdid] & 63488) >> 11) * 255 / 31) / 3);
                ++sdid;
            }
        } else if(srcFormat == CR_LUMINANCE_ALPHA) { /*------ A,L,R,G,B <- LA ------------------*/
            for(ctli = 0; ctli < lw; ctli += corw) {
                pdesc_data[ctli] = (unsigned char)((int)psrc_data[sdid] * (int)psrc_data[sdid + 1] / 255); //A
                sdid += 2;
            }
        } else if(srcFormat >= CR_ALPHA && srcFormat <= CR_BLUE) { /*------ A,L,R,G,B <- A,L,R,G,B */
            memcpy(pdesc_data, psrc_data, lw);
        }
    }
}
GN_API void GNg::ImgFmtRGB555toRGB565(unsigned short *pdesc_data, const unsigned short *psrc_data, int img_w, bool RGB_turn) {
    unsigned short c;
    int i = 0;
    if(RGB_turn) {
        for(; i < img_w; ++i) {
            c = psrc_data[i];
            pdesc_data[i] = (((c & 32767) >> 10)) | (((c & 1023) >> 5) * 63 / 31 << 5) | ((c & 31) << 11);
        }
    } else {
        for(; i < img_w; ++i) {
            c = psrc_data[i];
            pdesc_data[i] = (((c & 32767) >> 10) << 11) | (((c & 1023) >> 5) * 63 / 31 << 5) | (c & 31);
        }
    }
}
GN_API void GNg::ImgFmtRGB565toRGB555(unsigned short *pdesc_data, const unsigned short *psrc_data, int img_w, bool RGB_turn) {
    unsigned short c;
    int i = 0;
    if(RGB_turn) {
        for(; i < img_w; ++i) {
            c = psrc_data[i];
            pdesc_data[i] = ((c & 63488) >> 11) | (((c & 2016) >> 5) * 31 / 63 << 5) | ((c & 31) << 10); /*模板代码:RGB565toBGR555*/
        }
    } else {
        for(; i < img_w; ++i) {
            c = psrc_data[i];
            pdesc_data[i] = ((c & 63488) >> 1) | (((c & 2016) >> 5) * 31 / 63 << 5) | (c & 31);
        }
    }
}
GN_API void GNg::PixFmtRGB565toBGR555(uint8 *pdesc, const uint8 *psrc) {
    unsigned short c = *((unsigned short *)psrc);
    *((unsigned short *)pdesc) = ((c & 63488) >> 11) | (((c & 2016) >> 5) * 31 / 63 << 5) | ((c & 31) << 10); /*模板代码:RGB565toBGR555*/
}
GN_API void GNg::PixFmtRGBAtoBGRA(uint8 *pdesc, const uint8 *psrc) {
    pdesc[2] = psrc[0];
    pdesc[1] = psrc[1];
    pdesc[0] = psrc[2];
    pdesc[3] = psrc[3];
}
GN_API void GNg::PixFmtRGBtoBGR(uint8 *pdesc, const uint8 *psrc) {
    pdesc[2] = psrc[0];
    pdesc[1] = psrc[1];
    pdesc[0] = psrc[2];
}
GN_API void GNg::PixFmtLAtoBGRA(uint8 *pdesc, const uint8 *psrc) {
    pdesc[0] = psrc[0];
    pdesc[1] = psrc[0];
    pdesc[2] = psrc[0];
    pdesc[3] = psrc[1];
}
GN_API void GNg::PixFmtAtoBGRA(uint8 *pdesc, const uint8 *psrc) {
    *((unsigned int *)pdesc) = (psrc[0] << 24) | 0xffffff;
}
GN_API void GNg::PixFmtLtoBGR(uint8 *pdesc, const uint8 *psrc) {
    pdesc[0] = psrc[0];
    pdesc[1] = psrc[0];
    pdesc[2] = psrc[0];
}
GN_API void GNg::PixFmtRtoBGR(uint8 *pdesc, const uint8 *psrc) {
    pdesc[0] = 0;
    pdesc[1] = 0;
    pdesc[2] = psrc[0];
}
GN_API void GNg::PixFmtGtoBGR(uint8 *pdesc, const uint8 *psrc) {
    pdesc[0] = 0;
    pdesc[1] = psrc[0];
    pdesc[2] = 0;
}
GN_API void GNg::PixFmtBtoBGR(uint8 *pdesc, const uint8 *psrc) {
    pdesc[0] = psrc[0];
    pdesc[1] = 0;
    pdesc[2] = 0;
}
GN_API GNg::PFUNC_PIX_CV GNg::PixRGBturnFuncChoose(COLOR_FORMAT format) {
    switch(format) {
        case CR_RGBA: return PixFmtRGBAtoBGRA;
        case CR_RGB: return PixFmtRGBtoBGR;
        case CR_RGB565: return PixFmtRGB565toBGR555;
        case CR_LUMINANCE_ALPHA: return PixFmtLAtoBGRA;
        case CR_ALPHA: return PixFmtAtoBGRA;
        case CR_LUMINANCE: return PixFmtLtoBGR;
        case CR_RED: return PixFmtRtoBGR;
        case CR_GREEN: return PixFmtGtoBGR;
        case CR_BLUE: return PixFmtBtoBGR;
            default: break; // B9-3: 未列举枚举值兜底（保行为）
}
    return 0;
}
GN_API void GNg::ImgFmtMergeRGBandRGBtoRGBA(unsigned char *pdesc_data, const unsigned char *psrc_data, const unsigned char *psrc_data2, int img_w, bool inv_a) {
#define GT_TMP_CR_RGB_MERGE_POINTER_DEF(DB, SB, SB2, CPT, INV_PROC)                                           \
    unsigned char *_dsc = (unsigned char *)pdesc_data, *dsc_end = (unsigned char *)pdesc_data + (img_w * DB); \
    unsigned char *_src = (unsigned char *)psrc_data;                                                         \
    unsigned char *_src2 = (unsigned char *)psrc_data2;                                                       \
    if(inv_a) {                                                                                               \
        for(; _dsc < dsc_end; _dsc += DB, _src += SB, _src2 += SB2) {                                         \
            CPT                                                                                               \
                INV_PROC                                                                                      \
        }                                                                                                     \
    } else {                                                                                                  \
        for(; _dsc < dsc_end; _dsc += DB, _src += SB, _src2 += SB2) {                                         \
            CPT                                                                                               \
        }                                                                                                     \
    }

#define GT_TMP_CR_RGB_MERGE_TO_RGBA(SRC2B)                                                                                          \
    GT_TMP_CR_RGB_MERGE_POINTER_DEF(4, 3, SRC2B,                                                                                    \
                                    _dsc[0] = _src[0];                                                                              \
                                    _dsc[1] = _src[1];                                                                              \
                                    _dsc[2] = _src[2];                                                                              \
                                    _dsc[3] = ((unsigned short)_src2[0] + (unsigned short)_src2[1] + (unsigned short)_src2[2]) / 3; \
                                    , _dsc[3] = 255 - _dsc[3];)

    GT_TMP_CR_RGB_MERGE_TO_RGBA(3)
}
GN_API void GNg::ImgFmtMergeRGBandRGB_toRGBA(unsigned char *pdesc_data, const unsigned char *psrc_data, const unsigned char *psrc_data2, int img_w, bool inv_a){
    GT_TMP_CR_RGB_MERGE_TO_RGBA(4)} GN_API void GNg::ImgFmtMergeRGBandRGBtoLA(unsigned char *pdesc_data, const unsigned char *psrc_data, const unsigned char *psrc_data2, int img_w, bool inv_a) {
    GT_TMP_CR_RGB_MERGE_POINTER_DEF(2, 3, 3,
                                    _dsc[0] = ((unsigned short)_src[0] + (unsigned short)_src[1] + (unsigned short)_src[2]) / 3;
                                    _dsc[1] = ((unsigned short)_src2[0] + (unsigned short)_src2[1] + (unsigned short)_src2[2]) / 3;
                                    , _dsc[1] = 255 - _dsc[1];)
}
GN_API void GNg::ImgFmtMergeRGBto_A(unsigned char *pdesc_data, const unsigned char *psrc_data, int src_px_byte, unsigned char lum, int img_w, bool inv_a) {
    const unsigned char *psrc_data2 = 0;
    GT_TMP_CR_RGB_MERGE_POINTER_DEF(2, src_px_byte, 0,
                                    _dsc[1] = ((unsigned short)_src[0] + (unsigned short)_src[1] + (unsigned short)_src[2]) / 3;
                                    _dsc[0] = lum;
                                    , _dsc[1] = 255 - _dsc[1];)
}
GN_API void GNg::ImgFmtMerge(unsigned char *pdesc_data, int desc_px_byte, const unsigned char *psrc_data, int src_px_byte, int src_to_px_byte, int src_offset, const unsigned char *psrc_data2, int src_px_byte2, int src_to_px_byte2, int src_offset2, int img_w) {
    int ii = src_px_byte - 1, ii2 = src_px_byte2 - 1;

#define GT_TMP_CR_FMT_MERGE_POINTER_DEF(DB, SB, SB2)                                                          \
    unsigned char *_dsc = (unsigned char *)pdesc_data, *dsc_end = (unsigned char *)pdesc_data + (img_w * DB); \
    unsigned char *_src = (unsigned char *)psrc_data;                                                         \
    unsigned char *_src2 = (unsigned char *)psrc_data2;                                                       \
    unsigned short td;                                                                                        \
    int i;                                                                                                    \
    for(; _dsc < dsc_end; _dsc += DB, _src += SB, _src2 += SB2) {

    GT_TMP_CR_FMT_MERGE_POINTER_DEF(desc_px_byte, src_px_byte, src_px_byte2)
    if(src_to_px_byte == 1) {
        if(src_px_byte > src_to_px_byte) {
            td = *_src;
            for(i = 1; i < src_px_byte; ++i) td += _src[i];
            _dsc[src_offset] = td / (unsigned short)src_px_byte; //压缩为亮度
        } else _dsc[src_offset] = *_src;
    } else {
        for(i = 0; i < src_to_px_byte; ++i) _dsc[src_offset + i] = _src[i < ii ? i : ii];
    }
    if(src_to_px_byte2 == 1) {
        if(src_px_byte2 > src_to_px_byte2) {
            td = *_src2;
            for(i = 1; i < src_px_byte2; ++i) td += _src2[i];
            _dsc[src_offset2] = td / (unsigned short)src_px_byte2; //压缩为亮度
        } else _dsc[src_offset2] = *_src2;
    } else {
        for(i = 0; i < src_to_px_byte2; ++i) _dsc[src_offset2 + i] = _src2[i < ii2 ? i : ii2];
    }
}
}
#endif //_GN_FORMAT_H_
