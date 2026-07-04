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
extern "C" FILE * __cdecl __iob_func(unsigned i) {
	return __acrt_iob_func(i);
}
#endif
#endif
#endif
#include <libpng/png.h>

GN_API bool GN_AddUpperFmtName = 
#if defined _WIN32_ || defined _WIN32_WCE
false;
#else
true;
#endif
/********************************************************* 颜色格式 **********************************************************/

GN_API const char *GNg::ColorFmtNamesEn[] = { "AUTO", "RGBA", "RGB", "RGB565", "RGBA4444", "RGBA5551", "LA", "A", "L", "R", "G", "B", "INDEX", "RGBA16", "RGB16" };

GN_API bool GNg::ImgFmtHasAlpha(COLOR_FORMAT format){
	switch(format){
	case GNg::CR_RGBA:
		/*case GNg::CR_RGBA4444:*/
		/*case GNg::CR_RGBA5551:*/
	case GNg::CR_LUMINANCE_ALPHA:
	case GNg::CR_ALPHA:
	case GNg::CR_RGBA16:
	    return true;
	}
    return false;
}
GN_API bool GNg::ImgFmtIsLuminance(COLOR_FORMAT format){
	return (format == CR_LUMINANCE_ALPHA || format == CR_LUMINANCE);
}
GN_API GNg::COLOR_FORMAT GNg::ImgFmtGetFormat(short bit_num){
    COLOR_FORMAT fmt;
	switch(bit_num){
		case 32: fmt = CR_RGBA;   break;
		case 24: fmt = CR_RGB;    break;
		case 16: fmt = CR_RGB565; break;
		case  8: fmt = CR_PALETTE;break;
		case 48: fmt = CR_RGB16;  break;
		case 64: fmt = CR_RGBA16; break;
		default: fmt = CR_DEFAULT_FORMAT;break;
	}
	return fmt;
}
GN_API short GNg::ImgFmtGetByteNum(COLOR_FORMAT format){
     if     (format == CR_RGBA)return 4;
	 else if(format == CR_RGB )return 3;
	 else if(format == CR_RGB565 /*|| format == CR_RGBA4444 || format == CR_RGBA5551*/|| format == CR_LUMINANCE_ALPHA)return 2;
	 else if(format >= CR_ALPHA && format <= CR_PALETTE)return 1;
	 else if(format == CR_RGB16)return 12;
	 else if(format == CR_RGBA16)return 16;
	 else return 0;
}
GN_API void GNg::ImgFmtConvertTs(uint8 *pdesc_data,COLOR_FORMAT descFormat,const uint8 *psrc_data,COLOR_FORMAT srcFormat,int img_w,bool RGB_turn){
	int ctli, sdid = 0, rid = 0, bid = 0;
	if      (descFormat==CR_RGBA){ int corw = 4, lw = img_w*corw;    /******转换成RGBA****************/
	    if      (srcFormat==CR_RGBA){   /*------ RGBA <- RGBA ---------------------------------------*/
			if(RGB_turn){
				for(ctli=0;ctli<lw;ctli+=corw){
				    pdesc_data[ctli+2] = psrc_data[sdid++];//R
		            pdesc_data[ctli+1] = psrc_data[sdid++];//G
		            pdesc_data[ctli  ] = psrc_data[sdid++];//B
					pdesc_data[ctli+3] = psrc_data[sdid++];//A
	            }
			}else memcpy(pdesc_data, psrc_data, lw);

		}else if(srcFormat==CR_RGB){   /*------ RGBA <- RGB  ---------------------------------------*/
			if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = psrc_data[sdid++];//R
		        pdesc_data[ctli+1  ] = psrc_data[sdid++];//G
		        pdesc_data[ctli+bid] = psrc_data[sdid++];//B
				pdesc_data[ctli+3  ] = 255;//A
	        }
		}else if(srcFormat==CR_RGB565){   /*------ RGBA <- RGB565  --------------------------------*/
			unsigned short *tcolor=(unsigned short*)psrc_data;
			if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = ( tcolor[sdid]&31        )*255/31;//R
		        pdesc_data[ctli+1  ] = ((tcolor[sdid]&2016) >>5 )*255/63;//G
		        pdesc_data[ctli+bid] = ((tcolor[sdid]&63488)>>11)*255/31;//B
				pdesc_data[ctli+3  ] = 255;//A
                ++sdid;
	        }
		}else if(srcFormat==CR_LUMINANCE_ALPHA){ /*------ RGBA <- LA  -----------------------------*/
	        uint8 lum;
			for(ctli=0;ctli<lw;ctli+=corw){
				lum = psrc_data[sdid++];;
				pdesc_data[ctli  ] = lum;
		        pdesc_data[ctli+1] = lum;
		        pdesc_data[ctli+2] = lum;
				pdesc_data[ctli+3] = psrc_data[sdid++];//A
	        }
	    }else if(srcFormat==CR_ALPHA){   /*------ RGBA <- A  --------------------------------------*/
	        memset(pdesc_data, 0xff, lw);
			for(ctli=3;ctli<lw;ctli+=corw)
				pdesc_data[ctli] = psrc_data[sdid++];//A

	    }else if(srcFormat==CR_LUMINANCE){   /*------ RGBA <- L  ---------------------------------*/
	        uint8 lum;
			for(ctli=0;ctli<lw;ctli+=corw){
				lum = psrc_data[sdid++];
				pdesc_data[ctli  ] = lum;
		        pdesc_data[ctli+1] = lum;
		        pdesc_data[ctli+2] = lum;
				pdesc_data[ctli+3] = 255;
	        }
	    }else if(srcFormat==CR_RED){   /*------ RGBA <- R  ---------------------------------------*/
	        if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = psrc_data[sdid++];//R
		        pdesc_data[ctli+1  ] = 0;
		        pdesc_data[ctli+bid] = 0;
			    pdesc_data[ctli+3  ] = 255;
	        }
	    }else if(srcFormat==CR_GREEN){   /*------ RGBA <- G  ------------------------------------*/
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = 0;
		        pdesc_data[ctli+1] = psrc_data[sdid++];//G
		        pdesc_data[ctli+2] = 0;
			    pdesc_data[ctli+3] = 255;
	        }
	    }else if(srcFormat==CR_BLUE){   /*------ RGBA <- B  -------------------------------------*/
	        if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = 0;
		        pdesc_data[ctli+1  ] = 0;
		        pdesc_data[ctli+bid] = psrc_data[sdid++];//B
				pdesc_data[ctli+3  ] = 255;
	        }
	    }
	}else if(descFormat==CR_RGB){ int corw =3, lw =img_w*corw;/******转换成RGB****************/
	    if      (srcFormat==CR_RGBA){   /*------ RGB <- RGBA  -----------------------------------*/
	        if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = psrc_data[sdid++];//R
		        pdesc_data[ctli+1  ] = psrc_data[sdid++];//G
		        pdesc_data[ctli+bid] = psrc_data[sdid++];//B
				                               ++sdid;   //A
	        }
		}else if(srcFormat==CR_RGB){   /*------ RGB <- RGB  ------------------------------------*/
			if(RGB_turn){
                for(ctli=0;ctli<lw;ctli+=corw){
				    pdesc_data[ctli+2] = psrc_data[sdid++];//R
		            pdesc_data[ctli+1] = psrc_data[sdid++];//G
		            pdesc_data[ctli  ] = psrc_data[sdid++];//B
	            }
			}else memcpy(pdesc_data, psrc_data, lw);

		}else if(srcFormat==CR_RGB565){   /*------ RGB <- RGB565  ------------------------------*/
			unsigned short *tcolor=(unsigned short*)psrc_data;
			if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = ( tcolor[sdid]&31        )*255/31;//R
		        pdesc_data[ctli+1  ] = ((tcolor[sdid]&2016) >>5 )*255/63;//G
		        pdesc_data[ctli+bid] = ((tcolor[sdid]&63488)>>11)*255/31;//B
                ++sdid;
	        }
		}else if(srcFormat==CR_LUMINANCE_ALPHA){   /*------ RGB <- LA  -------------------------*/
	        uint8 lum;
			for(ctli=0;ctli<lw;ctli+=corw){
				lum = psrc_data[sdid++];
				pdesc_data[ctli  ] = lum;
		        pdesc_data[ctli+1] = lum;
		        pdesc_data[ctli+2] = lum;
				++sdid;   //A
	        }
	    }else if(srcFormat==CR_ALPHA||srcFormat==CR_LUMINANCE){/*------ RGB <- A,L-----*/
	        uint8 lum;
			for(ctli=0;ctli<lw;ctli+=corw){
				lum = psrc_data[sdid++];
				pdesc_data[ctli  ] = lum;
		        pdesc_data[ctli+1] = lum;
		        pdesc_data[ctli+2] = lum;//A | L
	        }
	    }else if(srcFormat==CR_RED){   /*------ RGB <- R  --------------------------------------*/
	        if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = psrc_data[sdid++];//R
		        pdesc_data[ctli+1  ] = 0;
		        pdesc_data[ctli+bid] = 0;
	        }
	    }else if(srcFormat==CR_GREEN){   /*------ RGB <- G  ------------------------------------*/
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = 0;
		        pdesc_data[ctli+1] = psrc_data[sdid++];//G
		        pdesc_data[ctli+2] = 0;
	        }
	    }else if(srcFormat==CR_BLUE){   /*------ RGB <- B --------------------------------------*/
	        if(RGB_turn){ rid=2;bid=0; }else{ rid=0;bid=2; }
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli+rid] = 0;
		        pdesc_data[ctli+1  ] = 0;
		        pdesc_data[ctli+bid] = psrc_data[sdid++];//B
	        }
	    }
	}else if(descFormat==CR_RGB565){ int corw =1, lw =img_w*corw;/******转换成压宿RGB****************/
	    unsigned short *tcolor=(unsigned short*)pdesc_data;
	    if      (srcFormat==CR_RGBA){   /*------ RGB565 <- RGBA --------------------------------*/
			if(RGB_turn){ rid=0;bid=2; }else{ rid=2;bid=0; }
			for(ctli=0;ctli<lw;ctli+=corw){
				tcolor[ctli]= psrc_data[sdid+rid]*31/255<<11 | psrc_data[sdid+1]*63/255<<5 | psrc_data[sdid+bid]*31/255;
	            sdid+=4;
			}
		}else if(srcFormat==CR_RGB){   /*------ RGB565 <- RGB ----------------------------------*/
			if(RGB_turn){ rid=0;bid=2; }else{ rid=2;bid=0; }
			for(ctli=0;ctli<lw;ctli+=corw){
				tcolor[ctli]= psrc_data[sdid+rid]*31/255<<11 | psrc_data[sdid+1]*63/255<<5 | psrc_data[sdid+bid]*31/255;
	            sdid+=3;
			}
		}else if(srcFormat==CR_RGB565){   /*------ RGB565 <- RGB565  --------------------------------*/
			if(RGB_turn){
			    unsigned short *scolor=(unsigned short*)psrc_data;
			    for(ctli=0;ctli<lw;ctli+=corw){
				    tcolor[ctli]= ((scolor[sdid]&31)<<11) | (scolor[sdid]&2016) | ((scolor[sdid]&63488)>>11);
                    ++sdid;
	            }
			}else  memcpy(pdesc_data, psrc_data, lw);

		}else if(srcFormat==CR_LUMINANCE_ALPHA){   /*------ RGB565 <- LA -----------------------*/
			for(ctli=0;ctli<lw;ctli+=corw){
				tcolor[ctli]= psrc_data[sdid]*31/255<<11 | psrc_data[sdid]*63/255<<5 | psrc_data[sdid]*31/255;
	            sdid+=2;
			}
	    }else if(srcFormat==CR_ALPHA||srcFormat==CR_LUMINANCE){   /*------ RGB565 <- A,L */
			for(ctli=0;ctli<lw;ctli+=corw){
				tcolor[ctli]= psrc_data[sdid]*31/255<<11 | psrc_data[sdid]*63/255<<5 | psrc_data[sdid]*31/255;
	            ++sdid;
			}
	    }else if(srcFormat==CR_RED){    /*------ RGB565 <- R -----------------------------------*/
			if(RGB_turn){ rid=11; }else{ rid=0; }
			for(ctli=0;ctli<lw;ctli+=corw){
				tcolor[ctli]= psrc_data[sdid]*31/255<<rid;
	            ++sdid;
			}
	    }else if(srcFormat==CR_GREEN){  /*------ RGB565 <- G -----------------------------------*/
			for(ctli=0;ctli<lw;ctli+=corw){
				tcolor[ctli]= psrc_data[sdid]*63/255<<5;
	            ++sdid;
			}
	    }else if(srcFormat==CR_BLUE){   /*------ RGB565 <- B -----------------------------------*/
	        if(RGB_turn){ bid=0; }else{ bid=11; }
			for(ctli=0;ctli<lw;ctli+=corw){
				tcolor[ctli]= psrc_data[sdid]*31/255<<bid;
	            ++sdid;
			}
	    }
	}else if(descFormat==CR_LUMINANCE_ALPHA){ int corw =2, lw =img_w*corw;/******转换成亮度和透明****************/
	    if      (srcFormat==CR_RGBA){   /*------ LA <- RGBA -----------------------------------*/
	        for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = (unsigned char)(((int)psrc_data[sdid]+(int)psrc_data[sdid+1]+(int)psrc_data[sdid+2])/3);
				sdid+=3;
				pdesc_data[ctli+1] = psrc_data[sdid++];//A
	        }
		}else if(srcFormat==CR_RGB){   /*------ LA <- RGB -------------------------------------*/
	        for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = (unsigned char)(((int)psrc_data[sdid]+(int)psrc_data[sdid+1]+(int)psrc_data[sdid+2])/3);
				sdid+=3;
				pdesc_data[ctli+1] = 255;//A
	        }
		}else if(srcFormat==CR_RGB565){   /*------ LA <- RGB565  ------------------------------*/
			unsigned short *tcolor=(unsigned short*)psrc_data;
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = (unsigned char)(((tcolor[sdid]&31)*255/31 + ((tcolor[sdid]&2016)>>5)*255/63 + ((tcolor[sdid]&63488)>>11)*255/31)/3);
				++sdid;
				pdesc_data[ctli+1] = 255;//A
	        }
		}else if(srcFormat==CR_LUMINANCE_ALPHA){   /*------ LA <- LA --------------------------*/
	        memcpy(pdesc_data, psrc_data, lw);

	    }else if(srcFormat==CR_ALPHA){   /*------ LA <- A -------------------------------------*/
	        for(ctli=0;ctli<lw;ctli+=corw){
		        pdesc_data[ctli  ] = 255;//L
				pdesc_data[ctli+1] = psrc_data[sdid++];//A
	        }
	    }else if(srcFormat==CR_LUMINANCE||srcFormat==CR_RED||srcFormat==CR_GREEN||srcFormat==CR_BLUE){ /*------ LA <- L,R,G,B */
	        for(ctli=0;ctli<lw;ctli+=corw){
		        pdesc_data[ctli  ] = psrc_data[sdid++];//L
				pdesc_data[ctli+1] = 255;//A
	        }
		}
	}else if(descFormat==CR_ALPHA||descFormat==CR_LUMINANCE||descFormat==CR_RED||descFormat==CR_GREEN||descFormat==CR_BLUE){
		int corw =1, lw =img_w*corw;                                    /******转换成其他单通道格式****************/
	    if      (srcFormat==CR_RGBA){   /*------ A,L,R,G,B <- RGBA ----------------------------*/
	        for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = (unsigned char)(((int)psrc_data[sdid]+(int)psrc_data[sdid+1]+(int)psrc_data[sdid+2])*(int)psrc_data[sdid+3]/765);//A
				sdid+=4;
	        }
		}else if(srcFormat==CR_RGB){    /*------ A,L,R,G,B <- RGB -----------------------------*/
	        for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = (unsigned char)(((int)psrc_data[sdid]+(int)psrc_data[sdid+1]+(int)psrc_data[sdid+2])/3);//A
				sdid+=3;
	        }
		}else if(srcFormat==CR_RGB565){  /*------ A,L,R,G,B <- RGB565  -----------------------*/
			unsigned short *tcolor=(unsigned short*)psrc_data;
			for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = (unsigned char)(((tcolor[sdid]&31)*255/31 + ((tcolor[sdid]&2016)>>5)*255/63 + ((tcolor[sdid]&63488)>>11)*255/31)/3);
				++sdid;
	        }
		}else if(srcFormat==CR_LUMINANCE_ALPHA){    /*------ A,L,R,G,B <- LA ------------------*/
	        for(ctli=0;ctli<lw;ctli+=corw){
				pdesc_data[ctli  ] = (unsigned char)((int)psrc_data[sdid]*(int)psrc_data[sdid+1]/255);//A
				sdid+=2;
	        }
	    }else if(srcFormat>=CR_ALPHA && srcFormat<=CR_BLUE){  /*------ A,L,R,G,B <- A,L,R,G,B */
	        memcpy(pdesc_data, psrc_data, lw);
	    }
	}
}
GN_API void GNg::ImgFmtRGB555toRGB565(unsigned short *pdesc_data, const unsigned short *psrc_data, int img_w, bool RGB_turn){
	unsigned short c;
	int i = 0;
	if(RGB_turn){
		for(; i < img_w; ++i){
			c = psrc_data[i];
			pdesc_data[i] = (((c&32767)>>10))	  | (((c&1023)>>5)*63/31 << 5) | ((c&31)<<11);
		}
	}else{
		for(; i < img_w; ++i){
			c = psrc_data[i];
			pdesc_data[i] = (((c&32767)>>10)<<11) | (((c&1023)>>5)*63/31 << 5) | (c&31);
		}
	}
}
GN_API void GNg::ImgFmtRGB565toRGB555(unsigned short *pdesc_data, const unsigned short *psrc_data, int img_w, bool RGB_turn){
	unsigned short c;
	int i = 0;
	if(RGB_turn){
		for(; i < img_w; ++i){
			c = psrc_data[i];
			pdesc_data[i] = ((c&63488)>>11)| (((c&2016) >>5 )*31/63 << 5) | ((c&31)<<10); /*模板代码:RGB565toBGR555*/
		}
	}else{
		for(; i < img_w; ++i){
			c = psrc_data[i];
			pdesc_data[i] = ((c&63488)>>1) | (((c&2016) >>5 )*31/63 << 5) | (c&31);
		}
	}
}
GN_API void GNg::PixFmtRGB565toBGR555(uint8 *pdesc, const uint8 *psrc){
	unsigned short c = *((unsigned short*)psrc);
	*((unsigned short*)pdesc) = ((c&63488)>>11)| (((c&2016) >>5 )*31/63 << 5) | ((c&31)<<10); /*模板代码:RGB565toBGR555*/
}
GN_API void GNg::PixFmtRGBAtoBGRA(uint8 *pdesc, const uint8 *psrc){
	pdesc[2] = psrc[0];
	pdesc[1] = psrc[1];
	pdesc[0] = psrc[2];
	pdesc[3] = psrc[3];
}
GN_API void GNg::PixFmtRGBtoBGR(uint8 *pdesc, const uint8 *psrc){
	pdesc[2] = psrc[0];
	pdesc[1] = psrc[1];
	pdesc[0] = psrc[2];
}
GN_API void GNg::PixFmtLAtoBGRA(uint8 *pdesc, const uint8 *psrc){
	pdesc[0] = psrc[0];
	pdesc[1] = psrc[0];
	pdesc[2] = psrc[0];
	pdesc[3] = psrc[1];
}
GN_API void GNg::PixFmtAtoBGRA(uint8 *pdesc, const uint8 *psrc){
	*((unsigned int *)pdesc) = (psrc[0]<<24) | 0xffffff;
}
GN_API void GNg::PixFmtLtoBGR(uint8 *pdesc, const uint8 *psrc){
	pdesc[0] = psrc[0];
	pdesc[1] = psrc[0];
	pdesc[2] = psrc[0];
}
GN_API void GNg::PixFmtRtoBGR(uint8 *pdesc, const uint8 *psrc){
	pdesc[0] = 0;
	pdesc[1] = 0;
	pdesc[2] = psrc[0];
}
GN_API void GNg::PixFmtGtoBGR(uint8 *pdesc, const uint8 *psrc){
	pdesc[0] = 0;
	pdesc[1] = psrc[0];
	pdesc[2] = 0;
}
GN_API void GNg::PixFmtBtoBGR(uint8 *pdesc, const uint8 *psrc){
	pdesc[0] = psrc[0];
	pdesc[1] = 0;
	pdesc[2] = 0;
}
GN_API GNg::PFUNC_PIX_CV GNg::PixRGBturnFuncChoose(COLOR_FORMAT format){
	switch(format){
	case CR_RGBA           : return PixFmtRGBAtoBGRA;
	case CR_RGB            : return PixFmtRGBtoBGR;
	case CR_RGB565         : return PixFmtRGB565toBGR555;
	case CR_LUMINANCE_ALPHA: return PixFmtLAtoBGRA;
	case CR_ALPHA          : return PixFmtAtoBGRA;
	case CR_LUMINANCE      : return PixFmtLtoBGR;
	case CR_RED            : return PixFmtRtoBGR;
	case CR_GREEN          : return PixFmtGtoBGR;
	case CR_BLUE           : return PixFmtBtoBGR;
	}
	return 0;
}
GN_API void GNg::ImgFmtMergeRGBandRGBtoRGBA(unsigned char *pdesc_data, const unsigned char *psrc_data, const unsigned char *psrc_data2, int img_w, bool inv_a){
   #define GT_TMP_CR_RGB_MERGE_POINTER_DEF( DB , SB , SB2 , CPT , INV_PROC ) \
	unsigned char *_dsc  = (unsigned char *)pdesc_data, *dsc_end = (unsigned char *)pdesc_data + (img_w * DB);\
	unsigned char *_src  = (unsigned char *)psrc_data;\
	unsigned char *_src2 = (unsigned char *)psrc_data2;\
	if(inv_a){\
        for(; _dsc < dsc_end; _dsc += DB, _src += SB, _src2 += SB2){\
            CPT \
            INV_PROC \
		}\
	}else{\
	    for(; _dsc < dsc_end; _dsc += DB, _src += SB, _src2 += SB2){\
	        CPT \
		}\
	}

   #define GT_TMP_CR_RGB_MERGE_TO_RGBA( SRC2B ) \
	GT_TMP_CR_RGB_MERGE_POINTER_DEF( 4 , 3 , SRC2B ,\
		_dsc[0] = _src[0];\
	    _dsc[1] = _src[1];\
	    _dsc[2] = _src[2];\
	    _dsc[3] = ((unsigned short)_src2[0] + (unsigned short)_src2[1] + (unsigned short)_src2[2]) / 3;\
	, _dsc[3] = 255 - _dsc[3]; )

	GT_TMP_CR_RGB_MERGE_TO_RGBA( 3 )
}
GN_API void GNg::ImgFmtMergeRGBandRGB_toRGBA(unsigned char *pdesc_data, const unsigned char *psrc_data, const unsigned char *psrc_data2, int img_w, bool inv_a){
	GT_TMP_CR_RGB_MERGE_TO_RGBA( 4 )
}
GN_API void GNg::ImgFmtMergeRGBandRGBtoLA(unsigned char *pdesc_data, const unsigned char *psrc_data, const unsigned char *psrc_data2, int img_w, bool inv_a){
    GT_TMP_CR_RGB_MERGE_POINTER_DEF( 2 , 3 , 3 ,
		_dsc[0] = ((unsigned short)_src [0] + (unsigned short)_src [1] + (unsigned short)_src [2]) / 3;
	    _dsc[1] = ((unsigned short)_src2[0] + (unsigned short)_src2[1] + (unsigned short)_src2[2]) / 3;
	, _dsc[1] = 255 - _dsc[1]; )
}
GN_API void GNg::ImgFmtMergeRGBto_A(unsigned char *pdesc_data, const unsigned char *psrc_data, int src_px_byte, unsigned char lum, int img_w, bool inv_a){
	const unsigned char *psrc_data2 = 0;
	GT_TMP_CR_RGB_MERGE_POINTER_DEF( 2 , src_px_byte , 0 ,
		_dsc[1] = ((unsigned short)_src [0] + (unsigned short)_src [1] + (unsigned short)_src [2]) / 3;
	    _dsc[0] = lum;
	, _dsc[1] = 255 - _dsc[1]; )
}
GN_API void GNg::ImgFmtMerge(unsigned char *pdesc_data, int desc_px_byte, const unsigned char *psrc_data, int src_px_byte, int src_to_px_byte,  int src_offset,const unsigned char *psrc_data2,int src_px_byte2,int src_to_px_byte2, int src_offset2, int img_w){
    int ii = src_px_byte - 1, ii2 = src_px_byte2 - 1;

   #define GT_TMP_CR_FMT_MERGE_POINTER_DEF( DB , SB , SB2 ) \
	unsigned char *_dsc  = (unsigned char *)pdesc_data, *dsc_end = (unsigned char *)pdesc_data + (img_w * DB);\
	unsigned char *_src  = (unsigned char *)psrc_data;\
	unsigned char *_src2 = (unsigned char *)psrc_data2;\
	unsigned short td;\
	int i;\
	for(; _dsc < dsc_end; _dsc += DB, _src += SB, _src2 += SB2){

	GT_TMP_CR_FMT_MERGE_POINTER_DEF(desc_px_byte , src_px_byte , src_px_byte2 )
		if(src_to_px_byte == 1){
			if(src_px_byte > src_to_px_byte){
                td = *_src;
				for(i = 1; i < src_px_byte; ++i)td += _src[i];
                _dsc[src_offset] = td / (unsigned short)src_px_byte;  //压缩为亮度
			}else _dsc[src_offset] = *_src;
		}else{
			for(i = 0; i < src_to_px_byte; ++i)_dsc[src_offset +i] = _src[i < ii ? i : ii];
		}
		if(src_to_px_byte2 == 1){
			if(src_px_byte2 > src_to_px_byte2){
                td = *_src2;
				for(i = 1; i < src_px_byte2; ++i)td += _src2[i];
                _dsc[src_offset2] = td / (unsigned short)src_px_byte2;  //压缩为亮度
			}else _dsc[src_offset2] = *_src2;
		}else{
			for(i = 0; i < src_to_px_byte2; ++i)_dsc[src_offset2+i] = _src2[i < ii2 ? i : ii2];
		}
	}
}
unsigned int GT_ImgRowEndID;
static float GT_ImgPixLinear4(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
   #define GT_TMP_IMG_LINEAR_FACTOR_GET( PB , PB1 ) \
	unsigned int i = (unsigned int)i_;\
	float i__ = i_ - (float)i;\
	unsigned char *pd = pdata + (i PB);\
	if(i__ > 0.0f && i < GT_ImgRowEndID){\
		float inv = 1.0f - i__;\
		unsigned char *pd1 = pd + PB1;

	GT_TMP_IMG_LINEAR_FACTOR_GET( <<2 , 4 )
		*c_add   += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
		c_add[1] += ((float)pd[1] * inv + (float)pd1[1] * i__) * wr;
		c_add[2] += ((float)pd[2] * inv + (float)pd1[2] * i__) * wr;
		c_add[3] += ((float)pd[3] * inv + (float)pd1[3] * i__) * wr;
	}else{
		*c_add   += (float)(*pd) * wr;
		c_add[1] += (float)pd[1] * wr;
		c_add[2] += (float)pd[2] * wr;
		c_add[3] += (float)pd[3] * wr;
	}
	return wr;
}
static float GT_ImgPixLinear3(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
	GT_TMP_IMG_LINEAR_FACTOR_GET( *3, 3 )
		*c_add   += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
		c_add[1] += ((float)pd[1] * inv + (float)pd1[1] * i__) * wr;
		c_add[2] += ((float)pd[2] * inv + (float)pd1[2] * i__) * wr;
	}else{
		*c_add   += (float)(*pd) * wr;
		c_add[1] += (float)pd[1] * wr;
		c_add[2] += (float)pd[2] * wr;
	}
	return wr;
}
static float GT_ImgPixLinear2(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
	GT_TMP_IMG_LINEAR_FACTOR_GET( <<1, 2 )
		*c_add   += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
		c_add[1] += ((float)pd[1] * inv + (float)pd1[1] * i__) * wr;
	}else{
		*c_add   += (float)(*pd) * wr;
		c_add[1] += (float)pd[1] * wr;
	}
	return wr;
}
static float GT_ImgPixLinear1(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
	GT_TMP_IMG_LINEAR_FACTOR_GET(   , 1 )
		*c_add   += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
	}else{
		*c_add   += (float)(*pd) * wr;
	}
	return wr;
}
static float GT_ImgLinePixMerge4(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
   #define GT_TMP_IMG_LINE_PIX_MERGE( PROC , PROC1 , PROC2 ) \
	unsigned char *pd;\
	unsigned int i = (unsigned int)i_; /*合并左端像素*/\
	float i__ = i_ - (float)i, wei = 0.0f;\
	if(i__ > 0.0f){ i__ = (1.0f - i__) * wr;\
		wei += i__;\
		PROC \
		++i;\
	}\
	unsigned int i2 = (unsigned int)i2_; /*合并右端像素*/\
	i__ = i2_ - (float)i2;\
	if(i__ > 0.0f){ i__ = i__ * wr;\
		wei += i__;\
		PROC1 \
	}\
	for(; i < i2; ++i){ /*合并中间连续像素*/\
		wei += wr;\
		PROC2 \
	}\
	return wei;

	#define GT_TMP_ADD_PIX_COLOR4( I , R ) \
		pd = pdata + (I <<2);\
		c_add[0] += (float)*pd   * R;\
		c_add[1] += (float)pd[1] * R;\
		c_add[2] += (float)pd[2] * R;\
		c_add[3] += (float)pd[3] * R;

	GT_TMP_IMG_LINE_PIX_MERGE( GT_TMP_ADD_PIX_COLOR4( i , i__ ) , GT_TMP_ADD_PIX_COLOR4( i2 , i__ ) , GT_TMP_ADD_PIX_COLOR4( i , wr ) )
}
static float GT_ImgLinePixMerge3(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
	#define GT_TMP_ADD_PIX_COLOR3( I , R ) \
		pd = pdata + (I *3);\
		c_add[0] += (float)*pd   * R;\
		c_add[1] += (float)pd[1] * R;\
		c_add[2] += (float)pd[2] * R;

	GT_TMP_IMG_LINE_PIX_MERGE( GT_TMP_ADD_PIX_COLOR3( i , i__ ) , GT_TMP_ADD_PIX_COLOR3( i2 , i__ ) , GT_TMP_ADD_PIX_COLOR3( i , wr ) )
}
static float GT_ImgLinePixMerge2(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
	#define GT_TMP_ADD_PIX_COLOR2( I , R ) \
		pd = pdata + (I <<1);\
		c_add[0] += (float)*pd   * R;\
		c_add[1] += (float)pd[1] * R;

	GT_TMP_IMG_LINE_PIX_MERGE( GT_TMP_ADD_PIX_COLOR2( i , i__ ) , GT_TMP_ADD_PIX_COLOR2( i2 , i__ ) , GT_TMP_ADD_PIX_COLOR2( i , wr ) )
}
static float GT_ImgLinePixMerge1(unsigned char *pdata, float i_, float i2_, float wr, float *c_add){
	#define GT_TMP_ADD_PIX_COLOR1( I , R ) \
		c_add[0] += (float)pdata[ I ] * R;

	GT_TMP_IMG_LINE_PIX_MERGE( GT_TMP_ADD_PIX_COLOR1( i , i__ ) , GT_TMP_ADD_PIX_COLOR1( i2 , i__ ) , GT_TMP_ADD_PIX_COLOR1( i , wr ) )
}
static void GT_ImgTgPixColorCpt4(unsigned char *pdata, float *cc, float wr){
	*pdata   = *cc   * wr;
	pdata[1] = cc[1] * wr;
	pdata[2] = cc[2] * wr;
	pdata[3] = cc[3] * wr;
}
static void GT_ImgTgPixColorCpt3(unsigned char *pdata, float *cc, float wr){
	*pdata   = *cc   * wr;
	pdata[1] = cc[1] * wr;
	pdata[2] = cc[2] * wr;
}
static void GT_ImgTgPixColorCpt2(unsigned char *pdata, float *cc, float wr){
	*pdata   = *cc   * wr;
	pdata[1] = cc[1] * wr;
}
static void GT_ImgTgPixColorCpt1(unsigned char *pdata, float *cc, float wr){
	*pdata   = *cc   * wr;
}
GN_API void GNg::ImgScale(uint8 *pdesc_data, int desc_w, int desc_h, const uint8 *psrc_data, int src_w, int src_h, short pix_byte, uint8 smooth){
    unsigned int dw = (unsigned int)desc_w, dh = (unsigned int)desc_h, sw = (unsigned int)src_w, sh = (unsigned int)src_h;
    unsigned int i, j, pb = (unsigned int)pix_byte;
	unsigned int picth = pb * dw, _picth = pb * sw; //行跨字节数
    uint8 *_pdl, *_pd, *pd = pdesc_data, *pd2 = (uint8*)psrc_data;
    float swf = (float)sw, shf = (float)sh;
	float i_ratio = swf / (float)dw, j_ratio = shf / (float)dh;
	float i_addv, j_addv;

	uint8 loc_buf[131072]; //128K
    GN::sTBUF_INT temp_buf(loc_buf, 131072);
	unsigned int *src_index_i = 0;

	if(smooth){//线性梯度采样 ---------------------------------------------------------------------------------------------------------------------
		if(smooth > 2 && (desc_w < src_w || desc_h < src_h)){
			bool is_min_scl = false;
			if(desc_w < src_w)is_min_scl = true;
			else GT_ImgRowEndID = src_w - 1;
			float (*pf_line_pix_merge)(uint8*, float, float , float, float*) = is_min_scl?GT_ImgLinePixMerge4:GT_ImgPixLinear4;
			void (*pf_tg_pix_color_cpt)(uint8*, float*, float) = GT_ImgTgPixColorCpt4;
			switch(pix_byte){
			case 3: pf_line_pix_merge = is_min_scl?GT_ImgLinePixMerge3:GT_ImgPixLinear3; pf_tg_pix_color_cpt = GT_ImgTgPixColorCpt3; break;
			case 2: pf_line_pix_merge = is_min_scl?GT_ImgLinePixMerge2:GT_ImgPixLinear2; pf_tg_pix_color_cpt = GT_ImgTgPixColorCpt2; break;
			case 1: pf_line_pix_merge = is_min_scl?GT_ImgLinePixMerge1:GT_ImgPixLinear1; pf_tg_pix_color_cpt = GT_ImgTgPixColorCpt1; break;
			}
			shf -= j_ratio * 0.5f; swf -= i_ratio * 0.5f;
			unsigned int j2, csize = pix_byte <<2/**sizeof(float)*/;
			uint8 *pd2j, *pd2_j, *pd2_jj, *_pd2j;
			float i_, j_ = 0.0f, i2_, j2_, j__, __j, wei, c4[4];
			if(desc_h < src_h){ //竖缩小
				for(; j_ < shf; j_ = j2_){
					j2_ = j_ + j_ratio;
					j = (unsigned int)j_;
					j__ = j_ - (float)j;
					pd2j = pd2 + j  * _picth;
					if(j__ > 0.0f){
						j__ = 1.0f - j__;
						  pd2_j = pd2j + _picth;
					}else pd2_j = pd2j;
					j2 = (unsigned int)j2_;
					__j = j2_ - (float)j2;
					pd2_jj = pd2 + j2 * _picth;

					for(i_ = 0.0f; i_ < swf; i_ = i2_, pd += pb){
						i2_ = i_ + i_ratio;
						wei = 0.0f; memset(c4, 0, csize);
						if(j__ > 0.0f)
							wei += pf_line_pix_merge(pd2j, i_, i2_, j__, c4);   /*合并上端行像素*/
						if(__j > 0.0f)
							wei += pf_line_pix_merge(pd2_jj, i_, i2_, __j, c4); /*合并下端行像素*/

						for(_pd2j = pd2_j; _pd2j < pd2_jj; _pd2j += _picth)
							wei += pf_line_pix_merge(_pd2j, i_, i2_, 1.0f,c4);  /*合并中间连续行像素*/

						pf_tg_pix_color_cpt(pd, c4, 1.0f / wei);
					}
				}
			}else{
				float inv;
				unsigned int endj = src_h - 1;
				for(; j_ < shf; j_ = j2_){
					j2_ = j_ + j_ratio;
					j = (unsigned int)j_;
					j__ = j_ - (float)j;
					pd2j = pd2 + j  * _picth;
					if(j__ > 0.0f && j < endj){
						  inv = 1.0f - j__;
						  pd2_j = pd2j + _picth;
					}else pd2_j = 0;

					for(i_ = 0.0f; i_ < swf; i_ = i2_, pd += pb){
						i2_ = i_ + i_ratio;
						wei = 0.0f; memset(c4, 0, csize);
						if(pd2_j){
							wei += pf_line_pix_merge(pd2_j, i_, i2_, j__, c4);
							wei += pf_line_pix_merge(pd2j,  i_, i2_, inv, c4);
						}else{
							wei += pf_line_pix_merge(pd2j,  i_, i2_, 1.0f,c4);
						}
						pf_tg_pix_color_cpt(pd, c4, 1.0f / wei);
					}
				}
			}
		}else{
			if(sh == 1){ //如遇到1象素高度的图象使它变为大于1高度来统一更好地处理
				src_index_i = (unsigned int*)temp_buf.GetSpace( (((dw + dh)<<2) + _picth) << 2 );
                pd2 = (uint8*)(&(src_index_i[ ((dw + dh)<<2) ]));
			    memcpy(pd2,          psrc_data, _picth);
                memcpy(pd2 + _picth, psrc_data, _picth);
				sh = 2;
				j_ratio = (float)sh / (float)dh;
			}else{
				src_index_i = (unsigned int*)temp_buf.GetSpace( ((dw + dh)<<4) ); //一次过分配好各种临时产生数据的容量,以减少性能开销
			}
            unsigned int *src_index_j = src_index_i + dw;
            uint8 *src_index_u1 = (uint8 *)(src_index_j  + dh);
			uint8 *src_index_1u = src_index_u1  + dw;
            uint8 *src_index_v1 = src_index_1u  + dw;
			uint8 *src_index_1v = src_index_v1  + dh;
            float *src_index_u  = (float*)(src_index_1v  + dh);
            float *src_index_v  = (src_index_u + dw);

			unsigned int _i, _j,  pb_picth = pb + _picth;
            float u, v, _u, _v;
			for(i = 0, i_addv = 0.0f; i < dw; ++i, i_addv += i_ratio){
			    _i = (unsigned int)i_addv;
			    src_index_i[i] = _i * pb; //预先计算好行插值样本索引, 以提高性能
				src_index_u[i] = u = i_addv - (float)_i;
				if     (u < 0.360001f ){ src_index_u1[i] = 0;  src_index_1u[i] = 8; }
				else if(u < 0.78f     ){ src_index_u1[i] = 1;  src_index_1u[i] = 1; }
				else                   { src_index_u1[i] = 8;  src_index_1u[i] = 0; }
			}
			for(_i = (sw-2)*pb, i = dw - 1; i > 0; --i){//使不能索引到最右边的一列
				if(src_index_i[i] > _i)src_index_i[i] = _i;
				else break;
			}
			//预先计算好原图象离目标点的相邻点的点索引偏移
			const unsigned int B_0 = 0,  B_1 = pb + 0,  B_2 = _picth + 0,  B_3 = pb_picth + 0;
            const unsigned int G_0 = 1,  G_1 = pb + 1,  G_2 = _picth + 1,  G_3 = pb_picth + 1;
			const unsigned int R_0 = 2,  R_1 = pb + 2,  R_2 = _picth + 2,  R_3 = pb_picth + 2;
			const unsigned int A_0 = 3,  A_1 = pb + 3,  A_2 = _picth + 3,  A_3 = pb_picth + 3;

			uint8 _u1, _1u,  _v1, _1v;

			unsigned int ci = (sw-1)*pb, cj;
			pd = ((dw-1)*pb) + pdesc_data;

			    //计算好最右边一列的象图颜色先 - 不插值
               #define GT_TMP_IMG_SCALE_POSE_COMPUTE1 \
				for(j = 0, j_addv = 0.0f; j < dh; ++j, j_addv += j_ratio, pd += picth){\
				    _j = (unsigned int)j_addv;\
			        src_index_j[j] = cj = _j * _picth; /*预先计算好列插值样本索引, 以提高性能*/\
					src_index_v[j] = v = j_addv - (float)_j;\
					if     (v < 0.360001f){ src_index_v1[j] = 0;  src_index_1v[j] = 8; }\
				    else if(v < 0.78f    ){ src_index_v1[j] = 1;  src_index_1v[j] = 1; }\
				    else                  { src_index_v1[j] = 8;  src_index_1v[j] = 0; }\
                    _pd = (cj + ci) + pd2;

               #define GT_TMP_IMG_SCALE_POSE_COMPUTE1_END \
				}\
				--dw; --dh;\
				for(_j = (sh-2)*_picth, j = dh; j > 0; --j){/*使不能索引到最底的一行*/\
					if(src_index_j[j] > _j)src_index_j[j] = _j;\
					else break;\
				}\
                pd = pdesc_data;

			    //续个目标图象点计算 - 使用插值
               #define GT_TMP_IMG_SCALE_POSE_COMPUTE2 \
				for(j = 0; j < dh; ++j, pd += pb){\
				    _v1 = src_index_v1[j];\
			        _1v = src_index_1v[j];\
					\
                    _pdl = src_index_j[j] + pd2;\
				    for(i = 0; i < dw; ++i, pd += pb){\
					    _pd = _pdl + src_index_i[i];\
						\
                        _u1 = src_index_u1[i];\
			            _1u = src_index_1u[i];

			   //续个目标图象点计算 - 使用线性插值
               #define GT_TMP_IMG_SCALE_POSE_COMPUTE2_L \
				for(j = 0; j < dh; ++j, pd += pb){\
				    _v = src_index_v[j];\
			         v = 1.0f - _v;\
					\
                    _pdl = src_index_j[j] + pd2;\
				    for(i = 0; i < dw; ++i, pd += pb){\
					    _pd = _pdl + src_index_i[i];\
						\
                        _u = src_index_u[i];\
			             u = 1.0f - _u;

                //计算最底一行的象图颜色 - 不插值
               #define GT_TMP_IMG_SCALE_POSE_COMPUTE3 \
				}	}\
					_pdl = src_index_j[j] + pd2;\
                    for(i = 0; i < dw; ++i, pd += pb){\
					    _pd = _pdl + src_index_i[i];

			if(smooth == 1){
	            switch(pix_byte){
				case 4:
                    GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
                        pd[A_0] = _pd[A_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2

                       #define GT_TMP_IMG_SCALE_LEVEL_PROC( CID ) \
                        pd[ CID##_0] = (((_pd[ CID##_0]>>_u1) + (_pd[ CID##_1]>>_1u))>>_v1)   +   (((_pd[ CID##_2]>>_u1) + (_pd[ CID##_3]>>_1u))>>_1v)

                        GT_TMP_IMG_SCALE_LEVEL_PROC( B );
					    GT_TMP_IMG_SCALE_LEVEL_PROC( G );
					    GT_TMP_IMG_SCALE_LEVEL_PROC( R );
					    GT_TMP_IMG_SCALE_LEVEL_PROC( A );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
                        pd[A_0] = _pd[A_0];
				    }
					break;
				case 3:
					GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2
                        GT_TMP_IMG_SCALE_LEVEL_PROC( B );
					    GT_TMP_IMG_SCALE_LEVEL_PROC( G );
					    GT_TMP_IMG_SCALE_LEVEL_PROC( R );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
				    }
					break;
				case 1:
					GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2
                        GT_TMP_IMG_SCALE_LEVEL_PROC( B );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
				    }
					break;
				case 2:
					GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2
                        GT_TMP_IMG_SCALE_LEVEL_PROC( B );
					    GT_TMP_IMG_SCALE_LEVEL_PROC( G );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
				    }
					break;
				}
			}else{
                switch(pix_byte){
				case 4:
                    GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
                        pd[A_0] = _pd[A_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2_L

                       #define GT_TMP_IMG_SCALE_LINEAR_PROC( CID ) \
                        pd[ CID##_0] = (_pd[ CID##_0] * u + _pd[ CID##_1] * _u) * v   +  (_pd[ CID##_2] * u + _pd[ CID##_3] * _u) * _v;

                        GT_TMP_IMG_SCALE_LINEAR_PROC( B );
					    GT_TMP_IMG_SCALE_LINEAR_PROC( G );
					    GT_TMP_IMG_SCALE_LINEAR_PROC( R );
					    GT_TMP_IMG_SCALE_LINEAR_PROC( A );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
                        pd[A_0] = _pd[A_0];
				    }
					break;
				case 3:
					GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2_L
                        GT_TMP_IMG_SCALE_LINEAR_PROC( B );
					    GT_TMP_IMG_SCALE_LINEAR_PROC( G );
					    GT_TMP_IMG_SCALE_LINEAR_PROC( R );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
				    }
					break;
				case 1:
					GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2_L
                        GT_TMP_IMG_SCALE_LINEAR_PROC( B );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
				    }
					break;
				case 2:
					GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
					GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2_L
                        GT_TMP_IMG_SCALE_LINEAR_PROC( B );
					    GT_TMP_IMG_SCALE_LINEAR_PROC( G );
					GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
				    }
					break;
				}
            }
		}
	}else{//邻近点采样 ---------------------------------------------------------------------------------------------------------------------
		src_index_i = (unsigned int*)temp_buf.GetSpace(dw << 2);
		for(i = 0, i_addv = 0.0f; i < dw; ++i, i_addv += i_ratio){
		    src_index_i[i] = (unsigned int)i_addv * pb; //预先计算好相对目标图象行的每个象素在原图象行的第几个位量上, 以提高性能
		}
	        //续个目标图象点计算
           #define GT_TMP_IMG_SCALE_POSE_COMPUTE \
			for(j = 0, j_addv = 0.0f; j < dh; ++j, j_addv += j_ratio){\
                _pdl = ((unsigned int)j_addv * _picth) + pd2;\
			    for(i = 0; i < dw; ++i, pd += pb){\
				    _pd = _pdl + src_index_i[i];

		switch(pix_byte){
		case 4:
			GT_TMP_IMG_SCALE_POSE_COMPUTE
                     pd[0] = _pd[0];
					 pd[1] = _pd[1];
					 pd[2] = _pd[2];
					 pd[3] = _pd[3];
			}	}
			break;
		case 3:
			GT_TMP_IMG_SCALE_POSE_COMPUTE
                     pd[0] = _pd[0];
			         pd[1] = _pd[1];
					 pd[2] = _pd[2];
			}	}
			break;
		case 1:
			GT_TMP_IMG_SCALE_POSE_COMPUTE
                     pd[0] = _pd[0];
			}	}
			break;
		case 2:
			GT_TMP_IMG_SCALE_POSE_COMPUTE
                     pd[0] = _pd[0];
			         pd[1] = _pd[1];
			}	}
			break;
		}
	}
}
GN_API uint8* GNg::ImgCopyToData(const uint8 *pdata, int pitch, int h, bool col_turn){
	int size = pitch * h;
	uint8 *t_data = new uint8[size];
	if(col_turn){
	    pdata += pitch*(h-1);
        uint8 *ps,*pe = t_data + size;
	    for(ps=t_data; ps<pe; ps+=pitch,pdata-=pitch) memcpy(ps, pdata ,pitch);
	
	}else memcpy(t_data,pdata,size);
	return t_data;
}
GN_API void GNg::ImgBlt     (const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, COLOR_FORMAT srcFormat){
    int pb = (int)ImgFmtGetByteNum(srcFormat);
	int src_pitch = src_w * pb,   desc_pitch = desc_w * pb,   row_size = _src_w * pb;

	uint8 *_psrc_data = (uint8*)psrc_data + (src_pitch *_src_y + _src_x * pb);
	pdesc_data += desc_pitch *_desc_y + _desc_x * pb;
	for(_src_h += _src_y;  _src_y < _src_h;  ++_src_y, _psrc_data += src_pitch, pdesc_data += desc_pitch){
		memcpy(pdesc_data, _psrc_data , row_size);
	}
}
GN_API void GNg::ImgTsBlt   (const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, COLOR_FORMAT srcFormat, COLOR_FORMAT descFormat, bool RGB_turn){
	int src_pb = (int)ImgFmtGetByteNum(srcFormat), desc_pb = (int)ImgFmtGetByteNum(descFormat);
	int src_pitch = src_w * src_pb,  desc_pitch = desc_w * desc_pb;

	uint8 *_psrc_data = (uint8*)psrc_data + (src_pitch *_src_y + _src_x * src_pb);
	pdesc_data += desc_pitch *_desc_y + _desc_x * desc_pb;
	for(_src_h += _src_y ;  _src_y < _src_h;  ++_src_y, _psrc_data += src_pitch, pdesc_data += desc_pitch){
	    ImgFmtConvertTs(pdesc_data, descFormat, _psrc_data , srcFormat, _src_w, RGB_turn);
	}
}
GN_API void GNg::ImgScaleBlt(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, int _desc_w, int _desc_h, COLOR_FORMAT srcFormat, uint8 *proc_buf){
	if(_src_w == _desc_w && _src_h == _desc_h){
	    ImgBlt(psrc_data, src_w, _src_x, _src_y, _src_w, _src_h, pdesc_data, desc_w, _desc_x , _desc_y , srcFormat);
	}else{
		int pb = (int)ImgFmtGetByteNum(srcFormat);
		int src_size = _src_w * _src_h * pb;
		bool buf_is_new = false;
		if(!proc_buf){
            proc_buf = new uint8[(src_size + (_desc_w * _desc_h * pb))<<1];
		    buf_is_new = true;
		}
        uint8 *__proc_buf = proc_buf + src_size;

		ImgBlt(psrc_data, src_w, _src_x, _src_y, _src_w, _src_h, proc_buf, _src_w, 0 , 0 , srcFormat); //先把原数据块移到一个完整的小区划里
        ImgScale(__proc_buf, _desc_w, _desc_h, proc_buf, _src_w, _src_h, (short)pb);                   //从小区划缩放到另一小区划
        ImgBlt(__proc_buf, _desc_w, 0, 0, _desc_w, _desc_h, pdesc_data, desc_w, _desc_x , _desc_y , srcFormat); //再从另一小区划里般到实际想要的地方

		if(buf_is_new)delete [] proc_buf;
	}
}
GN_API void GNg::ImgMultMultiplyRatio(uint8 *pdata, COLOR_FORMAT cfmt, int img_w, int x, int y, int w, int h, uint8 ratio, bool to_alpha){
    if(to_alpha && cfmt != CR_RGBA /*&& cfmt != CR_RGBA4444 && cfmt != CR_RGBA5551*/ && cfmt != CR_LUMINANCE_ALPHA && cfmt != CR_ALPHA)to_alpha = false;
	unsigned int r_ = (unsigned int)ratio;
	int pb = (int)ImgFmtGetByteNum(cfmt);
	int pitch = img_w * pb, row_size = w * pb, j;
    uint8 *_data = pdata + (y * pitch + x * pb), *_data_sta, *_data_end;
	if(to_alpha){
		uint8 aid = 0;
		switch(cfmt){
		case CR_RGBA:            aid = 3; break;
      /*case CR_RGBA4444:        aid = 3; break;
		case CR_RGBA5551:        aid = 3; break;*/
		case CR_LUMINANCE_ALPHA: aid = 1; break;
		}
       #define GT_TMP_IMG_RANGE_LOOP \
		for(j = 0; j < h; ++j, _data += pitch){\
			for(_data_sta = _data, _data_end = _data + row_size; _data_sta < _data_end; _data_sta += pb){
        GT_TMP_IMG_RANGE_LOOP
			    _data_sta[aid] = (uint8)((unsigned int)_data_sta[aid] * r_ >> 8);
		}	}
	}else{
		switch(cfmt){
		case CR_RGBA:
		case CR_RGB:
			GT_TMP_IMG_RANGE_LOOP
                _data_sta[0] = (uint8)((unsigned int)_data_sta[0] * r_ >> 8);
			    _data_sta[1] = (uint8)((unsigned int)_data_sta[1] * r_ >> 8);
			    _data_sta[2] = (uint8)((unsigned int)_data_sta[2] * r_ >> 8);
            }	}break;
		case CR_RGB565:{
			unsigned short *tcolor;
			GT_TMP_IMG_RANGE_LOOP
                tcolor = (unsigned short*)_data_sta;
			    tcolor[0] = (unsigned short)(((unsigned int)(tcolor[0]&31) * r_ >> 8) | ((unsigned int)((tcolor[0]&2016)>>5) * r_ >> 8 << 5) | ((unsigned int)((tcolor[0]&63488)>>11) * r_ >> 8 << 11));
			}	}  }break;
	  /*case CR_RGBA4444:
		case CR_RGBA5551:
		     break;*/
		case CR_LUMINANCE_ALPHA:
		case CR_LUMINANCE:
		case CR_RED:
		case CR_GREEN:
		case CR_BLUE:
            GT_TMP_IMG_RANGE_LOOP
                _data_sta[0] = (uint8)((unsigned int)_data_sta[0] * r_ >> 8);
            }	}break;
		}
	}
}
GN_API void GNg::ImgBlendRGBAtoRGBAusNormal(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y){
	int src_px_size = (int)ImgFmtGetByteNum(GNg::CR_RGBA);
    int desc_px_size = src_px_size;
    unsigned int  r_, _r;

   #define GT_TMP_IMAGE_PIXEL_RANGE_LOOP \
	int src_pitch = src_w * src_px_size,   desc_pitch = desc_w * desc_px_size,   row_size = _src_w * src_px_size;\
	uint8 *_psrc_data = (uint8*)psrc_data + (src_pitch *_src_y + _src_x * src_px_size);\
	pdesc_data += desc_pitch *_desc_y + _desc_x * desc_px_size;\
	uint8 *_pdesc_pix, *_psrc_pix, *_psrc_pix_end;\
	for(_src_h += _src_y ;  _src_y < _src_h;  ++_src_y, _psrc_data += src_pitch, pdesc_data += desc_pitch){\
		for(_pdesc_pix = pdesc_data, _psrc_pix = _psrc_data, _psrc_pix_end = _psrc_data + row_size;  _psrc_pix < _psrc_pix_end;  _pdesc_pix += desc_px_size, _psrc_pix += src_px_size){

    GT_TMP_IMAGE_PIXEL_RANGE_LOOP
		    r_ = (unsigned int)_psrc_pix[3];
			_r = 255 - r_;
            _pdesc_pix[0] = (uint8)(((unsigned int)_pdesc_pix[0] * _r >> 8) + ((unsigned int)_psrc_pix[0] * r_ >> 8));
			_pdesc_pix[1] = (uint8)(((unsigned int)_pdesc_pix[1] * _r >> 8) + ((unsigned int)_psrc_pix[1] * r_ >> 8));
			_pdesc_pix[2] = (uint8)(((unsigned int)_pdesc_pix[2] * _r >> 8) + ((unsigned int)_psrc_pix[2] * r_ >> 8));
			_pdesc_pix[3] = (unsigned int)_pdesc_pix[3] + r_ < 255 ? _pdesc_pix[3] + _psrc_pix[3] : 255;
	}	}
}
GN_API void GNg::ImgBlend(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, int src_px_size, int desc_px_size, void (*blend_func)(const uint8 *src_pix, uint8 *desc_pix)){
    GT_TMP_IMAGE_PIXEL_RANGE_LOOP
		    blend_func(_psrc_pix, _pdesc_pix);
	}	}
}
#pragma pack(push,2)			//设置编译器编译结构体两字节对齐
struct GT_sBMP_HEADER				//bmp文件头
{
	unsigned short       type;           //类型表示
    unsigned int         size;           //文件字节大小
    unsigned short       reserved1;      //保留占位
    unsigned short       reserved2;      //保留占位
    unsigned int         offBits;        //图象数据地址相对文件开始地址的偏移字节
};
#pragma pack(pop)				//取消刚才编译结构体对齐设置
struct GT_sBMP_INFO					//bmp文件信息
{
    unsigned int         size;           //信息结构的大小字节数
    int                  w;              //图象宽
    int                  h;              //图象高
    unsigned short       planes;         //图象帧数
    unsigned short       bit;            //颜色位数
    unsigned int         compression;	 //压缩方式 0为无压缩 也是bmp数据常态
    unsigned int         sizeImage;		 //图象数据大小字节
    int                  xPelsPerMeter;  //水平分辨率
    int                  yPelsPerMeter;  //垂直分辨率
    unsigned int         clrUsed;		 //调色板颜色数
    unsigned int         clrImportant;	 //保留占位
};
//	uint8	rgbBlue,rgbGreen,rgbRed,rgbReserved; //GT_sBMP_INFO 之后如有调色板 接下来的均是调色板各个颜色 BGRX

