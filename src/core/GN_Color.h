#ifndef _GN_COLOR_H_
#define _GN_COLOR_H_
namespace GNg{
/********************************************************* 颜色运算 **********************************************************/
union  sCOLOR;

struct GN_API sCOLOR4F       //浮点颜色类型
{
	float r, g, b, a;	//各个分量

	//赋值方法
	inline  sCOLOR4F():a(1.0f){}
	inline  sCOLOR4F(float _r, float _g, float _b, float _a=1.0f){ r = _r; g = _g; b = _b; a = _a; }
	inline void Init(float _r, float _g, float _b, float _a=1.0f){ r = _r; g = _g; b = _b; a = _a; }

	inline  sCOLOR4F(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a=255){
		float fc = 1.0f/255.0f;
		r = (float)_r * fc;  g = (float)_g * fc;  b = (float)_b * fc;  a = (float)_a * fc;
	}
	inline void Init(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a=255){
		float fc = 1.0f/255.0f;
		r = (float)_r * fc;  g = (float)_g * fc;  b = (float)_b * fc;  a = (float)_a * fc;
	}
	inline sCOLOR4F(const float *pf){ r = pf[0]; g = pf[1];  b = pf[2]; a = pf[3]; } //float 数组赋值
    inline sCOLOR4F(const sCOLOR4F& c){ r = c.r; g = c.g; b = c.b; a = c.a; }
	sCOLOR4F(unsigned int  argb);
	sCOLOR4F(      sCOLOR& c);
    sCOLOR4F(const sCOLOR& c);

    //等于赋值方法
	inline sCOLOR4F& operator = (const sCOLOR4F& c4f){ r = c4f.r; g = c4f.g; b = c4f.b; a = c4f.a; return *this; }
	sCOLOR4F& operator = (unsigned int  argb);
    sCOLOR4F& operator = (      sCOLOR& c);
	sCOLOR4F& operator = (const sCOLOR& c);

	//转换方法
    operator unsigned int ()const;
    operator sCOLOR()const;
	inline operator float*(){ return (float*)&r; }
    inline operator const float*()const{ return (const float*)&r; }
    inline operator sCOLOR4F*(){ return this; }
    inline operator const sCOLOR4F*()const{ return this; }
    inline operator sCOLOR4F&(){ return *this; }
    inline operator const sCOLOR4F&()const{ return *this; }

	//运算方法
    inline sCOLOR4F& operator += (const sCOLOR4F& c){ r += c.r;  g += c.g;  b += c.b;  a += c.a; return *this; }
    inline sCOLOR4F& operator -= (const sCOLOR4F& c){ r -= c.r;  g -= c.g;  b -= c.b;  a -= c.a; return *this; }
    inline sCOLOR4F& operator *= (const sCOLOR4F& c){ r *= c.r;  g *= c.g;  b *= c.b;  a *= c.a; return *this; }
    inline sCOLOR4F& operator /= (const sCOLOR4F& c){ r /= c.r;  g /= c.g;  b /= c.b;  a /= c.a; return *this; }

	inline sCOLOR4F& operator *= (float f){ r *= f;  g *= f;  b *= f;  a *= f; return *this; }
    inline sCOLOR4F& operator /= (float f){ float fInv = 1.0f / f; r *= fInv;  g *= fInv;  b *= fInv;  a *= fInv; return *this; }

	inline sCOLOR4F operator + (const sCOLOR4F& c)const{ sCOLOR4F c4(r + c.r, g + c.g, b + c.b, a + c.a); return c4; }
    inline sCOLOR4F operator - (const sCOLOR4F& c)const{ sCOLOR4F c4(r - c.r, g - c.g, b - c.b, a - c.a); return c4; }
    inline sCOLOR4F operator * (const sCOLOR4F& c)const{ sCOLOR4F c4(r * c.r, g * c.g, b * c.b, a * c.a); return c4; }
    inline sCOLOR4F operator / (const sCOLOR4F& c)const{ sCOLOR4F c4(r / c.r, g / c.g, b / c.b, a / c.a); return c4; }

