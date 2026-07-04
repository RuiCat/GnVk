#pragma once
#ifndef _GN_2DMATH_H_
#define _GN_2DMATH_H_

namespace GNg {

enum eGEOM_TYPE{           //几何图形类型
	 eGEOM_TRI,              //三角形
	 eGEOM_SPHERE,           //圆形
     eGEOM_RECT,             //矩形
	 eGEOM_QUADS,            //四边形
	 eGEOM_QUADS_PLANE,      //四边形面
     eGEOM_PLANE,            //平面
	 eGEOM_CAPSULE           //胶囊体
};
/*********************************************************** 2D向量 ****************************************************************/

//计算2D向量点积(向量1, 向量2)在参数后自行加上'.'或'->' 可以是指针 或 结构
#define GN_Vec2Dot(Vec2_1, Vec2_2) ( Vec2_1  x *  Vec2_2  x +  Vec2_1  y *  Vec2_2  y)
//计算2D向量与矩阵33特定列的点积(向量, 矩阵, 矩阵列号)
#define GN_Vec2DotM33Col( v , mat , col ) (v x * mat m1##col + v y * mat m2##col)
//计算2D向量和w为1与矩阵33特定列的点积(向量, 矩阵, 矩阵列号)
#define GN_Vec21DotM33Col( v , mat , col ) (v x * mat m1##col + v y * mat m2##col + mat m3##col)

//计算2D矩阵1行和矩阵2列的点积(矩阵1, 第几行, 矩阵2, 第几列)在参数后自行加上'.'或'->' 可以是指针 或 结构
#define GN_M33Dot(M33_1, n_row, M33_2, n_col) \
  ( M33_1 m##n_row##1 * M33_2 m1##n_col +\
	M33_1 m##n_row##2 * M33_2 m2##n_col +\
	M33_1 m##n_row##3 * M33_2 m3##n_col )

struct GN_API sVEC2  //2D向量
{
	float x, y;			//各个分量

	//赋值方法
	inline sVEC2(){}
	inline sVEC2(float _x,float _y):x(_x), y(_y){}
    inline void Init (float _x,float _y){ x = _x;  y = _y; }

	//运算方法
    inline sVEC2& operator +=(const sVEC2& v){ x += v.x; y += v.y; return *this; }
	inline sVEC2& operator -=(const sVEC2& v){ x -= v.x; y -= v.y; return *this; }
	inline sVEC2& operator *=(const sVEC2& v){ x *= v.x; y *= v.y; return *this; }
	inline sVEC2& operator /=(const sVEC2& v){ x /= v.x; y /= v.y; return *this; }
	
	inline sVEC2& operator +=(float f){ x += f; y += f; return *this; }
	inline sVEC2& operator -=(float f){ x -= f; y -= f; return *this; }
	inline sVEC2& operator *=(float f){ x *= f; y *= f; return *this; }
	inline sVEC2& operator /=(float f){ x /= f; y /= f; return *this; }
	
	inline sVEC2 operator *(const sVEC2& v)const{ return sVEC2(x * v.x, y * v.y); }
	inline sVEC2 operator /(const sVEC2& v)const{ return sVEC2(x / v.x, y / v.y); }
    inline sVEC2 operator +(const sVEC2& v)const{ return sVEC2(x + v.x, y + v.y); }
	inline sVEC2 operator -(const sVEC2& v)const{ return sVEC2(x - v.x, y - v.y); }
	
	inline sVEC2 operator *(float f)const{ return sVEC2(x * f, y * f); }
	inline sVEC2 operator /(float f)const{ return sVEC2(x / f, y / f); }
	inline sVEC2 operator +(float f)const{ return sVEC2(x + f, y + f); }
	inline sVEC2 operator -(float f)const{ return sVEC2(x - f, y - f); }

	friend GN_API sVEC2 operator * ( float, const struct sVEC2& );

	//正负方法
	inline sVEC2  operator +() const{ return *this; }
	inline sVEC2  operator -() const{ return sVEC2(-x, -y); }

	//转换方法
    inline operator float*(){ return (float*)this; }
	inline operator const float* () const{ return (const float*)this; }

	//比较方法
	inline bool operator == (const sVEC2& v) const{ return x == v.x && y == v.y; }
    inline bool operator != (const sVEC2& v) const{ return x != v.x || y != v.y; }

	inline float x_y(){ return x + y; }
    
	inline float Dot(/*const*/ sVEC2 **v){ return GN_Vec2Dot(   , (*v)-> ); }	//与另一向量点积(指向另一向量)
	inline float Dot(const sVEC2  *v){ return GN_Vec2Dot(   , v-> ); }			//与另一向量点积(另一向量)

	inline float Length(){ return sqrtf(GN_Vec2Dot(   ,  )); }		  //计算返回向量长度

	inline float Max(){ return x > y ? x : y; }                       //获取分量中的最大值
	float Max(uint8 *id_out);                                         //获取分量中的最大值(返回最大值分量顺序id)
	float MaxAbs();                                                   //获取分量中的绝对值最大
	float MaxAbs(uint8 *id_out);                                      //获取分量中的绝对值最大(返回最大值分量顺序id)
	inline float Min(){ return x < y ? x : y; }                       //获取分量中的最小值
	float Min(uint8 *id_out);                                         //获取分量中的最小值(返回最小值分量顺序id)
	float MinAbs();                                                   //获取分量中的绝对值最小
	float MinAbs(uint8 *id_out);                                      //获取分量中的绝对值最小(返回最小值分量顺序id)
};
extern GN_API sVEC2 Vector2D;

//标准化2D向量(返回已标准向量, 向量输入)
GN_API  void Vec2Normalize(sVEC2 *out, const sVEC2 *in);
//计算2D向量长度(目标向量)
inline float Vec2Length(const sVEC2 *v2){ return sqrtf(GN_Vec2Dot( v2-> , v2-> )); }
//计算2D向量点积(向量1, 向量2)
inline float Vec2Dot(const sVEC2 *p1, const sVEC2 *p2){ return GN_Vec2Dot( p1-> , p2-> ); }
//计算2D两向量的点积夹角(向量1, 向量2) 返回0为90度角 1为0度角 -1为180度角
GN_API float Vec2DotAngle(const sVEC2 *p1, const sVEC2 *p2);
//计算已标准好的2D两向量的点积夹角(标准向量1, 标准向量2) 这个效率比较高 返回0为90度角 1为0度角 -1为180度角
inline float Vec2DotAngleNz(const sVEC2 *p1n, const sVEC2 *p2n){ return GN_Vec2Dot( p1n-> , p2n-> ); }
//获得向量1与向量2之间的夹角(向量1, 向量2)返回夹角
inline float Vec2Angle(const sVEC2 *pV1,   const sVEC2 *pV2){ return acosf(Vec2DotAngle(pV1, pV2)) / GT_ANGLE_UNIT; }
//获得标准向量1与标准向量2之间的夹角(向量1, 向量2)返回夹角
inline float Vec2NzAngle(const sVEC2 *pV1Nz, const sVEC2 *pV2Nz){ return acosf(GN_Vec2Dot(pV1Nz-> , pV2Nz-> )) / GT_ANGLE_UNIT; }
//计算向量右方向垂直90度的向量(返回新向量, 向量输入) !两个参数不能为同一个向量
inline  void Vec2Right(sVEC2 *right_out, const sVEC2 *in){ right_out->x =-in->y; right_out->y = in->x; }
//计算沿原未标准好的向量方向赋予了速度的新向量(返回新向量, 向量输入, 速度值) 已标准好的新速度可以直接用向量*速度得到
inline  void Vec2Velocity(sVEC2 *out, const sVEC2 *in, float speed){ (*out) = (*in) * (speed / ((sVEC2*)in)->Length()); }

//旋转向量(目标向量, 旋转角度) 
GN_API void Vec2Rot(sVEC2 *pv, float angle); 
//获得由向量1转变到向量2时需要的旋转角度(返回转角, 向量1, 向量2)
GN_API void Vec2ToRot  (float *ag_out, const sVEC2 *pV1, const sVEC2 *pV2);
//获得由标准向量1转变到标准向量2时需要的旋转角度(返回转角, 向量1, 向量2)
GN_API void Vec2NzToRot(float *ag_out, const sVEC2 *pV1Nz, const sVEC2 *pV2Nz);
//计算两个向量指定比率之间插值的向量(向量输出, 起始向量, 目的向量, 头尾向量之间的比率)
GN_API void Vec2LinearRatio(sVEC2 *v_out, const sVEC2 *v_st, const sVEC2 *v_end, float ratio);

//计算向量的镜像向量(返回镜像向量, 向量, 镜像平面方向, 镜像平面离距离) 
inline void Vec2Mirror(sVEC2 *mir_out, const sVEC2 *v_in, const GNg::sVEC2 *Nz, float d){
    (*mir_out) = (*v_in) + ((*Nz) * ((GN_Vec2Dot( v_in-> , Nz-> ) - d) * (-2.0f)) );
} 
//计算已标准好的2D向量相对指定面法线的反射向量(返回标准好的反射向量, 标准向量, 标准面法线）
inline void Vec2ReflectNz(sVEC2 *refNz_out,const sVEC2 *ray_vNz,const sVEC2 *nNz){
    float d = GN_Vec2Dot( ray_vNz -> , nNz -> );
    /* if(d >= 0.0f)*ref_outNz = *ray_vNz; //不可能作出反弹 */
    *refNz_out = *ray_vNz + (*nNz * (d * -2.0f));
}
//计算2D向量相对指定面法线的反射向量(返回标准好的反射向量, 向量, 面法线）
inline void Vec2Reflect(sVEC2 *ref_outNz, const sVEC2 *ray_v, const sVEC2 *n){
    sVEC2 _v , _n;
    Vec2Normalize(&_v,ray_v);
    Vec2Normalize(&_n,n);
    Vec2ReflectNz(ref_outNz,&_v,&_n);
}
//计算以2D向量相对指定面法线的反射向量(返回反射向量, 向量, 面法线 需标准化好）
#define GN_Vec2Reflect( _ref_out , _ray_v , _nNz ) GNg::Vec2ReflectNz( _ref_out , _ray_v , _nNz )

//获得点v1到v2的距离(点v1, 点v2) 
inline float Vec2Dist(const sVEC2 *v1, const sVEC2 *v2){ 
    sVEC2 v1_2 = (*v1) - (*v2); 
	return v1_2.Length();
}
//获得点v1到v2到v3之间的点积夹角( 点v1, 点v2 , 点v3 ) 
inline float Vec2DotAngle(const sVEC2 *v1, const sVEC2 *v2, const sVEC2 *v3){ 
	sVEC2 v1_2 = (*v1) - (*v2), v3_2 = (*v3) - (*v2);
	return Vec2DotAngle( &v1_2, &v3_2 );
}
//获得点v1到v2到v3之间的弧度( 点v1, 点v2 , 点v3 ) 
inline float Vec2Radian(const sVEC2 *v1, const sVEC2 *v2, const sVEC2 *v3){ 
	float dot = Vec2DotAngle(v1, v2, v3); 
	return GN_DotAgToRadian( dot );
}
//获得向量1与向量2之间的弧度( 向量1, 向量2 ) 
inline float Vec2Radian(const sVEC2 *v1, const sVEC2 *v2){ 
	float dot = Vec2DotAngle(v1,  v2);
	return GN_DotAgToRadian( dot );
}
//获得点v1到v2到v3之间的角度( 点v1, 点v2 , 点v3 ) 
inline float Vec2Angle(const sVEC2 *v1, const sVEC2 *v2, const sVEC2 *v3){ 
    return Vec2Radian(v1, v2, v3) /GT_ANGLE_UNIT;
}
//获得向量XY分量的长度次序(向量, 返回最长的分量号, 返回最短的分量号) !分量号 0:x 1:y
GN_API void Vec2GetXYLenSequence(const sVEC2 *v, uint8 *l, uint8 *s);

template<class T, class T2>
//获得向量与另一组向量里的最大点积夹角 也就是最顺的方向(向量, 另一组向量, 另一组向量数, 如果有指定返回最大点积夹角是与第几个向量做点积的) 返回最大点积夹角 !如果做点积的方向是反的 这里也会它反过来 确保不会返回负值
float VecGetDotAngleAbsMax(T *v, T *vdata, T2 num, T2 *max_id_out=0){
	float max_d = -10000.0f, d;
    T2 i = 0, max_id;
	for(; i < num; ++i){
		d = vdata[i].Dot(v);
		if(d < 0.0f)d = -d;
		if(d > max_d){
            max_d = d;
            max_id = i;
	}	}
	if(max_id_out)*max_id_out = max_id;
	return max_d;
}
/*********************************************************** 2D变换参数结构 ************************************************************/

enum eTRANSF_TYPE{				 //变换类型
	 eTRANSF_OFS   = 0,				//偏移
	 eTRANSF_ROT   = 1,				//旋转
	 eTRANSF_SCL   = 2				//缩放
};
struct GN_API sTRANSF2D	  //2D变换信息结构
{
    float		x;		//位置x
	float		y;		//位置y
	float		sx;		//缩放x
	float		sy;		//缩放y
	float		r;		//旋转

	inline sTRANSF2D(){}
	inline sTRANSF2D(int _x, int _y, float _sx=1.0f,float _sy=1.0f, float _r=0.0f): x((float)_x),y((float)_y),sx(_sx),sy(_sy),r(_r){}
    inline void Init(int _x, int _y, float _sx=1.0f,float _sy=1.0f, float _r=0.0f){ x=(float)_x; y=(float)_y; sx=_sx; sy=_sy; r=_r; }
};
extern GN_API sTRANSF2D Ts2D;     //声明一个全局的默认变换信息

/******************************************************** 三角形3D ***************************************************************/

struct GN_API sTRI2D	//三角形2D
{
	sVEC2 a, b, c;		  //三个点位置

	sTRI2D(){}
    sTRI2D   (const sVEC2 *_a, const sVEC2 *_b, const sVEC2 *_c):a(*_a),b(*_b),c(*_c){}
	void Init(const sVEC2 *_a, const sVEC2 *_b, const sVEC2 *_c){ a = *_a;  b = *_b;  c = *_c; }
};
extern GN_API sTRI2D Tri2D;

enum eTRI_NEAR_TYPE {  //三角形靠近类型
     eTRI_NEAR_EDGE0,    //靠近三角形边0
     eTRI_NEAR_EDGE1,    //靠近三角形边1
	 eTRI_NEAR_EDGE2,    //靠近三角形边2
     e_TRI_NEAR_VEX0,    //靠近三角形顶点0
	 eTRI_NEAR_VEX1,     //靠近三角形顶点1
	 eTRI_NEAR_VEX2,     //靠近三角形顶点2
     eTRI_NEAR_FACE      //靠近三角形面
};
#define GN_TriAbcNext( ABC ) ( (ABC) < 2 ? (ABC) + 1 : 0)   //返回三角形的下一个顺序( 当前顺序 )
#define GN_TriAbcLast( ABC ) ( (ABC)     ? (ABC) - 1 : 2)   //返回三角形的上一个顺序( 当前顺序 )

struct GN_API sTRI_SPACE2D          //3D三角型的空间区域分割面结构
{
    sVEC2	   na, nb, nc;              //三边的外方向
	float      da, db, dc;              //三边的对应外方向离距离
	bool       united;                  //外方向是否已经标准化了

	//初始化(3角形点A, 3角形点B, 3角形点C, 是否一并标准化方向)
	void Init(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, bool to_unit=true);  

	//测试指定点是否在3D三角型的空间区域里(指定点)
	bool TestInclude(const sVEC2 *vex);

    //计算点与三角形的靠近距离(指定点, 如果有指定返回靠近那个边或面 参考eTRI_NEAR_TYPE)返回距离 !注意这个是靠近距离 如果点在三角形内该值为负数
    float NearDist(const sVEC2 *vex, uint8 *ne_type_out=0);

    //计算同平面的点与三角形的靠近关系(三角形, 指定点, 返回离最近点的距离, 返回靠近点点序0-3, 返回靠近边边序0-3, 返回靠近关系类型 参考eTRI_NEAR_TYPE )返回距离 !注意这个是靠近距离 如果点在三角形内该值为负数
	float NearRelation(const sTRI2D *tri, const sVEC2 *vex, float *vd_out, uint8 *v_abc_out, uint8 *e_abc_out, uint8 *ntype_out);
};

enum eORIENT_TYPE {    //朝向类型
     eORIENT_NULL,       //不确定
	 eORIENT_LEFT,       //向左
	 eORIENT_BOTTOM,     //向下
	 eORIENT_BACK,       //向后
     eORIENT_RIGHT,      //向右
     eORIENT_TOP,        //向上
     eORIENT_FRONT,      //向前

     eORIENT_TWO  = 8    //正反两面
};
//判断3角形是否正面(3角形)返回是否
inline bool TriIsFront(const sTRI2D &tri){
    sVEC2 vn,vn2 = tri.b - tri.a, vn3 = tri.c - tri.a;
    Vec2Right(&vn, &vn3); 
	if(GN_Vec2Dot( vn. , vn2. ) < 0.0f)return false;
	else return true;
}
//判断3角形是否正面(3角形点A, 点B, 点C)返回是否
inline bool TriIsFront(const sVEC2 &a, const sVEC2 &b, const sVEC2 &c){
	sVEC2 vn,vn2 = b-a, vn3 = c-a;
    Vec2Right(&vn, &vn3);
	if(GN_Vec2Dot( vn. , vn2. ) < 0.0f)return false;
	else return true;
}
//判断3角形是否正面(3角形点A, 点B, 点C)返回是否
inline bool TriIsFront(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){
	sVEC2 vn, vn2 = (*b) - (*a), vn3 = (*c) - (*a);
    Vec2Right(&vn, &vn3);
	if(GN_Vec2Dot( vn. , vn2. ) < 0.0f)return false;
	else return true;
}
//判断3角形是正还是反面(3角形点A, 点B, 点C)返回 eORIENT_TYPE 朝向类型 如果返回 eORIENT_NULL 表明三角形成一条直线无法确定正反
inline uint8 GetTriOrient(const sVEC2 &a, const sVEC2 &b, const sVEC2 &c){
	sVEC2 vn,vn2 = b-a, vn3 = c-a;

   #define GT_TMP_RET_TRI_ORIENT \
	if     (d < -0.0000001f)return eORIENT_BACK;\
	else if(d >  0.0000001f)return eORIENT_FRONT;\
	else return eORIENT_NULL

   #define GT_TMP_GET_TRI_ORIENT \
    Vec2Right(&vn, &vn3);\
    float d = GN_Vec2Dot( vn. , vn2. );\
    GT_TMP_RET_TRI_ORIENT
	
	GT_TMP_GET_TRI_ORIENT;
}
//判断3角形是正还是反面(3角形点A, 点B, 点C)返回 eORIENT_TYPE 朝向类型 如果返回 eORIENT_NULL 表明三角形成一条直线无法确定正反
inline uint8 GetTriOrient(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){
	sVEC2 vn, vn2 = (*b) - (*a), vn3 = (*c) - (*a);
    GT_TMP_GET_TRI_ORIENT;
}
//计算3角形宽比率(3角形点A, 点B, 点C) 返回宽比率:-1.0f - 0.0f - 1.0f 值越接近 0.0f 3角形越瘦长 值为负数表示3角形为反面
GN_API float TriWidthRatio(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c);

//确保三角型不为扁成一直线(3角形点A, 3角形点B, 3角形点C, 拉开距离)返回三角型原本是否为扁成一直线的
GN_API bool TriEnsure(sVEC2 &a, sVEC2 &b, sVEC2 &c, float d);

//计算3角形面积(3角形)返回面积
inline float TriArea(const sTRI2D &tri){
    sVEC2 b_a = tri.a - tri.b;
    sVEC2 v, b_c = tri.c - tri.b;
	GNg::Vec2Right(&v, &b_a);
    float area = GN_Vec2Dot( v. , b_c.) * 0.5f;
	return (area < 0.0f ? -area : area);
}
//计算3角形面积(3角形点A, 点B, 点C)返回面积
inline float TriArea(const sVEC2 &a, const sVEC2 &b, const sVEC2 &c){
    sVEC2 b_a = a - b;
    sVEC2 v, b_c = c - b;
	GNg::Vec2Right(&v, &b_a);
    float area = GN_Vec2Dot( v. , b_c.) * 0.5f;
	return (area < 0.0f ? -area : area);
}
//计算3角形面积的两倍(3角形点A, 点B, 点C)返回面积的两倍
inline float TriArea2(const sVEC2 &a, const sVEC2 &b, const sVEC2 &c){
	sVEC2 b_a = a - b, b_c = c - b;
    float area = b_c.x * (-b_a.y) + b_c.y * b_a.x; //Vec2Right
	return (area < 0.0f ? -area : area);
}
//由三边长度计算3角形面积的平方(3角形a长度, 3角形b长度, 3角形c长度)返回面积的平方
inline float TriArea_2(float ea, float eb, float ec){
	float p = (ea + eb + ec) * 0.5f;
	return ((p - ea) * (p - eb) * (p - ec) * p);
}
//由三边长度计算3角形面积(3角形a长度, 3角形b长度, 3角形c长度)返回面积
inline float TriArea(float ea, float eb, float ec){
	float p = (ea + eb + ec) * 0.5f;
	return sqrtf((p - ea) * (p - eb) * (p - ec) * p);
}
//计算3角形3个角弧度  (3角形点A, 3角形点B, 3角形点C, 返回角A弧度, 返回角B弧度, 返回角C弧度)
GN_API void TriRadians(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *ra, float *rb, float *rc);
//计算3角形3个角的角度(3角形点A, 3角形点B, 3角形点C, 返回角A, 返回角B, 返回角C)
GN_API void TriAngles(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *ag_a, float *ag_b, float *ag_c);
//计算3角形3个角的Cos值(3角形点A, 3角形点B, 3角形点C, 返回角A cos , 返回角B cos , 返回角C cos )
GN_API void TriCoss(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *cos_a, float *cos_b, float *cos_c);
//计算3角形3个角的Cos和Sin值(3角形点A, 3角形点B, 3角形点C, 返回角A cos , 返回角B cos , 返回角C cos , 返回角A sin , 返回角B sin , 返回角C sin )
GN_API void TriCosSins(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *cos_a, float *cos_b, float *cos_c, float *sin_a, float *sin_b, float *sin_c);
//计算3角形3个角的Cos和弧度值(3角形点A, 3角形点B, 3角形点C, 返回角A cos , 返回角B cos , 返回角C cos , 返回角A弧度, 返回角B弧度, 返回角C弧度 )
GN_API void TriCosRadians(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *cos_a, float *cos_b, float *cos_c, float *ra, float *rb, float *rc);

//获得3角形最长边长度的平方(3角形点A, 3角形点B, 3角形点C)返回最长边长度的平方
GN_API float TriGetMaxEdgeLen2(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c);
//获得3角形最长边长度      (3角形点A, 3角形点B, 3角形点C)返回最长边长度 
inline  float TriGetMaxEdgeLen (const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){ return sqrtf(TriGetMaxEdgeLen2(a, b, c)); }

//计算获得3角形外边方向(3角形点A, 3角形点B, 3角形点C, 外边方向x分量输出, 外边方向y分量输出, 外边对应的离距离输出)
inline void TriEdgeDirGet(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *nx_out/*[3]*/, float *ny_out/*[3]*/, float *d_out/*[3]*/){
    /*模板代码:计好三角面的外围边分割面*/
   #define GT_TMP_TRI_EXTERN_EDGE_COMPUTE( nax , nay , nbx, nby , ncx , ncy ) \
    nax = -(b->y - a->y);\
	nay =   b->x - a->x ;\
	nbx = -(c->y - b->y);\
	nby =   c->x - b->x ;\
	ncx = -(a->y - c->y);\
	ncy =   a->x - c->x ;\
	d_out[0] = a->x * nax + a->y * nay;\
	d_out[1] = b->x * nbx + b->y * nby;\
	d_out[2] = c->x * ncx + c->y * ncy
    GT_TMP_TRI_EXTERN_EDGE_COMPUTE( nx_out[0] , ny_out[0] , nx_out[1] , ny_out[1] , nx_out[2] , ny_out[2] );
}
//计算获得3角形外边方向(3角形点A, 3角形点B, 3角形点C, 外边方向输出, 外边对应的离距离输出)
inline void TriEdgeDirGet(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, sVEC2 *n_out/*[3]*/, float *d_out/*[3]*/){
    GT_TMP_TRI_EXTERN_EDGE_COMPUTE( n_out[0].x , n_out[0].y , n_out[1].x , n_out[1].y , n_out[2].x , n_out[2].y );
}
//计算指定点侧重于三角型点B和C的权重(3角形点A, 3角形点B, 3角形点C, 指定点, 返回B和C的权重) !点A的权重可以通过 1.0f - u - v 得到
inline void TriLinearUv(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, const sVEC2 *p, float *uv_out/*[2]*/){
	sVEC2 b_a = (*b) - (*a), c_a = (*c) - (*a), p_a = (*p) - (*a);
	sVEC2 e_a(b_a.y, -b_a.x), e_c(-c_a.y, c_a.x);
	uv_out[1] = GN_Vec2Dot(p_a. , e_a. ) / GN_Vec2Dot(c_a. , e_a. );
	uv_out[0] = GN_Vec2Dot(p_a. , e_c. ) / GN_Vec2Dot(b_a. , e_c. );
}
struct GN_API sTRI_LINEAR2D	 //三角型线性插值器 !如果在同一三角型内要计算很多采样点的值 那么用这个作处理会更有效率
{
	sVEC2 b_a, c_a;
	
	//初始化(3角形点A, 3角形点B, 3角形点C)
	void Init(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){
		b_a = (*b) - (*a), c_a = (*c) - (*a);
		sVEC2 b_a1 = b_a;
		sVEC2 e_a(b_a.y, -b_a.x), e_c(-c_a.y, c_a.x);
		b_a = e_a / GN_Vec2Dot(c_a.  , e_a. );
		c_a = e_c / GN_Vec2Dot(b_a1. , e_c. );
	}
	//获得指定点侧重于三角型点B和C的权重( 指定点减三角形点A的向量, 返回B和C的权重) !点A的权重可以通过 1.0f - u - v 得到
	void GetUv(sVEC2 p_a, float *uv_out/*[2]*/){
		uv_out[1] = GN_Vec2Dot(p_a. , b_a. );
		uv_out[0] = GN_Vec2Dot(p_a. , c_a. );
	}
};
//3角形相关值线性插值(角点a值的单位距离增量, 角点b值的单位距离增量, 角点c值的单位距离增量, 采集点位置x, 采集点位置y, a边面的外方向x, a边面的外方向y, a边面的离距离, b边面的外方向x, b边面的外方向y, b边面的离距离, c边面的外方向x, c边面的外方向y, c边面的离距离)
#define GNg_TriLinearValue2D_( va_ , vb_ , vc_ ,  vpx , vpy ,  nxa , nya , da , nxb , nyb , db , nxc , nyc , dc ) \
    (((da) - ((vpx) * (nxa) + (vpy) * (nya))) * (vc_)  +  ((db) - ((vpx) * (nxb) + (vpy) * (nyb))) * (va_)  +  ((dc) - ((vpx) * (nxc) + (vpy) * (nyc))) * (vb_))

//3角形相关值线性插值(角点a值, 角点b值, 角点c值, 采集点位置x, 采集点位置y, a边面的外方向x, a边面的外方向y, a边面的离距离, a边面到对角点的距离, b边面的外方向x, b边面的外方向y, b边面的离距离, b边面到对角点的距离, c边面的外方向x, c边面的外方向y, c边面的离距离, c边面到对角点的距离)
#define GNg_TriLinearValue2D( va , vb , vc ,  vpx , vpy ,  nxa , nya , da , da_c , nxb , nyb , db , db_a , nxc , nyc , dc , dc_b ) \
    ((((da) - ((vpx) * (nxa) + (vpy) * (nya))) / (da_c)) * (vc)  +  (((db) - ((vpx) * (nxb) + (vpy) * (nyb))) / (db_a)) * (va)  +  (((dc) - ((vpx) * (nxc) + (vpy) * (nyc))) / (dc_b)) * (vb))

template<class T>
//3角形相关值线性插值(角点a值, 角点b值, 角点c值, 采集点, 3角形点A, 3角形点B, 3角形点C)返回值
T inline TriLinearValue2D(T va, T vb, T vc, const sVEC2 *vp, const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){
    sVEC2 n[3];
	float      d[3];
	TriEdgeDirGet(a, b, c, n, d);
    float da_c = d[0] - GN_Vec2Dot( n[0]. , c-> ), db_a = d[1] - GN_Vec2Dot( n[1]. , a-> ), dc_b = d[2] - GN_Vec2Dot( n[2]. , b-> );
    return GNg_TriLinearValue2D( va , vb , vc ,  vp->x , vp->y ,  n[0].x , n[0].y , d[0] , da_c , n[1].x , n[1].y , d[1] , db_a , n[2].x , n[2].y , d[2] , dc_b );
}
struct GN_API sSPHERE2D    //圆2D
{
    sVEC2		cp;			 //圆心位置
	float       r;			 //半径

	sSPHERE2D(){}
	sSPHERE2D(float cp_x, float cp_y, float _r):cp(cp_x, cp_y),r(_r){}
    void Init(float cp_x, float cp_y, float _r){ cp.x = cp_x;  cp.y = cp_y;  r = _r; }
	sSPHERE2D(const sVEC2 *p, float _r):cp(*p), r(_r){}
	void Init(const sVEC2 *p, float _r){ cp = *p; r = _r; }
};
extern GN_API sSPHERE2D Sphere2D;

//按周长来获得圆面积(周长)
inline float SphereAreaC(float clen){
    return (clen / GT_ANGLE_360) * clen * 0.5f;
}
//按半径来获得圆面积(半径)
inline float SphereAreaR(float r){
    return r * r * GT_ANGLE_180;
}
/**************************************************** 矩型筐2D ***********************************************************************/

struct GN_API sRECT     //整型矩型
{
    int    left;			//左
    int    top;				//上
    int    right;			//右
    int    bottom;			//下

	sRECT(){}
	sRECT(int _left, int _top, int _right, int _bottom):left(_left), top(_top), right(_right), bottom(_bottom){ }
	void Init(int _left, int _top, int _right, int _bottom){ left=_left; top=_top; right=_right; bottom=_bottom; }

	inline int GetWidth() { return right  - left; }	//获得宽度
    inline int GetHeight(){ return bottom - top;  }	//获得高度

	int GetArea() { return ( right - left) * (bottom - top); }       //获得面积
	int GetPerim(){ return ((right - left) + (bottom - top)) << 1; } //获得边周长
	
	sVEC2 GetEdgeLen(){ return sVEC2(right-left, bottom-top); }		 //获得矩型的2个边长

	//测试指定的所有点都在空间矩型的那一边(点坐标X数组, 坐标Y数组, 点数)如果都不是同在一边返回0 返回1:矩型-x方向 2:-y方向 4:x方向 5:y方向
	int TestInThere(const int *px, int *py, int pn); 
};
extern GN_API sRECT Rect;

struct GN_API sRECT2    //矩型2D
{
    float    left;		    //左
    float    top;			//上
    float    right;			//右
    float    bottom;		//下

	sRECT2(){}
	sRECT2   (float _left, float _top, float _right, float _bottom):left(_left),top(_top),right(_right),bottom(_bottom){}
	void Init(float _left, float _top, float _right, float _bottom){ left=_left; top=_top; right=_right; bottom=_bottom; }
	sRECT2   (const sVEC2 *min, const sVEC2 *max):left(min->x), top(min->y), right(max->x), bottom(max->y){}
	void Init(const sVEC2 *min, const sVEC2 *max){ left = min->x;  top = min->y;  right = max->x;  bottom = max->y; }

    inline float GetWidth() { return right  - left; }	  //获得宽度
    inline float GetHeight(){ return bottom - top;  }	  //获得高度
    
	float GetArea(){  return ( right - left) * (bottom - top); }          //获得面积
	float GetPerim(){ return ((right - left) + (bottom - top)) * 2.0f; }  //获得边周长

	sVEC2 GetEdgeLen(){ return sVEC2(right-left, bottom-top); }			  //获得矩型的2个边长

	//测试指定的所有点都在空间矩型的那一边(顶点, 顶点数)如果都不是同在一边返回0 返回1:矩型-x方向 2:-y方向 4:x方向 5:y方向
	int TestInThere(const sVEC2 *vexs, int pn); 
};
extern GN_API sRECT2 Rect2D;

//计算相加矩形2到矩形1(矩型结构1, 矩型结构2)
GN_API void RectAdd(sRECT  *rc1, const sRECT  *rc2);
GN_API void RectAdd(sRECT2 *rc1, const sRECT2 *rc2);
//计算得到两个矩型相加之后的矩型(结果输出, 矩型结构1, 矩型结构2)
GN_API void RectAdd(sRECT  *out, const sRECT  *rc1, const sRECT  *rc2);
GN_API void RectAdd(sRECT2 *out, const sRECT2 *rc1, const sRECT2 *rc2);

template<class T, class T2>
//把多个矩型相加合并到首个矩形里(多个矩型, 矩型数)
void RectArrayCombine(T *rcs, T2 rcn){
	T2 ri = 1;
	for(; ri < rcn; ++ri)RectAdd(rcs, rcs + ri);
}
//计算得到两个矩型重叠部分(结果输出, 矩型结构1, 矩型结构2)
GN_API void RectOverlap(sRECT  *out, const sRECT  *rc1, const sRECT  *rc2);
GN_API void RectOverlap(sRECT2 *out, const sRECT2 *rc1, const sRECT2 *rc2);

//判断矩型是否正面矩型(目标矩型) 
GN_API bool RectIsFront(const sRECT  *rc);
GN_API bool RectIsFront(const sRECT2 *rc);

//判断两矩型是否相近的(矩型结构1, 矩型结构2)
GN_API bool RectSimilar(const sRECT  *rc1, const sRECT  *rc2);
GN_API bool RectSimilar(const sRECT2 *rc1, const sRECT2 *rc2);

//从两点建立一个矩型 (目标矩型,         点1x,        点1y,     点2x,       点2y)
GN_API void RectMakeFromP2(sRECT  &rc, int   p1x,   int p1y,   int p2x,   int p2y);
GN_API void RectMakeFromP2(sRECT2 &rc, float p1x, float p1y, float p2x, float p2y);
GN_API void RectMakeFromP2(sRECT2 &rc, const sVEC2 &p1,  const sVEC2 &p2);
//把矩型转为等边的正方型(矩型)
GN_API void RectToSquare(sRECT  &rc);
//把矩型转为等边的正方型(矩型)
GN_API void RectToSquare(sRECT2 &rc);

//得到矩型的中心位置坐标点(目标矩型, 中心输出) 
GN_API void RectGetCentre(const sRECT  &rc, int &cx, int &cy);
GN_API void RectGetCentre(const sRECT2 &rc, sVEC2 &c);

//获得矩型的4个角点(目标矩型, 角点数据输出 要求参数预先分配好4个点的数据空间)
GN_API void RectGetAnglePoint(const sRECT  &rc, int *x/*[4]*/, int *y/*[4]*/);
GN_API void RectGetAnglePoint(const sRECT2 &rc, sVEC2 *ap/*[4]*/);

//获得矩型最近视线的角点(目标矩型, 角点输出, 视线方向)
GN_API void RectGetNearPoint(const sRECT  &rc, int &npx, int &npy, int vx, int vy);
GN_API void RectGetNearPoint(const sRECT2 &rc, sVEC2 &np, const sVEC2 &v);

//获得矩型最长边的长度(矩型)返回长度
inline int   RectGetMaxEdgeLen(const sRECT &rc){
	int max_e_len = rc.right - rc.left, e_len2 = rc.bottom - rc.top;
	return(e_len2 > max_e_len)? e_len2 : max_e_len;
}
//获得矩型最长边的长度(矩型)返回长度
inline float RectGetMaxEdgeLen(const sRECT2 &rc){
	float max_e_len = rc.right - rc.left, e_len2 = rc.bottom - rc.top;
	return(e_len2 > max_e_len)? e_len2 : max_e_len;
}
//获得矩型最短边的长度(矩型)返回长度
inline int   RectGetMinEdgeLen(const sRECT &rc){
	int min_e_len = rc.right - rc.left, e_len2 = rc.bottom - rc.top;
	return(e_len2 < min_e_len)? e_len2 : min_e_len;
}
//获得矩型最短边的长度(矩型)返回长度
inline float RectGetMinEdgeLen(const sRECT2 &rc){
	float min_e_len = rc.right - rc.left, e_len2 = rc.bottom - rc.top;
	return(e_len2 < min_e_len)? e_len2 : min_e_len;
}
//裁剪掉线在矩型以外的部分(矩型, 线点1x, 线点1y, 线点2x, 线点2y)返回1表示线在矩型内并没有超出 返回-1表示有超出空间 返回0表示线在区域外
GN_API int RectCutLine(const sRECT *rc, int *p1x, int *p1y, int *p2x, int *p2y);
//裁剪掉线在矩型以外的部分(矩型, 线点1, 线点2)返回1表示线在矩型内并没有超出 返回-1表示有超出空间 返回0表示线在区域外
GN_API int RectCutLine(const sRECT2 *rc, sVEC2 *lp1, sVEC2 *lp2);

//计算由矩型中心向指定方向外延伸的一条直线会与矩型那个方向面相交(矩型宽, 矩型高, 外延伸的方向:需先标准化) 返回靠位标记 GNu::eNEAR_TYPE 所列举的值
GN_API unsigned short ComputeRectNearFromDirection(int w, int h, const sVEC2 &vNz);

//计算由矩型中心向指定方向外延伸的一条直线会与矩型那个方向面相交(矩型, 外延伸的方向:需先标准化) 返回靠位标记 GNu::eNEAR_TYPE 所列举的值
inline unsigned short ComputeRectNearFromDirection(const sRECT &rc, const sVEC2 &vNz){
    return  ComputeRectNearFromDirection(rc.right - rc.left, rc.bottom - rc.top, vNz);
}
//计算由矩型中心向指定方向外延伸的一条直线会与矩型那个方向面相交(矩型, 外延伸的方向:需先标准化) 返回靠位标记 GNu::eNEAR_TYPE 所列举的值
inline unsigned short ComputeRectNearFromDirection(const sRECT2 &rc, const sVEC2 &vNz){
    return  ComputeRectNearFromDirection((int)(rc.right - rc.left), (int)(rc.bottom - rc.top), vNz);
}
/************************************************** 平行四边形结构 **********************************************************
    
    4 -----*
    ah     |  
 ori*- aw ->               点轴是按这样构成四边面的 */

struct sQUADS_PLANE2D    //2D平行四边面结构  这种结构不限制边要跟坐标轴平行 可以在空间旋转
{
	sVEC2    ori;			//原点位置
    sVEC2    aw;			//宽轴 
    sVEC2    ah;			//高轴 

	inline sQUADS_PLANE2D(){}
	inline sQUADS_PLANE2D(sVEC2 *_ori, sVEC2 *_aw, sVEC2 *_ah):ori(*_ori),aw(*_aw),ah(*_ah){}
};
/*
    0----1
    |    |
	3----2                四边型按这样的点序构成*/

struct GN_API sQUADS2  //2D平行四边形  这个不限制边要跟坐标轴平行 可以在空间旋转
{
    sVEC2		 p0, p1, p2;    //角点  第4点可以计算得到

	inline sQUADS2(){}
	inline sQUADS2  (float p0x, float p0y, float p1x, float p1y, float p2x, float p2y):p0(p0x,p0y), p1(p1x,p1y), p2(p2x,p2y){}
    inline void Init(float p0x, float p0y, float p1x, float p1y, float p2x, float p2y){ p0.x=p0x; p0.y=p0y; p1.x=p1x; p1.y=p1y; p2.x=p2x; p2.y=p2y; }
	inline sQUADS2  (const sVEC2 *_p0, const sVEC2 *_p1, const sVEC2 *_p2): p0(*_p0), p1(*_p1), p2(*_p2){}
    inline void Init(const sVEC2 *_p0, const sVEC2 *_p1, const sVEC2 *_p2){ p0 = *_p0; p1 = *_p1; p2 = *_p2; }
};
extern GN_API sQUADS2 Quads2D;

//矩型筐范围转平行四边形空间范围(输出四边形, 矩型筐入)
GN_API void  RangeRectToQuads(sQUADS2 *q_out, const sRECT2 *rect_in);
//平行四边形空间范围转矩型筐范围(输出矩型筐, 四边形入, 四边型另外一点输出)
GN_API void  RangeQuadsToRect(sRECT2 *rect_out, /*const*/ sQUADS2 *q_in, sVEC2 *ap=0/*[1]*/);
//得到平行四边形的中心位置坐标点(目标矩型, 中心输出)
GN_API void  RangeQuadsGetCentre(const sQUADS2 &q_in, GNg::sVEC2 &c);
//获得四边型最长边的长度(四边形)返回长度
GN_API float RangeQuadsGetMaxEdgeLen(const sQUADS2 *q_in);
//获得四边型最短边的长度(四边形)返回长度
GN_API float RangeQuadsGetMinEdgeLen(const sQUADS2 *q_in);
//获得四边型的4个角点(目标四边型, 角点数据输出 要求参数预先分配好4个点的数据空间)
GN_API void  RangeQuadsGetAnglePoint(const sQUADS2 &q, sVEC2 ap[4]);
//获得四边型其余的那个角点(目标四边型, 角点数据输出 要求参数预先分配好1个点的数据空间)
GN_API void  RangeQuadsGetAnglePointOther(const sQUADS2 &q, sVEC2 &ap);
//反转四边型点序(目标四边型) !这通常用于修正经含有镜像的矩阵变换后点序不正确问题
GN_API void  RangeQuadsTurnOrde(sQUADS2 &qs);

//按四边形可视面ID来获得它的最前点ID(装填返回最前点ID的数组, 可视面ID的数组, 可视面数)返回最前点数
GN_API uint8 RangeQuadsGetNearPIdFromVisualPlane2D(uint8 * p_ids/*[2]*/, const uint8 *nids/*[2]*/, uint8 nn);
//获得四边型指定角点对向的相邻角点ID(装填返回对向角点ID的数组, 指定角点ID)
GN_API void  RangeQuadsGetAdjPointID_2D(uint8 * p_ids/*[2]*/, uint8 pid);
//获得四边型指定边面的2个角点(装填返回角点ID的数组, 面ID) !面ID: 1:四边型自身-x方向 2:自身-y方向 4:自身x方向 5:自身y方向
GN_API void  RangeQuadsGetPlanePointID_2D(uint8 * p_ids/*[2]*/, uint8 nid);
//获得四边型指定角点的对角点ID(指定角点ID)返回对角点ID
GN_API uint8 RangeQuadsGetOppPointID_2D(uint8 pid);

/********************************************** 2D平行四边形空间区域分割面 *************************************************************
          n1
        #-d1--#
   n0 d0|     |d_0
        #-d_1-#         数据表示 */

struct GN_API sQUADS_SPACE2		  //2D平行四边形空间区域
{
	sVEC2		n0, n1;				//空间面法线
	float       d0, d1, d_0, d_1;	//空间面离距离
  
	sQUADS_SPACE2(){}
	sQUADS_SPACE2(const sQUADS2 *q_in, bool to_unit=true);  //构建四边形的空间区域分割面(四边形, 是否单位化分割面法线)
    void Init(const sQUADS2 *q_in, bool to_unit=true);      //初始化四边形的空间区域分割面(四边形, 是否单位化分割面法线)
	void Init(const sRECT2 *rc_int);						//初始化四边形的空间区域分割面(矩形)
	void Normalize();                                       //标准化 使四边形的空间区域各分割面法线单位化 相对应得离距离也一起改变
    void Offset(const sVEC2 *v);                            //平行偏移四边形空间区域(偏移量)
	void UpdateD(const sQUADS2 *q_in);						//更新各面方向离距离(四边形)

    //测试指定点比较贴近四边形空间分割面的那一边(点, 返回贴近距离)返回1:四边型自身-x方向 2:自身-y方向 4:自身x方向 5:自身y方向
	int TestNearThere (const sVEC2 *p, float *d_out=0);

	//测试指定点比较靠近四边形空间分割面的那一边(点, 返回靠近距离)返回1:四边型自身-x方向 2:自身-y方向 4:自身x方向 5:自身y方向  返回0:表示点在点在四边形里 此时d_out返回GN_FLOAT_MAX
	int TestNearThere_(const sVEC2 *p, float *d_out=0); 

	//测试指定点比较远离四边形空间分割面的那一边前面(点, 返回远离距离 即点的最近距离)返回1:四边型自身-x方向 2:自身-y方向 4:自身x方向 5:自身y方向 !!参数d_out返回如果为负数 表明点在四边形内
	int TestFarThere (const sVEC2 *p, float *d_out=0);

	//测试三角形三点都在四边形空间分割面的那一边(三角形点a, 三角形点b, 三角形点c)如果都不是同在一边返回0 返回1:四边型自身-x方向 2:自身-y方向 3:自身-z方向 4:自身x方向 5:自身y方向 6:自身z方向
	int TestInThere(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c);

	//测试指定的所有点都在四边形空间分割面的那一边(顶点, 顶点数)如果都不是同在一边返回0 返回1:四边型自身-x方向 2:自身-y方向 4:自身x方向 5:自身y方向
	int TestInThere(const sVEC2 *vexs, int pn);

	//测试指定的所有点都在四边形空间分割面的那一边(顶点, 顶点数, 返回点在2个方向分割面对应的所在方位, 返回对应2个方向分割面已计算好的顶点离距离数, 返回已计算好的顶点离距离数据) !参数nids返回值意义和 TestInThere 一样 不同在于 TestInThere 一但检索到所有点同在其中一边就立即返回 但有时一个物体离得比较开时它的所有点可能同时在四边形的2边 而 TestInThereEx 就是用来获取对应所有方位的详细信息
    void TestInThereEx(const sVEC2 *vexs, int pn, int *nids/*[2]*/, int *dns/*[2]*/, float *dss/*[2][pn]*/);

    //测试指定的所有点都在四边形空间分割面的里边还是外面(顶点, 顶点数) 都在里面返回:1 都在外面返回:-1 其它返回:0
	int TestInclude(const sVEC2 *vexs, int pn);
    
    //测试四边形空间是否包含指定点()返回是否
	bool TestIncPoint(const sVEC2 *p);

	//获得四边型空间的面积 !要使返回的单位准确 请先标准化好空间区域分割面法线
    float GetAreaVol();

    //获得四边型的边周长   !要使返回的单位准确 请先标准化好空间区域分割面法线 和确保四边型的所有角为直角
	float GetPerim(){ return ((d0 - d_0) + (d1 - d_1)) * 2.0f; }

	//获得四边型最大边长   !要使返回的单位准确 请先标准化好空间区域分割面法线 和确保四边型的所有角为直角
	float GetMaxEdgeLen(){ return (d0 - d_0) > (d1 - d_1) ? (d0 - d_0) : (d1 - d_1); }
	//获得四边型最大边长   !要使返回的单位准确 请先标准化好空间区域分割面法线 和确保四边型的所有角为直角
	float GetMinEdgeLen(){ return (d0 - d_0) < (d1 - d_1) ? (d0 - d_0) : (d1 - d_1); }
	//获得四边型的2个边长  !要使返回的单位准确 请先标准化好空间区域分割面法线 和确保四边型的所有角为直角
	GNg::sVEC2 GetEdgeLen(){ return GNg::sVEC2(  d0 - d_0,	  d1 - d_1); }

	//获得四边形空间较薄的面ID(薄面的厚度)  返回4:x方向  5:y方向
	uint8 GetThinFace(float *thick_out);
	//获得四边形空间较薄的面ID(返回最大厚度除各个方向厚度的比率)  返回4:x方向  5:y方向
	uint8 GetThinFace(sVEC2 *tr_out);

	//获得离观察方向的最近的离距离(观察方向, 四边形的4个角点, 返回离距离 !返回值减摄像机位置的离距离 即为最近距离)返回最近角点ID
	uint8 GetLookNearD(const sVEC2 *v, /*const*/ GNg::sVEC2 *ap/*[4]*/, float *d_out);

	//获得按特定观察方向看到最正的面(观察方向, 返回对住观察点的面法线, 返回对应的离距离, 是否返回较近的面) 返回面id 返回1:四边型自身-x方向 2:自身-y方向 4:自身x方向 5:自身y方向
    uint8 GetLookFace(const sVEC2 *v, sVEC2 *n_out, float *d_out, bool ret_near=true);

	//获得按特定观察方向看到的前2面或后2面(观察方向, 返回向外的面法线, 返回对应的离距离, 返回2个面的id, 是否返回前2面) !返回的面id意义同上
    void  GetExternFaceFB(const sVEC2 *v, sVEC2 *n_outs/*[2]*/, float *d_outs/*[2]*/, uint8 *nid_outs/*[2]*/, bool ret_front=true);

	//获得指定方向面的法线和离距离(返回面法线, 方向面id, 法线返回是否向内)返回离距离
	float GetNormal(sVEC2 *n_out, int id, bool inside);

    //获得四边型的4个角点(角点数据输出 要求参数预先分配好4个点的数据空间 点序和四边形一致) !调用这个前n0 n1 需先标准化好
	void  GetAnglePoint(sVEC2 *ap/*[4]*/);

	//获得四边型(返回对应的四边型) 这个相对调用比较快 !但调用这个前n0 n1 需先标准化好 并且都互相垂直 如果不确定是否垂直那请调用 GetAnglePoint
    void  GetAnglePoint_1(sQUADS2 *q_out);

	//获得中心点()返回中心点 !调用这个前n0 n1 需先标准化好 并且都互相垂直
	sVEC2 GetCentre();

	//扩大范围(扩大大小)
	void Expand(float size);

	//扩大范围使能包围指定的所有点(顶点, 顶点数, 是否清除之前区域 使扩大的范围刚好适合指定的所有点)
	void Expand(sVEC2 *vexs, int pn, bool clear=false);

	//扩大范围使能包围指定的四边形(四边形, 是否清除之前区域 使扩大的范围刚好适合指定的四边面)
	void Expand(sQUADS2 *q, bool clear=false);

	//检测是否有扩大范围使能包围指定的所有点(顶点, 顶点数)返回是否有扩大了
	bool HasExpand(sVEC2 *vexs, int pn);
};
/******************************************************** 矩阵 *******************************************************/

struct GN_API sMAT33	 //3x3矩阵
{
   float  m11, m12, m13;
   float  m21, m22, m23;
   float  m31, m32, m33;

   inline sMAT33(){}
   //构造矩阵(是否单位矩阵)
   inline sMAT33(bool is_unit):
                  m12(0.0f),m13(0.0f),
        m21(0.0f),          m23(0.0f),
        m31(0.0f),m32(0.0f)
    {
        if(is_unit){ m11=1.0f; m22=1.0f; m33=1.0f; }
		else       { m11=0.0f; m22=0.0f; m33=0.0f; }
    }
	//初始化矩阵(是否单位矩阵)
    inline void Init(bool is_unit){
		         m12=0.0f;m13=0.0f;
        m21=0.0f;         m23=0.0f;
        m31=0.0f;m32=0.0f;

		if(is_unit){ m11=1.0f; m22=1.0f; m33=1.0f; }
		else{        m11=0.0f; m22=0.0f; m33=0.0f; }
    }
    inline sMAT33(float _11,float _12,float _13,
	           float _21,float _22,float _23,
			   float _31,float _32,float _33):
		m11(_11), m12(_12), m13(_13),
		m21(_21), m22(_22), m23(_23),
		m31(_31), m32(_32), m33(_33)
    {}
    inline void Init( float _11,float _12,float _13,
	           float _21,float _22,float _23,
			   float _31,float _32,float _33)
    {
        m11 = _11, m12 = _12, m13 = _13;
		m21 = _21, m22 = _22, m23 = _23;
		m31 = _31, m32 = _32, m33 = _33;
    }
	inline sMAT33	(const sVEC2 *row1, const sVEC2 *row2, const sVEC2 *row3){
		*((GNg::sVEC2*)&m11) = *row1; *((GNg::sVEC2*)&m21) = *row2; *((GNg::sVEC2*)&m31) = *row3;
		m13 = m23  = 0.0f; m33 = 1.0f;
	}
	inline void Init(const sVEC2 *row1, const sVEC2 *row2, const sVEC2 *row3){
		*((GNg::sVEC2*)&m11) = *row1; *((GNg::sVEC2*)&m21) = *row2; *((GNg::sVEC2*)&m31) = *row3;
		m13 = m23  = 0.0f; m33 = 1.0f;
	}
    sMAT33 operator *(const sMAT33 &_tm)const;									  //乘(矩阵)返回相乘后的矩阵
    inline float &operator ()(int8 col,int8 row){ return *(&m11+(col*3 +row)); }  //返回矩阵指定行列位置的元素
    inline void Col3To001(){ m13 = m23  = 0.0f; m33 = 1.0f; }					  //使矩阵最右一列设为默认值 
	inline GNg::sVEC2  Col1(){   return  GNg::sVEC2(m11,m21); }					  //返回第1列元素
	inline GNg::sVEC2  Col2(){   return  GNg::sVEC2(m12,m22); }					  //返回第2列元素
	inline GNg::sVEC2 *Row1(){   return (GNg::sVEC2*)&m11; }					  //返回第1行元素指针
	inline GNg::sVEC2 *Row2(){   return (GNg::sVEC2*)&m21; }					  //返回第2行元素指针
	inline GNg::sVEC2 *GetPos(){ return (GNg::sVEC2*)&m31; }					  //获得矩阵表示的轴位置点
};
extern GN_API sMAT33 Matrix33;

//组合2D变换结构(结果输出, 变换结构1, 变换结构2) 适合2D不带旋转的变换用       !!这个比重载*处理速度要更快 但应注意结果输出变量地址不要跟参数2或参数3相同
GN_API void MultTransf2D(sTRANSF2D *out, const sTRANSF2D *ts1, const sTRANSF2D *ts2);
//组合2D变换结构(结果矩阵输出, 变换结构1, 变换结构2)						  !!这个可兼容旋转 且如果是不带旋转的变换组合 处理速度会较快
GN_API void MultTransf2D(sMAT33 *out,    const sTRANSF2D *ts1, const sTRANSF2D *ts2);
//组合2D变换结构与矩阵33(结果矩阵输出, 变换结构1, 矩阵2)					  !!这个可兼容旋转 且如果是不带旋转的变换组合 处理速度会较快
GN_API void MultTransf2D(sMAT33 *out,    const sTRANSF2D *ts1, const sMAT33    *mat2);
//组合矩阵33(结果输出, 矩阵1, 矩阵2) 适合2D变换用                             !!这个比重载*处理速度要快些 但应注意结果输出变量地址不要跟参数2或参数3相同
GN_API void MultM33(sMAT33 *out, const sMAT33 *mat1, const sMAT33 *mat2);
//组合Z列均为标准001的两矩阵33(矩阵, 矩阵2) 适合2D变换用					  !!这个比 MultM33 处理速度要快
GN_API void MultM33T(sMAT33 *inout, const sMAT33 *mat2);
//组合仅左边Z列为标准001的两矩阵33(矩阵, 矩阵2) 适合2D变换用				  !!这个比 MultM33 处理速度要快
GN_API void MultM33L(sMAT33 *inout, const sMAT33 *mat2);
//组合移动到矩阵33(矩阵, 移动量, 是后组合 否前组合) 适合2D变换用 !调用这个比 乘上移动矩阵更有效率
GN_API void M33MultMove(sMAT33 *mat, const GNg::sVEC2 *ofs, bool is_back=true);
//组合移动到矩阵33(矩阵, 单分量移动量, 分量ID, 是后组合 否前组合) 适合2D变换用 !调用这个比 乘上移动矩阵更有效率
GN_API void M33MultMove1(sMAT33 *mat, float ofs, uint8 id, bool is_back=true);
//组合缩放到矩阵33(矩阵, x比列, y比列, z比列, 是后组合 否前组合) 适合2D变换用 !调用这个比 乘上缩放矩阵更有效率
GN_API void M33MultScale(sMAT33 *mat, float sx, float sy, bool is_back=true);
//组合缩放到矩阵33(矩阵, 单分量比列, 分量ID, 是后组合 否前组合) 适合2D变换用 !调用这个比 乘上缩放矩阵更有效率
GN_API void M33MultScale1(sMAT33 *mat, float scl, uint8 id, bool is_back=true);

//获取移动矩阵33(矩阵输出, 移动X, 移动Y)  适合2D变换用
GN_API void M33GetMove(sMAT33 *wout, float x, float y);
//获取比例矩阵33(矩阵输出, 比例X, 比例Y)  适合2D变换用
GN_API void M33GetScale(sMAT33 *wout, float sx, float sy);
//获取旋转矩阵33(矩阵输出, 旋转角度)  适合2D变换用
GN_API void M33GetRot(sMAT33 *wout, float angle);

//从TRANSF2D获取变换矩阵33(矩阵输出, 变换信息结构)  适合2D变换用
GN_API void M33GetTransf2D(sMAT33 *wout, const sTRANSF2D *ts);

//获取由2向量组成矩阵前2列数据的矩阵33(矩阵输出, 第1列向量, 第2列向量)
GN_API void M33GetV2Col(sMAT33 *wout, const sVEC2 *col1, const sVEC2 *col2);

//获取变换到四边形局部空间区域里的矩阵(四边形空间, 矩阵输出, 在它空间内的点通过输出矩阵变换后值范围是否落在-1到1之间 否则为0到1之间)
GN_API void M33GetQusdsLocal(const sQUADS_SPACE2 &qs, sMAT33 &mat, bool ndc=false);

//获取矩阵44的转置矩阵(矩阵输出, 矩阵输入)  适合3D变换用
GN_API void M33GetTranspose(sMAT33 &out, const sMAT33& in);
//获取矩阵33的逆矩阵(矩阵输出, 矩阵输入) 适合2D变换用
GN_API void M33GetInverse(sMAT33 &out, sMAT33& in);

//获得变换矩阵33的比例缩放因子(矩阵, 缩放X输出, 缩放Y输出)
GN_API void M33ScaleFactorGet(const sMAT33 *m_in, float *sx_out, float *sy_out);

//把变换矩阵33转为TRANSF2D结构(矩阵, 变换信息结构输出) 适合2D变换用 !如果变换矩阵有拉伸压扁效果会被自动忽略掉
GN_API void M33ToTransf2D(const sMAT33 *m_in,sTRANSF2D *ts_out);
//把变换矩阵33转为TRANSF2D结构(矩阵, 变换信息结构输出, 剩余的变换矩阵输出)返回是否有剩余的变换 适合2D变换用 某些变换矩阵带有拉伸压扁效果 而变换信息结构不能描述这些 因此这种矩阵就会有剩余矩阵输出 通常 剩余矩阵 * 变换信息结构 = 原输入矩阵
GN_API bool M44ToTransf2D(const sMAT33 *m_in,sTRANSF2D *ts_out, sMAT33 *residual);
//把变换矩阵33转为TRANSF2D结构(矩阵, 变换信息结构输出) 适合2D变换用 这个支持镜像翻转 !如果变换矩阵有拉伸压扁效果会被自动忽略掉
GN_API void M33ToTransf2DMir(const sMAT33 *m_in,sTRANSF2D *ts_out);

//计算两个矩阵按指定比率之间的插值矩阵(矩阵输出, 起始矩阵, 目标矩阵, 比率)
GN_API void M33LinearRatio(sMAT33 *m_out, const sMAT33 *m_start, const sMAT33 *m_end, float ratio);
//线性插值矩阵(矩阵输出 ,开始矩阵, 目标矩阵, 现在进度时间, 整个变化过程的所需时间, 是否忽略最尾行和最尾列的元素不用参加插值计算)
GN_API void M33Linear(sMAT33 *m_out, const sMAT33 *m_start, const sMAT33 *m_end, unsigned int  tm_time, unsigned int  end_time, bool ign_last_row_col=false);

//用矩阵33来转换点或向量(x输出, y输出, x输入, y输入, 变换矩阵, 是否为变换点)  适合2D变换用
GN_API void M33TransformCoord(float *out_x, float *out_y, const float in_x, const float in_y, const sMAT33 *matrix, bool isCoord=1);
//用矩阵33来转换点或向量(顶点输出, 顶点输入, 变换矩阵, 是否为变换点)  适合2D变换用  !!注意参数输入向量和输出向量不能为同一个向量
GN_API void M33TransformCoord(sVEC2 *out, const sVEC2 *in, const sMAT33 *matrix, bool isCoord=1);
//用矩阵33来转换点或向量(顶点, 变换矩阵, 是否为变换点)  适合2D变换用
GN_API void M33TransformCoord(sVEC2 *pv, const sMAT33 *matrix, bool isCoord=1);
//用矩阵33来转换点或向量指针分配的数组(顶点数组输出, 顶点数组输入, 顶点数, 是否为变换点) 适合2D变换用
GN_API void M33TransformCoords(sVEC2 *array_out, const sVEC2 *array_in, int num, const sMAT33 *matrix, bool isCoord=1);
//用矩阵33来转换点或向量指针分配的数组(顶点数组的指针, 顶点数, 变换矩阵, 是否为变换点) 适合2D变换用
GN_API void M33TransformCoords(sVEC2 **ppv_array,int num, const sMAT33 *matrix, bool isCoord=1);

//用矩阵33来转换TRANSF2D结构(变换信息结构输出, 变换信息结构输入, 变换矩阵)  适合2D变换用
GN_API void M33TransformTransf2D(sTRANSF2D *out, const sTRANSF2D *in, const sMAT33 *mat);
//用矩阵33来转换TRANSF2D结构(变换信息结构, 变换矩阵)  适合2D变换用
GN_API void M33TransformTransf2D(sTRANSF2D *ts, const sMAT33 *mat);

//用矩阵33来转换4边型(四边型结构, 变换矩阵)  适合2D变换用
GN_API void M33TransformQuads(sQUADS2 &qs, const sMAT33 &matrix);
//用矩阵33来转换4边型并一起计算范围矩型(矩型结构, 相对的四边型结构输出, 变换矩阵, 变换矩阵是否包含镜象变换, 变换矩阵xy缩放逆因子 默认0即当矩阵不包含缩放, 相对的四边型另外一点输出)  适合2D变换用 !!变换矩型会一并计算相对的四边型
GN_API void M33TransformRect(sRECT2 &rc, sQUADS2 &qs, const sMAT33 &matrix, bool has_mirror=false, const sVEC2 *scl_factor_inv=0, sVEC2 *ap=0/*[1]*/);

struct GN_API sVEC3		//3D向量
{
	float x, y, z;
};
//按顶点数据计算获得能包围这些顶点的范围盒(范围盒输出, 顶点数据, 顶点数) !里面已包含 RangeBoxEnsure 调用
GN_API void GetRangeBoxFromVertexs(sRECT2* rb, sVEC2* vex_data, int vex_num);

}; //end namespace GNg
#endif //endif _GN_2DMATH_H_