const char *GT_ERR_FileCreateFail = "写入 %s 文件 %s 失败! 文件创建失败!\n";

//读取BMP图片文件信息(bmp文件信息, 图片文件名或内存地址,使用内存模式指定大小, 读完是否一并关闭文件 否则就把文件指针跳转到数据区)返回正在打开的文件 返回0表示读取失败
GNf::sFILE *GT_LoadBmpFileInfo(char *filename_mem, GNg::COLOR_FORMAT *format, int *w,int *h,int *fdsize, bool close, int memsize){
    GNf::sFILE *fp = GNf::Open(filename_mem,"rb",memsize); //打开文件 filename_mem 和 memsize 可以是磁盘文件 或压缩包 或 内存数据块 具体开 memsize 怎么表示
	if (!fp)return 0;			//文件打开失败
	GT_sBMP_HEADER bmp_head;	//定义bmp文件头结构
	char *type_str = (char*)(&bmp_head);
	GNf::Read(type_str, sizeof(GT_sBMP_HEADER), 1, fp);  //读取文件头
	if(type_str[0] != 'B' || type_str[1] != 'M'){		 //首两个字节如不是 BM 即不是bmp文件
		GNf::Close(fp);			//关闭文件
		return 0;				//返回效验失败
	}
	GT_sBMP_INFO   bmp_info;	//定义bmp文件信息块结构
	GNf::Read((char*)(&bmp_info), sizeof(GT_sBMP_INFO), 1, fp);  //读取文件信息块
	if(bmp_info.h < 0)bmp_info.h = -bmp_info.h;					 //图象高有些软件输出可能为负数就反转为正数
	*w = bmp_info.w; *h = bmp_info.h;			  //返回图片的宽高
	*format = GNg::ImgFmtGetFormat(bmp_info.bit); //把颜色位数转为引擎表示的颜色格式
	*fdsize  = bmp_info.sizeImage;				  //返回图像文件数据大小

	if(close)GNf::Close(fp); //要求关闭就关闭文件
	else GNf::Seek(fp, bmp_head.offBits, GNf::eSEEK_START); //否则使文件指针跳转到数据区 以便接下来读取
	return fp;				 //返回文件
}
GN_API uint8* GNg::LoadBmpFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w,int *h, bool min_out, int memsize){
	int fdsize;
	GT_LoadBmpFileInfo(filename_mem, format, w, h, &fdsize, true, memsize); //读取文件信息后就要求顺便关闭文件
	return 0;  //暂时不处理缩略图返回0
}
GN_API uint8* GNg::LoadBmpFileData(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
    int fdsize;
	GNf::sFILE *fp = GT_LoadBmpFileInfo(filename_mem, format, w, h, &fdsize, false, memsize); //读取bmp文件信息并获得正在打开的文件
	if(!fp)return 0;
	int tw = *w, th = *h;				//宽高接下来频繁访问 把它们传到局部变量处理更高效
	unsigned int px_byte = ImgFmtGetByteNum(*format);  //获取指定颜色格式的单个像素占字节数
	unsigned int  pitch  = px_byte * (unsigned int)tw; //计算到图像一行数据所占字节数
	int			  size   = th * pitch;  //计算到图像数据总大小
	uint8 *tmp_data = new uint8[size];  //分配一块数据以用来保存图像
	unsigned int  sk = 0;
	if(fdsize > size/* + 2*/)sk = (fdsize - (size/* + 2*/))/th; //计算出图像文件数据里一行多出的字节数 bmp文件数据通常每行以4字节的倍数对齐
    //GN_Log("sizeImage=%d, sk=%d\n", bmp_info.sizeImage, sk);	//打印图像文件数据大小和一行多出的字节数到 RunLog.txt 文件里

	int i;
	if(col_turn && sk){  //当要求翻转列象素且有行多出的字节
		for(i = th-1; i >= 0; --i){	//接收数据从最末行开始历遍到首行
			GNf::Read((char*)(tmp_data+(i*pitch)), 1, pitch, fp); //读出一行行数据
			GNf::Seek(fp,sk,GNf::eSEEK_CUR);					  //跳过多余的对齐字节
		}
	}else if(col_turn){  //当要求翻转列象素
	    for(i = th-1; i >= 0; --i)  //接收数据从最末行开始历遍到首行
			GNf::Read((char*)(tmp_data+(i*pitch)), 1, pitch, fp); //读出一行行数据

	}else if(sk){		 //仅有行多出字节
	    for(i = 0; i < th; ++i){	//接收数据从首行开始历遍到最末行
			GNf::Read((char*)(tmp_data+(i*pitch)), 1, pitch, fp); //读出一行行数据
			GNf::Seek(fp,sk,GNf::eSEEK_CUR);					  //跳过多余的对齐字节
		}
	}else{				 //没反转也没行多出的字节
		if(GNf::Read((char*)tmp_data, 1, size, fp) <= 0){ //整片数据一次过读取
			GN_DeleteArray(tmp_data);				//当读取失败时删除刚分配的数据 并使 tmp_data 为0 以便待会返回0
			GN_Log("读取 bmp 文件数据时有错!\n");	//打印图像读取失败信息到 RunLog.txt 文件里
			GNf::Close(fp);						    //退出前先关闭文件
			return 0;								//失败退出
		}
	}
	uint8 *_data = tmp_data, *endd = tmp_data + size;
	for(; _data < endd; _data += px_byte)GN_Swap(uint8,_data[0],_data[2]); //历遍每个像素数据 使红蓝分量值调换一下 即把bmp颜色数据默认的BGR转为引擎用的RGB !您也可以调用GNg::ImgFmtConvertTs函数来转换 但该函数输入输出参数不能为同一指针

    GNf::Close(fp);		 //完成就关闭文件
	return tmp_data;	 //返回图像数据
}
GN_API bool GNg::SaveBmpFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn){
    GNf::sFILE *fp = GNf::Open(file_name,"wb",GNf::eFILE_STD); //打开创建文件(路径文件名, 以写入二进制方式, 创建于标准本地磁盘文件)
	if(!fp){ GN_Log(GT_ERR_FileCreateFail, "bmp", file_name); return false; }  //文件创建失败就打印失败原因到 RunLog.txt 文件里
	GT_sBMP_HEADER bmp_head;	//定义bmp文件头结构
	GT_sBMP_INFO   bmp_info;	//定义bmp文件信息结构
	int sbh = sizeof(GT_sBMP_HEADER);
	int sbi = sizeof(GT_sBMP_INFO);
	memset(&bmp_head,0,sbh);	//使文件头结构里的所有属性先设为0
	memset(&bmp_info,0,sbi);	//使文件信息结构里的所有属性先设为0
	int pixel_byte = (int)ImgFmtGetByteNum(srcformat); //获取指定颜色格式的单个像素占字节数
	bmp_info.bit = (unsigned short)pixel_byte << 3;	   //颜色位数=像素占字节数乘8   <<3相当于乘8 但性能更好
	bmp_info.w = w;  bmp_info.h = h;  //设置图片宽高
	bmp_info.planes = 1;			  //只有1帧图像
	bmp_info.size = 40;				  //文件信息结构
	bmp_info.xPelsPerMeter = 2874;	  //常规水平分辨率
	bmp_info.yPelsPerMeter = 2874;	  //常规水平分辨率
	unsigned int data_size = (unsigned int)pixel_byte * (unsigned int)w * (unsigned int)h; //计算到图像数据总大小
	bmp_info.sizeImage = data_size;   //指定图像数据大小
	//unsigned int   v = data_size % 4;
	//if(v)bmpI.sizeImage += 4 - v;
	bmp_head.type = 0x4d42;			  //文件效验标签 "BM" 的16进制
	bmp_head.offBits = 54;			  //无调色板的数据区在文件开始的偏移字节位置
	bmp_head.size = 54 +  bmp_info.sizeImage; //文件总大小字节
	GNf::Write((char*)&bmp_head,sbh,1,fp);	  //写入文件头信息
    GNf::Write((char*)&bmp_info,sbi,1,fp);    //写入文件信息
	uint8 *data2 = new uint8[data_size];	  //分配一块图像数据大小的内存
	GNg::ImgFmtConvertTs(data2, srcformat, data, srcformat, w * h, true); //使红蓝分量值调换一下 即把引擎用的颜色数据RGB转为bmp默认的BGR

	if(col_turn){  //当要求翻转列象素
	    unsigned int pitch = pixel_byte * (unsigned int)w; //计算到图像一行数据所占字节数
        int i = h - 1;  //写入数据从最末行开始历遍到首行
		for(; i >= 0; --i)GNf::Write((char*)(data2+(i*pitch)),1,pitch,fp); //要翻转图象就要一行行写

	}else GNf::Write((char*)data2,1,data_size,fp); //一次过写入整片图像数据

	GNf::Close(fp);		//写完关闭文件
	delete [] data2;	//删除临时数据
	return true;		//返回成功
}
struct GT_sTGA_HEADER		//TGA文件头
{
	uint8  imageInfoByteLen;    //图像信息长度字节
	uint8  colorTableHas;		//是否含有颜色表
	uint8  imageType;           //图像类型 2:无压缩 10:压缩
	uint8  colorTableInfo[5];   //颜色表信息
	uint8  imageInfo[10];       //图像信息
};
uint8 GT_LoadTgaFileInfo(GT_sTGA_HEADER *_header, GNg::COLOR_FORMAT *format,int *w,int *h, GNf::sFILE *fp){
	//memset(_header, 0, sizeof(GT_sTGA_HEADER));
	GNf::Read((char*)_header, 1, sizeof(GT_sTGA_HEADER), fp);
	if(_header->imageType != 2 && _header->imageType != 10){
		GN_Log("读取 tga 文件失败! 遇到未能解析类型为:%d 的数据!\n", (int)_header->imageType);
		return 0;
	}
	*w = (_header->imageInfo[5]<<8) + _header->imageInfo[4];
	*h = (_header->imageInfo[7]<<8) + _header->imageInfo[6];
	uint8 bitn = _header->imageInfo[8];
	*format = GNg::ImgFmtGetFormat(bitn);
	return bitn;
}
GN_API uint8* GNg::LoadTgaFileInfo(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool min_out,int memsize){
   #define GT_TMP_LOAD_TGA_FILE_INFO \
	GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize);\
	if(!fp)return 0;\
	GT_sTGA_HEADER header;\
	uint8 bitn = GT_LoadTgaFileInfo(&header, format, w, h, fp);

	GT_TMP_LOAD_TGA_FILE_INFO
	GNf::Close(fp);
	return 0;
}
GN_API uint8* GNg::LoadTgaFileData(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize, bool unpk){
	GT_TMP_LOAD_TGA_FILE_INFO
	if(!bitn){
		GNf::Close(fp);
		return 0;
	}
    /*if(*w <= 0 || *h <= 0){
		GNf::Close(fp);
		return 0;
    }*/
    char info[255] = {'\0'};  //读取图像信息
    GNf::Read(info, 1, header.imageInfoByteLen, fp);

	int pbyte = bitn>>3;
	int data_size = (*w) * (*h) * pbyte;
	uint8 *data = new uint8[data_size];
	if(header.imageType == 2){//对于非压缩格式
		GNf::Read((char*)data, 1, data_size, fp);

	}else if(header.imageType == 10){ //对于压缩格式
		int ofs = GNf::Tell(fp);
		GNf::Seek(fp, 0, GNf::eSEEK_END);
		int rsize = GNf::Tell(fp) - ofs;
		GNf::Seek(fp, ofs, GNf::eSEEK_START);
		uint8 *data1 = new uint8[rsize];
		GNf::Read((char*)data1, 1, rsize, fp); //一次性读过来再解析

		int pn = (*w) * (*h), pi = 0;
		uint8 chunk_flag, *_data1 = data1;
		if(bitn == 32){
		   #define GT_TMP_TGA_IMG_DATA_UNPAK( TYPE , N ) \
			TYPE *pd = (TYPE*)data;\
			do{\
				int i = 0;\
                if(_data1[0] & 0x80){\
                    int n = (_data1[0] & 0x7f) + 1; ++_data1;\
					TYPE cc = *((TYPE*)_data1); _data1 += N;\
                    for(; i < n && pi < pn; ++i, ++pi)pd[pi] = cc;\
                }else{\
                    int n = _data1[0] + 1; ++_data1;\
					for(; i < n && pi < pn; ++i, ++pi){\
						pd[pi] = *((TYPE*)_data1); _data1 += N;\
					}\
                }\
			}while(pi < pn);

			GT_TMP_TGA_IMG_DATA_UNPAK(unsigned int , 4)
		}else if(bitn == 24){
			GT_TMP_TGA_IMG_DATA_UNPAK(GNg::sCOLOR3, 3)
		}else if(bitn == 16){
			GT_TMP_TGA_IMG_DATA_UNPAK(unsigned short, 2)
		}
		delete [] data1;
	}
	GNf::Close(fp);
	if(unpk){
		if(bitn != 16)unpk = false;
	}
	if(col_turn || unpk){
		uint8 *data2 = new uint8[unpk ? (data_size>>1)*3 : data_size];
		if(unpk){ //BGR555 转 RGB888
			unsigned short *pkdata = (unsigned short*)data;
			uint8 *_data2 = data2;
			if(col_turn){
				int y = (*h)-1, i, n = (*w);
				for(; y >= 0; --y){
					pkdata = ((unsigned short*)data) + (n * y);
					i = 0;
				   #define GT_TMP_UNPK_BGR555 \
					for(;i < n; ++i, _data2 += 3){\
						_data2[0] = ((pkdata[i]&32767) >> 10)*255/31;\
						_data2[1] = ((pkdata[i]&1023 ) >>  5)*255/31;\
						_data2[2] = ( pkdata[i]&31   )		 *255/31;\
					}
					GT_TMP_UNPK_BGR555
				}
			}else{
				int i = 0, n = (*w) * (*h);
				GT_TMP_UNPK_BGR555
			}
			*format = GNg::CR_RGB;
		}else if(col_turn){
			uint8 *_data1, *_data2 = data2, *_dend;
			int rn = (*w) * pbyte, y = (*h)-1;
			for(; y >= 0; --y){
				_data1 = data + rn * y;
				_dend = _data1 + rn;
				for(; _data1 < _dend; _data1 += pbyte, _data2 += pbyte){
					_data2[2] = _data1[0];
					_data2[1] = _data1[1];
					_data2[0] = _data1[2];
					if(pbyte > 3)_data2[3] = _data1[3];
				}
			}
		}
		delete [] data;
		data = data2;
	}else{
		uint8 *_data1 = data, *_dend = data + (*w) * (*h) * pbyte;
		for(; _data1 < _dend; _data1 += pbyte){
			GN_Swap(uint8 , _data1[2], _data1[0]);
		}
	}
	return data;
}
GN_API bool GNg::SaveTgaFile(char *file_name, uint8 *data,COLOR_FORMAT srcformat,int w,int h,bool col_turn, uint8 comps){
	GNf::sFILE *fp = GNf::Open(file_name,"wb",GNf::eFILE_STD);
	if(!fp){ GN_Log(GT_ERR_FileCreateFail, "tga", file_name); return false; }
	col_turn = !col_turn;
	GT_sTGA_HEADER header;
	memset(&header, 0, sizeof(GT_sTGA_HEADER));
	header.imageType = comps ? 10 : 2;
	header.imageInfo[4] = w & 255;  header.imageInfo[5] = w >> 8;
	header.imageInfo[6] = h & 255;  header.imageInfo[7] = h >> 8;
	short byte_n = ImgFmtGetByteNum(srcformat);
	short byte_n1 = byte_n;
	int srsize = w * byte_n;
	if(byte_n <= 2){
		if(byte_n == 2){
			if(srcformat == CR_LUMINANCE_ALPHA)byte_n = 4;
		}else if(srcformat == CR_ALPHA)byte_n = 4;
		else byte_n = 3;
	}
	header.imageInfo[8] = byte_n << 3;
	header.imageInfo[9] = byte_n == 4 ? 8 : 0;
	GNf::Write((char*)&header, 1, sizeof(GT_sTGA_HEADER), fp);
	int rsize = w * byte_n;
	uint8 buf[16384];
	GN::sTBUF_INT tbuf(buf, 16384);
	//一行行转换一行行写
	uint8 *data1 = (uint8*)tbuf.GetSpace(comps ? rsize + (rsize>>1) : rsize);
	int y1 = col_turn ? h-1 : 0, y2 = col_turn ? -1 : h, add = col_turn ? -1 : 1;
	if(comps){
		--comps;
		uint8 *_data1, *_k, *_data, *__data, *_end, chunk;
		PFUNC_PIX_CV pfunc_pix_cv = PixRGBturnFuncChoose(srcformat);
		for(; y1 != y2; y1 += add){
			_data = data + y1 * srsize;
			_end = _data + srsize;

		   #define GT_TMP_WRITE_IMG_REL_DATA( TGA , CMPN , BYTE_SRC , BYTE_DST , PROC , ID , AK , FK ) \
			__data = _data + BYTE_SRC;\
			\
			_k = data1; chunk = 0; _data1 = data1 + 1;\
			PROC (_data1, _data ID ); _data1 += BYTE_DST;\
			\
			for(; __data < _end; __data += BYTE_SRC){ /*扫描一行像素*/\
				if(GN_MemCompareSimilar(_data ID , __data ID , CMPN , comps)){ /*值相异*/\
					if(chunk & 0x80){\
						++chunk;\
						if((chunk & 0x7f) == FK )goto GT_GOTO_##TGA##PushColorSeg;\
					}else if(!chunk){\
					   GT_GOTO_##TGA##PushEqualColor:\
						chunk = 0x81;\
					}else{\
						*_k = chunk - 1 AK ;\
						_data1 -= BYTE_DST;\
						_data = __data - BYTE_SRC;\
						_k = _data1; ++_data1;\
						PROC (_data1, _data ID ); _data1 += BYTE_DST;\
						goto GT_GOTO_##TGA##PushEqualColor;\
					}\
				}else{ /*值相等*/\
					if(chunk & 0x80){\
					   GT_GOTO_##TGA##PushNotEqualColor:\
						*_k = chunk AK ; chunk = 0;\
						_k = _data1; ++_data1;\
						_data = __data;\
						PROC (_data1, __data ID ); _data1 += BYTE_DST;\
					}else{\
						++chunk;\
						PROC (_data1, __data ID ); _data1 += BYTE_DST;\
						if(chunk == FK ){\
							GT_GOTO_##TGA##PushColorSeg:\
							__data += BYTE_SRC;\
							if(__data >= _end)break;\
							goto GT_GOTO_##TGA##PushNotEqualColor;\
						}\
					}\
				}\
			}\
			*_k = chunk AK ;\
			GNf::Write((char*)data1, 1, _data1 - data1, fp);

			GT_TMP_WRITE_IMG_REL_DATA( Tga , byte_n1, byte_n1 , byte_n , pfunc_pix_cv ,  ,  , 0x7f )
		}
	}else{
		for(; y1 != y2; y1 += add){
			if(srcformat == CR_RGBA || srcformat == CR_RGB){
				ImgFmtConvertTs(data1, srcformat, data + y1 * srsize, srcformat, w, true);
			}else if(srcformat == CR_LUMINANCE_ALPHA || srcformat == CR_ALPHA){
				ImgFmtConvertTs(data1, CR_RGBA, data + y1 * srsize, srcformat, w, true);
			}else if(srcformat == CR_RGB565){
				ImgFmtRGB565toRGB555((unsigned short*)data1, (unsigned short*)(data + y1 * srsize), w, true);
			}else{ //其它单通道色
				ImgFmtConvertTs(data1, CR_RGB,  data + y1 * srsize, srcformat, w, true);
			}
			GNf::Write((char*)data1, 1, rsize, fp);
		}
	}
	GNf::Close(fp);
	return true;
}
struct GT_sRPNG_FILE			//PNG文件数据读取器
{
	GNf::sFILE		*fp;			 //文件
	uint8		    zPngReadBuf[16]; //用来交换gim宽和高参数用
	unsigned int    zPngReadN;		 //用来作跨函数传递临时参数用, 读取gim文件时用到
};
inline void GT_PngReadCallback(png_structp png_ptr, png_bytep data, png_size_t length){
	GT_sRPNG_FILE *rpf = (GT_sRPNG_FILE*)(png_ptr->io_ptr);
	GNf::Read((char*)data, 1, length, rpf->fp);
	rpf->zPngReadN++;
	if(rpf->zPngReadN <= 3){
		uint8 ihdr[]={0x00,0x00,0x00,0x0d, 0x49,0x48,0x44,0x52}; //增补PNG第二个头标识 "IHDR"
		switch(rpf->zPngReadN){
		case 1: memcpy(data,ihdr  ,length); break;
		case 2: memcpy(data,ihdr+4,length); break;
		case 3: memcpy(data,rpf->zPngReadBuf,length); break;
		}
	}
}
GN_API uint8* GNg::LoadPngFileInfo(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool min_out,int memsize){
	*w = GN_INT_MIN; *h = 2147483637 /*GN_INT_MAX-10*/; //用于提示仅获取信息即可
	return LoadPngFileData(filename_mem, format, w, h, 0, memsize);
}
GN_API uint8* GNg::LoadPngFileData(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
	GNf::sFILE *fp = GNf::Open(filename_mem,"rb",memsize);
	if(!fp){
		//GN_Log("读取 png 文件失败! 不能打开该文件或该文件不存在!\n""read_png_file File  could not be opened for reading\n");
		return 0;
	}
	uint8 header[16];
    GNf::Read((char*)header, 1, 4, fp);
    GNf::Seek(fp,0,GNf::eSEEK_START);
	GT_sRPNG_FILE rpf;
	rpf.fp = fp;
    rpf.zPngReadN = 1000000;
    GNf::Read((char*)header,  1, 16, fp);
	if(png_sig_cmp((png_bytep)header, 0, 8) || strncmp((char*)(header+12),"IHDR",4)){
		GN_Log("读取 png 文件失败! 文件不是有效的PNG格式文件\n"); 
		GNf::Close(fp); return 0;
	}
    GNf::Seek(fp,8,GNf::eSEEK_START);

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); //获取图像头数据
    if (!png_ptr){ 
		GN_Log("读取 png 文件失败! 创建读取头失败!\n"); 
		GNf::Close(fp); return 0;
		
	}
    png_infop info_ptr = png_create_info_struct(png_ptr); //获取图像信息头
    if (!info_ptr){\
		GN_Log("读取 png 文件失败! 创建信息头失败!\n");
		png_destroy_read_struct(&png_ptr,&info_ptr,0);
		GNf::Close(fp); return 0;
	}
	png_set_read_fn(png_ptr, &rpf, GT_PngReadCallback); //设置读数据的回调函数
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);
	if(info_ptr->bit_depth != 8){
	    GN_Log("读取 png 文件失败! 该文件使用了非 8位/通道 的格式!\n");
	    png_destroy_read_struct(&png_ptr,&info_ptr,0);
		GNf::Close(fp); return 0;
	}
    int th = info_ptr->height;
	switch(info_ptr->color_type){
	case PNG_COLOR_TYPE_RGB_ALPHA : *format = GNg::CR_RGBA;     break;
	case PNG_COLOR_TYPE_RGB       : *format = GNg::CR_RGB;      break;
	case PNG_COLOR_TYPE_GRAY_ALPHA: *format = GNg::CR_ALPHA;    break;
	case PNG_COLOR_TYPE_GRAY      : *format = GNg::CR_LUMINANCE;break;
	case PNG_COLOR_TYPE_PALETTE   : *format = GNg::CR_PALETTE;  break;
	}
	uint8 *tmp_data = 0;
	if(*w == GN_INT_MIN && *h == 2147483637){
		;
	}else{
		uint8 loc_buf[65536]; //64K
		GN::sTBUF_INT temp_buf(loc_buf, 65536);
		int row_bytes = info_ptr->rowbytes;
		tmp_data = new uint8[th * row_bytes];//分配数据资源
		uint8 **row_ptrs = (uint8**)temp_buf.GetSpace(sizeof(uint8*) * th), *data1;
		int i = 0, th1 = th - 1;
		if(col_turn)for (data1 = tmp_data+(th1*row_bytes); i<th; ++i, data1 -= row_bytes)row_ptrs[i] = data1;
		else/*----*/for (data1 = tmp_data;				   i<th; ++i, data1 += row_bytes)row_ptrs[i] = data1;
		png_read_image(png_ptr, row_ptrs);  //读取图像数据
		//png_read_end(png_ptr, info_ptr);  //读取PNG结束字串
	}
	*w = info_ptr->width; *h = th;
	png_destroy_read_struct(&png_ptr,&info_ptr,0);
	GNf::Close(fp);
	return tmp_data;
}
inline void GT_PngWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length){
	GNf::Write((char*)data,1,length,(GNf::sFILE*)png_ptr->io_ptr);
}
GN_API bool GNg::SavePngFile(char *file_name,uint8 *data,COLOR_FORMAT srcformat,int w,int h,bool col_turn,unsigned int dateID){
	GNf::sFILE *fp = GNf::Open(file_name,"wb",GNf::eFILE_STD);
	if(!fp){ 
		GN_Log(GT_ERR_FileCreateFail, "png", file_name); 
		return false;
	}
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_ptr){
		GN_Log("写入 png 文件失败! 创建写入头失败!\n");
		GNf::Close(fp); return false;
	}
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr){
		GN_Log("写入 png 文件失败! 创建信息头失败!\n");
		png_destroy_write_struct(&png_ptr,&info_ptr);
		GNf::Close(fp); return false;
	}
    //png_init_io(png_ptr, (FILE*)fp->handle_buf);
	png_set_write_fn(png_ptr, fp, GT_PngWriteCallback, 0); /*设置写数据的回调函数*/
    if(setjmp(png_jmpbuf(png_ptr))){
		GN_Log("写入 png 文件失败! 写入信息头失败!\n");
		png_destroy_write_struct(&png_ptr,&info_ptr);
		GNf::Close(fp); return false;
	}
	int row_byte;
	png_byte color_type;
	uint8 *tmp_data = 0;
	if(srcformat == CR_RGB565 || srcformat == CR_LUMINANCE_ALPHA){ //转换为PNG读写规格并转换图象数据
		COLOR_FORMAT d_format;
		if(srcformat == CR_RGB565){
			d_format = CR_RGB;
			color_type = PNG_COLOR_TYPE_RGB;
		}else{
			d_format = CR_RGBA;
			color_type = PNG_COLOR_TYPE_RGB_ALPHA;
		}
		row_byte = w * ImgFmtGetByteNum(d_format);
		tmp_data = new uint8[h*row_byte];
		ImgFmtConvertTs(tmp_data,d_format,data,srcformat,h*w);
		data = tmp_data;
	}else{ //转换为PNG读写规格
		switch(srcformat){ //各种颜色类型的处理
		case CR_RGBA:      color_type = PNG_COLOR_TYPE_RGB_ALPHA;  break;
        case CR_RGB :      color_type = PNG_COLOR_TYPE_RGB;		   break;
		case CR_ALPHA:     color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
		case CR_LUMINANCE: color_type = PNG_COLOR_TYPE_GRAY;	   break;
		//case CR_RGBA4444: break;
		//case CR_RGBA5551: break;
		}
		row_byte = w * ImgFmtGetByteNum(srcformat);
	}
	uint8 loc_buf[65536]; //64K
	GN::sTBUF_INT temp_buf(loc_buf, 65536);
	uint8 **row_ptrs = (uint8**)temp_buf.GetSpace(sizeof(uint8*) * h);
	int i, h1 = h-1;
	if(col_turn)for(i=0; i<h; ++i)row_ptrs[i] = data + ((h1-i)*row_byte);
	else/*----*/for(i=0; i<h; ++i)row_ptrs[i] = data + (i*row_byte);

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

	if(setjmp(png_jmpbuf(png_ptr))){ ret = false; GN_Log("写入 png 文件失败! 写入结束标记时有错!\n"); }
    //结束并释放写缓冲
	png_write_end(png_ptr, info_ptr);
    GT_GOTO_ENDwritePNG:
	png_destroy_write_struct(&png_ptr,&info_ptr);
	
	//清除缓存数据
	if(tmp_data)delete []tmp_data;
	GNf::Close(fp);
	return ret;
}
struct GT_sHDR_HEADER{
  char		programtype[16];
  float		gamma;
  float		exposure;
  int		width;
  int		height;
};
static int GT_LoadHdrFileInfo(GNf::sFILE *fp, GT_sHDR_HEADER *info){
    char buf[128];
	if (!GNf::Gets(buf,128,fp)){
	   GT_GOTO_LoadHdrErr:
		GN_Log("读取 hdr 文件失败! 文件格式有误或损坏!\n");
		return -1;
	}
	if((buf[0] != '#')||(buf[1] != '?')){
      /* if you want to require the magic token then uncomment the next line */
      /*return -2; */
    }
    info->gamma = 1.0f;
    info->exposure = 1.0f;
	info->programtype[0] = 0;
    int i = 0, i2 = 2, pn = sizeof(info->programtype)-1;
    for(; i < pn; ++i, ++i2){
		if((buf[i2] == 0) || isspace(buf[i2]))break;
		info->programtype[i] = buf[i2];
    }
    info->programtype[i] = 0;
    if(!GNf::Gets(buf,128,fp))goto GT_GOTO_LoadHdrErr;

	float tempf;
    for(;;) {
		if ((buf[0] == 0)||(buf[0] == '\n'))goto GT_GOTO_LoadHdrErr;
		else if (!strcmp(buf,"FORMAT=32-bit_rle_rgbe\n"))break;
		else if (sscanf(buf,"GAMMA=%g",&tempf) == 1)info->gamma = tempf;
		else if (sscanf(buf,"EXPOSURE=%g",&tempf) == 1)info->exposure = tempf;
		if(!GNf::Gets(buf,128,fp))goto GT_GOTO_LoadHdrErr;
    }
	//if(!GNf::Gets(buf,128,fp))goto GT_GOTO_LoadHdrErr;
	//if(strcmp(buf,"\n"))goto GT_GOTO_LoadHdrErr;
	for(;;) {
		if(!GNf::Gets(buf,128,fp))goto GT_GOTO_LoadHdrErr;
		if (sscanf(buf,"-Y %d +X %d",&(info->height), &(info->width)) == 2)break;
	}
	return 0;
}
GN_API void GNg::PackRGBE0(uint8 *rgb, uint8 *rgbe/*[4]*/){
	rgbe[0] = rgb[0];
	rgbe[1] = rgb[1];
	rgbe[2] = rgb[2];
	rgbe[3] = 128;
}
GN_API void GNg::PackRGBE(/*GNg::sVEC3* */uint8 *rgb, uint8 *rgbe/*[4]*/){
	float *_c = (float*)rgb;
	int8 e = 0;
	float max_c = _c[0] > _c[1] ? _c[0] : _c[1], exp = 1.0f;
	if (_c[2] > max_c)max_c = _c[2];
	if(max_c > 1.0f){
		++e;
		int8 e2 = (int8)max_c;
		if((float)e2 < max_c)++e2;
		while((1<<e) < e2)++e;
		exp = (float)(1<<e);
	}
	exp = 255.0f / exp;
	rgbe[0] = (uint8)(_c[0] * exp);
	rgbe[1] = (uint8)(_c[1] * exp);
	rgbe[2] = (uint8)(_c[2] * exp);
	rgbe[3] = e + 128;
}
inline void GT_UnpkRGBE(/*GNg::sVEC3* */float *rgbf, uint8 *rgbe/*[4]*/){
	if(rgbe[3]){
		float f = (float)ldexp(1.0,rgbe[3]-(int)(128+8));
		rgbf[0] = rgbe[0] * f;
		rgbf[1] = rgbe[1] * f;
		rgbf[2] = rgbe[2] * f;
	}else{
		rgbf[0] = 0.0f; rgbf[1] = 0.0f; rgbf[2] = 0.0f;
	}
}
GN_API void GNg::UnpkRGBE(/*GNg::sVEC3* */float *rgbf, uint8 *rgbe/*[4]*/){
	GT_UnpkRGBE(rgbf, rgbe);
}
static int GT_ReadImgPixelsRGBE(GNf::sFILE *fp, float *data, int w, int h, bool col_turn){
	uint8 rgbe[4];
	if(col_turn){
		int w3 = w * 3, i;
		float *_data, *_end;
		while(h-- > 0){
			_data = data + w3 * h;
			_end = _data + w3;
			for(; _data < _end; _data += 3){
				if(GNf::Read((char*)rgbe, 4, 1, fp) < 1)return -1;
				GT_UnpkRGBE(_data, rgbe);
			}
		}
	}else{
		int wh = w * h;
		while(wh-- > 0){
		  if(GNf::Read((char*)rgbe, 4, 1, fp) < 1)return -1;
          GT_UnpkRGBE(data, rgbe);
		  data += 3;
		}
	}
	return 0;
}
static int GT_ReadImgPixelsRGBE_REL(GNf::sFILE *fp, float *data, int w, int h, bool col_turn){
  uint8 rgbe[4], *scanline_buffer = 0, *ptr, *ptr_end;
  int i, count, w3 = w * 3;
  uint8 buf[2];

  if((w < 8)||(w > 0x7fff))return GT_ReadImgPixelsRGBE(fp, data, w, h, col_turn);
  float *_data = data;
  while(h > 0){
	if(col_turn)_data = data + (h-1) * w3;
	if(GNf::Read((char*)rgbe, 4, 1, fp) < 1){
		GN_MemFree(scanline_buffer);
		return -1;
	}
    if((rgbe[0] != 2)||(rgbe[1] != 2)||(rgbe[2] & 0x80)){ //非压缩格式
      GT_UnpkRGBE(_data, rgbe);
      _data += 3;
	  GN_MemFree(scanline_buffer);
	  if(GT_ReadImgPixelsRGBE(fp, _data, w - 1, 1, col_turn))return -1;
      return GT_ReadImgPixelsRGBE(fp, data, w, (h - 1), col_turn);
    }
    if((((int)rgbe[2])<<8 | rgbe[3]) != w){
      GN_MemFree(scanline_buffer);
      return -1;
    }
    if(!scanline_buffer)scanline_buffer = (uint8*)malloc(4*w);
    if(!scanline_buffer)return -2;
    ptr = scanline_buffer;
    for(i=0; i<4; ++i){ //GT_TMP_TGA_IMG_DATA_UNPAK
      ptr_end = scanline_buffer + (i+1) * w;
      while(ptr < ptr_end){
		GNf::Read((char*)buf, 2, 1, fp);
		if(buf[0] > 128){
		  count = buf[0]-128;
		  if((count == 0)||(count > ptr_end - ptr)){
		    GN_MemFree(scanline_buffer);
			return -1;
		  }
		  while(count-- > 0)*ptr++ = buf[1];
		}else{
		  count = buf[0];
		  if((count == 0)||(count > ptr_end - ptr)){
		    GN_MemFree(scanline_buffer);
			return -1;
		  }
		  *ptr++ = buf[1];
		  if (--count > 0) {
			if(GNf::Read((char*)ptr, count, 1, fp) < 1){
			  GN_MemFree(scanline_buffer);
			  return -1;
		    }
		    ptr += count;
		  }
		}
      }
    }
    //把间开的通道值转换并复制到连续的
    for(i=0; i<w; ++i){
      rgbe[0] = scanline_buffer[i];
      rgbe[1] = scanline_buffer[i+w];
      rgbe[2] = scanline_buffer[i+2*w];
      rgbe[3] = scanline_buffer[i+3*w];
      GT_UnpkRGBE(_data, rgbe);
      _data += 3;
    }
    --h;
  }
  GN_MemFree(scanline_buffer);
  return 0;
}
GN_API uint8* GNg::LoadHdrFileInfo(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool min_out,int memsize){
   #define GT_TMP_LOAD_HDR_FILE_INFO \
	GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize);\
	if(!fp)return 0;\
	GT_sHDR_HEADER hinfo;\
	if(GT_LoadHdrFileInfo(fp, &hinfo)){\
		GNf::Close(fp);\
		return 0;\
	}\
	*format = CR_RGB16;\
	*w = hinfo.width;\
	*h = hinfo.height;

	GT_TMP_LOAD_HDR_FILE_INFO
	return 0;
}
GN_API uint8* GNg::LoadHdrFileData(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool col_turn, int memsize, float *max_ins){
	GT_TMP_LOAD_HDR_FILE_INFO
	int size = hinfo.width * hinfo.height * 3;
    uint8 *data = (uint8*)new float[size];

    if(GT_ReadImgPixelsRGBE_REL( fp, (float*)data, hinfo.width, hinfo.height, !col_turn)) {
        delete [] data; data = 0;
		GN_Log("读取 hdr 文件失败! 解释数据时遇到错误!\n");
    }
    GNf::Close(fp);
	if(max_ins){
		*format = CR_RGB;
		if(*max_ins < 0.0f){
			uint8 *data_lr = ImgDataHdrToLinearData((float*)data, *w, *h, 3, -(*max_ins));
			delete [] data;
			data = data_lr;
		}else{
			float *dataf = (float*)data, maxins = 0.0f;
			int  i = 0;
			for(; i < size; ++i){ if(dataf[i]>maxins)maxins = dataf[i]; }
			if(maxins > 3.0f)maxins = 3.0f;
			*max_ins = maxins;
			float scl = maxins > 1.0f ? 255.0f / maxins : 255.0f;
			for(i = 0; i < size; ++i)data[i] = dataf[i] < maxins ? (uint8)(dataf[i] * scl) : 255;
		}
	}
	return data;
}
GN_API bool GNg::SaveHdrFile(char *file_name,uint8 *data,COLOR_FORMAT srcformat,int w,int h,bool col_turn, uint8 comps_lv){
	GNf::sFILE *fp = GNf::Open(file_name,"wb",GNf::eFILE_STD);
	if(!fp){ GN_Log(GT_ERR_FileCreateFail, "hdr", file_name); return false; }
	col_turn = !col_turn;
	if(!comps_lv)comps_lv = 1;
	int px_byten = GNg::ImgFmtGetByteNum(srcformat);
	bool not_rgb_a = !(px_byten <= 4 && (GNg::CR_RGB || GNg::CR_RGBA));

	uint8 locbuf[16000];
	GN::sTBUF_INT tbuf(locbuf, 16000);
	int line_size = (w<<2);
	uint8 *wbuf = (uint8*)tbuf.GetSpace(384 >= line_size ? 384 : line_size);
	int head_size = sprintf((char*)wbuf, "#?RADIANCE\n# Made with GN\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", h , w);
	GNf::Write((char*)wbuf, 1, head_size, fp);

	uint8 *lbuf = 0, *_data, *_end, *_wpos;
	int srsize = w * px_byten;
	int	px_byten1 = px_byten;
	if(not_rgb_a){
		px_byten1 = 3;
		lbuf = (uint8*)tbuf.GetSpace(w * 3);
	}
	int srsize1 = w * px_byten1;
	int y1 = col_turn ? h-1 : 0, y2 = col_turn ? -1 : h, add = col_turn ? -1 : 1;
	if(comps_lv){
		uint8 comps = comps_lv - 1;
		uint8 line_header[4] = { 2, 2, (uint8)(w >> 8), (uint8)(w & 0xff) }; //压缩格式行标记
		uint8 *data1 = (uint8*)tbuf.GetSpace(line_size >> 1);
		uint8 *_data1, *_k, *__data, chunk;
		int i;
		for(; y1 != y2; y1 += add){
			GNf::Write((char*)line_header, 4, 1, fp);

		   #define GT_TMP_IMG_FMT_CONVERT_RGBE \
			_data = data + y1 * srsize;\
			_end = _data + srsize;\
			_wpos = wbuf;\
			if(px_byten > 4){ /*浮点格式*/\
				for(; _data < _end; _data += px_byten, _wpos += 4)GNg::PackRGBE(_data, _wpos);\
			}else{\
				if(not_rgb_a){\
					ImgFmtConvertTs(lbuf, CR_RGB, _data, srcformat, w, false);\
					_data = lbuf;\
					_end = _data + srsize1;\
					for(; _data < _end; _data += px_byten1, _wpos += 4)GNg::PackRGBE0(_data, _wpos);\
				}else{\
					for(; _data < _end; _data += px_byten,  _wpos += 4)GNg::PackRGBE0(_data, _wpos);\
				}\
			}
			GT_TMP_IMG_FMT_CONVERT_RGBE	//转换一行写一行

		   #define GT_TMP_SET_PVALUE( DST , SRC ) *(DST) = *(SRC)
			for(i = 0; i < 4; ++i){
				_data = wbuf;
				_end = _data + line_size;
				GT_TMP_WRITE_IMG_REL_DATA( Hdr , 1 , 4 , 1 , GT_TMP_SET_PVALUE , +i , +1 , 0x7e )
			}
		}
	}/*else{
		for(; y1 != y2; y1 += add){
			GT_TMP_IMG_FMT_CONVERT_RGBE
			GNf::Write((char*)wbuf, 1, line_size, fp);
		}
	}*/
	GNf::Close(fp);
	return true;
}
GN_API bool GNg::IsHdrFile(char *filename){
	char *ext_name = GN_StrGetExtName(filename);
	return ( ext_name && (!strcmp(ext_name,ext_name[0]>'Z'?"hdr":"HDR")) );
}
//计算像素周围3x3平均颜色(返回像素平均颜色, 图形数据, 像素位置X, Y, 图像宽, 高, 特定格式每像素占字节数)
static void GT_PixelAvgColor3x3(uint8 *px_out, /*const*/ uint8 *img_data, int x, int y, int w, int h, int pix_byte_n){
	uint8 *_pd;
	unsigned int buf[12];
	memset(buf, 0, sizeof(unsigned int) * pix_byte_n);
	int x2 = x < w-1 ? x + 1 : x;
	int y2 = y < h-1 ? y + 1 : y;
	int x1 = x > 1? x-1 : 0;
	int y1 = y > 1? y-1 : 0;
	int count = (x2 - x1 + 1) * (y2 - y1 + 1), rn;
	switch(pix_byte_n){
	case 3:
	   #define GT_TMP_LOOP_ADJ_PIXEL( PXPROC ) \
		for(; y1 <= y2; ++y1){\
			rn = y1 * w;\
			for(x = x1; x <= x2; ++x){\
				PXPROC ;\
			}\
		}
		GT_TMP_LOOP_ADJ_PIXEL( _pd = img_data + (rn + x) * 3; buf[0] += _pd[0]; buf[1] += _pd[1]; buf[2] += _pd[2] )
		px_out[0] = buf[0] / count; px_out[1] = buf[1] / count; px_out[2] = buf[2] / count;
		break;
	case 4:
		GT_TMP_LOOP_ADJ_PIXEL( _pd = img_data + (rn + x) * 4; buf[0] += _pd[0]; buf[1] += _pd[1]; buf[2] += _pd[2]; buf[3] += _pd[3] )
		px_out[0] = buf[0] / count; px_out[1] = buf[1] / count; px_out[2] = buf[2] / count; px_out[3] = buf[3] / count;
		break;
	case 1:
		GT_TMP_LOOP_ADJ_PIXEL( _pd = img_data + (rn + x); buf[0] += _pd[0] )
		px_out[0] = buf[0] / count;
		break;
	}
}
//像素颜色累积(颜色源, 累计目标颜色, 特定格式每像素占字节数)
static void GT_PixelColorAccum(uint8 *src, uint8 *dst, int pix_byte_n){
	if(dst[0] == 0xff && !dst[1] && dst[2] == 0xff){
		dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
	}else{
		dst[0] = ((unsigned short)dst[0] + (unsigned short)src[0]) >> 1;
		dst[1] = ((unsigned short)dst[1] + (unsigned short)src[1]) >> 1;
		dst[2] = ((unsigned short)dst[2] + (unsigned short)src[2]) >> 1;
	}
}
GN_API uint8* GNg::ImgDataHdrToLinearData(const float *data, int w, int h, int chn, float ha, float gamma){
   #define GT_TMP_HDR_TO_LINEAR( C ) (((C*(0.15f*C+0.05f)+0.004f)/(C*(0.15f*C+0.5f)+0.06f))-0.066667f)
	//float ha1 = ha > 1.0f ? ha : 1.0f;	/*模板代码:HDR to LINEAR*/
	//if(ha1 > hdrlmt)ha1 = hdrlmt;
	float ha1 = 1.0f / GT_TMP_HDR_TO_LINEAR( 11.2f );
	uint8 *ldata = new uint8[w * h * chn];
	uint8 *_ldata = ldata;
	float *_data = (float*)data, *edata = (float*)data + w * h * chn, c;
	if(chn < 4){
		for(; _data < edata; ++_data, ++_ldata){
			 c = *_data * ha;
			 c = c ? pow(GT_TMP_HDR_TO_LINEAR( c ) * ha1, gamma) * 255.0f : 0.0f;
			 *_ldata = c < 255.0f ? (uint8)c : 255;
		}
	}else{
		float *edata1;
		for(; _data < edata; ++_data, ++_ldata){
			edata1 = _data + 3;
			for(; _data < edata1; ++_data, ++_ldata){
				c = *_data * ha;
				c = c ? pow(GT_TMP_HDR_TO_LINEAR( c ) * ha1, gamma) * 255.0f: 0.0f;
				*_ldata = c < 255.0f ? (uint8)c : 255;
			}
			*_ldata = (*_data) < 1.0f ? (uint8)((*_data) * 255.0f) : 255;
		}
	}
	return ldata;
}
GN_API uint8* GNg::LoadImgFileData(eIMG_FILE_TYPE file_type, char *filename_mem, COLOR_FORMAT *format,int *w,int *h,int memsize){
    uint8 *data = 0;
	switch(file_type){
	case eIMG_FILE_PNG:
	case eIMG_FILE_GIM: data = LoadPngFileData(filename_mem,format,w,h,false,memsize); break;
	case eIMG_FILE_BMP: data = LoadBmpFileData(filename_mem,format,w,h,true, memsize); break;
	case eIMG_FILE_TGA: data = LoadTgaFileData(filename_mem,format,w,h,true, memsize); break;
	case eIMG_FILE_HDR: data = LoadHdrFileData(filename_mem,format,w,h,true,memsize); break;
	case eIMG_FILE_UNKNOWN:{
						*format = (COLOR_FORMAT)GNf::GetExtNamePos(filename_mem,memsize);
						data = FileImage->LoadData(filename_mem,format,w,h,false,memsize); }break;
	}
	return data;
}
GN_API bool GNg::SaveImgFile(char *file_name,unsigned char *data,COLOR_FORMAT srcformat,int w,int h, bool col_turn, uint8 comps_lv){
    return FileImage->Save(file_name,data,srcformat,w,h,col_turn,comps_lv);
}
GN_API bool GNg::ImgFileScale(const char *dst_file_name, int dst_w, int dst_h, char *filename_mem, eIMG_FILE_TYPE file_type,int memsize, uint8 comps_lv){
	COLOR_FORMAT color_fmt;
	int w, h;
	uint8 *data = LoadImgFileData(file_type, filename_mem, &color_fmt,&w,&h, memsize);
	if(!data)return false; //读取不到源文件

	uint8 *data_tg = new uint8[dst_w * dst_h * (int)ImgFmtGetByteNum(color_fmt)];
	ImgScale(data_tg, dst_w, dst_h, data, w, h,ImgFmtGetByteNum(color_fmt));

	bool save_ok = FileImage->Save((char*)dst_file_name,data_tg,color_fmt,dst_w,dst_h,false,comps_lv);
	delete [] data_tg;
	delete [] data;
    return save_ok;
}
GN_API GNg::eIMG_FILE_TYPE GNg::GetImgFmtTypeFromFileName(const char *filename){
	char *ext_name = GN_StrGetExtName(filename), ext_name2[8];
	if(ext_name){
		char ofs = 0;
	    GT_GOTO_CheckImgFmtSupport:
		if(!strcmp(ext_name,"png"))return eIMG_FILE_PNG;
		else if(!strcmp(ext_name,"gim"))return eIMG_FILE_GIM;
		else if(!strcmp(ext_name,"bmp"))return eIMG_FILE_BMP;
		else if(!strcmp(ext_name,"tga"))return eIMG_FILE_TGA;
		else if(!strcmp(ext_name,"hdr"))return eIMG_FILE_HDR;
		if(!ofs && ext_name[0] <= 'Z' && ext_name[1] <= 'Z'){
			strcpy(ext_name2, ext_name);
			ext_name = ext_name2;
			char ofs = 'a' - 'A';
			int i = 0;
			while(ext_name[i]){ ext_name[i] += ofs; ++i; } //大写名转小写再检测
			goto GT_GOTO_CheckImgFmtSupport;
		}
	}
	return eIMG_FILE_UNKNOWN;
}
GN_API bool GNg::CheckImgFmtSupportFromFileName(const char *filename){
	char *ext_name = GN_StrGetExtName(filename);
	if(ext_name)return FileImage->CheckFmtSupportFromExtName(ext_name)?true:false;
	return false;
}
static char GT_SupportExtNamesImage[160];
GN_API char *const GNg::GetSupportImgFmtFileExtName(bool upper){
	if(!GT_SupportExtNamesImage[0])FileImage->GetSupportFmtFileExtName(GT_SupportExtNamesImage, upper);
	return (char *const)GT_SupportExtNamesImage;
}
GN_API const char *GNg::ImgFileTypeNames[] = { "bmp","png","tga","gim","hdr" };