	inline sCOLOR4F operator + (float f)const{ sCOLOR4F c4(r + f, g + f, b + f, a + f); return c4; }
    inline sCOLOR4F operator - (float f)const{ sCOLOR4F c4(r - f, g - f, b - f, a - f); return c4; }
	inline sCOLOR4F operator * (float f)const{ sCOLOR4F c4(r * f, g * f, b * f, a * f); return c4; }
    inline sCOLOR4F operator / (float f)const{ float fInv = 1.0f / f; sCOLOR4F c4(r * fInv, g * fInv, b * fInv, a * fInv); return c4; }

	friend sCOLOR4F operator * (float, const sCOLOR4F&);

	//正负方法
	inline sCOLOR4F operator + ()const{ return *this; }
    inline sCOLOR4F operator - ()const{ sCOLOR4F c4(-r, -g, -b, -a); return c4; }

	//比较方法
    inline bool operator == (const sCOLOR4F& c) const{ return r == c.r && g == c.g && b == c.b && a == c.a; }
    inline bool operator != (const sCOLOR4F& c) const{ return r != c.r || g != c.g || b != c.b || a != c.a; }

	//分量转换设值方法( R , G , B )
	inline void SetRGB(unsigned char _r,unsigned char _g,unsigned char _b){
		float fc = 1.0f/255.0f;
        r = (float)_r * fc;  g = (float)_g * fc;  b = (float)_b * fc;
	}
	//分量Alpha设值方法( A )
	inline void SetAlpha(unsigned char _a){ a = (float)_a / 255.0f; }

	//获得平均亮度
	inline float GetLuminance()const{ return (r + g + b) * 0.333333f;  }

	//获得RGB分量其中最大的值
	inline float MaxRGB()const{
	    float m = r > g ? r : g;
		return m > b ? m : b;
	}
	//获得RGBA分量其中最大的值
	inline float Max()const{
	    float m = r > g ? r : g;
        if(a > m)m = a;
		return m > b ? m : b;
	}
	//获得RGB分量其中最小的值
	inline float MinRGB()const{
	    float m = r < g ? r : g;
		return m < b ? m : b;
	}
	//获得RGBA分量其中最小的值
	inline float Min()const{
	    float m = r < g ? r : g;
        if(a < m)m = a;
		return m < b ? m : b;
	}
};
extern GN_API sCOLOR4F Color4F;		//黑色

union GN_API sCOLOR           //32位颜色类型
{
   unsigned int  COLOR;  //类似 windows 的 DWORD 颜色格式
   struct {
       unsigned char b;
       unsigned char g;
       unsigned char r;
	   unsigned char a;
   }BGRA;                //类似 windows 的 ARGB 颜色格式 和上面顺序相同
   struct {
       unsigned char r;
       unsigned char g;
       unsigned char b;
	   unsigned char a;
   }RGBA;                //顺序同于 OpenGL RGBA 颜色格式

