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
#ifdef _GN_LINEAR_H_
GN::sLIST<GT_sVALUE_CHANGE_BASE*> GT_sVALUE_CHANGE_BASE::valueProcList;

GT_sVALUE_CHANGE_CR::GT_sVALUE_CHANGE_CR(unsigned int  *change_value, unsigned int  target_value, float cross_range, float linear_sheep, void (* end_porc)(void*), void *us_parame)
:endPorc(end_porc),usParame(us_parame),crossRange(cross_range),linearSheep(linear_sheep),pValue(change_value),tgValue(target_value){
	tgValueA = (float)((target_value & 0xff000000)>>24);
	tgValueR = (float)((target_value & 0x00ff0000)>>16);
	tgValueG = (float)((target_value & 0x0000ff00)>> 8);
	tgValueB = (float)((target_value & 0x000000ff)    );
	unsigned int  cr = *change_value;
	lsValueA = (float)((cr			 & 0xff000000)>>24);
	lsValueR = (float)((cr			 & 0x00ff0000)>>16);
	lsValueG = (float)((cr			 & 0x0000ff00)>> 8);
	lsValueB = (float)((cr			 & 0x000000ff)    );
	if(!endPorc && usParame)memset(usParame, 1, 1);
}
bool GT_sVALUE_CHANGE_CR::ValueChange(){
	float lnrsp = linearSheep * GN::TimeRatioL;
    if(lnrsp > 1.0f)lnrsp = 1.0f;
	lsValueA += (tgValueA - lsValueA) * lnrsp;
	lsValueR += (tgValueR - lsValueR) * lnrsp;
	lsValueG += (tgValueG - lsValueG) * lnrsp;
	lsValueB += (tgValueB - lsValueB) * lnrsp;
    *pValue = ((unsigned int)lsValueA<<24)|((unsigned int)lsValueR<<16)|((unsigned int)lsValueG<<8)|((unsigned int)lsValueB);
	if((tgValueA > lsValueA ? tgValueA - lsValueA : lsValueA - tgValueA) < 1.0f
	 &&(tgValueR > lsValueR ? tgValueR - lsValueR : lsValueR - tgValueR) < 1.0f
	 &&(tgValueG > lsValueG ? tgValueG - lsValueG : lsValueG - tgValueG) < 1.0f
	 &&(tgValueB > lsValueB ? tgValueB - lsValueB : lsValueB - tgValueB) < 1.0f
	){
        * pValue = tgValue;
	    if(endPorc)endPorc(usParame);
		else if(usParame)memset(usParame, 0, 1);
		  return true;
	}else return false;
}
GN_API void GN_ValueChangeUpdateAll(){
	GN::sLIST<GT_sVALUE_CHANGE_BASE*>::iterator iv = GT_sVALUE_CHANGE_BASE::valueProcList.begin(), ev = GT_sVALUE_CHANGE_BASE::valueProcList.end();
	for(; iv != ev; ){
		if((*iv)->ValueChange()){
            delete (*iv);
			iv = GT_sVALUE_CHANGE_BASE::valueProcList.erase(iv);
		}else ++iv;
	}
}
GN_API void GN_ValueChangeClearAll(){
	GN_DeleteListP( GT_sVALUE_CHANGE_BASE , GT_sVALUE_CHANGE_BASE::valueProcList );
}
/*-------------------------------------------------------------------------------------------------------------------------------------------------------------*/
GN_API unsigned int  GNg::LinearColorMultiplyRatio(unsigned int start,unsigned int end,unsigned int tmtime,unsigned int endtime,unsigned int ratio_color){
	unsigned int temp;
	uint8 *ps = (uint8*)&start, *pe = (uint8*)&end, *pt = (uint8*)&temp, *pr = (uint8*)&ratio_color;

	#define GT_TMP_COLOR_MULTIPLY_RATIO( I , RT ) \
		if(pe[ I ]>ps[ I ]) \
		    pt[ I ] = (ps[ I ] + (pe[ I ]-ps[ I ]) * tmtime/endtime) RT ;\
		else\
            pt[ I ] = (ps[ I ] - (ps[ I ]-pe[ I ]) * tmtime/endtime) RT ;
	
	GT_TMP_COLOR_MULTIPLY_RATIO( 0 , *pr[0]/255 )
	GT_TMP_COLOR_MULTIPLY_RATIO( 1 , *pr[1]/255 )
	GT_TMP_COLOR_MULTIPLY_RATIO( 2 , *pr[2]/255 )
	GT_TMP_COLOR_MULTIPLY_RATIO( 3 , *pr[3]/255 )
	return temp;
}
GN_API unsigned int  GNg::LinearColorMultiplyAlphaRatio(unsigned int start,unsigned int end,unsigned int tmtime,unsigned int endtime, float ratio){
   #define GT_TMP_LINEAR_COLOR \
	unsigned int temp;\
	uint8 *ps = (uint8*)&start, *pe = (uint8*)&end, *pt = (uint8*)&temp;\
	for(uint8 pi=0;pi<4; ++pi){\
		GT_TMP_COLOR_MULTIPLY_RATIO( pi , ) \
	}
	GT_TMP_LINEAR_COLOR
	pt[3] = (uint8)((float)pt[3] * ratio);
    return temp;
}
GN_API unsigned int  GNg::LinearColor(unsigned int start,unsigned int end,unsigned int tmtime,unsigned int endtime){
    GT_TMP_LINEAR_COLOR
	return temp;
}
GN_API float GNg::SLinear(float start,float end, unsigned int tmtime,unsigned int endtime, float value,unsigned short mode){
	float tm_t = (float)tmtime, end_t = (float)endtime;
	float end_inv = 1.0f / end_t, se = end - start; //距离
	if(value > 0.0f){
		if(value >= 1.0f)value = 0.999999f;
	    float tm_1 = 0.0f, var = (float)(endtime>>1)*value, var2 = end_t*(1-value);
		if(mode & L_START){
			if(tm_t > var){ 
				tm_1 = tm_t - var;
				tm_t = var;
			}
			if(!(mode & L_DOUBLE)){  //到结尾锁死
				if(tm_1 > var2)tm_1 = var2;
			} 
            return start + se*tm_t*end_inv*2.0f + se*tm_1*end_inv*(2.0f- tm_1/var2);

	    }else if(mode & L_END){
		    float etv = end_t - var;
			if(tm_t > etv){ 
				tm_1 = tm_t - etv;
				tm_t = etv; 
			}
			if(!(mode & L_DOUBLE)){  //到结尾锁死
				if(tm_t < var)tm_t = var;
			}  
			return start + se*(tm_t-var)*end_inv*((tm_t-var)/var2) + se*tm_1*end_inv*2.0f;
	    }else{ //mode == L_CENTRE
		    float tm_2 = 0.0f, v1122 = (var + var2) * 0.5f,  v12 = var * 0.5f, v22 = var2 * 0.5f;
		    if(tm_t > v1122){ 
				tm_1 = tm_t - v1122;
				tm_t = v1122; 
			}
			if(tm_1 > var){ 
				tm_2 = tm_1 - var;
				tm_1 = var;
			}
			if(!(mode & L_DOUBLE)){   
				if(tm_t < v12)tm_t = v12; 
				if(tm_2 > v22)tm_2 = v22;
			}
			return start + se*(tm_t-v12)*end_inv*((tm_t-v12)/v22) + se*tm_1*end_inv*2.0f + se*tm_2*end_inv*(2.0f-tm_2/var2*2.0f);
	    }
	}else{
		if	 (mode & L_START){
			float tm_t_einv = tm_t * end_inv;
			return start + se*tm_t_einv * (2.0f-tm_t_einv);
		}else if(mode & L_END){
			float tm_t_einv = tm_t * end_inv;
			return start + se*tm_t_einv * (	 tm_t_einv);
		}else{ //mode == L_CENTRE
		    float tm_tt = 0.0f, en2 = end_t * 0.5f;
		    if(tm_t > en2){ 
				  tm_tt = tm_t- en2; tm_t = en2;
			}
			float tm_t_einv = tm_t * end_inv, tm_tt_einv = tm_tt * end_inv;
	   	    return start + se*tm_t_einv * tm_t_einv*2.0f + se*tm_tt_einv*(2.0f-tm_tt_einv*2.0f);
	    }
	}
}
GN_API unsigned int  GNg::SLinearColor(unsigned int start,unsigned int end,unsigned int tmtime,unsigned int endtime,float value,unsigned short mode){
	unsigned int temp;
	uint8 *ps = (uint8*)&start, *pe = (uint8*)&end, *pt = (uint8*)&temp;
	pt[0] = (uint8)SLinear((float)ps[0],(float)pe[0],tmtime,endtime,value,mode);
	pt[1] = (uint8)SLinear((float)ps[1],(float)pe[1],tmtime,endtime,value,mode);
	pt[2] = (uint8)SLinear((float)ps[2],(float)pe[2],tmtime,endtime,value,mode);
	pt[3] = (uint8)SLinear((float)ps[3],(float)pe[3],tmtime,endtime,value,mode);
	return temp;
}
GN_API GNg::sVEC2 GNg::SLinear(sVEC2 start,sVEC2 end,unsigned int  tmtime,unsigned int  endtime,float value,unsigned short mode){
    sVEC2 temp;
	temp.x= SLinear(start.x,end.x,tmtime,endtime,value,mode);
	temp.y= SLinear(start.y,end.y,tmtime,endtime,value,mode);
	return temp;
}
GN_API void GNg::GenSLinearTypeData(const float *line_list, uint8 *sl_type_list_out, int count, bool off_end){
	uint8 v0 = 2, v1 = 2, v2 = 2; //值意义 1:下波 2:平线 3:上波 用来衡量当前处理节点和它之后的两个节点的上落波状态
	float *_now = (float*)line_list, *_end  = (float*)(line_list + count);
	float *_end2 = _end - 2;

	for( ; _now < _end; ++_now, ++sl_type_list_out){
		if(_now < _end2){
			v1 = _now[0] < _now[1] ? 3 : 1; //检测后第1段是上波还是落波
			v2 = _now[1] < _now[2] ? 3 : 1; //检测后第2段是上波还是落波
		}else{
		  if(_now < _end2+1){				//余下两个点的处理
			v1 = _now[0] < _now[1] ? 3 : 1;
		  }else v1 = 2;
			v2 = 2;
		}
		if(off_end){  //断开首尾
		   if(v0 == 2)v0 = v1;
		   if(v2 == 2)v2 = v1;
		}
		//根据比较三点两段的上落状态来判断曲线的缓动表示
		if(v1 == v0)*sl_type_list_out = (v1 == v2 ? L_NONE : L_START);
		else		*sl_type_list_out = (v1 == v2 ? L_END : L_CENTRE);
		v0 = v1;
	}
}
#endif //_GN_LINEAR_H_