GN_API void GNg::GenCylinderDataToMem(uint8 *p_memory,int w, int h, float value){
	float cp = (float)w * 0.5f-0.5f;
	float  r = (float)(w) * 0.5f, _r, _x, _y, _l;
	int i, j, id = 0;
	//if(is_solid){
        _r = r * value;
		float r_ = r - _r;
		for(j = 0; j < h; ++j){
		    for(i = 0; i < w; ++i, ++id){
		        _l = (float)i - cp;
				if(_l < 0.0f)_l = -_l;
				if     (_l >  r)p_memory[id] = 0;
				else if(_l > _r)p_memory[id] = (uint8)((1.0f - (_l - _r) / r_) * 255.0f);
				else            p_memory[id] = 255;
			}
		}
	//}
}
GN_API void GNg::GenSphereDataToMem(uint8 *p_memory,int wh, int b, float value, bool is_solid){
	float cp = (float)wh * 0.5f-0.5f;
	float  r = (float)(wh-(b<<1)) * 0.5f, _r, _x, _y, _l;
	int i, j, id = 0;
	if(is_solid){
        _r = r * value;
		float r_ = r - _r;
		for(j = 0; j < wh; ++j){
		    for(i = 0; i < wh; ++i, ++id){
		        _x = (float)i - cp;
                _y = (float)j - cp;
				_l = sqrtf(_x * _x + _y * _y);
				if     (_l >  r)p_memory[id] = 0;
				else if(_l > _r)p_memory[id] = (uint8)((1.0f - (_l - _r) / r_) * 255.0f);
				else            p_memory[id] = 255;
			}
		}
	}else{
		r -= 0.5f;
	    for(j = 0; j < wh; ++j){
		    for(i = 0; i < wh; ++i, ++id){
		        _x = (float)i - cp;
                _y = (float)j - cp;
				_l = sqrtf(_x * _x + _y * _y);
				_r = _l > r ? _l - r : r -_l;
				if(_r >= 1.0f)p_memory[id] = 0;
				else          p_memory[id] = 255 - (uint8)(255.0f * _r);
			}
		}
	}
}
GN_API float GNg::GenSphereDataToMem(uint8 *p_memory,int wh, int b, const float *_keys, int kn){
	GNg::sVEC2 *keys = (GNg::sVEC2*)_keys;
	float cp = (float)wh * 0.5f-0.5f;
	float  r = (float)(wh-(b<<1)) * 0.5f, _x, _y, _l;
	int i, j, id = 0;

	uint8 loc_buf[4096]; 
	GN::sTBUF_INT temp_buf(loc_buf, 4096); 
	sVEC3* keys2 = (sVEC3*)temp_buf.GetSpace(sizeof(sVEC3) * kn); 
	/*缩放值来适配图像空间*/
	float max_v = keys->y, max_t = keys[kn - 1].x; 
	for (i = 1; i < kn; ++i) {
		if (keys[i].y > max_v)max_v = keys[i].y; 
	}
	int ki1, ki2, ki; 
	float  tscl = r / max_t, vscl = 255.0f / max_v; 
	for (i = 0; i < kn; ++i) {
		keys2[i].x = keys[i].x * tscl; 
		keys2[i].y = keys[i].y * vscl;
		if (i) {
			ki = i - 1;
			keys2[ki].z = (keys2[i].y - keys2[ki].y) / (keys2[i].x - keys2[ki].x); /*预先计算好插值因子*/
		}
	}
	--kn; keys2[kn].z = 0.0f;

	for(j = 0; j < wh; ++j){
	    for(i = 0; i < wh; ++i, ++id){
	        _x = (float)i - cp;
            _y = (float)j - cp;
			_l = sqrtf(_x * _x + _y * _y);
			if     (_l >  r)p_memory[id] = 0;
			else if(_l < 0.76f)p_memory[id] = (uint8)keys2->y; //使中心像素不插值
			else{
				ki1 = 0; ki2 = kn;
				GN_FindNearDataID2(_l < keys2[ki].x, ki1, ki2, ki);
				p_memory[id] = (uint8)(((_l - keys2[ki1].x) * keys2[ki1].z + keys2[ki1].y));
			}
		}
	}
	return max_v;
}
void GNg::GenSphereDataToMem3D(uint8 *p_memory,int whd, int b, float value, float sqv, bool is_half){
	bool  us_sq = sqv > 0.0f ? true : false;
	float cp = (float)whd * 0.5f-0.5f, max_value = sqv > 0.0f ? sqv : 255.0f;
	float  r = (float)(whd-(b<<1)) * 0.5f, _r, _x, _y, _z, _l;
    _r = r * value;
	float r_ = r - _r;
	int i, j, l, id = 0, w = is_half ? (whd >> 1) : whd;
	if(is_half){
		int _w = w - b;
        for(l = 0; l < whd; ++l){
			for(j = 0; j < whd; ++j){
				for(i = 0; i < b; ++i, ++id)p_memory[id] = 0;
			    for(     ; i < w; ++i, ++id){
			        _x = (float)(i + _w) - cp;

                   #define GT_TMP_COMPUTE_SPHERE3D_VALUE \
                    _y = (float)j - cp;\
                    _z = (float)l - cp;\
					_l = sqrtf(_x * _x + _y * _y + _z * _z);\
					if     (_l >  r) p_memory[id] = 0;\
					else if(_l > _r){ _l = 1.0f - (_l - _r) / r_;  p_memory[id] = (uint8)(us_sq ? _l * _l * max_value : _l * max_value); }\
					else            p_memory[id] = max_value

					GT_TMP_COMPUTE_SPHERE3D_VALUE;
		}	}	}
	}else{
	    for(l = 0; l < whd; ++l){
			for(j = 0; j < whd; ++j){
                for(i = 0; i < w; ++i, ++id){
			        _x = (float)i - cp;
                    GT_TMP_COMPUTE_SPHERE3D_VALUE;
		}	}   }
	}
}
GN_API void GNg::GenRectDataToMem(uint8 *p_memory,int w, int h, float px, float py, float vx, float vy){
    float _x, _y, _l;
	int i, j, id = 0;
	for(j = 0; j < h; ++j){
	    for(i = 0; i < w; ++i, ++id){
	        _x = (float)i - px;
            _y = (float)j - py;
			_l = _x * vx + _y * vy;
			if(_l < 0.01f)     p_memory[id] = 255;
			else if(_l > 0.99f)p_memory[id] = 0;
			else               p_memory[id] = (uint8)((1.0f - _l) * 255.0f);
		}
	}
}
GN_API void GNg::GenCheckerboardDataToMem(uint8 *p_memory, int cb_size, int wn, int hn, uint8 lum){
    int row_size = cb_size * wn;
	int layer_size = row_size * cb_size;
    uint8 *pdata = p_memory, *pdata1 = p_memory + row_size;
	uint8 a, ai, ai2 = hn > 1 ? 2 : 1;
	int i, i2, j;
	for(ai = 0; ai < ai2; ++ai){
		for(i = 0, j = 0; j < wn; ++j){
			a = (j & 1) == ai ? lum : 0xff;
			for(i2 = i + cb_size; i < i2; ++i)pdata[i] = a;
		}
		for(i = 1; i < cb_size; ++i){
            memcpy(pdata1, pdata, row_size);
			pdata1 += row_size;
		}
        pdata += layer_size;
        pdata1 = pdata + row_size;
	}
	uint8 *pdata2[2] = { p_memory , p_memory + layer_size };
	for(j = 2; j < hn; ++j){
        memcpy(pdata, pdata2[j & 1], layer_size);
        pdata += layer_size;
	}
}
GN_API void GNg::GenArrowDataToMem(uint8 *p_memory,int w,int h,bool style, int frame_w){
	int j, x, pw = w-1, ph = h>>1;
    uint8 *psm;            /*历遍开始地址*/
	uint8 *pem = p_memory; /*历遍结束地址*/
	uint8 *pm,  *px;	   /*当前地址 和 边界点地址*/
	uint8 *pm1, *px_;

   #define GT_TMP_DRAW_ARROW_TO_MEM(func1,func2, cmp , add_proc ) \
	for(j=0;j<h;++j){ /*历遍每行*/\
		if(j<ph)x = (int) func1 (0,pw,j,ph);    /*计算找到箭头每行边界点*/\
		else    x = (int) func2 (pw,0,(j-ph),ph); /*计算找到箭头每行边界点*/\
	    psm = pem;\
        pem = pem + w;\
		px = psm + x;\
		add_proc \
		for(pm=psm; pm<pem; ++pm){\
		    if( cmp )*pm = 255;\
			else *pm = 0;\
		}\
	}
	if(frame_w){
		if(style){ //尖头
		    GT_TMP_DRAW_ARROW_TO_MEM( GN_Linear , GN_Linear ,		   pm < pm1 || (pm > px_ && pm <= px) , pm1 = psm + frame_w; px_ = px - frame_w; );
		}else{     //圆头
			GT_TMP_DRAW_ARROW_TO_MEM( GN_SLinearStart , GN_SLinearEnd , pm < pm1 || (pm > px_ && pm <= px) , pm1 = psm + frame_w; px_ = px - frame_w; );
		}
	}else{
		if(style){ //尖头
		    GT_TMP_DRAW_ARROW_TO_MEM( GN_Linear , GN_Linear , pm <= px ,  );
		}else{     //圆头
			GT_TMP_DRAW_ARROW_TO_MEM( GN_SLinearStart , GN_SLinearEnd , pm <= px ,  );
		}
	}
}
GN_API uint8 *GNg::GenNumberImgData(int &fw_out, int &fh_out){
    const uint8 number_bits[] = /*0123456789 号码图像的位数据*/
	{ 0x78 , 0x30 , 0x3c , 0x3c , 0x0c , 0x7e , 0x38 , 0x7e , 0x3c , 0x3c ,
	  0xcc , 0x38 , 0x66 , 0x66 , 0x0c , 0x06 , 0x18 , 0x60 , 0x66 , 0x66 ,
	  0xec , 0x3e , 0x66 , 0x66 , 0x6c , 0x06 , 0x0c , 0x30 , 0x66 , 0x66 ,
	  0xec , 0x30 , 0x60 , 0x60 , 0x6c , 0x06 , 0x3e , 0x30 , 0x6e , 0x66 ,
	  0xcc , 0x30 , 0x30 , 0x38 , 0x6c , 0x3e , 0x66 , 0x18 , 0x3c , 0x66 ,
	  0xdc , 0x30 , 0x18 , 0x60 , 0x66 , 0x60 , 0x66 , 0x18 , 0x76 , 0x7c ,
	  0xdc , 0x30 , 0x0c , 0x66 , 0xfe , 0x60 , 0x66 , 0x0c , 0x66 , 0x30 ,
	  0xcc , 0x30 , 0x06 , 0x66 , 0x60 , 0x30 , 0x66 , 0x0c , 0x66 , 0x18 ,
	  0x78 , 0x30 , 0x7e , 0x3c , 0x60 , 0x1e , 0x3c , 0x0c , 0x3c , 0x1c };

	const int fw = 8; //号码图像单字符宽
	const int fh = 9; //号码图像单字符高

    const int pn = fw * fh * 10;
    uint8 *number_data = new uint8[pn];
	memset(number_data, 0, pn);
	int pi = 0, bi;
	for(; pi < pn; ++pi){
		bi = pi >> 3;
		if( number_bits[bi] & (1<<(pi - (bi << 3))) )number_data[pi] = 255; /*模板代码:把位记载的数据释放为字节的数据*/
	}
    fw_out = fw;
    fh_out = fh;
	return number_data;
}
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
GN_API bool GNa::CheckAudioFmtSupportFromFileName(const char *filename){
    char *ext_name = GN_StrGetExtName(filename);
    if(ext_name)return FileAudio->CheckFmtSupportFromExtName(ext_name);
	return false;
}
static char GT_SupportExtNamesAudio[128];
GN_API char *const GNa::GetSupportAudioFmtFileExtName(bool upper){
	if(!GT_SupportExtNamesAudio[0])FileAudio->GetSupportFmtFileExtName(GT_SupportExtNamesAudio, upper);
	return (char *const)GT_SupportExtNamesAudio;
}
GN_API unsigned int GNa::LoadWavFileInfo(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
   #define GT_TMP_SET_WAV_INFO \
	*format   = *(unsigned short*)(td+20); /*格式*/\
	*channels = *(unsigned short*)(td+22); /*声道数*/\
	*bit      = *(unsigned short*)(td+34); /*声道位*/\
	*freq     = *(unsigned int  *)(td+24); /*频率*/\
	*size     = *(unsigned int  *)(td+40); /*音频数据的大小*/

   #define GT_TMP_LOAD_WAV_HEAD \
	GNf::sFILE *fp = GNf::Open(filename_mem,"rb",memsize);\
    if(!fp)return 0;  /*文件打开失败返回0*/\
	char td[44];\
	GNf::Read(td,1,44,fp); /*读取wav头信息共44个字节*/\
	if(td[0]!='R'||td[1]!='I'||td[2]!='F'||td[3]!='F'){ /*判断是否不是wav格式文件*/\
		GNf::Close(fp);\
		return 0;\
	}\
	GT_TMP_SET_WAV_INFO

    GT_TMP_LOAD_WAV_HEAD;
	GNf::Close(fp);		  //关闭文件
	return GNa::ComputePlayTime(*channels , *bit, *freq, *size); //计算返回音频总播放时间 单位毫秒
}
GN_API uint8* GNa::LoadWavFileData(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	GT_TMP_LOAD_WAV_HEAD;				   //读取wav 文件头信息
	uint8 *data = new uint8[*size];		   //分配数据内存空间
	GNf::Read((char*)data, 1, *size, fp);  //一次过读取音频数据 !wav无压缩数据比较简单
	GNf::Close(fp);						   //关闭文件
	return data;						   //返回数据
}
GN_API bool GNa::SaveWavFile(char *file_name, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq,unsigned int size, uint8 *data){
   #define GT_TMP_SAVE_WAV_HEAD \
	GNf::sFILE *fp = GNf::Open((char*)file_name,"wb",GNf::eFILE_STD); /*创建并打开文件(路径文件名, 以写入二进制方式, 创建于标准本地磁盘文件)*/\
	if(!fp){ GN_Log(GT_ERR_FileCreateFail, "wav", file_name); return 0; } /*文件创建失败就打印失败原因到 RunLog.txt 文件里*/\
	char td[44]; /*44字节临时变量空间*/\
	strncpy(td, "RIFF", 4); /*首4个字节为wav的文件效验标签*/\
	memset(td+4, 0, 40);	/*其它40个字节先全设为0*/\
	*(unsigned short*)(td+20) = format  ; /*格式*/\
	*(unsigned short*)(td+22) = channels; /*声道数*/\
	*(unsigned short*)(td+34) = bit     ; /*声道位*/\
	*(unsigned int  *)(td+24) = freq    ; /*频率*/\
	*(unsigned int  *)(td+40) = size    ; /*音频数据的大小*/\
	GNf::Write(td, 1, 44, fp);  //写入文件头信息

	GT_TMP_SAVE_WAV_HEAD		//先保存文件头信息
	GNf::Write((char*)data, 1, size, fp); //再一次过写入音频数据
	GNf::Close(fp);				//关闭文件
	return true;				//返回成功
}
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
const char *GT_ERR_ReadMp3FileFail = "读取 mp3 文件失败! 解释数据有错!\n";

