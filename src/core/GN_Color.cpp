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
#ifdef _GN_COLOR_H_

/********************************************************* 颜色运算 **********************************************************/
GNg::sCOLOR4F operator * (float f, const GNg::sCOLOR4F& c ){
    GNg::sCOLOR4F c4(f * c.r, f * c.g, f * c.b, f * c.a); return c4;
}
GNg::sCOLOR4F::sCOLOR4F(unsigned int  argb){
	#define GT_TMP_COLOR_TO_COLOR4F( C ) \
	const float f = 1.0f / 255.0f;\
	r = f * (float) (unsigned char) ( C >> 16);\
	g = f * (float) (unsigned char) ( C >>  8);\
	b = f * (float) (unsigned char) ( C >>  0);\
	a = f * (float) (unsigned char) ( C >> 24);
	GT_TMP_COLOR_TO_COLOR4F( argb )
}
GNg::sCOLOR4F::sCOLOR4F(      sCOLOR& c){ GT_TMP_COLOR_TO_COLOR4F( c.COLOR ) }
GNg::sCOLOR4F::sCOLOR4F(const sCOLOR& c){ GT_TMP_COLOR_TO_COLOR4F( c.COLOR ) }
GNg::sCOLOR4F::operator unsigned int ()const{ GNg::sCOLOR tmp(r,g,b,a); return tmp.COLOR; }
GNg::sCOLOR4F::operator GNg::sCOLOR () const{ GNg::sCOLOR tmp(r,g,b,a); return tmp; }
GNg::sCOLOR4F& GNg::sCOLOR4F::operator = (unsigned int  argb){ GT_TMP_COLOR_TO_COLOR4F( argb ) return *this; }
GNg::sCOLOR4F& GNg::sCOLOR4F::operator = (      sCOLOR& c){ GT_TMP_COLOR_TO_COLOR4F( c.COLOR ) return *this; }
GNg::sCOLOR4F& GNg::sCOLOR4F::operator = (const sCOLOR& c){ GT_TMP_COLOR_TO_COLOR4F( c.COLOR ) return *this; }

GN_API GNg::sCOLOR4F GNg::Color4F(0.0f,0.0f,0.0f);
GN_API GNg::sCOLOR   GNg::Color(0xff000000);

GN_API unsigned int  GNg::GetColorChange(int speed){ //返回颜色幻彩值
   #define GT_TMP_NEXT_CHANGE_COLOR \
	static uint8 Tr = 255, Tg = 255, Tb = 0;\
	if(Tr){\
		  if(Tr >= 255 && !Tg)Tb -= speed;\
	}else if(Tb <  255		 )Tb += speed;\
	if(Tb){\
		  if(Tb >= 255 &&  Tg)Tg -= speed;\
          else if(Tr<255&& Tr)Tr += speed;\
		  else if(!Tr  && !Tg)Tr += speed;\
	}else if(Tg >= 255 &&  Tr)Tr -= speed;\
	 else					  Tg += speed;

	GT_TMP_NEXT_CHANGE_COLOR
	return (0xff000000|(Tr<<16)|(Tg<<8)|Tb);
}
GN_API GNg::sCOLOR4F GNg::GetColorChange4F(int speed){
	GT_TMP_NEXT_CHANGE_COLOR
	float fc = 1.0f/255.0f;
	return GNg::sCOLOR4F((float)Tr*fc,(float)Tg*fc,(float)Tb*fc,1.0f);
}
GN_API unsigned int  GNg::AddColor(unsigned int  s_color,unsigned int  d_color){
	uint8 *ps = (uint8*)&s_color, *pr = (uint8*)&d_color;
    
	unsigned short b = (unsigned short)ps[0] + (unsigned short)pr[0], g = (unsigned short)ps[1] + (unsigned short)pr[1], r = (unsigned short)ps[2] + (unsigned short)pr[2];
	return (d_color&0xff000000)|((r<0xff?r:0xff) << 16)|((g<0xff?g:0xff) << 8)|(b<0xff?b:0xff);
}
GN_API unsigned int  GNg::AddColorRatio(unsigned int  s_color,unsigned int  d_color){
	uint8 *ps = (uint8*)&s_color, *pr = (uint8*)&d_color;

    uint8 *_p = ps;
	unsigned int  a = ps[3];
    (*_p) = ((unsigned int)(*_p) * a) / 255; ++_p;
	(*_p) = ((unsigned int)(*_p) * a) / 255; ++_p;
	(*_p) = ((unsigned int)(*_p) * a) / 255; ++_p;

    unsigned short b = (unsigned short)ps[0] + (unsigned short)pr[0], g = (unsigned short)ps[1] + (unsigned short)pr[1], r = (unsigned short)ps[2] + (unsigned short)pr[2];
	return (d_color&0xff000000)|((r<0xff?r:0xff) << 16)|((g<0xff?g:0xff) << 8)|(b<0xff?b:0xff);
}
GN_API unsigned int  GNg::BlendColorRatio(unsigned int  s_color, unsigned int  d_color){
    unsigned int  temp;
    uint8 *ps = (uint8*)&s_color, *pr = (uint8*)&d_color, *pt = (uint8*)&temp;
	float sf = (float)ps[3] / 255.0f;
	float df = 1.0f - sf;
	pt[0] = (uint8)((float)ps[0] * sf + (float)pr[0] * df);
	pt[1] = (uint8)((float)ps[1] * sf + (float)pr[1] * df);
	pt[2] = (uint8)((float)ps[2] * sf + (float)pr[2] * df);
    pt[3] = 255;
	return temp;
}
GN_API unsigned int  GNg::BlendColor(unsigned int  color,unsigned int  color2, float ratio){
	unsigned int  temp;
    uint8 *ps = (uint8*)&color2, *pd = (uint8*)&color, *pt = (uint8*)&temp;
	float r1 = 1.0f - ratio;
	pt[0] = (uint8)((float)ps[0] * ratio + (float)pd[0] * r1);
	pt[1] = (uint8)((float)ps[1] * ratio + (float)pd[1] * r1);
	pt[2] = (uint8)((float)ps[2] * ratio + (float)pd[2] * r1);
	pt[3] = (uint8)((float)ps[3] * ratio + (float)pd[3] * r1);
	return temp;
}
//颜色百份比乘
GN_API unsigned int  GNg::MultiplyColorRatio(unsigned int  s_color,unsigned int  ratio_color){
    unsigned int  temp;
    uint8 *ps=(uint8*)&s_color, *pr=(uint8*)&ratio_color, *pt=(uint8*)&temp;
    float rt = 1.0f / 255.0f;
	pt[0] = (uint8)((float)pr[0] * rt * (float)ps[0]);
	pt[1] = (uint8)((float)pr[1] * rt * (float)ps[1]);
	pt[2] = (uint8)((float)pr[2] * rt * (float)ps[2]);
	pt[3] = (uint8)((float)pr[3] * rt * (float)ps[3]);
	return temp;
}
#endif //_GN_COLOR_H_