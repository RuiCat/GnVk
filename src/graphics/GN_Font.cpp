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
#include "GN_Def.h"
#ifdef GN_HAS_FREETYPE
#include "GN_FontFT.h"
#endif
#ifdef _GN_FONT_H_
GNg::sDRAW_ARRANGE::sDRAW_ARRANGE():zParame(GNg::ArrangeFmt),zFontMapFormat(GNg::eFONT_NUMBER),zDrawRngData(0),zRowNum(0){}

GN_API GNg::eFONT_MAP_FORMAT GNg::QueryFontMapFormat(sIMAGE *fontmap){
	float ratio = (float)(fontmap->width) / (float)(fontmap->height);
#ifdef GN_HAS_FREETYPE
	if(FT_FontSys && fontmap == FT_FontSys->GetAtlas()) return eFONT_FREETYPE;
#endif
	if(ratio > 0.1f && fontmap ->texnum >= 30)return eFONT_FREETYPE;
	else if(ratio > 4.0f && fontmap ->texnum == 1)return eFONT_NUMBER;
	else return eFONT_ASCII;
}
void GNg::sDRAW_ARRANGE::zClassifyFontMapFormat(sIMAGE *fontmap){
	float ratio = (float)(fontmap->width) / (float)(fontmap->height);
#ifdef GN_HAS_FREETYPE
	if(FT_FontSys && fontmap == FT_FontSys->GetAtlas()){
		zFontMapFormat = eFONT_FREETYPE;
		return;
	}
#endif
	if(ratio > 0.1f && fontmap ->texnum >= 30){
		if(!zParame.width )zParame.width  = fontmap->width >> 5;
        else if(zParame.width>1)zParame.width >>= 1;\
        if(!zParame.height)zParame.height = fontmap->height / 18;
	    zTu_w = fontmap->uRange / 16.0f;
		zTv_h = fontmap->vRange / 18.0f;
		zFontMapFormat = eFONT_FREETYPE;
	}else if(ratio > 4.0f && fontmap ->texnum == 1){
	    if(!zParame.width )zParame.width  = fontmap ->width / 10;  //没指定就使用默认宽度
	    if(!zParame.height)zParame.height = fontmap ->height;		 //没指定就使用默认高度
	    zTu_w = fontmap->uRange * 0.1f;
		zTv_h = fontmap->vRange;
		zFontMapFormat = eFONT_NUMBER;

	}else/* if(ratio > 0.9f && fontmap ->texnum ==1)*/{
        if(!zParame.width )zParame.width  = fontmap->width >> 4;
	    if(!zParame.height)zParame.height = fontmap->height >> 3;
	    zTu_w = fontmap->uRange / 16.0f;
		zTv_h = fontmap->vRange / 8.0f;
		zFontMapFormat = eFONT_ASCII;
	} 
}
void GNg::sDRAW_ARRANGE::zArrangeFmtEnsure(){
	if(  zParame.i_compartment)zParame.i_compartment += zParame.width;  //计算字的横跨度
	else zParame.i_compartment = (short)(zParame.width);
    if(  zParame.j_compartment)zParame.j_compartment += zParame.height; //计算字的竖跨度
	else zParame.j_compartment = (short)(zParame.height * 1.8f);
}
int GNg::sDRAW_ARRANGE::GetInRectOffsetTotalH(char *text, unsigned short count, int w, int h){
   #ifdef GN_DEBUG
    if(!w || !h || !text)GN_Log("调用 GetInRectOffsetTotalH 参数不能为0\n");
   #endif
	int vn = 0, enter_n = 1, sn, str_pitch;
	char *p_str = text, *p_str2 = text + (count?(unsigned int)count:(unsigned int)strlen(text));
    if(zParame.erect_print){
        str_pitch = zParame.j_compartment;
		sn = h / str_pitch;
		bool double_code = false;
        while(p_str < p_str2){
			if(*p_str == '\n'){ ++enter_n; vn = 0; }
			else if(zParame.auto_enter & eENTER_TOP_BOTTOM){
				if((*p_str) & 128){
					if(double_code){ ++vn; double_code = false; }
					else double_code = true;
				}else ++vn;
				if(vn > sn)++enter_n;
			}
            ++p_str;
		}
	}else{
	    str_pitch = zParame.i_compartment;
		sn = w / str_pitch;
		while(p_str < p_str2){
			if(*p_str == '\n'){ ++enter_n; vn = 0; }
			else if(zParame.auto_enter & eENTER_LEFT_RIGHT){
				++vn;
				if(vn > sn){
					++enter_n;
					vn = ((*p_str) & 128) ? 1 : 0;
				}
			}
            ++p_str;
		}
	}
	return (int)(zParame.erect_print?zParame.i_compartment:zParame.j_compartment) * enter_n;			
}
void GNg::sDRAW_ARRANGE::GetTextRange(sRECT *_range_out, int count, float ofs_u, float ofs_v){
	unsigned short max_len = 0, lid = 0, len;
	uint8 ri = 0, re = zRowNum - 1;
	for( ; ri < re; ++ri){ //统计好最长的一条字串是第几行且记录该行最大字数
	    len = zRowSIDs[ri+1] - zRowSIDs[ri];
		if(len > max_len){ 
			max_len = len; lid = ri;
	}	}
    len = count - zRowSIDs[ri];
	if(len > max_len){ 
			max_len = len; lid = ri;
	}
	unsigned short rsid = zRowSIDs[lid];
	if(zParame.erect_print){ //竖向翻转输出
		if(zParame.overturn_print){
			_range_out->left   = zDrawRngData[count-1].x + (int)((float)zDrawRngData->w * ofs_u);
			_range_out->right  = zDrawRngData->x + (int)((float)zDrawRngData->w * (1.0f + ofs_u));
		}else{				 //竖向输出
			_range_out->left   = zDrawRngData->x + (int)((float)zDrawRngData->w * ofs_u);
			_range_out->right  = zDrawRngData[count-1].x + (int)((float)zDrawRngData->w * (1.0f + ofs_u));
		}
		_range_out->top    = zDrawRngData[rsid].y + (int)((float)zDrawRngData->h * ofs_v);
		_range_out->bottom = zDrawRngData[rsid+max_len-1].y + (int)((float)zDrawRngData->h * (1.0f + ofs_v));
	}else{					 //横向翻转输出
	    if(zParame.overturn_print){
			_range_out->left   = zDrawRngData[rsid+max_len-1].x + (int)((float)zDrawRngData->w * ofs_u);
			_range_out->right  = zDrawRngData[rsid].x + (int)((float)zDrawRngData->w * (1.0f + ofs_u));
		}else{				 //横向输出
			_range_out->left   = zDrawRngData[rsid].x + (int)((float)zDrawRngData->w * ofs_u);
			_range_out->right  = zDrawRngData[rsid+max_len-1].x + (int)((float)zDrawRngData->w * (1.0f + ofs_u));
		}
		_range_out->top    = zDrawRngData->y + (int)((float)zDrawRngData->h * ofs_v);
	    _range_out->bottom = zDrawRngData[count-1].y + (int)((float)zDrawRngData->h * (1.0f + ofs_v));
	}
}
void GNg::sDRAW_ARRANGE::GetText(char *text_out, int count){
	sDRAW_RANGE	*rngs = zDrawRngData;
	float u_inv = 1.0f / zTu_w, v_inv;
	int i = 0;
	switch(zFontMapFormat){
	case eFONT_NUMBER:
		for(; i < count; ++i){
			text_out[i] = (char)(rngs[i].u1 * u_inv) + '0';
		}
		text_out[i] = 0;
		break;
	case eFONT_ASCII:
		v_inv = 1.0f / zTv_h;
		for(; i < count; ++i){
			text_out[i] = ((uint8)(rngs[i].v1 * v_inv)<<4)|(uint8)(rngs[i].u1 * u_inv);
		}
		text_out[i] = 0;
		break;
	case eFONT_FREETYPE:
		v_inv = 1.0f / zTv_h;
		uint8 t_col, t_col2;
		int ti = 0;
		for(; i < count; ++i){
		  if(rngs[i].tid == 29)
			text_out[ti++] = ((uint8)(rngs[i].v1 * v_inv)<<4)|(uint8)(rngs[i].u1 * u_inv);
		  else{
			t_col = (uint8)(rngs[i].v1 * v_inv);
			t_col2 = t_col / 6;
			text_out[ti+1] = (((t_col-t_col2*6)<<4)|(uint8)(rngs[i].u1 * u_inv)) + 0xa0;
			text_out[ti  ] = t_col2 + rngs[i].tid * 3 + 0xa1;
			ti += 2;
		  }
		}
		text_out[ti] = 0;
		break;
	}
}
/************************************ 类 cTEXT_ART **********************************************/
 GNg::cTEXT_ART::cTEXT_ART():
    zpFontMap(&Image),
	zTextCount(0),
	zDrawRngCount(0)
{}
void GNg::cTEXT_ART::Release(){
	sDRAW_ARRANGE::Release();
	zDrawRngCount = 0;
    zTextCount = 0;
}
void GNg::cTEXT_ART::CopyData(cTEXT_ART &src){
	src.Release();
	this->Release();
	*this = src;
}
#ifdef GN_HAS_FREETYPE
bool GNg::cTEXT_ART::SetFreeTypeFont(int fontSizePx) {
	if(!FT_LoadSystemFont(fontSizePx)) return false;
	zpFontMap = FT_FontSys->GetAtlas();
	zFontMapFormat = eFONT_FREETYPE;
	// 从 FreeType 获取度量信息
	zParame.width  = (short)(FT_FontSys->GetFontSize());
	zParame.height = (short)(FT_FontSys->GetLineHeight());
	zTu_w = 1.0f; zTv_h = 1.0f;  // UV 由 FT_ComputeArrangeData 直接设置
	zArrangeFmtEnsure();
	return true;
}
#endif
void GNg::cTEXT_ART::Init(sIMAGE *font_map,sARRANGE_FMT *format_parame, eFONT_MAPLACK_SHOW_MODE lack_type){
	this->Release();
	if(font_map)zpFontMap = font_map;
	else{
		switch(lack_type){
	   #ifdef _GN_RUNINFO_H_
		case eFONT_MAPLACK_SHOW_DEFAULT: zpFontMap = GN::RunRec.aFont.GetFontMap(); break;
       #endif
		case eFONT_MAPLACK_SHOW_CUBE: zpFontMap = &Image; break;
		case eFONT_MAPLACK_SHOW_NULL: zpFontMap = 0;	  break;
		}
	}
	zParame = *format_parame;
    zClassifyFontMapFormat(zpFontMap);
	zArrangeFmtEnsure();
}
void GNg::cTEXT_ART::TextOutNumber(unsigned int number, int x,int y,int w,int h, unsigned int color,unsigned short bmode){
    char text[12];
	unsigned short ti;
	float tuv;                 //纹理坐标
	int px = x, py = y, step1; //光标当前处
    unsigned short t_count;
	t_count = sprintf(text,"%d",number);
	zTextCount = t_count;

	if(!w || !h){  // 一般简单模式处理 *******************************************************************************************
		if(zParame.erect_print){
			if(zParame.overturn_print){ // 竖方向翻转输出
	            py = (int)(y + (zParame.j_compartment * t_count - zParame.j_compartment + zParame.height)* zParame.near_pointV);
			    step1 = -zParame.j_compartment;
			}else{                      // 竖方向顺着输出
		        py = (int)(y - (zParame.j_compartment * t_count - zParame.j_compartment + zParame.height)* zParame.near_pointV);
				step1 =  zParame.j_compartment;
			}
		   #define GT_TMP_TEXT_OUT_NUMBER( W , H , NEXT ) \
		    for(ti=0; ti<t_count; ++ti){\
		        tuv = (float)((text[ti]-'0')*zTu_w); /*纯数字编码规律*/\
		        GNg::ImageDraw(zpFontMap,px,py, W , H ,color,0,bmode,tuv,0.0f,tuv+zTu_w,zTv_h);\
                NEXT ;\
		    }
			GT_TMP_TEXT_OUT_NUMBER( zParame.width , zParame.height , py += step1 )
		}else{
			if(zParame.overturn_print){ // 横方向翻转输出
	            px = (int)(x + (zParame.i_compartment * t_count - zParame.i_compartment + zParame.width)* zParame.near_pointU);
				step1 = -zParame.i_compartment;
			}else{                      // 横方向顺着输出
			    px = (int)(x - (zParame.i_compartment * t_count - zParame.i_compartment + zParame.width)* zParame.near_pointU);
		        step1 =  zParame.i_compartment;
			}
			GT_TMP_TEXT_OUT_NUMBER( zParame.width , zParame.height , px += step1 )
		}
	}else{       // 填满筐模式处理 ***********************************************************************************************

		float ratio_w = 0.8f, ratio_h = 0.6f, ofs = 0.0f;
		int   i_comp = 0, j_comp = 0, font_w = w, font_h = h;
		if(zParame.erect_print){
			ratio_h = (float)(zParame.height) / (float)(zParame.j_compartment);
            j_comp = (int)((float)h / ((float)t_count-1.0f + ratio_h)); //按比例来计算出高度和竖跨度
			if(t_count < 3){
				j_comp = (int)((float)j_comp * (float)t_count * 0.4f);
			    ofs = (h - j_comp * (t_count-1+ ratio_h)) * 0.5f;
			}
			font_h = (int)(j_comp * ratio_h);
			if(zParame.overturn_print){ // 竖方向翻转输出
		        py = (int)(y+h-font_h-ofs);
		        j_comp = -j_comp;
			}else{                      // 竖方向顺着输出
		        py = (int)(y+ofs);
			}
			GT_TMP_TEXT_OUT_NUMBER( font_w , font_h , py += j_comp )
		}else{
			ratio_w = (float)(zParame.width) / (float)(zParame.i_compartment);
            i_comp = (int)((float)w / ((float)t_count-1.0f + ratio_w)); //按比例来计算出宽度和横跨度
			if(t_count < 3){
                i_comp = (int)((float)i_comp * (float)t_count * 0.4f);
				ofs = (w - i_comp * (t_count-1+ ratio_w)) * 0.5f;
			}
		    font_w = (int)(i_comp * ratio_w);
			if(zParame.overturn_print){ // 横方向翻转输出
		        px = (int)(x+w-font_w-ofs);
		        i_comp = -i_comp;
			}else{                      // 横方向顺着输出
		        px = (int)(x+ofs);
			}
			GT_TMP_TEXT_OUT_NUMBER( font_w , font_h , px += i_comp )
		}
	}
}
int GNg::cTEXT_ART::SetText(char *text,unsigned short count,int x,int y,int w,int h,int text_offset_x,int text_offset_y){
#ifdef GN_HAS_FREETYPE
	if(FT_FontSys && FT_FontSys->GetAtlas() &&
	   (zFontMapFormat == eFONT_FREETYPE || zpFontMap == FT_FontSys->GetAtlas())) {
		zTextCount = GNg::FT_ComputeArrangeData(&zDrawRngData, zDrawRngCount,
		                           text, count,
								   x,y,w,h, text_offset_x,text_offset_y,
			                       &zParame, zRowSIDs, &zRowNum, &zTotRngW,&zTotRngH);
	} else
#endif
	zTextCount = GNg::ComputeArrangeRangeData(&zDrawRngData, zDrawRngCount,
		                           text, count,
								   x,y,w,h, text_offset_x,text_offset_y,
			                       &zParame,zFontMapFormat, zTu_w,zTv_h,
								   zRowSIDs, &zRowNum, &zTotRngW,&zTotRngH);

	if(zTextCount > (zDrawRngCount >> 2))zDrawRngNST = GN::TimeGetRunCount();
	else if(GN::TimeGetRunCount() - zDrawRngNST > 3000)zDrawRngCount = 0;

	zFx = x; zFy = y; zFw = w; zFh = h;
	return zTextCount;
}
int GNg::cTEXT_ART::SetText(bool in_rect, GNu::eNEAR_TYPE near_type, char *text,unsigned short count, int x,int y,int w,int h){
    int text_count = SetText(text, 0, 0, 0); //先计算好文本显示排位和范围宽高先
	if(!text_count)return 0;
	int _x, _y;
	int  cx = x + (w >> 1),
		 cy = y + (h >> 1); //计算范围中心
	
	if(in_rect){ //在格筐内显示 ----------------------------------------------
		zFx = x; zFy = y; zFw = w; zFh = h; //指定文本显示剪裁区划
		switch(near_type){
		  case GNu::eNEAR_LEFT:
            if(zParame.overturn_print && zTotRngW > w)x -= zTotRngW - w;
			_y = cy - (zTotRngH >> 1);
            if(_y > y)y = _y;
			break;
		  case GNu::eNEAR_RIGHT:
			_x = w - zTotRngW + x;
            if(_x > x)x = _x;
			else if(zParame.overturn_print)x -= zTotRngW - w;
			_y = cy - (zTotRngH >> 1);
            if(_y > y)y = _y;
			break;
		  case GNu::eNEAR_BOTTOM:
			_y = h - zTotRngH + y + (zParame.j_compartment - zParame.height);
			if(_y > y)y = _y;
		  case GNu::eNEAR_TOP:
			_x = (cx - (zTotRngW >> 1));
			if(_x > x)x = _x;
			else if(zParame.overturn_print)x -= zTotRngW - w;
			break;
		}
	}else{		 //在格筐外显示 ----------------------------------------------
	    switch(near_type){
		  case GNu::eNEAR_LEFT:
			x = x - zTotRngW - (w >> 4);
			y = cy - (zTotRngH >> 1);
			break;
		  case GNu::eNEAR_RIGHT:
			x = x + w + (w >> 4);
			y = cy - (zTotRngH >> 1);
			break;
		  case GNu::eNEAR_TOP:
			y = y - zTotRngH + (zParame.j_compartment - zParame.height);
			x = cx - (zTotRngW >> 1);
			break;
		  case GNu::eNEAR_BOTTOM:
			y = y + h;
			x = cx - (zTotRngW >> 1);
			break;
		}
	} //----------------------------------------------------------------------
    int ox = x - zDrawRngData->x, oy = y - zDrawRngData->y; //计算出位置差
	sDRAW_RANGE *_rng_data = zDrawRngData, *rng_end = zDrawRngData + zTextCount;
	for(; _rng_data < rng_end; ++_rng_data){
	    _rng_data->x += ox;   //再偏移已计算好的显示范围
        _rng_data->y += oy;
	}
	return text_count;
}
void GNg::cTEXT_ART::Show(unsigned int  color,unsigned short bmode){
	sIMAGE *fontMap = zpFontMap;
#ifdef GN_HAS_FREETYPE
	if(FT_FontSys && FT_FontSys->GetAtlas() &&
	   (zFontMapFormat == eFONT_FREETYPE || zpFontMap == FT_FontSys->GetAtlas()))
		fontMap = FT_FontSys->GetAtlas();
#endif
	if(zFw && zFh && !zParame.rect_fill){
		ViewCur->ScissorRectSetup(zFx, zFy, zFw, zFh);
        ImageDrawMP(fontMap, zDrawRngData, zTextCount, color, bmode);
		ViewCur->ScissorRectRemove();
	}else
	    ImageDrawMP(fontMap, zDrawRngData, zTextCount, color, bmode);
}
int GNg::cTEXT_ART::OutText(char *text,unsigned short count,int x,int y,int w,int h,unsigned int color,unsigned short bmode,int text_offset_x,int text_offset_y){
    count = SetText(text,count, x,y,w,h, text_offset_x,text_offset_y);
    Show(color, bmode);
	return count;
}
bool GNg::cTEXT_ART::GetVIDfromText(unsigned short buf_id, unsigned short *str_id_out, uint8 *row_id_out){
   #ifdef GN_DEBUG
	if(!str_id_out || !row_id_out)GN_Log("调用 cTEXT_ART::GetVIDfromText 指针参数不能为空!\n");
   #endif
	sDRAW_RANGE *_dw_rng = zDrawRngData;
	*row_id_out = 0; *str_id_out = 0;
	if(!_dw_rng || !zTextCount || buf_id < _dw_rng->strid || buf_id > _dw_rng[zTextCount-1].strid)return false;
	int id = (int)(zRowNum - 1), id2, end_id = zTextCount;
	for(; id >= 0; --id){				 //从显示文字的尾行开始向前找
		*row_id_out = (uint8)id;
		if(buf_id >= _dw_rng[zRowSIDs[id]].strid){
			for(id2 = (int)zRowSIDs[id]; id2 < end_id; ++id2){  //确定在这行内就从这行第一个显示文字开始向后找
				*str_id_out = id2;
				if(buf_id == _dw_rng[id2].strid)return true;   //返回找到可视文字
			}
		    return false;
		}
        end_id = (int)zRowSIDs[id];
	}
	return false;
}
bool GNg::cTEXT_ART::GetVIDfromCoord(int px,int py, unsigned short *str_id_out, uint8 *row_id_out, sDRAW_RANGE *info_out){
   #ifdef GN_DEBUG
	if(!info_out || !str_id_out || !row_id_out)GN_Log("调用 cTEXT_ART::GetVIDfromCoord 失败! 指针参数不能为空!\n");
   #endif
	sDRAW_RANGE *_dw_rng = zDrawRngData;
	if(!zRowNum || !_dw_rng)return false;
	const uint8 row_eid = zRowNum - 1;
	const unsigned short teid = zTextCount - 1;
	unsigned short sid1 = 0, sid2 = teid;
	int row = 0;
	if(zParame.erect_print){
		int row_w  = _dw_rng->w,
			row_x1 = _dw_rng->x,
		    row_x2 = _dw_rng[zRowSIDs[row_eid]].x;

		if(zFontMapFormat == eFONT_FREETYPE){ //中文里的英文数字宽都扩充到和中文字一样
			if(_dw_rng->tid == 29){
				row_w <<= 1; //占半格的英文数字
		        row_x1 -= (row_w >> 2);
			}
			if(_dw_rng[zRowSIDs[row_eid]].tid == 29)
		        row_x2 -= (row_w >> 2);
		}
		if(zParame.overturn_print){ //竖方向翻转输出
			if(px <= row_x2){
			   #define GT_TMP_TEXT_GET_VID_RETURN_LAST \
			    *str_id_out = teid;\
                *row_id_out = row_eid; /*鼠标不在文本范围内 返回尾字符ID*/\
				*info_out = _dw_rng[teid];\
			    return false;

				GT_TMP_TEXT_GET_VID_RETURN_LAST
			}
			if(px >= row_x1 + row_w){
			   #define GT_TMP_TEXT_GET_VID_RETURN_FRIST \
				*str_id_out = 0;/*鼠标不在文本范围内 返回首字符ID*/\
                *row_id_out = 0;\
				*info_out = _dw_rng[0];\
				return false;

				GT_TMP_TEXT_GET_VID_RETURN_FRIST
			}
			if(zRowNum > 1){
			    row = (row_x1 + row_w - px) / ((row_x1 - row_x2) / (int)row_eid);
			    sid1 = zRowSIDs[row]; /*计算从第几行与第几字开始检索*/
		        if(row < (int)row_eid)sid2 = zRowSIDs[row+1] - 1; /*计算检索到第几行与第几字结束*/
		    }
		}else{                      //竖方向顺着输出
			if(px <= row_x1){
				GT_TMP_TEXT_GET_VID_RETURN_FRIST
			}
			if( px >= row_x2 + row_w){
				GT_TMP_TEXT_GET_VID_RETURN_LAST
			}
			if(zRowNum > 1){
			   #define GT_TMP_TEXT_CPT_SREACH_RANGE( X ) \
			    row = (p##X - row_##X##1) / ((row_##X##2 - row_##X##1) / (int)row_eid);\
			    sid1 = zRowSIDs[row]; /*计算从第几行与第几字开始检索*/\
		        if(row < (int)row_eid)sid2 = zRowSIDs[row+1] - 1; /*计算检索到第几行与第几字结束*/

				GT_TMP_TEXT_CPT_SREACH_RANGE( x )
		    }
		}
        /*if(px > _dw_rng[sid1].x + _dw_rng[sid1].w)return false;*/ //鼠标不在文本范围内
		if(py < _dw_rng[sid1].y){
		   #define GT_TMP_TEXT_GET_VID_RETURN_( SID ) \
		    *str_id_out = SID ;\
			*row_id_out = (uint8)row;\
            *info_out = _dw_rng[ SID ];
		    
			GT_TMP_TEXT_GET_VID_RETURN_( sid1 )
			return false;
		}
		if(py > _dw_rng[sid2].y + _dw_rng[sid2].h){
			GT_TMP_TEXT_GET_VID_RETURN_( sid2 )
			return false;
		}
		int idi = (sid2 == sid1) ? sid1 : ((py - _dw_rng[sid1].y) / ((_dw_rng[sid2].y - _dw_rng[sid1].y) / (sid2 - sid1)) + sid1);
		GT_TMP_TEXT_GET_VID_RETURN_( idi )
        return true;
	}else{
		int row_y1 = _dw_rng->y,
			row_y2 = _dw_rng[zRowSIDs[row_eid]].y;

		if(py < row_y1){
			GT_TMP_TEXT_GET_VID_RETURN_FRIST
		}
		if(py > row_y2 + _dw_rng[zRowSIDs[row_eid]].h){
			GT_TMP_TEXT_GET_VID_RETURN_LAST
		}
		if(zRowNum > 1){
			GT_TMP_TEXT_CPT_SREACH_RANGE( y )
		}
        /*if(py > _dw_rng[sid1].y + _dw_rng[sid1].h)return false;*/ //鼠标不在文本范围内
		
		//深入检索行里被点中的字符ID
		unsigned short &rid1 = zParame.overturn_print ? sid2 : sid1;
		unsigned short &rid2 = zParame.overturn_print ? sid1 : sid2;
		if(px < _dw_rng[rid1].x){
            GT_TMP_TEXT_GET_VID_RETURN_( rid1 )
			return false;
		}\
		if(px > _dw_rng[rid2].x + _dw_rng[rid2].w){
            GT_TMP_TEXT_GET_VID_RETURN_( rid2 )
			return false;
		}
		unsigned short idi;
		for(;;){   //采用二分查找法检索
		    idi = (sid2 - sid1) >> 1;
		    if(idi){
			    idi += sid1;
			    if(px < _dw_rng[idi].x)rid2 = idi;
			    else rid1 = idi;
		    }else{ //不能再分了
			    idi = rid2;
		        if(px < _dw_rng[idi].x)idi = rid1;
				break;
		    }
		}
        GT_TMP_TEXT_GET_VID_RETURN_( idi )
		return true;
	}
}
void GNg::cTEXT_ART::GetTextShowRange(short *x, short *y, short *w, short *h){
	if(zTextCount){
		sDRAW_RANGE *_dw_rng = zDrawRngData;
		*x = _dw_rng->x; *y = _dw_rng->y;
		*w = zTotRngW,   *h = zTotRngH;
		 if(zParame.overturn_print)*x += _dw_rng->w - zTotRngW;
		if(zFw || zFh){
			if(*x < (short)zFx)*x = zFx;
			if(*y < (short)zFy)*y = zFy;
			short crs = (*x + *w) - (zFx + zFw);
			if(crs > 0)*w -= crs;
                  crs = (*y + *h) - (zFy + zFh);
            if(crs > 0)*h -= crs;
		}
	}else{
	    *x = 0; *y = 0;
		*w = 0; *h = 0;
	}
}
/************************************************ cTEXT_ARTEX ****************************************************************/
unsigned int GNg::GT_sTEXT_LABLE_RANGE::CallLableProc(){
	uint8 locbuf[8192];
	GN::sTBUF_INT tbuf(locbuf, 8192);
	char *com_lable = (char*)tbuf.GetSpace(pLable->lableStr.Num() + extStr.Num());
	strcpy(com_lable, pLable->lableStr);
    if(extStr)strcat(com_lable, extStr);
	return pLable->onCpProc(com_lable, start, end);
}
void GNg::cTEXT_ARTEX::LableSet(const char *lable_str, bool _absolute, sCOLOR c, 
							 unsigned int (*on_cp_proc)(const char *str, unsigned short start_vid, unsigned short end_vid))
{
	zLables.push_back( GT_sTEXT_LABLE_STR(lable_str, _absolute , c , on_cp_proc ) );
}
void GNg::cTEXT_ARTEX::LableSegmentGen(const char *text){
   #ifdef GN_DEBUG
    if(!text)GN_Log("调用 cTEXT_ART::LableSegmentGen 参数不能为空!\n");
   #endif
	zLableRanges.clear();
    if(zLables.empty())return;
	GN::sLIST<GT_sTEXT_LABLE_STR>::iterator itlable = zLables.begin(), etlable = zLables.end();
	GT_sTEXT_LABLE_STR *_lable;
	char *pstr, *pstr2, *__lable;
	bool is_absolu;
	unsigned int lable_len;
	unsigned short r_st, r_end;
	for(; itlable != etlable; ++itlable){ //检索文本里对应的所有标签首尾位置
		_lable = &(*itlable);
        __lable = _lable->lableStr;		  //取出这些属性以使处理更快
        lable_len = _lable->lableStr.Num() - 1;
		is_absolu = _lable->absolute;
        pstr = (char*)text;
		while(*pstr){
		    if(*pstr == *__lable){
				if(strncmp(pstr, __lable , lable_len))++pstr;
				else{
                    r_st = (unsigned short)(pstr - text);
					pstr += lable_len;
					if(is_absolu){
					     r_end = (unsigned short)(pstr - text) - 1; //记录标签的首尾在文本的什么位置
					     zLableRanges.push_back(GT_sTEXT_LABLE_RANGE(_lable, r_st, r_end));
					}else{
						 pstr2 = pstr;
						 while(!isspace(*pstr))++pstr;  //跳过这段可视字符
						 
						 r_end = (unsigned short)(pstr - text) - 1; //记录标签的首尾在文本的什么位置
					     zLableRanges.push_back(GT_sTEXT_LABLE_RANGE(_lable, r_st, r_end));
						 
						 char ch = *pstr;  *pstr = 0;
						 zLableRanges.back().extStr(pstr2, 0); //复制相对标签名多余部分的字符进去
                         *pstr = ch;
					}
					if(!(*pstr))break;
				}
		    }else ++pstr;
		}
	}
}
int GNg::cTEXT_ARTEX::SetText(char *text,unsigned short count, int x,int y,int w,int h, int offset_x,int offset_y){
    cTEXT_ART::SetText(text,count, x,y,w,h, offset_x,offset_y);
	if(zLableRanges.empty())return zTextCount;

    GN::sLIST<GT_sTEXT_LABLE_RANGE>::iterator itlable = zLableRanges.begin(), etlable = zLableRanges.end();
    GT_sTEXT_LABLE_RANGE *_lable_rng;
	sDRAW_RANGE *_dw_rng = zDrawRngData;
	int teid = zTextCount - 1;
	unsigned short i, i1, i2, r_st, r_end, r_se, last_strid = _dw_rng[teid].strid;
    uint8 j, j1, j2,  row_eid = zRowNum - 1;
	for(; itlable != etlable; ++itlable){ //查找标签在文本的首尾位置对应显示在那个可视字符里
        _lable_rng = &(*itlable);
		r_st = _lable_rng->start, r_end = _lable_rng->end;
		if(r_st > last_strid || r_end < _dw_rng->strid){
			_lable_rng->startVid = 0xff; 
			_lable_rng->endVid	 = 0xff; //超出显示范围
			continue; 
		}
		r_se = r_end - r_st;
		j1 = 0, j2 = row_eid;
		GN_FindNearDataID( r_st < _dw_rng[zRowSIDs[j]].strid , j1 , j2 , j )
		_lable_rng->startRow = j;
		
		i1 = zRowSIDs[j],  i2 = (unsigned short)(((zRowNum-j) > 1)? (zRowSIDs[j+1]-1) : teid);
        GN_FindNearDataID( r_st < _dw_rng[i].strid , i1 , i2 , i )
		_lable_rng->startVid = i;
		
		i1 = i,  i2 = (i + r_se >= (unsigned short)zTextCount) ? (unsigned short)teid :( i + r_se);
        GN_FindNearDataID( r_end < _dw_rng[i].strid , i1 , i2 , i )
        _lable_rng->endVid = i;
	}
	return zTextCount;
}
void GNg::cTEXT_ARTEX::Show(unsigned int color,unsigned short blendmode){
	cTEXT_ART::Show(color, blendmode);
	if(zLableRanges.empty())return;
	bool us_scissor = !zParame.rect_fill && zFw && zFh, on_cp;
	if(us_scissor)ViewCur->ScissorRectSetup(zFx,zFy,zFw,zFh);

    GN::sLIST<GT_sTEXT_LABLE_RANGE>::iterator itlable = zLableRanges.begin(), etlable = zLableRanges.end();
    GT_sTEXT_LABLE_RANGE *_lable_rng;
	sDRAW_RANGE *_dw_rng = zDrawRngData, *_dw_rng1;
	sDRAW_RANGE  *_arrg, *_arrg2;
	sARRANGE_FMT *_fmt = &zParame;
	unsigned int  lable_cc;
	uint8  min_row_id, max_row_id , rj;
	unsigned short min_id, max_id, ri, *row_sids = zRowSIDs;
	int rx,ry,rw,rh, msmx = ViewCur->mouseMX, msmy = ViewCur->mouseMY;
    for(; itlable != etlable; ++itlable){
        _lable_rng = &(*itlable);
		min_id = _lable_rng->startVid; max_id = _lable_rng->endVid;
		if(min_id == 0xff /*&& max_id == 0xff*/)continue;

		lable_cc = _lable_rng->pLable->color;
		if(_lable_rng->pLable->onCpProc){   //鼠标拾取和回调处理
			if((_fmt->erect_print ? (_dw_rng[max_id].x - _dw_rng[min_id].x == 0) : (_dw_rng[max_id].y - _dw_rng[min_id].y == 0))){ //单行显示
                _arrg  = _dw_rng + min_id;
                _arrg2 = _dw_rng + max_id;  //检测鼠标指针是否落在标签范围
                if(_fmt->erect_print)		on_cp = (msmx >= _arrg->x)  && (msmx <= _arrg->x  + _arrg->w ) && (msmy >= _arrg->y) && (msmy <= _arrg2->y + _arrg2->h);
				else{
				    if(_fmt->overturn_print)on_cp = (msmx >= _arrg2->x) && (msmx <= _arrg->x  + _arrg->w ) && (msmy >= _arrg->y) && (msmy <= _arrg->y  + _arrg->h);
					else                    on_cp = (msmx >= _arrg->x)  && (msmx <= _arrg2->x + _arrg2->w) && (msmy >= _arrg->y) && (msmy <= _arrg->y  + _arrg->h);
				}
				if(on_cp)lable_cc = _lable_rng->CallLableProc();

			}else{ //多行显示
				min_row_id = _lable_rng->startRow;
				_dw_rng1 = _dw_rng + row_sids[min_row_id+1];
				if(_fmt->erect_print){
					if(_fmt->overturn_print)max_row_id = (_dw_rng[min_id].x - _dw_rng[max_id].x) / (_dw_rng[min_id].x - _dw_rng1->x) + min_row_id;
					else                    max_row_id = (_dw_rng[max_id].x - _dw_rng[min_id].x) / (_dw_rng1->x - _dw_rng[min_id].x) + min_row_id;
				}else                       max_row_id = (_dw_rng[max_id].y - _dw_rng[min_id].y) / (_dw_rng1->y - _dw_rng[min_id].y) + min_row_id;
			    ++max_id;
				GT_DrawTextSelectRange( (_arrg2->tid != 29) , (_arrg->tid != 29) ,\
					if(msmx >= rx && msmx <= (rx + rw) && msmy >= ry && msmy <= (ry + rh))lable_cc = _lable_rng->CallLableProc();\
				)
                --max_id;
			}
		}
		ImageDrawMP(zpFontMap, _dw_rng + min_id, max_id - min_id + 1, lable_cc , blendmode); //用不同颜色重绘一次标签的字符
	}
	if(us_scissor)ViewCur->ScissorRectRemove();
}
#endif // _GN_FONT_H_