//获得Mp3标签内容大小
inline int GT_Mp3GetTagSize(uint8 *size) {
	return size[0] * 2097152 + size[1] * 16384 + size[2] * 128 + size[3];
}
//获得Mp3的音频数据帧开始指针(Mp3文件数据, 数据大小)
static char *GT_Mp3GetFramesStartPtr(char *_data, int data_size){
	if(_data[0] == 'I' && _data[1] == 'D' && _data[2] == '3'){
		return _data + (GT_Mp3GetTagSize((uint8*)_data+6) + 10); //跳过文件标签内容和标签头
	}else{
		unsigned short *_data2 = (unsigned short*)_data, *_end = (unsigned short*)(_data + data_size);
		while(_data2 < _end && (_data2[0] & 0xF0FF) != 0xF0FF)++_data2;	//跳过无效数据
		return (char*)(_data2 < _end ? _data2 : 0);
	}
}
inline unsigned short GT_Mp3TranslateBitrate(unsigned short ver, unsigned short layer, uint8 y) {
	unsigned short x;
	switch(ver){
	case 1:
		x = layer - 1;
		break;
	case 2:
		x = layer == 1? 3 : 4;
	default: return 0xffff;
	}
	static const unsigned short bitrateIndex[16][5] = {
	{ 0,0,0,0,0 },
	{ 32,32,32,32,8 },
	{ 64,48,40,48,16 },
	{ 96,56,48,56,24 },
	{ 128,64,56,64,32 },
	{ 160,80,64,80,40 },
	{ 192,96,80,96,48 },
	{ 224,112,96,112,56 },
	{ 256,128,112,128,64 },
	{ 288,160,128,144,80 },
	{ 320,192,160,160,96 },
	{ 352,224,192,176,112 },
	{ 384,256,224,192,128 },
	{ 416,320,256,224,144 },
	{ 448,384,320,256,160 },
	{ 0, 0, 0, 0, 0 } };
	return bitrateIndex[y][x];
}
GN_API unsigned int GNa::LoadMp3FileInfo(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	int flen = memsize;
	char *data = filename_mem;
	if(memsize < GNf::eFILE_MEM || GNf::sFILE::DefaultH){
		data = GNf::FileFullLoadToMemory(filename_mem, &flen, memsize);
		if(!data)return 0;  /*文件不正确返回*/
	}
	uint8 *_data = (uint8*)GT_Mp3GetFramesStartPtr(data, flen);
	if(!_data)return 0;

	uint8 ch_mk; //以下开始便是数据帧了 --------------------------------------------
	const unsigned int sample_rate[4][3] = {
	{ 44100, 22050, 11025 },
	{ 48000, 24000, 12000 },
	{ 32000, 16000, 8000  },
	{ 0,	 0,		0	  }	};
	unsigned short version, layer, bi, *info2 = (unsigned short*)_data;
	unsigned int freq1 = 0, frame_n = 0;
	for(;(info2[0] & 0xF0FF) == 0xF0FF; ++frame_n){ //统计音频总帧数和获取信息
		version = (info2[0] & 0xF00) >> 11;
		layer = 4 - ((info2[0] & 0x600) >> 9);
	    bi = GT_Mp3TranslateBitrate(version, layer, (_data[2] & 0xF0) >> 4);
		if(bi == 0xffff){
			if(data != filename_mem)delete [] data;
			GN_Log(GT_ERR_ReadMp3FileFail);
			return 0;
		}
		ch_mk = _data[3];
		freq1 = sample_rate[(_data[2] & 0x0F) >> 2][version - 1];
		uint8 padding = (_data[2] & 0x0F) >> 1;

		_data += layer == 1?\
			 (12000 * (unsigned int)bi / freq1 + padding) << 2 :\
			 144000 * (unsigned int)bi / freq1 + padding;
		info2 = (unsigned short*)_data;
	}
	//GN_Log("frame_n=%d\n", frame_n);
	if(data != filename_mem)delete [] data;
	*format = 0;//WAVE_FORMAT_UNKNOWN;
	*channels = (ch_mk&0xc0) == 0xc0 ? 1 : 2;
	*bit = 16;
	*freq = freq1;
	unsigned int smpln = (layer == 1 ? 384 : 1152) * frame_n;
	*size = smpln * (*channels) << 1; //16位2个字节
	return smpln / freq1 * 1000;
}
#define MINIMP3_IMPLEMENTATION
#ifdef _MAC_
#include <xmmintrin.h>
#endif
#include<minimp3.h>
GN_API uint8* GNa::LoadMp3FileData(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	int flen;
	char *data = GNf::FileFullLoadToMemory(filename_mem, &flen, memsize);
    if(!data)return 0;  /*文件不正确返回*/
	uint8_t *data1 = (uint8_t*)data;

	mp3dec_t   decer;
	mp3dec_t *_decer = &decer;
	mp3dec_frame_info_t   finfo;
	mp3dec_frame_info_t *_finfo = &finfo;
	mp3dec_init(_decer);

	const unsigned int pcm_size = flen * 12;
	uint8 *pcm_data = new uint8[pcm_size];
	mp3d_sample_t *pcmd = (mp3d_sample_t*)pcm_data;
	int smpln = mp3dec_decode_frame(_decer, data1, flen, pcmd, _finfo);
	if(!smpln){
		GN_Log(GT_ERR_ReadMp3FileFail);
		delete [] pcm_data;
		delete [] data;
		return 0;
	}
	*format = 0;//WAVE_FORMAT_UNKNOWN;
	*channels = _finfo->channels;
	*bit = 16;
	*freq = _finfo->hz;
	unsigned int tot_smpln = 0;
	if(_finfo->channels == 2){
		for(;;){
			tot_smpln += smpln << 1;
			data1 += _finfo->frame_bytes;
			flen  -= _finfo->frame_bytes;
			if(flen <= 0)break;
			smpln = mp3dec_decode_frame(_decer, data1, flen, pcmd + tot_smpln, _finfo);
			//GN_Log("%d, %d %d %d %d %d %d, %d\n", smpln, _finfo->frame_bytes, _finfo->frame_offset, _finfo->channels, _finfo->hz, _finfo->layer, _finfo->bitrate_kbps, flen);
		}
	}else{
		for(;;){
			tot_smpln += smpln;
			data1 += _finfo->frame_bytes;
			flen  -= _finfo->frame_bytes;
			if(flen <= 0)break;
			smpln = mp3dec_decode_frame(_decer, data1, flen, pcmd + tot_smpln, _finfo);
		}
	}
	delete [] data;
	*size = tot_smpln << 1; //16位2个字节

	if(pcm_size - (*size) > 524288){ //缓冲多出512K就裁掉空的部分
		uint8 *pcm_data2 = new uint8[*size];
		memcpy(pcm_data2, pcm_data,  *size);
		delete [] pcm_data;
		return pcm_data2;
	}
	return pcm_data;
}
/*---------------------------------------------------- 默认音频文件插件功能 ----------------------------------------------------------------*/

