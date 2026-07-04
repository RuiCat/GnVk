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

#include"GN.h"
#ifdef _GN_LAYOUT_H_
/**********************************************************  sARRANGE_FMT结构  ***********************************************************************/
void GNg::sARRANGE_FMT::Init(bool _rect_fill,float _near_pointU,float _near_pointV,short _i_compartment,short _j_compartment,short _width,short _height,unsigned char _auto_enter,bool _erect_print,bool _overturn_print){
    rect_fill       = _rect_fill;
    near_pointU     = _near_pointU;
	near_pointV     = _near_pointV;
    i_compartment   = _i_compartment;
	j_compartment   = _j_compartment;
	width           = _width;
	height          = _height;
	auto_enter      = _auto_enter;
	erect_print     = _erect_print;
	overturn_print  = _overturn_print;
}
GN_API GNg::sARRANGE_FMT  GNg::ArrangeFmt(false);
GN_API GNg::sDRAW_RANGE   GNg::DrawRange(0,0);

GNg::sDRAW_IN_AREA::sDRAW_IN_AREA():nmin_x(2),nmin_y(2),ntype_x(GNu::eNEAR_LEFT_TOP),ntype_y(GNu::eNEAR_LEFT_TOP){ c = 0xffffffff; }

void GNg::sDRAW_IN_AREA::EnsureInclude(sDRAW_AREA2 *rng){
	short size;
	if(ntype_x != GNu::eNEAR_SCALE){
		if(ntype_x)size = w + x + nmin_x;
		else	   size = w + (nmin_x<<1);
		if(rng->w < size)rng->w = size;
	}
	if(ntype_y != GNu::eNEAR_SCALE){
		if(ntype_y)size = h + y + nmin_y;
		else	   size = h + (nmin_y<<1);
		if(rng->h < size)rng->h = size;
	}
}
unsigned int GNg::sDRAW_IN_AREA::DrawBeginPosGet(short &x_out, short &y_out, sDRAW_AREA2 *rng){
	if(ntype_x){
		if(ntype_x&1)x_out = x + rng->x;
		else x_out = rng->x + rng->w - (x + w);
	}else x_out = rng->x + (short)((int)(rng->w-(nmin_x<<1) - w) * (*((unsigned short*)&x)) >> 16) + nmin_x;
	if(ntype_y){
		if(ntype_y&1)y_out = y + rng->y;
		else y_out = rng->y + rng->h - (y + h);
	}else y_out = rng->y + (short)((int)(rng->h-(nmin_y<<1) - h) * (*((unsigned short*)&y)) >> 16) + nmin_y;

	return GNg_MultiplyColorRatio( c , rng->c );
}
void GNg::sDRAW_IN_AREA::DrawRangeGet(sDRAW_AREA2 &out, sDRAW_AREA2 *rng){
	out.w = w; out.h = h;
	if(ntype_x){
		if(ntype_x==1)out.x = x + rng->x;
		else if(ntype_x==3){ out.w = rng->w - x - nmin_x; out.x = x + rng->x;  }
		else out.x = rng->x + rng->w - (x + w);
	}else out.x = rng->x + (short)((int)(rng->w-(nmin_x<<1) - w) * (*((unsigned short*)&x)) >> 16) + nmin_x;
	if(ntype_y){
		if(ntype_y==1)out.y = y + rng->y;
		else if(ntype_y==3){ out.h = rng->h - y - nmin_y; out.y = y + rng->y;  }
		else out.y = rng->y + rng->h - (y + h);
	}else out.y = rng->y + (short)((int)(rng->h-(nmin_y<<1) - h) * (*((unsigned short*)&y)) >> 16) + nmin_y;
	
	out.c = GNg_MultiplyColorRatio( c , rng->c );
}
//如字数大于范围数就重新分配更多绘制范围空间
#define GT_CheckAndReNewDrawRanges() \
    if(ni > buf_count){\
		buf_count = (ni < 64 ? 64 : (ni + (ni >> 1)));\
		if(_rng_data)delete [] _rng_data;\
		_rng_data = new GNg::sDRAW_RANGE[buf_count];\
	}

GNg::sDRAW_RANGE *GT_CheckAndReNewDrawRanges1(GNg::sDRAW_RANGE *_rng_data, int &buf_count, uint8 jnum0, uint8 jnum, GT_sSEGMENT_S *jnums2){
	int nj = jnum0, ni = 0;
	for(; nj < jnum; ++nj)if(jnums2[nj].end - jnums2[nj].start > 0)ni += jnums2[nj].end - jnums2[nj].start;
	GT_CheckAndReNewDrawRanges();
	return _rng_data;
}
#define GT_CheckAndReNewDrawRanges2() \
	if(new_data){\
	    _rng_data = GT_CheckAndReNewDrawRanges1(_rng_data, buf_count, jnum0, jnum, jnums2);\
        new_data = false;\
	}

