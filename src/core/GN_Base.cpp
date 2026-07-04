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
#ifdef _GN_BASE_H_
#ifndef _GN_FILE_H_
#include <stdarg.h>
GN_API void GN_Log(const char *fmt, ...){
    va_list va;
	va_start(va,fmt);
	vprintf(fmt,va);
	va_end(va);
}
#endif
GN_API float GN::TimeRatio  = 1.0f;
GN_API float GN::TimeRatioL = 1.0f;

GN_API void GN_GetSymmetDispersionValue0_1(float &v, float &s){
	if(v < 0.5f)v += 0.5f; //直接跳到后半截
	else if((1.0f - v) > s)v = v + s * 2.0f - 0.5f;
	else{ //分到尾 就重新分小一半的大小
		s *= 0.5f;
		v = s;
	}
}
GN_API uint8 GN_GetDiscreteGridIdX(uint8 id, uint8 size){
	uint8 x = 0;
	size >>= 1;
	while(size){
		if(id & 1)x += size;
		size >>= 1;
		id >>= 2;
	}
	return x;
}
GN_API uint8 GN_GetDiscreteGridIdY(uint8 id, uint8 size){
	uint8 y = 0; 
	size >>= 1;
	while(size){
		if(((id & 2)>>1) != (id & 1))y += size;
		size >>= 1;
		id >>= 2;
	}
	return y;
}
GN_API  float GN_RandomF0_1(){
	float res;
	unsigned int tmp;
    static unsigned int seed = 0x13371337;

	seed *= 16807;
	tmp = seed ^ (seed >> 4) ^ (seed << 15);
	*((unsigned int *)&res) = (tmp >> 9) | 0x3F800000;

	return (res - 1.0f);
}
#endif // _GN_BASE_H_