   //赋值系列
   inline sCOLOR(){ RGBA.a = 255; }
   inline sCOLOR(unsigned int  argb){ COLOR = argb; }
   inline sCOLOR(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a=255, bool RGB_turn=false){
		if(RGB_turn){ RGBA.a = _a; RGBA.r = _r; RGBA.g = _g; RGBA.b = _b; }
		else{		  BGRA.a = _a; BGRA.r = _r; BGRA.g = _g; BGRA.b = _b; }
   }
   inline void Init(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a=255, bool RGB_turn=false){
		if(RGB_turn){ RGBA.a = _a; RGBA.r = _r; RGBA.g = _g; RGBA.b = _b; }
		else{		  BGRA.a = _a; BGRA.r = _r; BGRA.g = _g; BGRA.b = _b; }
   }
   inline sCOLOR(float _r, float _g, float _b, float _a=1.0f, bool RGB_turn=false){
       #define GT_TMP_INIT_COLOR( ADD_PROC1 , ADD_PROC2 , C ) \
	    if(RGB_turn){ ADD_PROC1  RGBA.r=(unsigned char)(255.0f* C##r); RGBA.g=(unsigned char)(255.0f* C##g); RGBA.b=(unsigned char)(255.0f* C##b); }\
		else{		  ADD_PROC2  BGRA.r=(unsigned char)(255.0f* C##r); BGRA.g=(unsigned char)(255.0f* C##g); BGRA.b=(unsigned char)(255.0f* C##b); }

       #define GT_TMP_INIT_COLOR_( ADD_PROC1 , ADD_PROC2 , C ) \
        if(RGB_turn){ ADD_PROC1  RGBA.r=(unsigned char)(255.0f* C r); RGBA.g=(unsigned char)(255.0f* C g); RGBA.b=(unsigned char)(255.0f* C b); }\
		else{		  ADD_PROC2  BGRA.r=(unsigned char)(255.0f* C r); BGRA.g=(unsigned char)(255.0f* C g); BGRA.b=(unsigned char)(255.0f* C b); }

	    GT_TMP_INIT_COLOR( RGBA.a=(unsigned char)(255.0f*_a); , BGRA.a=(unsigned char)(255.0f*_a); , _ )
   }
   inline void Init(float _r, float _g, float _b, float _a=1.0f, bool RGB_turn=false){
	    GT_TMP_INIT_COLOR( RGBA.a=(unsigned char)(255.0f*_a); , BGRA.a=(unsigned char)(255.0f*_a); , _ )
   }
   inline sCOLOR(const sCOLOR   &color) { COLOR = color.COLOR; }
   inline sCOLOR(const sCOLOR4F &color4, bool RGB_turn=false){
	   GT_TMP_INIT_COLOR_( RGBA.a=(unsigned char)(255.0f*color4.a); , BGRA.a=(unsigned char)(255.0f*color4.a); , color4. )
   }
   inline void Init(const sCOLOR4F &color4, bool RGB_turn=false){
	   GT_TMP_INIT_COLOR_( RGBA.a=(unsigned char)(255.0f*color4.a); , BGRA.a=(unsigned char)(255.0f*color4.a); , color4. )
   }
   //等于赋值系列
   inline sCOLOR& operator = (unsigned int  argb){ COLOR = argb; return *this; }
   inline sCOLOR& operator = (const sCOLOR& c){ COLOR = c.COLOR; return *this; }

   //转换系列
   inline operator unsigned int  () const{ return COLOR; }

   //3D颜色类型的转换相关
   inline sCOLOR(sCOLOR4F &color){ Init(color.r,color.g,color.b,color.a); }
   inline sCOLOR(const sCOLOR4F &color){ Init(color.r,color.g,color.b,color.a); }
   inline sCOLOR& operator = (const sCOLOR4F& c4f){ Init(c4f.r,c4f.g,c4f.b,c4f.a); return *this; }
   inline operator sCOLOR4F()const{ sCOLOR4F c4(BGRA.r,BGRA.g,BGRA.b,BGRA.a); return c4; }

   //分量转换设值系列
   inline void SetRGB(float _r, float _g, float _b, bool RGB_turn=false){
	   GT_TMP_INIT_COLOR(  ,  , _ )
   }
   //分量Alpha设值方法( A )
   inline void SetAlpha(float _a){ BGRA.a = (unsigned char)(255.0f* _a); }

   //获得平均亮度
   inline unsigned char GetLuminance()const{ return (RGBA.r + RGBA.g + RGBA.b + 1) / 3;  }

   //获得RGB分量其中最大的值
   inline unsigned char MaxRGB()const{
	    unsigned char m = BGRA.r > BGRA.g ? BGRA.r : BGRA.g;
		return m > BGRA.b ? m : BGRA.b;
   }
   //获得RGBA分量其中最大的值
   inline unsigned char Max()const{
	    unsigned char m = BGRA.r > BGRA.g ? BGRA.r : BGRA.g;
        if(BGRA.a > m)m = BGRA.a;
		return m > BGRA.b ? m : BGRA.b;
   }
   //获得RGB分量其中最小的值
   inline unsigned char MinRGB()const{
	    unsigned char m = BGRA.r < BGRA.g ? BGRA.r : BGRA.g;
		return m < BGRA.b ? m : BGRA.b;
   }
   //获得RGBA分量其中最小的值
   inline unsigned char Min()const{
	    unsigned char m = BGRA.r < BGRA.g ? BGRA.r : BGRA.g;
        if(BGRA.a < m)m = BGRA.a;
		return m < BGRA.b ? m : BGRA.b;
   }
};
extern GN_API sCOLOR Color;		//黑色

#pragma pack(push,1)
struct GN_API sCOLOR3		//24位颜色类型
{
	uint8 r, g, b;
};
#pragma pack(pop)

#define eMODULATE_COLOR 4		 //调整颜色

//获得一个幻彩变化的颜色值(变化速度值)返回颜色 参数要奇数才行如[1,3,5,7]不同速度的均匀过度 [11,13,15,17]不同速度带有一点杂色的过度 [51]每个均不同的颜色
GN_API unsigned int  GetColorChange(int chg_speed=1);

//获得一个幻彩变化的颜色值(变化速度值)返回颜色 参数要奇数才行如[1,3,5,7]不同速度的均匀过度 [11,13,15,17]不同速度带有一点杂色的过度 [51]每个均不同的颜色
GN_API sCOLOR4F GetColorChange4F(int chg_speed=1);

//返回用源颜色忽略透明直接加亮到目标颜色后的颜色(源颜色, 目标颜色)返回颜色
GN_API unsigned int  AddColor(unsigned int  s_color, unsigned int  d_color);

//返回用源颜色加亮到目标颜色后的颜色(源颜色, 目标颜色)返回颜色
GN_API unsigned int  AddColorRatio(unsigned int  s_color, unsigned int  d_color);

//返回用源颜色混合目标颜色后的颜色(源颜色, 目标颜色)返回颜色
GN_API unsigned int  BlendColorRatio(unsigned int  s_color, unsigned int  d_color);

//返回颜色2按ratio给定权重混合到颜色1的颜色(颜色1, 颜色2, 混合权重)返回颜色
GN_API unsigned int  BlendColor(unsigned int  color, unsigned int  color2, float ratio);

//返回两颜色调和后的新颜色(源颜色, 调和颜色)返回颜色
GN_API unsigned int  MultiplyColorRatio(unsigned int  s_color, unsigned int  ratio_color);

//返回指定颜色调至特定不透明度下的颜色(源颜色,  不透明度0.0f-1.0f)返回颜色
inline unsigned int  MultiplyAlphaRatio(unsigned int  s_color, float ratio){
	uint8 *pt = (uint8*)&s_color;
    pt[3] = (uint8)((float)pt[3] * ratio);
	return s_color;
}
//返回指定颜色调至特定亮度下的颜色(源颜色, 亮度0.0f-1.0f)返回颜色
inline unsigned int  MultiplyLumRatio(unsigned int  s_color, float lum){
	uint8 *pt = (uint8*)&s_color;
	pt[0] = (uint8)((float)pt[0] * lum);
	pt[1] = (uint8)((float)pt[1] * lum);
	pt[2] = (uint8)((float)pt[2] * lum);
	return s_color;
}
//返回两颜色调和后的新颜色(源颜色, 调和颜色)返回颜色 !这个会先检测颜色是否没变就直接返回颜色
#define GNg_MultiplyColorRatio( SC , RC ) (SC==0xffffffff ? RC : (RC==0xffffffff ? SC : GNg::MultiplyColorRatio(SC, RC)))
}; //end namespace GNg
#endif //_GN_COLOR_H_