struct GT_sAUDID_STREAM_WAV :public GNa::iFILE_AUDIO::cSTREAM
{
	GNf::sFILE		*fp;			//文件

	GT_sAUDID_STREAM_WAV(GNf::sFILE	*_fp=0):fp(_fp){}
	virtual void Seek(float progre);
	virtual unsigned int Read(char *buf, unsigned int count);
	virtual unsigned int Write(const char *buf, int count);
	virtual void Close();
};
struct GT_sAUDID_STREAM_MP3 :public GNa::iFILE_AUDIO::cSTREAM
{
	char			*file_data;		//mp3文件数据 !该值为0时 表示数据不是它拥有的
	uint8_t			*audio_data;	//指向mp3音频数据的开始
	int				 audio_size;	//mp3音频数据大小
	int				 offset;		//当前音频数据读取位置
	mp3dec_t		 decer;			//mp3解码器

	GT_sAUDID_STREAM_MP3(char *fdata=0, uint8_t *adata=0, int asize=0):file_data(fdata),audio_data(adata),audio_size(asize),offset(0){ mp3dec_init(&decer); }
	virtual void Seek(float progre);
	virtual unsigned int Read(char *buf, unsigned int count);
	virtual unsigned int Write(const char *buf, int count);
	virtual void Close();
};
void GT_sAUDID_STREAM_WAV::Close(){ 
	if(!fp->buf_size){	 //文件流是以写方式打开的
		unsigned int size = GNf::Tell(fp) - 44; //写完获得当前文件写位置偏移减去文件头大小就是wav音频数据大小
		GNf::Seek(fp, 40, GNf::eSEEK_START);	//跳回文件第40个字节处为wav文件储存音频数据大小值的位置
		GNf::Write((char*)&size, 1, 4, fp);		//写入数据大小的值
	}
	GNf::Close(fp); delete this;				//完成写入关闭文件 并删除自己的内存空间
}
void GT_sAUDID_STREAM_MP3::Close(){ GN_DeleteArray(file_data); delete this; }