int GNg::ComputeArrangeRangeData(sDRAW_RANGE **data_out, int &buf_count, char *text,unsigned short count,int x,int y,int w,int h,int text_ofs_x,int text_ofs_y,
					  const sARRANGE_FMT *parm, uint8 mode, float tu_w, float tv_h,unsigned short *row_hids_out,unsigned char *row_count_out,short *text_w_out, short *text_h_out)
{
	unsigned short t_count;        //统计字数用
	unsigned short ti, tj, ti2;
	unsigned short id = 0;         //统计已处理字数 并返回
	unsigned short tex_id = 0;     //临时纹理索引
	
	int   px = x, py = y;          //光标行到的当前处
	int   ww = w, hh = h;		   //临时宽度与高度
	int   i_comp = parm->i_compartment;   //字符当前显示用的横间隔
	int   j_comp = parm->j_compartment;   //字符当前显示用的竖间隔
	int   font_w = parm->width;			  //字符当前显示用的字宽
	int   font_h = parm->height;		  //字符当前显示用的字高
	int   wcw;							  //中文字比英文字多出的一半宽度
	char  t_row,t_col,t_col2;			  //记录截取纹理的格数
	
	char  enter = mode ? '\n' : 0xff;
	uint8 jnum = 0;						  //记录字显示的行数 最大256行 不能超出
	float tu, tv;						  //临时纹理坐标用
	float ofs = 0.0f;					  //用来补齐经'\n'之后所占用一个字宽的偏移
	*row_count_out = 0;
	sDRAW_RANGE *_rng_data = *data_out;   //记录排好的位置数据

	if(!ww || !hh){   //***************************************** 无筐的处理 **********************************************************************

        #define GT_TMP_STATIS_ROW_CHAR_NUM \
		    uint8 jnums1[256]; /*保存每行里的字数 最大256个*/\
	        jnums1[jnum]=0;\
		    if(count){\
               t_count = count;\
		       for(ti=0;ti<t_count;++ti){\
			       if(text[ti]==enter)jnums1[++jnum] = 1;\
			       else jnums1[jnum]++;\
		       }\
	        }else{\
               t_count = 0;\
	           while(text[t_count]){\
			       if(text[t_count]==enter)jnums1[++jnum] = 1;\
			       else jnums1[jnum]++;\
			       ++t_count;\
		       }\
	        }++jnum;\
			if(text_h_out){\
				short trn, tri ;\
				for(trn = 0,tri = 0; tri < jnum; ++tri)if(jnums1[tri] > trn)trn = jnums1[tri];\
				if(parm->erect_print){\
		            *text_h_out = trn * j_comp;\
                    *text_w_out = (short)jnum * (short)i_comp;\
				}else{\
				    *text_h_out = (short)jnum * j_comp;\
                    *text_w_out = trn * (short)i_comp;\
				}\
			}

		#define GT_TMP_COLLECT_COL_ST_END( TI ) \
			if(text[ TI ]==enter){\
		        jnums2[jnum++].start >>= 1;\
                jnums2[jnum].start = 2;\
		        jnums2[jnum].end = 1;\
			}else{\
		        jnums2[jnum].start += 2 - ((text[ TI ]&128)>>7);\
			    jnums2[jnum].end++;\
			}

		//中文打竖排列时的统计行列数
        #define GT_TMP_STATIS_GB2312_ERECT_PRINT_NUM \
		    GT_sSEGMENT_S jnums2[256];/*保存每行里的字数 最大256个*/\
	        jnums2->start = 0; jnums2->end = 0;\
		    if(count){\
               t_count = count;\
		       for(ti=0;ti<t_count;++ti){\
			       GT_TMP_COLLECT_COL_ST_END( ti ) \
		       }\
	        }else{\
               t_count = 0;\
	           while(text[t_count]){\
				   GT_TMP_COLLECT_COL_ST_END( t_count ) \
			       ++t_count;\
		       }\
	        }jnums2[jnum++].start >>= 1;\
			if(text_h_out){\
				short trn, tri ;\
				for(trn = 0,tri = 0; tri < jnum; ++tri)if(jnums2[tri].start > trn)trn = jnums2[tri].start;\
				*text_h_out = trn * j_comp;\
                *text_w_out = (short)jnum * (short)i_comp;\
			}

		//记录一个纯数字绘制范围
        #define GT_TMP_PUSH_RANGE_NUMBER \
		        tu = (float)(((int)((uint8*)text)[ti]-48)*tu_w); /*纯数字编码规律*/\
		        _rng_data[id].Init(px,py,font_w,font_h,tu,0.0f,(tu+tu_w),tv_h,0,ti)

		//记录一个ASCII字绘制范围
        #define GT_TMP_PUSH_RANGE_ASCII \
		        t_col = text[id]>>4;\
                t_row = text[id]&0x0f; /*ASCII编码规律*/\
                tu =(float)t_row * tu_w; tv = (float)t_col * tv_h;\
				_rng_data[id].Init(px,py,font_w,font_h,tu,tv,(tu+tu_w),(tv+tv_h),0,id)

		//计算获得GB2312字索引UV
        #define GT_FMT_GB2312_GET_UV \
                t_col = text[ti]-0xa1;\
				tex_id = t_col/3;\
				t_col2 = (t_col - tex_id * 3) * 6;\
				t_col = (text[ti+1]-0xa0);\
				t_row = t_col & 0x0f; /*GB2312编码规律*/\
				t_col >>= 4;\
                t_col += t_col2;\
                tu = (float)t_row * tu_w;\
				tv = (float)t_col * tv_h

		//记录一个GB2312字绘制范围
		#define GT_TMP_PUSH_RANGE_GB2312(SCL,RP1,RP2,RP3) \
		   if(text[ti]&128){  /*汉字*/\
				GT_FMT_GB2312_GET_UV;\
				_rng_data[id].Init((px-RP2),py,(font_w<<1),font_h,tu,tv,(tu+tu_w),(tv+tv_h),tex_id,ti);\
				RP3 += RP1 SCL;\
				++ti,++ti2;\
			}else{\
				t_col = text[ti]>>4;\
                t_row = text[ti]&0x0f;\
                tu =(float)t_row * tu_w; tv = (float)t_col * tv_h;\
				_rng_data[id].Init((px+wcw),py,font_w,font_h,tu,tv,(tu+(tu_w2)),(tv+tv_h),29,ti);\
				RP3 += RP1;\
		   }
		if(parm->erect_print){
			if(mode == eFONT_ASCII){
				GT_TMP_STATIS_ROW_CHAR_NUM
				int nj, ni = 0;
				for(nj=0; nj<jnum; ++nj) ni += jnums1[nj];
				GT_CheckAndReNewDrawRanges();
				if(parm->overturn_print){ // 竖方向翻转输出 -------------------------------------------------------------------------
				   #define GT_TMP_LOOP_PUSH_CHAR_RANGES_ERECT( PUSH_FUNC , OFS , ADD ) \
					ti = 0;\
					px = (int)(x OFS (parm->i_compartment * jnum - parm->i_compartment + parm->width)* parm->near_pointU);\
		            for(tj=0; tj<jnum; ++tj){\
		               row_hids_out[*row_count_out] = id;\
					   py = (int)(y -(parm->j_compartment * jnums1[tj] - parm->j_compartment + parm->height)* parm->near_pointV - ofs * (1.0f-parm->near_pointV));\
		               for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){\
					       PUSH_FUNC ;\
	                       py += parm->j_compartment;\
		               }\
		               px ADD parm->i_compartment;\
                       ofs = parm->j_compartment;\
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);\
				    }
					GT_TMP_LOOP_PUSH_CHAR_RANGES_ERECT( GT_TMP_PUSH_RANGE_ASCII , + , -= )
			    }else{                     // 竖方向顺着输出 --------------------------------------------------------------------------
				    GT_TMP_LOOP_PUSH_CHAR_RANGES_ERECT( GT_TMP_PUSH_RANGE_ASCII , - , += )
			    }
			}else if(mode == eFONT_FREETYPE){
			    GT_TMP_STATIS_GB2312_ERECT_PRINT_NUM;
				int nj, ni = 0;
				for(nj=0; nj<jnum; ++nj) ni += jnums2[nj].end;
				GT_CheckAndReNewDrawRanges();
				float tu_w2 = tu_w * 0.5f;
				wcw = font_w>>1;
				i_comp <<= 1;
				ti = 0;
				if(parm->overturn_print){ // GB2312竖方向翻转输出 -------------------------------------------------------------------
					px = (int)(x + (i_comp * jnum - i_comp + (parm->width<<1))* parm->near_pointU);
		            for(tj=0; tj<jnum; ++tj){
					   row_hids_out[*row_count_out] = id;
		               py = (int)(y -(parm->j_compartment * jnums2[tj].start - parm->j_compartment + parm->height)* parm->near_pointV - ofs * (1.0f-parm->near_pointV));
		               for(ti2=0; ti2<jnums2[tj].end; ++ti,++ti2,++id){
					       GT_TMP_PUSH_RANGE_GB2312( ,parm->j_compartment,0,py);
		               }
		               px -= i_comp;
                       ofs = parm->j_compartment;
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				    }
			    }else{                      // GB2312竖方向,顺着输出 ------------------------------------------------------------------
					px = (int)(x - (i_comp * jnum - i_comp + (parm->width<<1))* parm->near_pointU);
		            for(tj=0; tj<jnum; ++tj){
					   row_hids_out[*row_count_out] = id;
		               py = (int)(y -(parm->j_compartment * jnums2[tj].start - parm->j_compartment + parm->height)* parm->near_pointV - ofs * (1.0f-parm->near_pointV));
		               for(ti2=0; ti2<jnums2[tj].end; ++ti,++ti2,++id){
			               GT_TMP_PUSH_RANGE_GB2312( ,parm->j_compartment,0,py);
		               }
		               px += i_comp;
                       ofs = parm->j_compartment;
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				    }
			    }
			}else if(mode == eFONT_NUMBER){
				GT_TMP_STATIS_ROW_CHAR_NUM
			    int nj,ni = 0;
				for(nj=0; nj<jnum; ++nj) ni += jnums1[nj];
				GT_CheckAndReNewDrawRanges();

				if(parm->overturn_print){ // 竖方向翻转输出 -------------------------------------------------------------------------
					GT_TMP_LOOP_PUSH_CHAR_RANGES_ERECT( GT_TMP_PUSH_RANGE_NUMBER , + , -= )
			    }else{                    // 竖方向顺着输出 --------------------------------------------------------------------------
				    GT_TMP_LOOP_PUSH_CHAR_RANGES_ERECT( GT_TMP_PUSH_RANGE_NUMBER , - , += )
			    }
			}
		}else{
			GT_TMP_STATIS_ROW_CHAR_NUM
			int nj,ni = 0;
			for(nj=0; nj<jnum; ++nj) ni += jnums1[nj];
			GT_CheckAndReNewDrawRanges();
			wcw = 0;
			if(parm->overturn_print){ // 横方向翻转输出 -----------------------------------------------------------------------------
				py = (int)(y - (parm->j_compartment * jnum - parm->j_compartment + parm->height)* parm->near_pointV);
		        if(mode == eFONT_ASCII){
				   #define GT_TMP_LOOP_PUSH_CHAR_RANGES( PUSH_FUNC , OFS , ADD ) \
				    ti = 0;\
					for(tj=0; tj<jnum; ++tj){\
					  row_hids_out[*row_count_out] = id;\
		              px = (int)(x OFS (parm->i_compartment * jnums1[tj] - parm->i_compartment + parm->width)* parm->near_pointU OFS ofs * (1.0f-parm->near_pointU));\
				          for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){\
			                  PUSH_FUNC ;\
	                          px ADD parm->i_compartment;\
		                  }\
		              py += parm->j_compartment;\
                      ofs = parm->i_compartment;\
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);\
				    }
					GT_TMP_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_ASCII , + , -= )
				}else if(mode == eFONT_FREETYPE){
					float tu_w2 = tu_w * 0.5f;
					ti = 0;
					for(tj=0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(x +(parm->i_compartment * jnums1[tj] - parm->i_compartment + parm->width)* parm->near_pointU + ofs * (1.0f-parm->near_pointU));
					  for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){
				              GT_TMP_PUSH_RANGE_GB2312(*2,-(parm->i_compartment),parm->i_compartment,px);
					  }
                      py += parm->j_compartment;
                      ofs = parm->i_compartment;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
				}else if(mode == eFONT_NUMBER){
				   GT_TMP_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_NUMBER , + , -= )
				}
			}else{                      // 横方向顺着输出 -----------------------------------------------------------------------------
				py = (int)(y - (parm->j_compartment * jnum - parm->j_compartment + parm->height)* parm->near_pointV);
		        if(mode == eFONT_ASCII){
				   GT_TMP_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_ASCII , - , += )
				}else if(mode == eFONT_FREETYPE){
					float tu_w2 = tu_w * 0.5f;
					ti = 0;
					for(tj=0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(x -(parm->i_compartment * jnums1[tj] - parm->i_compartment + parm->width)* parm->near_pointU - ofs * (1.0f-parm->near_pointU));
					      for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){
				              GT_TMP_PUSH_RANGE_GB2312(<<1,parm->i_compartment,0,px);
					      }
                      py += parm->j_compartment;
                      ofs = parm->i_compartment;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
				}else if(mode == eFONT_NUMBER){
				   GT_TMP_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_NUMBER , - , += )
				}
			}
		}
	}else if(parm->rect_fill){ //******* 筐内填满的处理 *****************************************************************************************
		uint8 max_row_num = 0;
		float ratio_w = (float)(parm->width )/(float)(parm->i_compartment),
			  ratio_h = (float)(parm->height)/(float)(parm->j_compartment);
       
        #define GT_TMP_STATIS_ROW_CHAR_NUM1 \
		    uint8 jnums1[256]; /*保存每行里的字数 最大256个*/\
	        jnums1[jnum]=0;\
		    if(count){\
               t_count = count;\
		       for(ti=0; ti<t_count; ++ti){\
			       if(text[ti] == enter){\
				       max_row_num = max_row_num < jnums1[jnum] ? jnums1[jnum] : max_row_num;\
                       jnums1[++jnum]=1;\
			       }else jnums1[jnum]++;\
		       }\
	        }else{\
               t_count=0;\
	           while(text[t_count]){\
			       if(text[t_count] == enter){\
				       max_row_num = max_row_num < jnums1[jnum] ? jnums1[jnum] : max_row_num;\
                       jnums1[++jnum]=1;\
			       }else jnums1[jnum]++;\
			       ++t_count;\
		       }\
	        }\
			max_row_num = max_row_num < jnums1[jnum] ? jnums1[jnum] : max_row_num;\
			++jnum

		if(parm->erect_print){
            if(mode == eFONT_ASCII){
			    GT_TMP_STATIS_ROW_CHAR_NUM1;
			    int nj, ni = 0;
				for(nj=0; nj<jnum; ++nj)ni += jnums1[nj];
				GT_CheckAndReNewDrawRanges();
				
			   #define GT_TMP_FONT_DRAW_RNG_CPT \
				i_comp = (int)((float)ww / ((float)jnum-1.0f + ratio_w));\
                j_comp = (int)((float)hh / ((float)max_row_num - 2.0f + ratio_h));\
				font_w = (int)(i_comp * ratio_w);\
			    font_h = (int)(j_comp * ratio_h)
				
				GT_TMP_FONT_DRAW_RNG_CPT;

			    if(parm->overturn_print){ // 竖方向翻转输出 ------------------------------------------------------------------------------
				   #define GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES( PUSH_FUNC , ADD ) \
				    ti = 0;\
					for(tj=0; tj<jnum; ++tj){\
					   row_hids_out[*row_count_out] = id;\
				       py =(int)(( hh  - (jnums1[tj]) * j_comp + j_comp - font_h - (jnums1[tj]>>1) )* parm->near_pointV + y - ofs * (1.0f-parm->near_pointV));\
				       for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){\
			               PUSH_FUNC ;\
	                       py += j_comp+(ti&1);\
		               }\
		               px ADD i_comp + (tj&1);\
                       ofs = (float)j_comp;\
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);\
				    }
					px = x + ww - font_w;
					GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_ASCII , -= )
			    }else{                    // 竖方向顺着输出 -------------------------------------------------------------------------------
					GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_ASCII , += )
				}
            }else if(mode == eFONT_FREETYPE){
			    GT_sSEGMENT_S jnums2[256]; /*保存每行里的字数 最大256个*/
	            jnums2->start=0; jnums2->end=0;
		        if(count){
                   t_count = count;
		           for(ti=0; ti<t_count; ++ti){
				       if(text[ti]==enter){
		                   jnums2[jnum].start >>= 1;
		                   max_row_num = max_row_num < jnums2[jnum].start ? jnums2[jnum].start : max_row_num;
                           jnums2[++jnum].start = 2;
		                   jnums2[  jnum].end = 1;
					   }else{
		                   jnums2[jnum].start += 2 - ((text[ti]&128)>>7);
					       jnums2[jnum].end++;
					   }
		           }
	            }else{
                   t_count=0;
	               while(text[t_count]){
				       if(text[t_count]==enter){
		                   jnums2[jnum].start >>= 1;
		                   max_row_num = max_row_num < jnums2[jnum].start ? jnums2[jnum].start : max_row_num;
                           jnums2[++jnum].start = 2;
						   jnums2[  jnum].end = 1;
					   }else{
					       jnums2[jnum].start += 2 - ((text[t_count]&128)>>7);
					       jnums2[jnum].end++;
					   }
				       ++t_count;
		           }
	            }
				jnums2[jnum].start >>= 1;
		        max_row_num = max_row_num < jnums2[jnum].start?jnums2[jnum].start:max_row_num;
		        ++jnum;
			    int nj, ni = 0;
				for(nj=0; nj<jnum; ++nj)ni += jnums2[nj].end;
				GT_CheckAndReNewDrawRanges();
				GT_TMP_FONT_DRAW_RNG_CPT;
			    font_w >>= 1;
				wcw = font_w >> 1;
				float tu_w2 = tu_w * 0.5f;
				ti = 0;
                if(parm->overturn_print){ // GB2312竖方向翻转输出 ------------------------------------------------------------------------
		            px = x + ww - (font_w<<1);
					for(tj=0; tj<jnum; ++tj){
					   row_hids_out[*row_count_out] = id;
				       py =(int)(( hh - (jnums2[tj].start) * j_comp + j_comp - font_h - (int)((float)jnums2[tj].start/3.0f) )* parm->near_pointV + y - ofs * (1.0f-parm->near_pointV));
				       for(ti2=0; ti2<jnums2[tj].end; ++ti,++ti2,++id){
			               GT_TMP_PUSH_RANGE_GB2312( ,(j_comp+(id&1)),0,py);
		               }
		               px -= i_comp+1;
                       ofs = (float)j_comp;
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				    }
			    }else{                      // GB2312竖方向顺着输出 ------------------------------------------------------------------------
					for(tj=0; tj<jnum; ++tj){
					   row_hids_out[*row_count_out] = id;
				       py =(int)(( hh - (jnums2[tj].start) * j_comp + j_comp - font_h - (int)((float)jnums2[tj].start/3.0f) )* parm->near_pointV + y - ofs * (1.0f-parm->near_pointV));
				       for(ti2=0; ti2<jnums2[tj].end; ++ti,++ti2,++id){
			               GT_TMP_PUSH_RANGE_GB2312( ,(j_comp+(id&1)),0,py);
		               }
		               px += i_comp+1;
                       ofs = (float)j_comp;
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				    }
				}
			}else if(mode == eFONT_NUMBER){
			    GT_TMP_STATIS_ROW_CHAR_NUM1;
			    int nj, ni = 0;
				for(nj=0; nj<jnum; ++nj)ni += jnums1[nj];
				GT_CheckAndReNewDrawRanges();
				GT_TMP_FONT_DRAW_RNG_CPT;

			    if(parm->overturn_print){ // NUMBER竖方向翻转输出 ------------------------------------------------------------------------
		            px = x + ww - font_w;
					GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_NUMBER , -= )
			    }else{                    // NUMBER竖方向顺着输出 -------------------------------------------------------------------------
		            GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES( GT_TMP_PUSH_RANGE_NUMBER , += )
				}
            }
		}else{
		    GT_TMP_STATIS_ROW_CHAR_NUM1;
			int nj,ni = 0;
			for(nj=0; nj<jnum; ++nj)ni += jnums1[nj];
			GT_CheckAndReNewDrawRanges();

			i_comp = (int)((float)ww / ((float)max_row_num - 2.0f + ratio_w)); //按比例来计算得新的宽度和横跨度
            j_comp = (int)((float)hh / ((float)jnum		   - 1.0f + ratio_h)); //按比例来计算得新的高度和竖跨度
            font_w = (int)(i_comp * ratio_w);
			font_h = (int)(j_comp * ratio_h);
			wcw = 0;
			if(parm->overturn_print){ // 横方向翻转输出 ----------------------------------------------------------------------------------
		        if(mode == eFONT_ASCII){
				   #define GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES_1( PUSH_FUNC , OFS , ADD ) \
				    ti = 0;\
					for(tj=0; tj<jnum; ++tj){\
					   row_hids_out[*row_count_out] = id;\
				       OFS ;\
					   for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){\
			               PUSH_FUNC ;\
	                       px ADD i_comp+(ti&1);\
		               }\
		               py += j_comp+(tj&1);\
                       ofs = (float)i_comp;\
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);\
				    }
					GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES_1( GT_TMP_PUSH_RANGE_ASCII ,\
					 px =(int)(x + ww -( ww  - (jnums1[tj]) * i_comp + font_w - (int)((float)jnums1[tj]*1.35f)) * parm->near_pointU - font_w + ofs * (1.0f-parm->near_pointU)) ,\
					 -= )
				}else if(mode == eFONT_FREETYPE){
					float tu_w2 = tu_w * 0.5f;
					ti = 0;
					for(tj=0; tj<jnum; ++tj){
					   row_hids_out[*row_count_out] = id;
				       px =(int)(x + ww -( ww  - (jnums1[tj]) * i_comp + font_w - (int)((float)jnums1[tj]*1.35f)) * parm->near_pointU - font_w + ofs * (1.0f-parm->near_pointU));
				       for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){
			               GT_TMP_PUSH_RANGE_GB2312(*2,-(i_comp+(ti&1)),i_comp,px);
		               }
		               py += j_comp+(tj&1);
                       ofs = (float)i_comp;
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				    }
				}else if(mode == eFONT_NUMBER){
					GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES_1( GT_TMP_PUSH_RANGE_NUMBER ,\
					 px =(int)(x + ww -( ww  - (jnums1[tj]) * i_comp + font_w - (int)((float)jnums1[tj]*1.35f)) * parm->near_pointU - font_w + ofs * (1.0f-parm->near_pointU)) ,\
					 -= )
				}
			}else{                     // 横方向顺着输出 -----------------------------------------------------------------------------------
		        if(mode == eFONT_ASCII){
					GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES_1( GT_TMP_PUSH_RANGE_ASCII ,\
					 px =(int)(( ww  - (jnums1[tj]) * i_comp + i_comp - font_w - (int)((float)jnums1[tj]/1.4f) )* parm->near_pointU + x - ofs * (1.0f-parm->near_pointU)) ,\
					 += )
				}else if(mode == eFONT_FREETYPE){
					float tu_w2 = tu_w * 0.5f;
					ti = 0;
					for(tj=0; tj<jnum; ++tj){
				       row_hids_out[*row_count_out] = id;												  //弥补因为float转int截去的小数差距
					   px =(int)(( ww  - (jnums1[tj]) * i_comp + i_comp - font_w - (int)((float)jnums1[tj]/1.4f) )* parm->near_pointU + x - ofs * (1.0f-parm->near_pointU));
				       for(ti2=0; ti2<jnums1[tj]; ++ti,++ti2,++id){
			                GT_TMP_PUSH_RANGE_GB2312(<<1,(i_comp+(ti&1)),0,px);
		               }
		               py += j_comp+(tj&1);
                       ofs = (float)i_comp;
					   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				    }
				}else if(mode == eFONT_NUMBER){
					GT_TMP_INRECT_LOOP_PUSH_CHAR_RANGES_1( GT_TMP_PUSH_RANGE_NUMBER ,\
					 px =(int)(( ww  - (jnums1[tj]) * i_comp + i_comp - font_w - (int)((float)jnums1[tj]/1.4f) )* parm->near_pointU + x - ofs * (1.0f-parm->near_pointU)) ,\
					 += )
				}
			}
		}
		if(text_h_out){
			*text_h_out = (short)h;  *text_w_out = (short)w;
		}
	}else{ //****************************** 在筐内详细的处理 ***********************************************************************************
		ww -= parm->width;  hh -= parm->height;  //先忽略字的宽和高 能够轻易的计算出心点坐标
        uint8 row_num = (uint8)(ww / parm->i_compartment) + 1,		//计算筐内每行最大可放几字
			  col_num = (uint8)(hh / parm->j_compartment) + 1;		//计算筐内每列最大可放几字
		uint8 row_num2 = row_num + 1, col_num2 = col_num + (mode?1:2);  //估计行和列最多可以看到的格数
		bool row_enter = (parm->auto_enter & GNg::eENTER_LEFT_RIGHT) ? true : false;  //需横向自动换行
		bool col_enter = (parm->auto_enter & GNg::eENTER_TOP_BOTTOM) ? true : false;  //需竖向自动换行
		bool is_n = false;                   //当前字符是否为'\n'
		bool new_data = true;                //是否需要新分配数据
		uint8 jnum0 = 0;					 //起始行号
		GT_sSEGMENT_S jnums2[256];			 //保存每行的起始显示编号和结束显示编号 最多256行
		jnums2[0].start=0; jnums2[0].end=0;  //先将头尾设为0以防出轨
		short ofs_num;						 //每行或每列偏移格数
	    unsigned short vis_num;				 //每行或每列可视格数
		int fi = 0, td =0;                   //中文踢除辅助用
		
		if(parm->erect_print){
			if(mode == eFONT_FREETYPE){
			    row_num >>=1; row_num2 = row_num + 1;
                i_comp <<= 1;
				td = i_comp >> 1;
                short max_row_num = 0, max_row_num1 = 0;
				if(count){ //.GB2312查找并统计文本要显示的行数和每行字数 .............................
                    t_count = count;
			        for(ti=0; ti<t_count; ++ti){
					   #define GT_TMP_COLLECT_ROW_ST_END( TI ) \
				        is_n = text[ TI ]==enter;\
					    jnums2[jnum].end++;\
						max_row_num1 += 2 -((text[ TI ]&128)>>7);\
						if(is_n || (col_enter && ((fi+=2-((text[ TI ]&128)>>7))>>1) >= col_num)){\
				            jnums2[jnum++].end -= is_n;\
							max_row_num1 -= 2;\
							if(max_row_num < max_row_num1)max_row_num = max_row_num1;\
							max_row_num1 = 0;\
							fi = 0;\
							jnums2[jnum].start = jnums2[jnum-1].end + is_n;\
                            jnums2[jnum].end   = jnums2[jnum].start;\
					        if(row_enter && jnum >= row_num)break;\
					    }
						GT_TMP_COLLECT_ROW_ST_END( ti )
			        }
	            }else{
                    t_count=0;
	                while(text[t_count]){
				        GT_TMP_COLLECT_ROW_ST_END( t_count )
				        ++t_count;
			        }
	            }/*if(!row_enter || !col_enter)*/++jnum;
                if(max_row_num < max_row_num1)max_row_num = max_row_num1;
				if(text_h_out){
					*text_h_out = (max_row_num>>1) * j_comp;
                    *text_w_out = (short)jnum * (short)i_comp;
			    }
			}else{
			    if(count){ //.ASCII查找并统计文本要显示的行数和每行字数 .............................
                    t_count = count;
			        for(ti=0; ti<t_count; ++ti){
				        is_n = text[ti]==enter;
					   #define GT_TMP_COLLECT_ROW_ST_END1 \
					    if( is_n || ((++jnums2[jnum].end - jnums2[jnum].start) >= col_num && col_enter)){\
                            ++jnum;\
							jnums2[jnum].start = jnums2[jnum-1].end + is_n;\
                            jnums2[jnum].end   = jnums2[jnum].start;\
                            if(row_enter && jnum >= row_num)break;\
					    }
						GT_TMP_COLLECT_ROW_ST_END1
			        }
	            }else{
                    t_count=0;
	                while(text[t_count]){
				        is_n = text[t_count]==enter;
					    GT_TMP_COLLECT_ROW_ST_END1
				        ++t_count;
			        }
	            }/*if(!row_enter || !col_enter)*/++jnum;
				if(text_h_out){
					short trn = 0, tri = 0;
					for(; tri < jnum; ++tri)if(jnums2[tri].end - jnums2[tri].start > trn)trn = jnums2[tri].end - jnums2[tri].start;
					*text_h_out = trn * j_comp;
                    *text_w_out = (short)jnum * (short)i_comp;
			    }
			}
			int i_comp1;
			if(parm->overturn_print){ // 竖方向翻转输出 ----------------------------------------------------------------------------------
				px = (int)(text_ofs_x -( ww  - (jnum - jnum0 -1) * i_comp)* parm->near_pointU);
				
				ofs_num = (short)(px / (int)i_comp); // --- 踢除筐外行的字符编号
				if(ofs_num > 0){
				    jnum0 += ofs_num;
					jnum = ((jnum - jnum0) > row_num2) ? jnum0 + row_num2 : jnum;
					px += ww + x - ofs_num * i_comp - td;
				}else{
					vis_num = row_num2 + ofs_num<0?0:row_num2 + ofs_num;
					jnum = ((jnum - jnum0) > vis_num)? (jnum0 + vis_num) : jnum;
					px += ww + x - td;
				}
				i_comp1 = -i_comp;
			}else{                      // 竖方向顺着输出 ----------------------------------------------------------------------------------
				px = (int)(text_ofs_x+ ((mode==eFONT_FREETYPE?ww-parm->width:ww) - (jnum         -1) * i_comp)* parm->near_pointU);

				ofs_num = (short)(px / (int)i_comp); // --- 踢除筐外行的字符编号
				if(ofs_num < 0){
				    jnum0 -= ofs_num;
					jnum = ((jnum - jnum0) > row_num2) ? jnum0 + row_num2 : jnum;
					px += x - ofs_num * i_comp;
				}else{
					vis_num = row_num2 - ofs_num<0?0:row_num2 - ofs_num;
					jnum = ((jnum - jnum0) > vis_num)? (jnum0 + vis_num) : jnum;
					px += x;
				}
				i_comp1 = i_comp;
			}
			if(mode == eFONT_ASCII){
			    for(tj=jnum0; tj<jnum; ++tj){
			       row_hids_out[*row_count_out] = id;
			       py = (int)(text_ofs_y + ( hh  - (jnums2[tj].end - jnums2[tj].start -1 ) * parm->j_compartment)* parm->near_pointV);
				  
				  #define GT_TMP_CULL_COL_OUT_RECT_STRID /*踢除筐外列的字符编号*/\
				   ofs_num = (short)(py / (int)parm->j_compartment);\
				   if(ofs_num < 0){\
				       jnums2[tj].start -= ofs_num;\
					   jnums2[tj].end=((jnums2[tj].end - jnums2[tj].start) > col_num2) ? jnums2[tj].start + col_num2 : jnums2[tj].end;\
					   py += y - ofs_num * parm->j_compartment;\
				   }else{\
					   vis_num = col_num2 - ofs_num<0?0:col_num2 - ofs_num;\
					   jnums2[tj].end = ((jnums2[tj].end - jnums2[tj].start) > vis_num )? jnums2[tj].start + vis_num : jnums2[tj].end;\
				       py += y;\
				   }\
                   GT_CheckAndReNewDrawRanges2();

			       GT_TMP_CULL_COL_OUT_RECT_STRID

				   for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
			           t_col = text[ti]>>4;
                       t_row = text[ti]&0x0f; /*ASCII编码规律*/
                       tu = (float)t_row * tu_w; tv = (float)t_col * tv_h;
					   _rng_data[id].Init(px,py,parm->width,parm->height,tu,tv,(tu+tu_w),(tv+tv_h),0,ti);
	                   py += parm->j_compartment;
		           }
				   px += i_comp1;
				   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				}\
            }else if(mode == eFONT_FREETYPE){
				float tu_w2 = tu_w * 0.5f;
			    wcw = font_w >> 1;
			    for(tj=jnum0; tj<jnum; ++tj){
		           row_hids_out[*row_count_out] = id;
			       py = (int)(text_ofs_y + ( (hh<<1)  - (jnums2[tj].end - jnums2[tj].start -1+3) * j_comp)* parm->near_pointV / 2.3f);

				   ofs_num = (short)(py / (int)j_comp); // ---------------------- 踢除筐列的字符编号
			       if(ofs_num < 0){
			            ofs_num = -ofs_num;
			            fi = 0; td = 0;
						while(fi < ofs_num && (unsigned short)td + jnums2[tj].start < jnums2[tj].end){
			                td += ((text[td + jnums2[tj].start]&128)>>7) + 1;
							++fi;  //顺着从头检索有几个字符ID出筐
						}
						if(fi < ofs_num)jnums2[tj].start = jnums2[tj].end;
						else{
							jnums2[tj].start += td;
					        td = col_num2;
			                py += y + fi * j_comp;
							fi = 0;
						}
				    }else{
			            td = col_num2 - ofs_num;
						py += y;
                        fi = 0;
				    }
					GT_CheckAndReNewDrawRanges2(); // ----------------------------------------------

					for(ti = jnums2[tj].start; ti<jnums2[tj].end && fi<td; ++ti,++id){
						if(text[ti]&128){
						    GT_FMT_GB2312_GET_UV;
							_rng_data[id].Init(px,py,(font_w+font_w),font_h,tu,tv,(tu+tu_w),(tv+tv_h),tex_id,ti);
							py += j_comp;
							++ti;
						}else{
						    t_col = text[ti]>>4;
                            t_row = text[ti]&0x0f;
                            tu =(float)t_row * tu_w; tv = (float)t_col * tv_h;
						    _rng_data[id].Init((px+wcw),py,font_w,font_h,tu,tv,(tu+(tu_w2)),(tv+tv_h),29,ti);
	                        py += j_comp;
						}
						++fi;
		            }
					px += i_comp1;
					if(id != row_hids_out[*row_count_out])++(*row_count_out);
				}
			}else if(mode == eFONT_NUMBER){
			    for(tj=jnum0; tj<jnum; ++tj){
				   row_hids_out[*row_count_out] = id;
			       py = (int)(text_ofs_y + ( hh  - (jnums2[tj].end - jnums2[tj].start - 1) * parm->j_compartment)* parm->near_pointV);

				   GT_TMP_CULL_COL_OUT_RECT_STRID

				   for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
					   GT_TMP_PUSH_RANGE_NUMBER;
	                   py += parm->j_compartment;
		           }
				   px += i_comp1;
				   if(id != row_hids_out[*row_count_out])++(*row_count_out);
				}
            }
		}else{ /******************************************************************************************************************************/
		    if(count){ // 查找并统计文本要显示的行数和每行字数 -----------------------------------------
                t_count = count;
				if(row_enter){
                    for(ti=0; ti<t_count; ++ti){

                       #define GT_TMP_ARRANGE_IF( TID ) \
				        if(text[ TID ]==enter){\
							++jnum;\
                            jnums2[jnum].end = jnums2[jnum].start = jnums2[jnum-1].end + 1;\
                            if(col_enter && jnum>=col_num)break;\
						}else{

						   #define GT_TMP_ARRANGE_ENTER_CMP( _TID ) /*中文特别处理 以免在字中间断开*/\
							if(!(fi&1) && (jnums2[jnum].end - jnums2[jnum].start >= row_num)){\
								++jnum;\
								jnums2[jnum].end = jnums2[jnum].start = jnums2[jnum-1].end;\
                                if(col_enter && jnum>=col_num)break;\
							}\
						    if(text[ _TID ]&128)++fi

                        GT_TMP_ARRANGE_IF( ti )
                            GT_TMP_ARRANGE_ENTER_CMP( ti );
						    ++jnums2[jnum].end;
						}
					}
				}else{
				    for(ti=0; ti<t_count; ++ti){
						GT_TMP_ARRANGE_IF( ti )
							++jnums2[jnum].end;
					}	}
				}
			}else{
                t_count=0;
				if(row_enter){
		            while(text[t_count]){
					    GT_TMP_ARRANGE_IF( t_count )
                            GT_TMP_ARRANGE_ENTER_CMP( t_count );
						    ++jnums2[jnum].end;
						}
					    ++t_count;
				    }
				}else{
				    while(text[t_count]){
					    GT_TMP_ARRANGE_IF( t_count )
							++jnums2[jnum].end;
						}
					    ++t_count;
				    }
				}
	        }/*if(!col_enter || !row_enter)*/++jnum;

            if(text_h_out){
				short trn = 0, tri = 0;
				for(; tri < jnum; ++tri)if(jnums2[tri].end - jnums2[tri].start > trn)trn = jnums2[tri].end - jnums2[tri].start;
				*text_h_out = (short)jnum * j_comp;
                *text_w_out = trn * (short)i_comp;
			}
			py = (int)(text_ofs_y + ( hh  - (jnum - 1) * parm->j_compartment)* parm->near_pointV);
			
			ofs_num = (short)(py / (int)parm->j_compartment); // --- 横方向踢除筐外列的字符编号
			if(ofs_num < 0){
				jnum0 -= ofs_num;
				jnum = ((jnum - jnum0) > col_num2) ? jnum0 + col_num2 : jnum;
				py += y - ofs_num * parm->j_compartment;
			}else{
				vis_num = col_num2 - ofs_num<0?0:col_num2 - ofs_num;
				jnum = ((jnum - jnum0) > vis_num) ? (jnum0 + vis_num) : jnum;
				py += y;
			} // ------------------------------------------------------------------------------
			if(parm->overturn_print){ // 横方向翻转输出 ------------------------------------------------------------------------------------
				if(mode == eFONT_ASCII){
				   for(tj=jnum0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(text_ofs_x -( ww  - (jnums2[tj].end - jnums2[tj].start - 1) * parm->i_compartment)* parm->near_pointU);
		              
					 #define GT_TMP_CULL_ROW_OUT_RECT_STRID /*踢除筐外行的字符编号*/\
				      ofs_num = (short)(px / (int)parm->i_compartment);\
				      if(ofs_num > 0){\
				          jnums2[tj].start += ofs_num;\
					      jnums2[tj].end=((jnums2[tj].end - jnums2[tj].start) > row_num2) ? jnums2[tj].start + row_num2 : jnums2[tj].end;\
					      px += x + ww - ofs_num * parm->i_compartment;\
				      }else{\
					      vis_num = row_num2 + ofs_num<0?0:row_num2 + ofs_num;\
					      jnums2[tj].end =((jnums2[tj].end - jnums2[tj].start) > vis_num )? jnums2[tj].start + vis_num : jnums2[tj].end;\
				          px += x + ww;\
				      }\
					  GT_CheckAndReNewDrawRanges2();
				      
					  GT_TMP_CULL_ROW_OUT_RECT_STRID

				      for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
			                  t_col = text[ti]>>4;
                              t_row = text[ti]&0x0f; //ASCII编码规律
                              tu =(float)t_row * tu_w; tv = (float)t_col * tv_h;
						      _rng_data[id].Init(px,py,parm->width,parm->height,tu,tv,(tu+tu_w),(tv+tv_h),0,ti);
	                          px -= parm->i_compartment;
		              }
		              py +=  parm->j_compartment;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
                }else if(mode == eFONT_FREETYPE){
				   float tu_w2 = tu_w * 0.5f;
				   for(tj=jnum0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(text_ofs_x -( ww  - (jnums2[tj].end - jnums2[tj].start - 1) * i_comp)* parm->near_pointU);

					     ofs_num = (short)(px / (int)i_comp); // --------------------------------------------------------- 踢除筐外行的字符编号
				         if(ofs_num > 0){
						     td = ofs_num - 1; //中文的
							 if((unsigned short)td + jnums2[tj].start >= jnums2[tj].end)jnums2[tj].start = jnums2[tj].end;
							 else{
						         fi = 0;   //中文的行踢除需这样检查一下 不然的话会间断中文字 但增加了一些开销
						         while((td+1) && text[td + jnums2[tj].start]&128){ ++fi; --td; } //向前番搜 以搜到英文或行前为止
						         ofs_num -= (fi & 1); //中文的
						         jnums2[tj].start += ofs_num;
					             jnums2[tj].end = ((jnums2[tj].end - jnums2[tj].start) > row_num2) ? jnums2[tj].start + row_num2 : jnums2[tj].end;
					             px += x + ww - ofs_num * i_comp;
							 }
				         }else{
						     vis_num =row_num2 + ofs_num<0?0:row_num2 + ofs_num;
					         jnums2[tj].end = ((jnums2[tj].end - jnums2[tj].start) > vis_num)? jnums2[tj].start + vis_num : jnums2[tj].end;
						     px += x + ww;
				         }
						 GT_CheckAndReNewDrawRanges2(); // ------------------------------------------------------------------------------------

					  for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
						  if(text[ti]&128){  //汉字
						      GT_FMT_GB2312_GET_UV;
							  _rng_data[id].Init((px-i_comp),py,(font_w+font_w),font_h,tu,tv,(tu+tu_w),(tv+tv_h),tex_id,ti);
							  px -= i_comp + i_comp;
							  ++ti;
						  }else{ //中文里的英文数字
						      t_col = text[ti]>>4;
                              t_row = text[ti]&0x0f;
                              tu =(float)t_row * tu_w; tv = (float)t_col * tv_h;
						      _rng_data[id].Init(px,py,font_w,font_h,tu,tv,(tu+(tu_w2)),(tv+tv_h),29,ti);
	                          px -= i_comp;
						  }
		              }
		              py += j_comp;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
				}else if(mode == eFONT_NUMBER){
				   for(tj=jnum0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(text_ofs_x -( ww  - (jnums2[tj].end - jnums2[tj].start - 1) * parm->i_compartment)* parm->near_pointU);

					  GT_TMP_CULL_ROW_OUT_RECT_STRID

				      for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
			                  tu = (float)(((int)((uint8*)text)[ti]-48)*tu_w); /*纯数字编码规律*/
		                      _rng_data[id].Init(px,py,parm->width,parm->height,tu,0.0f,(tu+tu_w),tv_h,0,ti);
	                          px -= parm->i_compartment;
		              }
		              py += parm->j_compartment;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
                }
			}else{                    // 横方向顺着输出 -----------------------------------------------------------------------------------
				if(mode == eFONT_ASCII){
				   for(tj=jnum0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(text_ofs_x + ( ww  - (jnums2[tj].end - jnums2[tj].start - 1) * parm->i_compartment)* parm->near_pointU); //排位原的的计算公式
		              
					 #define GT_TMP_CULL_ROW_OUT_RECT_STRID2 /*踢除筐外行的字符编号*/\
				      ofs_num = (short)(px / (int)parm->i_compartment);\
				      if(ofs_num < 0){\
				          jnums2[tj].start -= ofs_num;\
					      jnums2[tj].end = ((jnums2[tj].end - jnums2[tj].start) > row_num2) ? jnums2[tj].start + row_num2 : jnums2[tj].end;\
					      px += x - ofs_num * parm->i_compartment;\
				      }else{\
					      vis_num =row_num2 - ofs_num<0?0:row_num2 - ofs_num;\
					      jnums2[tj].end = ((jnums2[tj].end - jnums2[tj].start) > vis_num ) ? jnums2[tj].start + vis_num : jnums2[tj].end;\
				          px += x;\
				      }\
					  GT_CheckAndReNewDrawRanges2();

					  GT_TMP_CULL_ROW_OUT_RECT_STRID2

				      for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
			               t_col = text[ti]>>4;
                           t_row = text[ti]&0x0f; //ASCII编码规律
                           tu =(float)t_row * tu_w; tv = (float)t_col * tv_h;
						   _rng_data[id].Init(px,py,parm->width,parm->height,tu,tv,(tu+tu_w),(tv+tv_h),0,ti);
	                       px += parm->i_compartment;
		              }
		              py += parm->j_compartment;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
				}else if(mode == eFONT_FREETYPE){
				   float tu_w2 = tu_w * 0.5f;
				   for(tj=jnum0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(text_ofs_x + ( ww  - (jnums2[tj].end - jnums2[tj].start - 1) * i_comp)* parm->near_pointU); //排位计算公式

					     ofs_num = (short)(px / (int)i_comp); // --------------------------------------------------------- 踢除筐外行的字符编号
				         if(ofs_num < 0){
						     td =~ofs_num;
							 if((unsigned short)td + jnums2[tj].start >= jnums2[tj].end)jnums2[tj].start = jnums2[tj].end;
							 else{
								 fi = 0;   //中文的行踢除需这样检查一下 不然的话会间断中文字 但增加了一些开销
						         while((td+1) && text[td + jnums2[tj].start]&128 ){ ++fi; --td; } //向前番搜 以搜到英文或行前为止
						         ofs_num += (fi & 1);//中文的
						         jnums2[tj].start -= ofs_num;
					             jnums2[tj].end = ((jnums2[tj].end - jnums2[tj].start) > row_num2) ? jnums2[tj].start + row_num2 : jnums2[tj].end;
					             px += x - ofs_num * i_comp;
							 }
				         }else{
						     vis_num = row_num2 - ofs_num<0?0:row_num2 - ofs_num;
					         jnums2[tj].end = ((jnums2[tj].end - jnums2[tj].start) > vis_num)? jnums2[tj].start + vis_num : jnums2[tj].end;
						     px += x;
				         }
						 GT_CheckAndReNewDrawRanges2(); // ------------------------------------------------------------------------------------

					  for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
						  if(text[ti]&128){  //汉字
						      GT_FMT_GB2312_GET_UV;
							  _rng_data[id].Init(px,py,(font_w+font_w),font_h,tu,tv,(tu+tu_w),(tv+tv_h),tex_id,ti);
							  px += i_comp+i_comp;
							  ti++;
						  }else{ //汉字中的英文数字
						      t_col = text[ti]>>4;
                              t_row = text[ti]&0x0f;
                              tu = (float)t_row * tu_w; tv = (float)t_col * tv_h;
						      _rng_data[id].Init(px,py,font_w,font_h,tu,tv,(tu+(tu_w2)),(tv+tv_h),29,ti);
	                          px += i_comp;
						  }
		              }
		              py += j_comp;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
				}else if(mode == eFONT_NUMBER){
				   for(tj=jnum0; tj<jnum; ++tj){
					  row_hids_out[*row_count_out] = id;
		              px = (int)(text_ofs_x + ( ww  - (jnums2[tj].end - jnums2[tj].start - 1) * parm->i_compartment)* parm->near_pointU); //排位原的计算公式

					  GT_TMP_CULL_ROW_OUT_RECT_STRID2

				      for(ti = jnums2[tj].start; ti<jnums2[tj].end; ++ti,++id){
			               tu = (float)(((int)((uint8*)text)[ti]-48)*tu_w); /*纯数字编码规律*/
						   _rng_data[id].Init(px,py,parm->width,parm->height,tu,0.0f,(tu+tu_w),tv_h,0,ti);
	                       px += parm->i_compartment;
		              }
		              py += parm->j_compartment;
					  if(id != row_hids_out[*row_count_out])++(*row_count_out);
				   }
				}
			}
		}
	} //*********************************************** end if (!ww || !hh) ******************************************************************
    *data_out = _rng_data;
	return (int)id;
}
#endif //_GN_LAYOUT_H_