void GT_sAUDID_STREAM_WAV::Seek(float progre){
	GNf::Seek(fp, 44 + (int)(fp->buf_size * progre), GNf::eSEEK_START); //使文件读取跳转到音频播放进度对应的文件位置 44表示文件头所占字节
}
unsigned int GT_sAUDID_STREAM_WAV::Read(char *buf, unsigned int count){
	return GNf::Read(buf, 1, count, fp);	//在当前文件位置读出指定大小的数据
}
unsigned int GT_sAUDID_STREAM_WAV::Write(const char *buf, int count){
	count = GNf::Write(buf, 1, count, fp);	//在当前文件位置写入指定大小的数据
	return count > 0 ? count : 0;			//使返回值最小为0
}
void GT_sAUDID_STREAM_MP3::Seek(float progre){
	offset = (int)(audio_size * progre);
}
unsigned int GT_sAUDID_STREAM_MP3::Read(char *buf, unsigned int count){
	mp3dec_t *_decer = &decer;
	mp3dec_frame_info_t   finfo;
	mp3dec_frame_info_t *_finfo = &finfo;
	mp3d_sample_t *pcmd = (mp3d_sample_t*)buf;
	unsigned int read_size = 0, smpln2;
	count >>= 1;
	do{
		smpln2 = mp3dec_decode_frame(_decer, audio_data + offset, audio_size - offset, pcmd + read_size, _finfo) * _finfo->channels;
		offset += _finfo->frame_bytes;
		read_size  += smpln2;
	}while(_finfo->frame_bytes && read_size + smpln2 <= count);
	return read_size << 1;
}
unsigned int GT_sAUDID_STREAM_MP3::Write(const char *buf, int count){ 
	return 0; 
}
GNa::iFILE_AUDIO::cSTREAM *GNa::iFILE_AUDIO::Open(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	GNf::sFILE *fp = GNf::Open(filename_mem,"rb",memsize); //打开文件 filename_mem 和 memsize 可以是磁盘文件 或压缩包 或 内存数据块 具体看 memsize 怎么表示
	if(!fp)return 0;  //打开失败返回0
	char td[44];
	GNf::Read(td, 1, 44, fp);	  //读取wav文件头信息
	if(!strncmp(td, "RIFF", 4)){  //校验文件头信息 判断是否wav格式文件
		GT_TMP_SET_WAV_INFO		  //返回文件头的信息到输出参数
		fp->buf_size = *size;	  //文件该属性指定为数据大小 以便之后读取跳转 计算出文件读取位置
		return new GT_sAUDID_STREAM_WAV(fp); //创建文件流对象并返回
	}else{ // Mp3
		if(GNf::sFILE::DefaultH || memsize == GNf::eFILE_STD || memsize == GNf::eFILE_SAFE){ //网络文件或本地文件
			memsize = GNf::GetSize(fp);
			filename_mem = new char[memsize];
			memcpy(filename_mem, td, 44);
			GNf::Read(filename_mem+44, 1, memsize-44, fp);  //把整个Mp3度到缓存
			GNf::Close(fp);
			fp = GNf::Open(filename_mem,"rb",memsize);
			fp->special_mark = -1;  //视这段内存是压缩包解压的 这样在关闭文件时就能一并删除
		}
		if(!LoadMp3FileInfo(fp->handle_buf,format,channels,bit,freq,size, fp->buf_size)){
			GNf::Close(fp);
			return 0;
		}
		char *frame_st = GT_Mp3GetFramesStartPtr(fp->handle_buf, fp->buf_size);
		GT_sAUDID_STREAM_MP3 *fmp3 = new GT_sAUDID_STREAM_MP3(fp->special_mark?fp->handle_buf:0, (uint8_t*)frame_st, fp->buf_size - (frame_st - fp->handle_buf));
		fp->special_mark = 0;		//数据以被所管 提示之后无需删除
		GNf::Close(fp);
		return fmp3;
	}
}
GNa::iFILE_AUDIO::cSTREAM *GNa::iFILE_AUDIO::Create(const char *file_name,unsigned short format,unsigned short channels,unsigned short  bit,unsigned int  freq){
	char *ext_name = GN_StrGetExtName(file_name);	//获得文件扩展名
	if(ext_name && !strcmp(ext_name, ext_name[0]>'Z'?"mp3":"MP3"))return 0; /*模本代码:暂时不支持保存成Mp3*/
	unsigned int size = 0;					//初始音频数据大小为0
	GT_TMP_SAVE_WAV_HEAD					//先保存wav文件头信息
	fp->buf_size = 0;						//文件该属性设为0 标记是写文件
	return new GT_sAUDID_STREAM_WAV(fp);	//创建文件流对象并返回
}
GNa::iFILE_AUDIO::iFILE_AUDIO():module(0){
	GT_MX_PushFront( FileAudio , this );
	GT_SupportExtNamesAudio[0] = 0;
}
GNa::iFILE_AUDIO::~iFILE_AUDIO(){
   #define GT_TMP_REMOVE_PLUGINS_NODE( TYPE , LIST ) \
	if(module){ GT_ModuleClose(module); module = 0; }\
	if(LIST == this)LIST = this->next_node;\
	else{\
		TYPE *last = LIST;\
		while(last->next_node != this)last = last->next_node;\
		last->next_node = this->next_node;\
	}
	GT_TMP_REMOVE_PLUGINS_NODE( iFILE_AUDIO , FileAudio )
	GT_SupportExtNamesAudio[0] = 0;
}
unsigned int GNa::iFILE_AUDIO::LoadInfo(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	int yes = GNf::IsTypeFile(filename_mem, *format, "wav", "RIFF", 4, memsize);  //判断提供的文件名或文件内容是否指定类型的文件
	if(yes){ //如是 或不确定
		unsigned int play_time = LoadWavFileInfo(filename_mem,format,channels,bit,freq,size,memsize); //尝试读取wav音频文件信息
		if(play_time || yes == 1)return play_time;  //如调佣成功 或 已确定是wav文件 就返回音频播放时间 无论有否
	}
	return LoadMp3FileInfo(filename_mem,format,channels,bit,freq,size,memsize);
}
uint8* GNa::iFILE_AUDIO::LoadData(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	int yes = GNf::IsTypeFile(filename_mem, *format, "wav", "RIFF", 4, memsize);  //判断提供的文件名或文件内容是否指定类型的文件
	if(yes){ //如是 或不确定
		uint8 *data = LoadWavFileData(filename_mem,format,channels,bit,freq,size,memsize); //尝试读取wav音频文件数据
		if(data || yes == 1)return data;			//如调佣成功 或 已确定是wav文件 就返回音频数据 无论有否
	}
	return LoadMp3FileData(filename_mem,format,channels,bit,freq,size,memsize);
}
bool GNa::iFILE_AUDIO::Save(const char *file_name,  unsigned short  format,unsigned short  channels,unsigned short  bit, unsigned int freq,unsigned int  size, uint8 *data){
	char *ext_name = GN_StrGetExtName(file_name); /*模本代码:暂时不支持保存成Mp3*/
	if(ext_name && !strcmp(ext_name, "mp3"))return false; 
	return SaveWavFile((char*)file_name, format, channels, bit, freq, size, data);
}
bool GNa::iFILE_AUDIO::CheckFmtSupportFromExtName(const char *extname){
/*
	if(!strcmp(extname,"ogg") || !strcmp(extname,"OGG"))return true;
	return next_node->CheckFmtSupportFromExtName(extname); //调用之前默认的方法
*/
	if(extname[0] > 'Z'){
		  if(!strcmp(extname,"wav") || !strcmp(extname,"mp3"))return true;
	}else if(!strcmp(extname,"WAV") || !strcmp(extname,"MP3"))return true;
	return false;
}
void GNa::iFILE_AUDIO::GetSupportFmtFileExtName(char *out, bool upper){
/*  
	strcpy(out, upper?"ogg,OGG,":"ogg,");
	next_node->GetSupportFmtFileExtName(out+(upper?8:4), upper); //调用之前默认的方法
*/
	if(upper)strcpy(out, "wav,WAV,mp3,MP3");
	else	 strcpy(out, "wav,mp3");
}
GNa::iFILE_AUDIO GT_FileAudio;
GN_API GNa::iFILE_AUDIO	*GNa::FileAudio = &GT_FileAudio;

GN_API int	GN_PluginsFileAudioRemove(){
   #define GT_TMP_REMOVE_PLUGIN( _PLG , PLG ) \
	int count = 0;\
	while(_PLG != &PLG){\
		delete _PLG;\
		++count;\
	}\
	return count;
	GT_TMP_REMOVE_PLUGIN( GNa::FileAudio , GT_FileAudio )
}
/*----------------------------------------------------- 默认图像文件插件功能 --------------------------------------------------------------*/
GNg::iFILE_IMAGE::iFILE_IMAGE():module(0){
	GT_MX_PushFront( FileImage , this );
	GT_SupportExtNamesImage[0] = 0;
}
GNg::iFILE_IMAGE::~iFILE_IMAGE(){
	GT_TMP_REMOVE_PLUGINS_NODE( iFILE_IMAGE , FileImage )
	GT_SupportExtNamesImage[0] = 0;
}
uint8* GNg::iFILE_IMAGE::LoadInfo(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool min_out,int memsize){
   #define GT_TMP_FILE_IMAGE_LOAD( PROC , ARG1 , ARG2 , ADDPROC ) \
	unsigned short ext_pos = *format;\
	uint8 *data;\
	int yes = GNf::IsTypeFile(filename_mem, ext_pos, "png", "塒NG", 4, memsize);\
	if(yes){\
		data = LoadPngFile##PROC(filename_mem,format,w,h, ARG1 ,memsize);\
		if(data || yes == 1)return data;\
	}else{\
		uint8 gim_head[] = { 0x11,0x02,0x28,0x02 };\
		yes = GNf::IsTypeFile(filename_mem, ext_pos, "gim", (char*)gim_head, 4, memsize);\
		if(yes){\
			data = LoadPngFile##PROC(filename_mem,format,w,h, ARG1 ,memsize);\
			if(data || yes == 1)return data;\
		}\
	}\
	yes = GNf::IsTypeFile(filename_mem, ext_pos, "bmp", "BM", 2, memsize);\
	if(yes){\
		data = LoadBmpFile##PROC(filename_mem,format,w,h, ARG2 ,memsize);\
		if(data || yes == 1)return data;\
	}\
	yes = GNf::IsTypeFile(filename_mem, ext_pos, "hdr", "#?", 2, memsize);\
	if(yes){\
		float ins = -8.0f; /*亮度提高*/\
		data = ADDPROC ;\
		if(data || yes == 1)return data;\
	}\
	return LoadTgaFile##PROC(filename_mem,format,w,h, ARG2 ,memsize);

	GT_TMP_FILE_IMAGE_LOAD( Info , min_out , min_out , LoadHdrFileInfo(filename_mem,format,w,h,min_out,memsize) )
}
uint8* GNg::iFILE_IMAGE::LoadData(char *filename_mem,COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
	GT_TMP_FILE_IMAGE_LOAD( Data , col_turn , !col_turn , LoadHdrFileData(filename_mem,format,w,h,!col_turn,memsize,&ins) )
}
bool  GNg::iFILE_IMAGE::Save(char *file_name,uint8 *data,COLOR_FORMAT srcformat,int w,int h,bool col_turn, uint8 comps_lv){
	bool save_ok = false;
	char *ext_name = GN_StrGetExtName(file_name);
	if(ext_name){
		if(!strcmp(ext_name,"bmp")){
		    save_ok = SaveBmpFile((char*)file_name,data,srcformat,w,h,col_turn);
		}else if(!strcmp(ext_name,"png") || !strcmp(ext_name,"gim")){
		    save_ok = SavePngFile((char*)file_name,data,srcformat,w,h,col_turn);
		}else if(!strcmp(ext_name,"tga")){
		    save_ok = SaveTgaFile((char*)file_name,data,srcformat,w,h,col_turn, comps_lv);
		}else if(!strcmp(ext_name,"hdr")){
		    save_ok = SaveHdrFile((char*)file_name,data,srcformat,w,h,col_turn, comps_lv);
		}
	}else{
        GN_Log("保存文件名没有类型扩展名,无法确定保存的文件类型!\n");
	}
    return save_ok;
}
uint8* GT_LoadBmpFileData(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
	return GNg::LoadBmpFileData(filename_mem, format, w, h, !col_turn, memsize);
}
uint8* GT_LoadTgaFileData(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
	return GNg::LoadTgaFileData(filename_mem, format, w, h, !col_turn, memsize);
}
uint8* GT_LoadHdrFileData(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
	float ins = -8.0f; /*亮度提高*/
	return GNg::LoadHdrFileData(filename_mem, format, w, h, !col_turn, memsize, &ins);
}
GNg::PF_LOAD_IMG_FILE_DATA GNg::iFILE_IMAGE::CheckFmtSupportFromExtName(const char *extname){
	/*if(GNf::sFILE::EnableDedicatedOnly){
		if(!strcmp(extname,"gim"))return LoadPngFileData;
	}else*/
	{
		if(extname[0] > 'Z'){
			if	   (!strcmp(extname,"png") || !strcmp(extname,"gim"))return LoadPngFileData;
			else if(!strcmp(extname,"bmp"))return GT_LoadBmpFileData;
			else if(!strcmp(extname,"tga"))return GT_LoadTgaFileData;
			else if(!strcmp(extname,"hdr"))return GT_LoadHdrFileData;
		}else{
			if	   (!strcmp(extname,"PNG"))return LoadPngFileData;
			else if(!strcmp(extname,"BMP"))return GT_LoadBmpFileData;
			else if(!strcmp(extname,"TGA"))return GT_LoadTgaFileData;
			else if(!strcmp(extname,"HDR"))return GT_LoadHdrFileData;
		}
	}
	return 0;
}
void GNg::iFILE_IMAGE::GetSupportFmtFileExtName(char *out, bool upper){
	strcpy(out, "png,bmp,tga,hdr,gim");
	if(upper)strcpy(out+19, ",PNG,BMP,TGA,HDR");
}
GNg::iFILE_IMAGE GT_FileImage;
GN_API GNg::iFILE_IMAGE	*GNg::FileImage = &GT_FileImage;

GN_API int	GN_PluginsFileImageRemove(){
	GT_TMP_REMOVE_PLUGIN( GNg::FileImage , GT_FileImage )
}
/******************************************************* 变量值调整信息 ************************************************************/
GN::sADJUST_VALUE_GROUP::~sADJUST_VALUE_GROUP(){
	if(args){
		uint8 i = 0;
		for(; i < argn; ++i){
			switch(args[i]->type){
			case eVALUE_INT:   delete (sADJUST_VALUE_INT*)  args[i]; break;
			case eVALUE_FLOAT: delete (sADJUST_VALUE_FLOAT*)args[i]; break;
			case eVALUE_ULONG: delete (sADJUST_VALUE_ULONG*)args[i]; break;
			case eVALUE_BYTE:  delete (sADJUST_VALUE_BYTE*) args[i]; break;
			default:		   delete						args[i]; break;
			}
		}
		delete [] args;
		args = 0;
	}
}
GN_API void GN::ValuePrint(sVALUE_NAME *value, void *v, void *ptv){
	switch(value->type){

    #define GT_TEMP_VALUE_PRINT_CASE( TYPE , T ) \
	case GN::eVALUE_##TYPE :{\
		GN::sADJUST_VALUE_##TYPE *var = (GN::sADJUST_VALUE_##TYPE *)value;\
		int v1 = (int)(*(ptv ? ( T *)v : var->ptr));\
		if(var->enum_name){\
		    P< P<char> > enum_names = GN_StrSplitToData(var->enum_name,'|');\
			   GN_Log("  %s = %s\n", var->name, (v1 < (int)enum_names.Num()? enum_names[v1].Begin(): "无效值!"));\
		}else{ GN_Log("  %s = %d\n", var->name,  v1 ); }\
	}break;

	GT_TEMP_VALUE_PRINT_CASE( BYTE , uint8 )
	GT_TEMP_VALUE_PRINT_CASE( INT  ,  int  )
	case GN::eVALUE_FLOAT:{
		GN::sADJUST_VALUE_FLOAT *var = (GN::sADJUST_VALUE_FLOAT*)value;
			   GN_Log("  %s = %f\n", var->name,  *(ptv ? (float*)v : var->ptr) );
	}break;
	case GN::eVALUE_ULONG:{
		GN::sADJUST_VALUE_ULONG *var = (GN::sADJUST_VALUE_ULONG*)value;
			   GN_Log("  %s = %08x\n", var->name,  *(ptv ? (unsigned int *)v : var->ptr) );
	}break;
	}
}
GN_API void GN::ValuesNotEqualExec(const uint8 *data, sVALUE_NAME ** args, uint8 argn, void (*proc)(sVALUE_NAME *, void *, void *), void *us_arg){
   #define GT_TEMP_VALUES_FIND_SIZE( PROC ) \
	uint8 *ptr = (uint8*)data;\
	void *v;\
	unsigned int tsize;\
	uint8 i = 0;\
	for(; i < argn; ++i){\
		tsize = ValueTypeSize((eVALUE_TYPE)args[i]->type);\
		v = ((GN::sADJUST_VALUE_ULONG*)args[i])->ptr;\
		PROC ; ptr += tsize;\
	}
	GT_TEMP_VALUES_FIND_SIZE( if(memcmp( v , ptr , tsize))proc(args[i],ptr,us_arg) )
}
GN_API unsigned int GN::ValuesRead (const uint8 *data, sVALUE_NAME ** args, uint8 argn){
	GT_TEMP_VALUES_FIND_SIZE( memcpy( v , ptr , tsize) )
	return ptr - data;
}
GN_API unsigned int GN::ValuesWrite(uint8 *data, sVALUE_NAME ** args, uint8 argn){
	uint8 *ptr = (uint8*)data;
	void *v;
	unsigned int tsize;
	uint8 i = 0;
   if(data){
	for(; i < argn; ++i){
		tsize = ValueTypeSize((eVALUE_TYPE)args[i]->type);
		v = ((GN::sADJUST_VALUE_ULONG*)args[i])->ptr;
		memcpy( ptr , v , tsize); ptr += tsize;
	}
   }else{
	for(; i < argn; ++i){
		tsize = ValueTypeSize((eVALUE_TYPE)args[i]->type);
		ptr += tsize;
	}
   }
	return ptr - data;
}
/*********************************************** zip 压缩解压摸块 *********************************************************************/
#ifdef GN_USING_UNZIP
#include<libzip/zip.c>
extern "C"{
#include<libzip/unzip.c>
}
int GNd::cZIP::Open(const char *filename_mem,int memsize, int level, int is_add){
    zFile = zipOpen(filename_mem,memsize,is_add);
	if(zFile){
		zLevel = level;
		return 0;
	}else return -1;
}
int GNd::cZIP::Close(){
    if(!zFile)return -1;
	int ret = zipClose(zFile);
	zFile = 0;
	return ret;
}
#ifdef GN_DEBUG
#define GT_ZIP_Error( FF ) \
    if(!zFile){\
        GN_Log("cZIP::%s() 失败! zip 还没打开! 请先调用 cZIP::Open(...)!\n"/*"cZIP::%s() falie! zip do not Open! please cZIP::Open(...)!\n"*/, FF );\
    }
#else
#define GT_ZIP_Error( FF )
#endif
int GNd::cZIP::AddDir(const char *in_zip_name, const char *g_name, bool add_all_sub__){
    GT_ZIP_Error( "AddDir" )
	if(g_name){
		GN::sLIST< P<GNf::sFILE_INFO> > f_list, d_list;
		if(GNf::FindFileDirInfoData(g_name,true, &f_list, &d_list) == -1 || d_list.empty()){
			GN_Log("cZIP::AddDir 不存在目录: %s!\n"/*"cZIP::AddDir do not directory: %s!\n"*/, g_name);
			return -1;
		}
	    return AddDir(in_zip_name, d_list.front(), add_all_sub__);
	}else{
	   #define GT_TMP_ZIP_ADD_DIR_NAME_MAKE \
        char *_dir = ZipCurProcFileName;\
	    int len = strlen(in_zip_name);\
	    if(_dir != (char*)in_zip_name)strncpy(_dir, in_zip_name, len);\
		strcpy(_dir+len,"/")

        GT_TMP_ZIP_ADD_DIR_NAME_MAKE;
		zip_fileinfo zf;
        zipMakeFileInfo(&zf, _A_SUBDIR);
		if(zipOpenNewFileInZip(zFile, _dir, &zf,0,0,0,0,0,zLevel?Z_DEFLATED:0,zLevel) >= 0){
		     zipCloseFileInZip(zFile);
              return 0;
		}else return -1;
	}
}
int GNd::cZIP::AddFile(const char *in_zip_name,const char *g_name_mem,int memsize){
   #ifdef GN_DEBUG
	GT_ZIP_Error( "AddFile" )
	if(!g_name_mem)GN_Log("cZIP::AddFile() 失败! 参数:g_name_mem 不能为空的!\n"/*"cZIP::AddFile() falie! parame: g_name_mem is empty!\n"*/);
   #endif
	if(memsize < GNf::eFILE_MEM){
		GN::sLIST< P<GNf::sFILE_INFO> > f_list, d_list;
		if(GNf::FindFileDirInfoData(g_name_mem,true, &f_list, &d_list) == -1 || f_list.empty()){
            GN_Log("cZIP::AddFile 不存在文件: %s!\n"/*"cZIP::AddFile do not file: %s!\n"*/, g_name_mem);
			return -1;
		}
	    return AddFile(in_zip_name, f_list.front());
	}else{ //直接加内存文件
        if((char*)in_zip_name != (char*)ZipCurProcFileName)strcpy(ZipCurProcFileName,in_zip_name);
		zip_fileinfo zf;
        zipMakeFileInfo(&zf, _A_NORMAL);
		int ret = 0;
		if(zipOpenNewFileInZip(zFile, in_zip_name, &zf,0,0,0,0,0,zLevel?Z_DEFLATED:0,zLevel) >= 0){
			float *_porg = &ZipCurProcFileProgress;
            *_porg = 0.0f;
			if(zipWriteInFileInZip(zFile, (voidp)g_name_mem, memsize) < 0){
				GN_Log("cZIP::AddFile 压缩文件数据时有错!\n"/*"cZIP::AddFile zip file data error!\n"*/);
				ret = -1;
			}
            *_porg = 1.0f;
			zipCloseFileInZip(zFile);
		}else ret = -1;
		return ret;
	}
}
int GNd::cZIP::AddDir(const char *in_zip_name, const GNf::sFILE_INFO *f_info, bool add_all_sub__){
    GT_TMP_ZIP_ADD_DIR_NAME_MAKE;
	zip_fileinfo zf;
    zipGetFileInfo(&zf, f_info);
    int ret = 0;
	if(zipOpenNewFileInZip(zFile, _dir, &zf,0,0,0,0,0, zLevel?Z_DEFLATED:0, zLevel) >= 0){
		zipCloseFileInZip(zFile);
		if(add_all_sub__){
			++len;
			char find_str[384];
		    int gdir_len = strlen(f_info->global_filename);
			strncpy(find_str , f_info->global_filename, gdir_len);
            strcpy(find_str+gdir_len, "/*.*");
		    GN::sLIST< P<GNf::sFILE_INFO> > f_list, d_list;
			GNf::FindFileDirInfoData(find_str,true, &f_list, &d_list);
			GN::sLIST< P<GNf::sFILE_INFO> >::iterator itf, etf;
            GNf::sFILE_INFO *p_info;

			if(f_list.size()){
				itf = f_list.begin(), etf = f_list.end();
				for(; itf != etf; ++itf){
					p_info = (*itf);
					strcpy(_dir+len, p_info->name);
					if(AddFile(_dir, p_info) < 0){
						GN_ErrorLast.New(384);
						sprintf(GN_ErrorLast,"压缩目录里的文件: %s 中途有错!",p_info->global_filename);
					    ret = -1;
					}
				}
			}
            if(d_list.size()){
				char *_name;
				itf = d_list.begin(), etf = d_list.end();
				for(; itf != etf; ++itf){
					p_info = (*itf);
					_name = p_info->name;
					if(_name[0] == '.' && ((_name[1]=='.' && _name[2]==0) || _name[1]==0)){
						;
					}else{
					    strcpy(_dir+len,_name);
					    if(AddDir(_dir, p_info, add_all_sub__) < 0)ret = -1;
					}
				}
			}
		}
	}else ret = -1;
	return ret;
}
int GNd::cZIP::AddFile(const char *in_zip_name,const GNf::sFILE_INFO *f_info){
    if((char*)in_zip_name != (char*)ZipCurProcFileName)strcpy(ZipCurProcFileName,in_zip_name);
	zip_fileinfo zf;
    zipGetFileInfo(&zf, f_info);
    int ret = 0;
	if(zipOpenNewFileInZip(zFile, in_zip_name, &zf,0,0,0,0,0,zLevel?Z_DEFLATED:0,zLevel) >= 0){
		bool eg = GNf::sFILE::EnableGlobalPath;
		GNf::sFILE::EnableGlobalPath = true;
		GNf::sFILE *fp = GNf::Open((char*)f_info->global_filename,"rb",GNf::eFILE_STD);
		GNf::sFILE::EnableGlobalPath = eg;
		if(fp){
			int buf_size = GN_USING_FILE_BUF << 3;
			int f_size = GNf::GetSize(fp), _size = 0, _r_size;
			char *buf = new char[(f_size > buf_size) ? buf_size : f_size];
			float &porg = ZipCurProcFileProgress, porg_ratio = 1.0f / (float)f_size;
			porg = 0.0f;
			while(_size < f_size){
				_r_size = GNf::Read(buf,1, ((f_size - _size) > buf_size) ? buf_size : (f_size - _size), fp);
				if(_r_size > 0){
					if(zipWriteInFileInZip(zFile, buf, _r_size) < 0){
						GN_Log("cZIP::AddFile 压缩 %s 文件数据时有错!\n"/*"cZIP::AddFile zip %s file data error!\n"*/, f_info->global_filename);
						ret = -1;
						break;
					}else{
						_size += _r_size;
						porg = _size * porg_ratio;
					}
				}else{
				    GN_Log("cZIP::AddFile 读取 %s 文件数据时有错!\n"/*"cZIP::AddFile read %s file data error!\n"*/, f_info->global_filename);
					ret = -1;
					break;
				}
			}
		    delete [] buf;
		    GNf::Close(fp);
		}else ret = -1;
	    zipCloseFileInZip(zFile);
	}else ret = -1;
	return ret;
}
GN::sNUMBER<uint8> GT_ZipIDs;
unzFile            GT_ZipPacks[32];
GN_API  int GNd::ZipPackOpen(const char *zip_pak_name_mem, int memsize){
    unzFile uf = unzOpen(zip_pak_name_mem,memsize);
	if(uf){
		if(GT_ZipIDs.ProbeID() < 32){
			int zid = (int)GT_ZipIDs.GetID();
            GT_ZipPacks[zid] = uf;
            return -(zid+1);
		}
	}
	return 0;
}
GN_API void GNd::ZipPackClose(int zp_id){
	if(zp_id < 0){
	    int zid = -(zp_id+1);
        unzClose(GT_ZipPacks[zid]);
		GT_ZipIDs.ReSetIDToData((uint8)zid);
	}
}
GN_API uint8 *GNd::UnzipLoadFileData(const char *zip_pak_name_mem, const char *inzipfile, unsigned int *size, int memsize){
   #ifdef GN_DEBUG
	if(!zip_pak_name_mem || !inzipfile){ GN_Log("UnzipLoadFileData 没有指定好压缩包文件名或须解压的文件名!\n"/*"UnzipLoadFileData do not assigned file name or in zip file!\n"*/); }
   #endif
	unzFile uf = unzOpen(zip_pak_name_mem,memsize);
    if(unzLocateFile(uf,inzipfile,0) != UNZ_OK){
		GN_Log("文件 %s 不存在于 %s 压缩包里!\n"/*"file %s not found in the %s zipfile\n"*/,inzipfile, (memsize < GNf::eFILE_MEM)?zip_pak_name_mem:"\0" );
        unzClose(uf);
		return 0;
    }
	static const char *unzErrorStr = "错误 %d 在于解压调用 %s\n"; //"error %d with zipfile in %s\n"
    unz_file_info file_info;
    char filename_inzip[384];
    int err = unzGetCurrentFileInfo(uf,&file_info,filename_inzip,384,0,0,0,0);
	if (err != UNZ_OK){
	    GN_Log(unzErrorStr, err, "unzGetCurrentFileInfo");
	    unzClose(uf);
		return 0;
	}
  	err = unzOpenCurrentFile(uf);
	if (err != UNZ_OK){
	    GN_Log(unzErrorStr, err, "unzOpenCurrentFile");
        unzClose(uf);
		return 0;
	}
    *size = file_info.uncompressed_size;
    uint8 *buf = new uint8[file_info.uncompressed_size];

	unzReadCurrentFile(uf,buf,file_info.uncompressed_size); //一次过读取整个文件
    unzCloseCurrentFile (uf);
	unzClose(uf);
    return buf;
}
GN_API  int GNd::Unzip(const char *zip_pak_name_mem, const char *out_path, int memsize, bool remove_pak){
   #ifdef GN_DEBUG
	if(!zip_pak_name_mem){ GN_Log("解压缩! 没有指定好压缩包文件名或数据!\n"/*"Unzip pack do not assigned zip file name!\n"*/); }
   #endif
    unzFile unzf = unzOpen(zip_pak_name_mem,memsize);
	unsigned int name_len = 384;
	char *str, *read_name = ZipCurProcFileName;
	if(out_path && out_path[0]){
        if(GNf::Access(out_path,0)){
			if(GNf::Mkdir(out_path) == -1){ //创建输出目录
			    unzClose(unzf);
				GN_ErrorLast("创建输出目录失败!"/*"create output dir faile!"*/,0);
				return -1;
			}
		}
		unsigned int  p_len = strlen(out_path);
		strncpy(read_name, out_path, p_len); read_name[p_len++] = '/'; read_name[p_len] = 0;
		name_len -= p_len;
        str = (char*)(read_name + p_len);
	}else str = read_name;
	
	bool (*on_unzip_file_begin)(const char *,const void **, int) = OnUnZipFileBeginCallBack;
	void (*on_unzip_file_end)  (const char *) = OnUnZipFileEndCallBack;
	unz_file_info file_info;
	GNf::sFILE *outfile = 0;
	float porg_ratio, &porg = ZipCurProcFileProgress;
	int f_size, _size, r_size, _w_size, buf_size = GN_USING_FILE_BUF << 3;
	char *buf = new char[buf_size], *fname, err_msg[384];
	do{
		 if(unzGetCurrentFileInfo(unzf, &file_info, str, name_len, 0,0,0,0) != UNZ_OK){
		    #define GT_TMP_UNZ_RETURN( VAL ) \
			 delete [] buf;\
			 unzClose(unzf);\
			 return VAL

			 GN_ErrorLast("解压时获得其中一个文件信息失败!"/*"unzGetCurrentFileInfo faile!"*/,0);
			 GT_TMP_UNZ_RETURN( -2 );
		 }
         f_size = file_info.uncompressed_size;
		 if(!f_size && str[file_info.size_filename-1] == '/'){
			 if(GNf::Access(read_name,0)){
				 if(GNf::Mkdir(read_name) == -1){ //是目录和该目录不存在时就创建它
					 GN_ErrorLast("创建压缩包里的目录失败!"/*"create in pack dir faile!"*/,0);
					 GT_TMP_UNZ_RETURN( -3 );
				 }
			 }
		 }else if(strcmp(str+(file_info.size_filename-3),".db")){ //过滤掉.db文件
			 if(on_unzip_file_begin){
				 if(!on_unzip_file_begin(read_name, (const void **)&unzf, f_size))continue;
			 }
			 if(unzOpenCurrentFile(unzf) != UNZ_OK){
				 sprintf(err_msg,"解压缩文件定位打开文件: %s 失败!"/*"unzipfile in open file: %s faile!"*/,read_name);
				 GN_ErrorLast(err_msg,0);
				 GT_TMP_UNZ_RETURN( -4 );
			 }
			 _size = 0;
			 outfile = GNf::Open(read_name,"wb",GNf::eFILE_STD);
			 if(!outfile){ //文件解压目标位置的目录可能还未创建就创建它
				 fname = GN_StrRemoveFileName(read_name);
                 GNf::Mkdir(read_name);
                 *(fname-1) = '/';
                 outfile = GNf::Open(read_name,"wb",GNf::eFILE_STD);
				 if(!outfile)goto GT_GOTO_UnZipOnceFileEnd;
			 }
			 porg_ratio = 1.0f / (float)f_size;
			 porg = 0.0f;
             while(_size < f_size){
			 	 r_size = ((f_size - _size) > buf_size) ? buf_size : (f_size - _size);
				 if(unzReadCurrentFile(unzf,buf,r_size) >= 0){
					 _w_size = GNf::Write(buf,1,r_size,outfile);
					 if(_w_size != r_size){
						 sprintf(err_msg,"解压缩文件 %s 写入解压好的数据时失败!"/*"unzipfile: %s write data faile!"*/,read_name);
                         GN_ErrorLast(err_msg,0);
						 break;
					 }
				 }else{
				     sprintf(err_msg,"解压缩文件 %s 的数据时失败!"/*"unzReadCurrentFile: %s faile!"*/,read_name);
                     GN_ErrorLast(err_msg,0);
					 break;
				 }
                 _size += _w_size;
				 porg = _size * porg_ratio;
			 }
			 GNf::Close(outfile);
            GT_GOTO_UnZipOnceFileEnd:
			 unzCloseCurrentFile(unzf);
             if(on_unzip_file_end)on_unzip_file_end(read_name);
			 if(_size < f_size){
				 sprintf(err_msg,"解压文件: %s 时遇到错误,解压缩被中断!",read_name);
				 GN_ErrorLast(err_msg,0);
			     GT_TMP_UNZ_RETURN( -5 );
			 }
	     }
	}while( unzGoToNextFile(unzf) != UNZ_END_OF_LIST_OF_FILE);

	delete [] buf;
	unzClose(unzf);
	if(remove_pak && memsize == GNf::eFILE_STD)GNf::Del(zip_pak_name_mem);
	return 0;
}
GN_API int GNd_UnzOpenCurrentFile(unzFile file){ return unzOpenCurrentFile( file ); }
GN_API int GNd_UnzReadCurrentFile(unzFile file, voidp buf, unsigned len){ return unzReadCurrentFile(file, buf, len); }
GN_API int GNd_UnzCloseCurrentFile(unzFile file){ return unzCloseCurrentFile( file ); }

GN_API char  GNd::ZipCurProcFileName[384] = {0};
GN_API float GNd::ZipCurProcFileProgress  = 1.0f;
GN_API bool (*GNd::OnUnZipFileBeginCallBack)(const char*, const void **, int) = 0;
GN_API void (*GNd::OnUnZipFileEndCallBack)  (const char*) = 0;
#endif
#endif //endif _GN_FORMAT_H_
