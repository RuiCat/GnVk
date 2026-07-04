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
#ifdef _GN_2DMATH_H_
/*********************************************************** 2D向量 ****************************************************************/
GN_API GNg::sVEC2  GNg::Vector2D(0.0f,0.0f);

GN_API GNg::sVEC2 GNg::operator * ( float f, const struct sVEC2& v ){
    GNg::sVEC2 tmp(f * v.x, f * v.y); 
	return  tmp;
}
#define GT_TMP_VECTOR2D_CMP_FUNC_DEF( NAME , CMP ) \
float GNg::sVEC2::NAME(uint8 *id_out){\
	float m;\
	if(x CMP y){  m = x; *id_out = 0; }\
	else       {  m = y; *id_out = 1; }\
    return m;\
}\
float GNg::sVEC2::NAME##Abs(){\
	float x_ = x < 0.0f ? -x : x, y_ = y < 0.0f ? -y : y;\
    return x_ CMP y_ ? x_ : y_;\
}\
float GNg::sVEC2::NAME##Abs(uint8 *id_out){\
    float x_ = x < 0.0f ? -x : x, y_ = y < 0.0f ? -y : y;\
    float m;\
	if(x_ CMP y_){  m = x_; *id_out = 0; }\
	else         {  m = y_; *id_out = 1; }\
    return m;\
}

GT_TMP_VECTOR2D_CMP_FUNC_DEF( Max , > )
GT_TMP_VECTOR2D_CMP_FUNC_DEF( Min , < )

GN_API void GNg::Vec2Normalize(sVEC2 *out,const sVEC2 *in){
     float len2 = in->x * in->x + in->y * in->y;
	 if(len2){
		 float len = sqrtf(len2);
         out->x = in->x / len;
		 out->y = in->y / len;
	 }else{
		 out->x = 0.0f;
		 out->y = 0.0f;
	 }
}
GN_API float GNg::Vec2DotAngle(const sVEC2 *p1,const sVEC2 *p2){
    sVEC2 v1,v2;
	Vec2Normalize(&v1,p1);
	Vec2Normalize(&v2,p2);
	return GN_Vec2Dot(v1., v2.);
}
GN_API void GNg::Vec2Rot(sVEC2 *pv,float angle){
    float fx = angle * GT_ANGLE_UNIT;
	float cos_ = cosf(fx), sin_ = sinf(fx);
	float x = pv->x * cos_ - pv->y * sin_;
	  pv->y = pv->x * sin_ + pv->y * cos_;
    pv->x = x;
}
GN_API void GNg::Vec2ToRot  (float *ag_out,const sVEC2 *pV1,  const sVEC2 *pV2){
    sVEC2 v1,v2,v3;
	Vec2Normalize(&v1,pV1);
	Vec2Normalize(&v2,pV2);
	sVEC2 v1_2 = (v1 + v2) * 0.5f;
	float len = Vec2Length(&v1_2);
	*ag_out = len < 1.0f ? 2.0f * acosf(len) / GT_ANGLE_UNIT : 0.0f;
	v3.x = -v1.y;
	v3.y =  v1.x;
	if(GN_Vec2Dot( v2. , v3. ) < 0.0f)*ag_out = -(*ag_out);
}
GN_API void GNg::Vec2NzToRot(float *ag_out,const sVEC2 *pV1Nz,const sVEC2 *pV2Nz){
    sVEC2 v1_2 = ((*pV1Nz) + (*pV2Nz)) * 0.5f, v3;
	float len = Vec2Length(&v1_2);
	*ag_out = len < 1.0f ? 2.0f * acosf(len) / GT_ANGLE_UNIT : 0.0f;
    v3.x = -pV1Nz->y;
	v3.y =  pV1Nz->x;
	if(GN_Vec2Dot( pV2Nz-> , v3. ) < 0.0f)*ag_out = -(*ag_out);
}
GN_API void GNg::Vec2LinearRatio(sVEC2 *v_out, const sVEC2 *v_start, const sVEC2 *v_end, float ratio){
    v_out->x =  v_start->x + (v_end->x - v_start->x) * ratio;
	v_out->y =  v_start->y + (v_end->y - v_start->y) * ratio;
}
GN_API void GNg::Vec2GetXYLenSequence(const sVEC2 *v, uint8 *l, uint8 *s){
    float vx = v->x < 0.0f ? -v->x : v->x, vy = v->y < 0.0f ? -v->y : v->y;
	if(vx > vy){ *l = 0; *s = 1; }
	else       { *l = 1; *s = 0; }
}
/*************************************************************************************************/

GN_API GNg::sTRANSF2D  GNg::Ts2D(0,0);            //全局默认变换参数
/*************************************************************************************************/

GN_API float GNg::TriWidthRatio(const sVEC2 *a,const sVEC2 *b,const sVEC2 *c){
	GNg::sVEC2 v1 = (*b) - (*a), v2 = (*c) - (*b), v3 = (*a) - (*c); 
	GNg::sVEC2* _v[3] = { &v1, &v2, &v3 }; 
	float lens[3] = { GN_Vec2Dot(v1. , v1.), GN_Vec2Dot(v2. , v2.), GN_Vec2Dot(v3. , v3.) };
	uint8 l, m, s;
	GN_Sequence3(lens, &l, &m, &s);
	GNg::sVEC2 rv;
	GNg::Vec2Right(&rv, _v[l]);
	return GNg::Vec2Dot( &rv , _v[l < 2 ? l + 1 : 0] ) / (-lens[l]);
}
GN_API bool GNg::TriEnsure(sVEC2 &a, sVEC2 &b, sVEC2 &c, float d){
	sVEC2 c_a = c - a, b_a = b - a, ca1;
	Vec2Right(&ca1, &c_a);
	if(GN_Vec2Dot( ca1. , b_a. ))return false;
	else{ //成一直线
		float *ofsv;
		uint8 aid;
		if(ca1.x || ca1.y){
			ca1.MaxAbs(&aid);
			ofsv = (&(b.x)) + (int)aid;
		}else{
			b_a.MinAbs(&aid);
			ofsv = (&(c.x)) + (int)aid;
		}
		int cbi = (int)(*ofsv);
		(*ofsv) += (*ofsv) < (float)cbi + 0.5f ? d : -d;
		return true;
	}
}
GN_API GNg::sTRI2D GNg::Tri2D;  //3角形

void GNg::sTRI_SPACE2D::Init(const sVEC2* a, const sVEC2* b, const sVEC2* c, bool to_unit) {
	GNg::TriEdgeDirGet(a, b, c, &na, &da); 
	united = to_unit; 
	if (to_unit) {
		GNg::Vec2Normalize(&na, &na);
		GNg::Vec2Normalize(&nb, &nb);
		GNg::Vec2Normalize(&nc, &nc);
		da = na.Dot(a); db = nb.Dot(b); dc = nc.Dot(c);
	}
}
bool GNg::sTRI_SPACE2D::TestInclude(const sVEC2* vex) {
	if (GN_Vec2Dot(vex->, na.) > da)return false;
	if (GN_Vec2Dot(vex->, nb.) > db)return false;
	if (GN_Vec2Dot(vex->, nc.) > dc)return false;
	return true;
}
float GNg::sTRI_SPACE2D::NearDist(const sVEC2 *vex, uint8 *ne_type_out){
   #define GT_TMP_POINT_TO_TRI_NEAR_DIST_CPT /*三角形和点最近距离计算*/\
	float d0 = GN_Vec2Dot( vex-> , na. ) - da;\
    float d1 = GN_Vec2Dot( vex-> , nb. ) - db;\
    float d2 = GN_Vec2Dot( vex-> , nc. ) - dc;\
	if(!united){\
		d0 /= GNg::Vec2Length(&na);\
		d1 /= GNg::Vec2Length(&nb);\
		d2 /= GNg::Vec2Length(&nc);\
	}
    GT_TMP_POINT_TO_TRI_NEAR_DIST_CPT
	if(ne_type_out){
	    float max_d;
		if(d1 > d0)   { max_d = d1; *ne_type_out = eTRI_NEAR_EDGE1; }
		else          { max_d = d0; *ne_type_out = eTRI_NEAR_EDGE0; }
		if(d2 > max_d){ max_d = d2; *ne_type_out = eTRI_NEAR_EDGE2; }
		return max_d;
	}else{
	    float max_d = d1 > d0 ? d1 : d0;
	    return d2 > max_d ? d2 : max_d;
    }
}
float GNg::sTRI_SPACE2D::NearRelation(const sTRI2D *tri, const sVEC2 *vex, float *vd_out, uint8 *v_abc_out, uint8 *e_abc_out, uint8 *ntype_out){
	//计算三角型靠近关系信息
	GT_TMP_POINT_TO_TRI_NEAR_DIST_CPT
	float dv[3] = { d0, d1, d2 };
	uint8 near_eid = dv[0] > dv[1] ? 0 : 1; //离最近的边序号
	if (dv[2] > dv[near_eid])near_eid = 2;
	uint8 far_eid = dv[0] < dv[1] ? 0 : 1;	//离最远的边序号
	if (dv[2] < dv[ far_eid]) far_eid = 2;
    float max_d = dv[near_eid];
    float min_d = dv[ far_eid];
    uint8 vid = GN_TriAbcLast( far_eid );
	float l = GNg::Vec2Dot( (&(tri->a))+(unsigned int)vid , (&na)+(unsigned int)far_eid ) - (&da)[far_eid];
	if(!united)l /= GNg::Vec2Length((&na)+(unsigned int)far_eid);
	
	*vd_out = GNg::Vec2Dist(vex, (&(tri->a))+(unsigned int)vid);
    *v_abc_out = vid;
    *e_abc_out = near_eid;
	{
        if(abs((*vd_out) / l) < 0.32f)*ntype_out = e_TRI_NEAR_VEX0 + vid;
		else{
			if(max_d < 0.0f){
                vid = GN_TriAbcLast( near_eid );
		        l = GNg::Vec2Dot( (&(tri->a))+(unsigned int)vid , (&na)+(unsigned int)near_eid ) - (&da)[near_eid];
		        if(!united)l /= GNg::Vec2Length((&na)+(unsigned int)near_eid);
                if(max_d / l < 0.32f)*ntype_out = /*eTRI_NEAR_EDGE0 + */near_eid;
				else *ntype_out = eTRI_NEAR_FACE;
			}else *ntype_out = /*eTRI_NEAR_EDGE0 + */near_eid;
		}
	}
	return max_d;
}
#define GT_TMP_TRI_COMPUTE_COS( X ) \
	sVEC2 vb_a = (*b) - (*a), vc_a = (*c) - (*a), vc_b = (*c) - (*b);\
	Vec2Normalize(&vb_a, &vb_a);\
    Vec2Normalize(&vc_a, &vc_a);\
    Vec2Normalize(&vc_b, &vc_b);\
    *cos_a = GN_Vec2Dot( vb_a., vc_a.);\
	*cos_b =-GN_Vec2Dot( vb_a., vc_b.);\
	*cos_c = GN_Vec2Dot( vc_a., vc_b.)

GN_API void GNg::TriRadians(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *ra, float *rb, float *rc){
	sVEC2 vb_a = (*b) - (*a), vc_a = (*c) - (*a), vc_b = (*c) - (*b);
	*ra = Vec2Radian(&vb_a, &vc_a); vb_a = -vb_a;
	*rb = Vec2Radian(&vb_a, &vc_b);
	*rc = GT_ANGLE_180 - ((*ra) + (*rb));
}
GN_API void GNg::TriAngles(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *ag_a, float *ag_b, float *ag_c){
	float ra, rb, rc, inv1 = 1.0f / GT_ANGLE_UNIT;
	GNg::TriRadians(a, b, c, &ra, &rb, &rc);
	*ag_a = ra * inv1;
	*ag_b = rb * inv1;
	*ag_c = rc * inv1;
}
GN_API void GNg::TriCoss(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *cos_a, float *cos_b, float *cos_c){
	GT_TMP_TRI_COMPUTE_COS( X );
}
GN_API void GNg::TriCosSins(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *cos_a, float *cos_b, float *cos_c, float *sin_a, float *sin_b, float *sin_c){
	GT_TMP_TRI_COMPUTE_COS( X );
    sVEC2 tmp = vc_a - (vb_a * (*cos_a));
    *sin_a = Vec2Length(&tmp);
                   tmp = vc_b + (vb_a * (*cos_b));
    *sin_b = Vec2Length(&tmp);
                   tmp = vc_a - (vc_b * (*cos_c));
    *sin_c = Vec2Length(&tmp);
}
GN_API void GNg::TriCosRadians(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, float *cos_a, float *cos_b, float *cos_c, float *ra, float *rb, float *rc){
	GT_TMP_TRI_COMPUTE_COS( X );
    *ra = acosf(*cos_a);
	*rb = acosf(*cos_b);
	*rc = GT_ANGLE_180 - ((*ra) + (*rb));
}
GN_API float GNg::TriGetMaxEdgeLen2(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){
	sVEC2 ea = (*b) - (*a), eb = (*c) - (*b) , ec = (*a) - (*c);
	float da = GN_Vec2Dot( ea. , ea. ), db = GN_Vec2Dot( eb. , eb. ), dc = GN_Vec2Dot( ec. , ec. );
	if(db > da)da = db;
	return da > dc ? da : dc;
}
/*********************************************************** 2D圆 ****************************************************************/
GN_API GNg::sSPHERE2D GNg::Sphere2D(0.0f,0.0f,128.0f);

/*********************************************************** 2D矩形筐 ************************************************************/
int GNg::sRECT::TestInThere(const int *px, int *py, int pn){
    int i;
	for(i = 0; i < pn && px[i] < left;   ++i);
    if(i >= pn)return 1;
    for(i = 0; i < pn && py[i] < top;    ++i);
    if(i >= pn)return 2;
    for(i = 0; i < pn && px[i] > right;  ++i);
    if(i >= pn)return 4;
    for(i = 0; i < pn && py[i] > bottom; ++i);
    if(i >= pn)return 5;
    return 0;
}
GN_API GNg::sRECT  GNg::Rect(0, 0, 640, 480);

int GNg::sRECT2::TestInThere(const sVEC2 *vexs, int pn){
    int i;
	for(i = 0; i < pn && vexs[i].x < left;   ++i);
    if(i >= pn)return 1;
    for(i = 0; i < pn && vexs[i].y < top;    ++i);
    if(i >= pn)return 2;
    for(i = 0; i < pn && vexs[i].x > right;  ++i);
    if(i >= pn)return 4;
    for(i = 0; i < pn && vexs[i].y > bottom; ++i);
    if(i >= pn)return 5;
    return 0;
}
GN_API GNg::sRECT2  GNg::Rect2D(0.0f, 0.0f, 640.0f, 480.0f);

GN_API void GNg::RectAdd(sRECT *rc1,  const sRECT *rc2){
   #define GT_TMP_RECT_ADD \
    if(rc2->left   < rc1->left)  rc1->left   = rc2->left;\
	if(rc2->top    < rc1->top)   rc1->top    = rc2->top;\
	if(rc2->right  > rc1->right) rc1->right  = rc2->right;\
	if(rc2->bottom > rc1->bottom)rc1->bottom = rc2->bottom

	GT_TMP_RECT_ADD;
}
GN_API void GNg::RectAdd(sRECT2 *rc1,const sRECT2 *rc2){
    GT_TMP_RECT_ADD;
}
GN_API void GNg::RectAdd(sRECT   *out,const sRECT   *rc1,const sRECT   *rc2){
   #define GT_TMP_RECT_ADD2 \
    out->left   = (rc1->left  < rc2->left)   ? rc1->left  : rc2->left;\
	out->top    = (rc1->top   < rc2->top )   ? rc1->top   : rc2->top;\
    out->right  = (rc1->right > rc2->right ) ? rc1->right : rc2->right;\
	out->bottom = (rc1->bottom> rc2->bottom )? rc1->bottom: rc2->bottom

	GT_TMP_RECT_ADD2;
}
GN_API void GNg::RectAdd(sRECT2 *out,const sRECT2 *rc1,const sRECT2 *rc2){
    GT_TMP_RECT_ADD2;
}
GN_API void GNg::RectOverlap(sRECT *out,const sRECT *rc1,const sRECT *rc2){
   #define GT_TMP_RECT_OVERLAP \
	out->left   = (rc1->left  > rc2->left)   ? rc1->left  : rc2->left;\
	out->top    = (rc1->top   > rc2->top )   ? rc1->top   : rc2->top;\
    out->right  = (rc1->right < rc2->right ) ? rc1->right : rc2->right;\
	out->bottom = (rc1->bottom< rc2->bottom )? rc1->bottom: rc2->bottom

	GT_TMP_RECT_OVERLAP;
}
GN_API void GNg::RectOverlap(sRECT2 *out,const sRECT2 *rc1,const sRECT2 *rc2){
    GT_TMP_RECT_OVERLAP;
}
GN_API bool GNg::RectIsFront(const sRECT *rc){
    if(rc->right < rc->left || rc->bottom < rc->top)return false;
	return true;
}
GN_API bool GNg::RectIsFront(const sRECT2 *rc){
    if(rc->right < rc->left || rc->bottom < rc->top)return false;
	return true;
}
GN_API bool GNg::RectSimilar(const sRECT   *rc1, const sRECT   *rc2){
	int gap = (rc1->right - rc1->left) + (rc1->bottom - rc1->top);
	return GN_MemCompareSimilar((int*)rc1, (int*)rc2, 4, gap ? (gap/22) : 0);
}
GN_API bool GNg::RectSimilar(const sRECT2 *rc1, const sRECT2 *rc2){
	return GN_MemCompareFloat((float*)rc1, (float*)rc2, 4, ((rc1->right - rc1->left) + (rc1->bottom - rc1->top)) * 0.045f);
}
GN_API void GNg::RectMakeFromP2(sRECT   &rc, int p1x,   int p1y,   int p2x,   int p2y){
   #define GT_TMP_RECT_MAKE \
	if(p1x <= p2x){ rc.left = p1x; rc.right = p2x; }\
	else		  { rc.left = p2x; rc.right = p1x; }\
	if(p1y <= p2y){ rc.top = p1y; rc.bottom = p2y; }\
	else		  { rc.top = p2y; rc.bottom = p1y; }

    GT_TMP_RECT_MAKE
}
GN_API void GNg::RectMakeFromP2(sRECT2 &rc, float p1x, float p1y, float p2x, float p2y){
    GT_TMP_RECT_MAKE
}
GN_API void GNg::RectMakeFromP2(sRECT2 &rc, const sVEC2 &p1, const sVEC2 &p2){
    RectMakeFromP2(rc, p1.x, p1.y, p2.x, p2.y);
}
GN_API void GNg::RectToSquare(sRECT &rc){
	int r1 = (rc.right - rc.left) >> 1, r2 = (rc.bottom - rc.top) >> 1;
	int max_r = r1 > r2 ? r1 : r2;
	rc.left = rc.left + r1 - max_r;  rc.right  = rc.right  - r1 + max_r;
	rc.top  = rc.top  + r2 - max_r;	 rc.bottom = rc.bottom - r2 + max_r;
}
GN_API void GNg::RectToSquare(sRECT2 &rc){
	float r1 = (rc.right - rc.left) * 0.5f, r2 = (rc.bottom - rc.top) * 0.5f;
	float max_r = r1 > r2 ? r1 : r2;
	rc.left = rc.left + r1 - max_r;  rc.right  = rc.right  - r1 + max_r;
	rc.top  = rc.top  + r2 - max_r;	 rc.bottom = rc.bottom - r2 + max_r;
}
GN_API void GNg::RectGetCentre(const sRECT  &rc, int &cx, int &cy){
    cx = (rc.left + rc.right)  >> 1;
	cy = (rc.top  + rc.bottom) >> 1;
}
GN_API void GNg::RectGetCentre(const sRECT2 &rc, sVEC2 &c){
    c.x = (rc.left + rc.right)  * 0.5f;
	c.y = (rc.top  + rc.bottom) * 0.5f;
}
GN_API void GNg::RectGetAnglePoint(const sRECT &rc, int x[4], int y[4]){
    x[0] = rc.left,  y[0] = rc.top;
    x[1] = rc.right, y[1] = rc.top;
	x[2] = rc.right, y[2] = rc.bottom;
	x[3] = rc.left,  y[3] = rc.bottom;
}
GN_API void GNg::RectGetAnglePoint(const sRECT2 &rc, sVEC2 ap[4]){
    ap->x   = rc.left,  ap->y   = rc.top;
    ap[1].x = rc.right, ap[1].y = rc.top;
	ap[2].x = rc.right, ap[2].y = rc.bottom;
	ap[3].x = rc.left,  ap[3].y = rc.bottom;
}
GN_API void GNg::RectGetNearPoint(const sRECT   &rc, int &npx, int &npy, int vx, int vy){
    npx = vx > 0 ? rc.left : rc.right;
    npy = vy > 0 ? rc.top  : rc.bottom;
}
GN_API void GNg::RectGetNearPoint(const sRECT2 &rc, GNg::sVEC2 &np, const GNg::sVEC2 &v){
    np.x = v.x > 0.0f ? rc.left : rc.right;
    np.y = v.y > 0.0f ? rc.top : rc.bottom;
}
GN_API int GNg::RectCutLine(const sRECT *rc, int *p1x, int *p1y, int *p2x, int *p2y){
	sVEC2 p1((float)(*p1x),(float)(*p1y)), p2((float)(*p2x),(float)(*p2y));
    sRECT2 rc2d((float)rc->left, (float)rc->top, (float)rc->right, (float)rc->bottom);
    int ret = RectCutLine(&rc2d, &p1, &p2);
	if(ret == -1){
	    *p1x = p1.x; *p1y = p1.y;
		*p2x = p2.x; *p2y = p2.y;
	}
	return ret;
}
GN_API int GNg::RectCutLine(const sRECT2 *rc, sVEC2 *lp1, sVEC2 *lp2){
    int ret = -1;
	float left = rc->left, right = rc->right, bottom = rc->top, top = rc->bottom;
	float rt, _x, _y;
	GNg::sVEC2 v1_2 = (*lp2) - (*lp1);
	uint8 in = 0;
    
    //检测连线与矩型每个面的关系 GT_TMP_CHECK_RAY_INT_RECT_PLANE
   #define GT_TMP_CHECK_RAY_INT_RECT2D_PLANE( PROC , P1 , P2 , X , LEFT , Y , BOTTOM , TOP , CMP ) \
	if( P1 -> X  CMP  LEFT ){\
	    if( P2 -> X  CMP  LEFT )return 0;\
		rt = ( LEFT - P1 -> X ) / v1_2. X ;\
		_##Y = rt * v1_2. Y  + P1 -> Y ;\
		if(_##Y < BOTTOM || _##Y > TOP );\
		else{\
           P1 -> X = LEFT ;\
		   P1 -> Y = _##Y ;\
		   PROC ;\
		}\
	}else ++in

    //检测连线与矩型各个面的关系
   #define GT_TMP_CHECK_RAY_INT_RECT2D( PROC_ , P_1 , P_2 ) \
    GT_TMP_CHECK_RAY_INT_RECT2D_PLANE ( PROC_ , P_1 , P_2 , x , left ,   y , bottom , top , < ); /*测试点在-X面的前面*/\
	GT_TMP_CHECK_RAY_INT_RECT2D_PLANE ( PROC_ , P_1 , P_2 , y , bottom , x , left , right , < ); /*测试点在-Y面的前面*/\
    GT_TMP_CHECK_RAY_INT_RECT2D_PLANE ( PROC_ , P_1 , P_2 , x , right ,  y , bottom , top , > ); /*测试点在 X面的前面*/\
	GT_TMP_CHECK_RAY_INT_RECT2D_PLANE ( PROC_ , P_1 , P_2 , y , top ,    x , left , right , > ); /*测试点在 Y面的前面*/\

	GT_TMP_CHECK_RAY_INT_RECT2D( goto GT_GOTO_CheckNextRayIntRect2D , lp1 , lp2 );
	if(in >= 4){ //点在方盒里面
		ret = 1;
        GT_GOTO_CheckNextRayIntRect2D:
		in = 0;
        v1_2 = (*lp1) - (*lp2);
		GT_TMP_CHECK_RAY_INT_RECT2D( return -1 , lp2 , lp1 ); //再反过来用另一点去连线检测
        /*if(in >= 4)*/return ret; //点在方盒里面
	}
	return 0;
}
GN_API unsigned short GNg::ComputeRectNearFromDirection(int w, int h, const sVEC2 &vNz){
	if(vNz.x){
		float vx = vNz.x > 0.0f ? vNz.x : -vNz.x;
		float xr = (float)w / vx;
        float hy = (float)h / xr;
		if     (vNz.y < 0.0f && vNz.y < -hy)return GNu::eNEAR_TOP;
		else if(vNz.y > 0.0f && vNz.y >  hy)return GNu::eNEAR_BOTTOM;
		else{
			 if(vNz.x < 0.0f			   )return GNu::eNEAR_LEFT;
		     else							return GNu::eNEAR_RIGHT;
		}
	}else{
			 if(vNz.y < 0.0f			   )return GNu::eNEAR_TOP;
			 else							return GNu::eNEAR_BOTTOM;
	}
}
/******************************************************* 2D平行四边形 *****************************************************************/
#define GN_MIN_RANGE_BOX_SIZE   0.0005f

GN_API GNg::sQUADS2  GNg::Quads2D(0.0f,0.0f, 640.0f,0.0f, 640.0f,480.0f);

GN_API void GNg::RangeRectToQuads(sQUADS2 *q_out,const sRECT2 *rc_in){
	q_out->p0.x = rc_in->left;
	q_out->p0.y = rc_in->top;
	q_out->p1.x = rc_in->right;
	q_out->p1.y = rc_in->top;
	q_out->p2.x = rc_in->right;
	q_out->p2.y = rc_in->bottom;
}
GN_API void GNg::RangeQuadsToRect(sRECT2 *rc_out,/*const*/ sQUADS2 *q_in, sVEC2 *ap){
	sVEC2 *p0 = &(q_in->p0), *p1 = &(q_in->p1), *p2 = &(q_in->p2), tp;
	sVEC2 *p3 = ap ? ap : (&tp);
	*p3 = (q_in->p2 - q_in->p1) + q_in->p0;

    //统计3D矩型大小
   #define GT_TMP_RECT2D_SIZE_COUNTS( RC_NEAR , XY , CMP ) \
	RC_NEAR = p0-> XY;\
	if(p1-> XY CMP RC_NEAR ) RC_NEAR = p1-> XY;\
    if(p2-> XY CMP RC_NEAR ) RC_NEAR = p2-> XY;\
	if(p3-> XY CMP RC_NEAR ) RC_NEAR = p3-> XY;\

	float left, top, bottom, right;
    GT_TMP_RECT2D_SIZE_COUNTS( left   , x , < );
	GT_TMP_RECT2D_SIZE_COUNTS( top    , y , < );
    GT_TMP_RECT2D_SIZE_COUNTS( right  , x , > );
	GT_TMP_RECT2D_SIZE_COUNTS( bottom , y , > );

    rc_out->left   = left;
	rc_out->top    = top;
    rc_out->right  = right;
	rc_out->bottom = bottom;
}
GN_API void GNg::RangeQuadsGetCentre(const sQUADS2 &q_in, GNg::sVEC2 &c){
    c = (q_in.p0 + q_in.p2) * 0.5f;
}
GN_API float GNg::RangeQuadsGetMaxEdgeLen(const sQUADS2 *q_in){
   #define GT_TMP_GET_QUADS_EDGE_LEN2D( CMP ) \
    sVEC2 p1_0 = q_in->p1 - q_in->p0, p2_1 = q_in->p2 - q_in->p1;\
	float len1 = Vec2Length(&p1_0),\
		  len2 = Vec2Length(&p2_1);\
    if(len2 CMP len1)len1 = len2;\
	return len1
	GT_TMP_GET_QUADS_EDGE_LEN2D( > );
}
GN_API float GNg::RangeQuadsGetMinEdgeLen(const sQUADS2 *q_in){
   GT_TMP_GET_QUADS_EDGE_LEN2D( < );
}
GN_API void GNg::RangeQuadsGetAnglePoint(const sQUADS2 &q, sVEC2 ap[4]){
    ap[0] = q.p0, ap[1] = q.p1, ap[2] = q.p2;
    ap[3] = q.p2 - q.p1 + q.p0;
}
GN_API void GNg::RangeQuadsGetAnglePointOther(const sQUADS2 &q, sVEC2 &ap){
    ap = q.p2 - q.p1 + q.p0;
}
GN_API void GNg::RangeQuadsTurnOrde(sQUADS2 &qs){
	sVEC2 qv2 = qs .p0 ;
	sVEC2 qv1 = qv2 - qs .p1 ;
    qs .p0 = qs .p1;
    qs .p1 = qv2;
    qs .p2 = qs .p2 + qv1;
}
GN_API uint8 GNg::RangeQuadsGetNearPIdFromVisualPlane2D(uint8 * p_ids/*[2]*/, const uint8 *nids/*[2]*/, uint8 nn){
	 /*模板代码:定义2D四边形面数据*/
	static const uint8 quadsFaceData2D[7][2] = { {255,255}, {3,0}, {0,1}, {255,255}, {1,2}, {2,3}, {255,255} }; //四边形面点索引数据
	uint8 i = 0, nid;

    /*模板代码:查找四边形面共享ID*/
	uint8 psns[4] = {0};
	for(; i < nn; ++i){ 
        nid = nids[i];
	    psns[quadsFaceData2D[nid][0]]++;
		psns[quadsFaceData2D[nid][1]]++;
	}
    i = 0;
	if(psns[ quadsFaceData2D[nid][0] ] >= nn)p_ids[i++] = quadsFaceData2D[nid][0];
    if(psns[ quadsFaceData2D[nid][1] ] >= nn)p_ids[i++] = quadsFaceData2D[nid][1];
	return i;
}
GN_API void GNg::RangeQuadsGetAdjPointID_2D(uint8 * p_ids/*[2]*/, uint8 pid){
    switch(pid){
	case 0: p_ids[0] = 1; p_ids[1] = 3;  return;
	case 1: p_ids[0] = 0; p_ids[1] = 2;  return;
	case 2: p_ids[0] = 3; p_ids[1] = 1;  return;
	case 3: p_ids[0] = 2; p_ids[1] = 0;  return;
	}
}
GN_API void GNg::RangeQuadsGetPlanePointID_2D(uint8 * p_ids/*[2]*/, uint8 nid){
	switch(nid){
	case 1: p_ids[0] = 0; p_ids[1] = 3; return;
	case 2: p_ids[0] = 3; p_ids[1] = 2; return;
	case 4: p_ids[0] = 2; p_ids[1] = 1; return;
	case 5: p_ids[0] = 1; p_ids[1] = 0; return;
	}
}
GN_API uint8 GNg::RangeQuadsGetOppPointID_2D(uint8 pid){
    switch(pid){
	case  0:  return 2;
	case  1:  return 3;
	case  2:  return 0;
	default:  return 1;
	}
}
/********************************************************* sQUADS_SPACE2 ***********************************************************/
GNg::sQUADS_SPACE2::sQUADS_SPACE2(const sQUADS2 *q_in, bool to_unit){
   #define GT_TMP_COMPUTE_QUADS_SPACE2D \
    sVEC2 vx = q_in->p0 - q_in->p1,  vy = q_in->p2 - q_in->p1,  *p = (sVEC2*)(&q_in->p0);\
    \
	Vec2Right(&n0 , &vy); if(GN_Vec2Dot( n0. , vx. ) < 0.0f)n0 = -n0;\
	if(to_unit)Vec2Normalize(&n0 , &n0 );\
	d0  = GN_Vec2Dot( n0. , p-> );\
    p = (sVEC2*)(&q_in->p1);\
    d_0 = GN_Vec2Dot( n0. , p-> );\
	\
    Vec2Right(&n1 , &vx); if(GN_Vec2Dot( n1. , vy. ) > 0.0f)n1 = -n1;\
	if(to_unit)Vec2Normalize(&n1 , &n1 );\
	d1  = GN_Vec2Dot( n1. , p-> );\
    p = (sVEC2*)(&q_in->p2);\
	d_1 = GN_Vec2Dot( n1. , p-> )

    GT_TMP_COMPUTE_QUADS_SPACE2D;
}
void GNg::sQUADS_SPACE2::Init(const sQUADS2 *q_in, bool to_unit){
    GT_TMP_COMPUTE_QUADS_SPACE2D;
}
void GNg::sQUADS_SPACE2::Init(const sRECT2 *rc_int){
	n0.Init(-1.0f,0.0f);
	n1.Init(0.0f,-1.0f);
	d0 = -rc_int->left;  d_0 = -rc_int->right;
	d1 = -rc_int->top;   d_1 = -rc_int->bottom;
}
void GNg::sQUADS_SPACE2::Normalize(){
	float l0 = Vec2Length(&n0), l1 = Vec2Length(&n1);
    n0 /= l0;  d0 /= l0; d_0 /= l0;
	n1 /= l1;  d1 /= l1; d_1 /= l1;
}
void GNg::sQUADS_SPACE2::Offset(const sVEC2 *v){
    float d =  GN_Vec2Dot( n0. , v-> );
	d0 += d; d_0 += d;
	      d =  GN_Vec2Dot( n1. , v-> );
	d1 += d; d_1 += d;
}
void GNg::sQUADS_SPACE2::UpdateD(const sQUADS2 *q_in){
	const sVEC2 *_p = &q_in->p0;
	d0 = GN_Vec2Dot( n0. , _p-> );
	d1 = GN_Vec2Dot( n1. , _p-> );
				_p = &q_in->p2;
	d_0= GN_Vec2Dot( n0. , _p-> );
	d_1= GN_Vec2Dot( n1. , _p-> );
}
int GNg::sQUADS_SPACE2::TestNearThere(const sVEC2 *p, float *d_out){
    float d, d_min = GN_FLOAT_MAX;
	int ret = 1;

   #define GT_TMP_QUADS_SPACE_NEAR_THERE( ID , MIN , MAX ) \
    d = GN_Vec2Dot( n##ID. , p-> );\
	if( d > d##ID  )d = d - d##ID ;\
	else            d = d##ID - d ;\
	if( d < d_min ){\
		d_min = d;\
	    ret = MIN ;\
	}\
    if( d > d_##ID  )d = d - d_##ID ;\
	else             d = d_##ID - d ;\
	if( d < d_min ){\
		d_min = d;\
	    ret = MAX ;\
	}
	GT_TMP_QUADS_SPACE_NEAR_THERE( 0 , 1 , 4 );
	GT_TMP_QUADS_SPACE_NEAR_THERE( 1 , 2 , 5 );
	if(d_out)*d_out = d_min;
    return ret;
}
int GNg::sQUADS_SPACE2::TestNearThere_(const sVEC2 *p, float *d_out){
    float d, d_min = GN_FLOAT_MAX;
	int ret = 0;

   #define GT_TMP_QUADS_SPACE_NEAR_THERE_( ID , MIN , MAX ) \
    d = GN_Vec2Dot( n##ID. , p-> );\
	if( d > d##ID  ){\
		d = d - d##ID ;\
		if(d < d_min){\
			d_min = d;\
			ret = MIN ;\
		}\
	}else if( d < d_##ID ){\
		d = d##ID - d ;\
		if( d < d_min ){\
		    d_min = d;\
			ret = MAX ;\
		}\
	}
	GT_TMP_QUADS_SPACE_NEAR_THERE_( 0 , 1 , 4 );
	GT_TMP_QUADS_SPACE_NEAR_THERE_( 1 , 2 , 5 );
	if(d_out)*d_out = d_min;
    return ret;
}
int GNg::sQUADS_SPACE2::TestFarThere(const sVEC2 *p, float *d_out){
    float d, d_, d_max = GN_FLOAT_MIN;
	int ret = 1;

   #define GT_TMP_QUADS_SPACE_FAR_THERE( ID , MIN , MAX ) \
    d = GN_Vec2Dot( n##ID. , p-> );\
    d_ = d - d##ID;\
	if(d_ > d_max){\
        d_max = d_;\
        ret = MIN ;\
	}\
	d_ = d_##ID - d;\
	if(d_ > d_max){\
        d_max = d_;\
        ret = MAX ;\
	}
	GT_TMP_QUADS_SPACE_FAR_THERE( 0 , 1 , 4 );
	GT_TMP_QUADS_SPACE_FAR_THERE( 1 , 2 , 5 );
	if(d_out)*d_out = d_max;
    return ret;
}
int GNg::sQUADS_SPACE2::TestInThere(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){
	const sVEC2 *vexs[3] = { a, b, c };
	const int pn = 3;
	float d;
	int i, ret;
   #define GT_TMP_QUADS_SPACE_CMP_THERE( ID , MIN , MAX , DD , DT , PROC ) \
	for(ret = i = 0; i < pn; ++i){\
	    DD = GN_Vec2Dot( n##ID. , vexs[i] DT );\
		if      (ret == MIN ){\
            if( DD <= d##ID  )break;\
		}else if(ret == MAX ){\
            if( DD >= d_##ID )break;\
		}else{\
	        if     ( DD > d##ID  )ret = MIN ;\
			else if( DD < d_##ID )ret = MAX ;\
			else break;\
		}\
	}\
	if(i >= pn)PROC
    GT_TMP_QUADS_SPACE_CMP_THERE( 0 , 1 , 4 , d , -> , return ret);
	GT_TMP_QUADS_SPACE_CMP_THERE( 1 , 2 , 5 , d , -> , return ret);
    return 0;
}
int GNg::sQUADS_SPACE2::TestInThere(const sVEC2 *vexs, int pn){
	float d;
	int i, ret;
    GT_TMP_QUADS_SPACE_CMP_THERE( 0 , 1 , 4 , d , . , return ret);
	GT_TMP_QUADS_SPACE_CMP_THERE( 1 , 2 , 5 , d , . , return ret);
    return 0;
}
void GNg::sQUADS_SPACE2::TestInThereEx(const sVEC2 *vexs, int pn, int *nids/*[2]*/, int *dns/*[2]*/, float *dss/*[2][pn]*/){
	float *ds1 = dss + pn;
	int i, ret;
	nids[0] = nids[1] = 0;
	GT_TMP_QUADS_SPACE_CMP_THERE( 0 , 1 , 4 , dss[i] , . , { nids[0] = ret; dns[0] = pn; }else dns[0] = i + 1 );
	GT_TMP_QUADS_SPACE_CMP_THERE( 1 , 2 , 5 , ds1[i] , . , { nids[1] = ret; dns[1] = pn; }else dns[1] = i + 1 );
}
int GNg::sQUADS_SPACE2::TestInclude(const sVEC2 *vexs, int pn){
    float d = GN_Vec2Dot( n0. , vexs[0]. );
	int i, ret = ( d > d0 || d < d_0 )? -1 : 1;

	for(i = 1 ; i < pn; ++i){
	    d = GN_Vec2Dot( n0. , vexs[i]. );
		if((( d > d0 || d < d_0 )? -1 : 1) != ret)return 0;
	}
    for(i = 0 ; i < pn; ++i){
	    d = GN_Vec2Dot( n1. , vexs[i]. );
		if((( d > d1 || d < d_1 )? -1 : 1) != ret)return 0;
	}
	return ret;
}
bool GNg::sQUADS_SPACE2::TestIncPoint(const sVEC2 *p){
    float d;
	d = GN_Vec2Dot( p-> , n0. ); if(d > d0 || d < d_0)return false;
	d = GN_Vec2Dot( p-> , n1. ); if(d > d1 || d < d_1)return false;
	return true;
}
float GNg::sQUADS_SPACE2::GetAreaVol(){
    return (d0 - d_0) * (d1 - d_1);
}
uint8 GNg::sQUADS_SPACE2::GetThinFace(float *thick_out){
    float t0 = d0 - d_0, t1 = d1 - d_1;
	if(t0 < t1){ *thick_out = t0; return 4; }
	else       { *thick_out = t1; return 5; }
}
uint8 GNg::sQUADS_SPACE2::GetThinFace(sVEC2 *tr_out){
    float t0 = d0 - d_0, t1 = d1 - d_1;
	if(t0 < t1){ tr_out->x = t1/t0; tr_out->y = 1.0f;  return 4; }
	else       { tr_out->x = 1.0f;  tr_out->y = t0/t1; return 5; }
}
uint8 GNg::sQUADS_SPACE2::GetLookNearD(const sVEC2 *v, /*const*/ GNg::sVEC2 *ap, float *d_out){
	sVEC2 *_p;
	float near_dist, d;
	uint8 near_id;
	if(GN_Vec2Dot( v-> , n0. ) > 0.0f){
        _p = ap + 1; near_dist = GN_Vec2Dot( v-> , _p-> ); near_id = 1;
		_p = ap + 2; d = GN_Vec2Dot( v-> , _p-> ); if(d < near_dist){ near_dist = d; near_id = 2; }
	}else{
        _p = ap    ; near_dist = GN_Vec2Dot( v-> , _p-> ); near_id = 0;
		_p = ap + 3; d = GN_Vec2Dot( v-> , _p-> ); if(d < near_dist){ near_dist = d; near_id = 3; }
	}
	*d_out = near_dist;
	return near_id;
}
uint8 GNg::sQUADS_SPACE2::GetLookFace(const sVEC2 *v, sVEC2 *n_out, float *d_out, bool ret_near){
	float n0d = GN_Vec2Dot(n0. , v-> );
	float n1d = GN_Vec2Dot(n1. , v-> );
	float n0d_ = n0d < 0.0f ? -n0d : n0d;
	float n1d_ = n1d < 0.0f ? -n1d : n1d;

    uint8 nid;
	if(ret_near){
		if(n0d_ > n1d_){ if(n0d < 0.0f){ *n_out = n0; *d_out = d0; nid = 1; }else{ *n_out = -n0; *d_out = -d_0; nid = 4; } }
		else           { if(n1d < 0.0f){ *n_out = n1; *d_out = d1; nid = 2; }else{ *n_out = -n1; *d_out = -d_1; nid = 5; } }
	}else{
	    if(n0d_ > n1d_){ if(n0d < 0.0f){ *n_out = n0; *d_out = d_0; nid = 4; }else{ *n_out = -n0; *d_out = -d0; nid = 1; } }
		else           { if(n1d < 0.0f){ *n_out = n1; *d_out = d_1; nid = 5; }else{ *n_out = -n1; *d_out = -d1; nid = 2; } }
	}
	return nid;
}
void GNg::sQUADS_SPACE2::GetExternFaceFB(const sVEC2 *v, sVEC2 *n_outs/*[2]*/, float *d_outs/*[2]*/, uint8 *nid_outs/*[2]*/, bool ret_front){
    bool e0 = GN_Vec2Dot( v-> , n0. ) > 0.0f;
	bool e1 = GN_Vec2Dot( v-> , n1. ) > 0.0f;
	if(ret_front){
        if(e0){ n_outs[0] = -n0; d_outs[0] = -d_0; nid_outs[0] = 4; }else{ n_outs[0] = n0; d_outs[0] = d0; nid_outs[0] = 1; }
		if(e1){ n_outs[1] = -n1; d_outs[1] = -d_1; nid_outs[1] = 5; }else{ n_outs[1] = n1; d_outs[1] = d1; nid_outs[1] = 2; }
	}else{
		if(e0){ n_outs[0] = n0; d_outs[0] = d0; nid_outs[0] = 1; }else{ n_outs[0] = -n0; d_outs[0] = -d_0; nid_outs[0] = 4; }
		if(e1){ n_outs[1] = n1; d_outs[1] = d1; nid_outs[1] = 2; }else{ n_outs[1] = -n1; d_outs[1] = -d_1; nid_outs[1] = 5; }
	}
}
float GNg::sQUADS_SPACE2::GetNormal(sVEC2 *n_out, int id, bool inside){
	if(inside){
		switch(id){
		case 1:*n_out = -n0; return -d0;
		case 2:*n_out = -n1; return -d1;
		case 4:*n_out =  n0; return  d_0;
		case 5:*n_out =  n1; return  d_1;
		}
	}else{
	    switch(id){
		case 1:*n_out =  n0; return  d0;
		case 2:*n_out =  n1; return  d1;
		case 4:*n_out = -n0; return -d_0;
		case 5:*n_out = -n1; return -d_1;
		}
	}
	return 0.0f;
}
void GNg::sQUADS_SPACE2::GetAnglePoint(sVEC2 *ap){
	/*模版代码:按四边形空间面来获回它的边角点*/
	sVEC2 n, _n, p;
	Vec2Right(&_n, &n1);    //0 -> 1 的方向
    p = n1 * d1;
	float rt = GN_Vec2Dot( n0. , _n. );
	float t1 = ( d0  - GN_Vec2Dot( n0. , p. )) / rt; /*射粒点到平面n0,计算t的有效距离*/
    ap[0] = t1 * _n + p;         //计算得到角点0
          t1 = ( d_0 - d0) / rt;
    ap[1] = t1 * _n + ap[0];     //计算得到角点1

	n = -n0;
    Vec2Right(&_n, &n);     //1 -> 2 的方向
          rt = GN_Vec2Dot( n1. , _n. );
          t1 = ( d_1 - d1) / rt; /*射粒点到平面n1,计算t的有效距离*/
    ap[2] = t1 * _n + ap[1];     //计算得到角点2

    RangeQuadsGetAnglePointOther(*((sQUADS2*)ap), ap[3]);
}
void GNg::sQUADS_SPACE2::GetAnglePoint_1(sQUADS2 *q_out){
    q_out->p0 = n0 * d0 + n1 * d1;
    q_out->p1 = q_out->p0 + (d_0 - d0) * n0;
    q_out->p2 = q_out->p1 + (d_1 - d1) * n1;
}
GNg::sVEC2 GNg::sQUADS_SPACE2::GetCentre(){
	return n0 * ((d0 + d_0) * 0.5f) + n1 * ((d1 + d_1) * 0.5f);
}
void GNg::sQUADS_SPACE2::Expand(float size){
	d0 += size; d_0 -= size;
	d1 += size; d_1 -= size;
}
void GNg::sQUADS_SPACE2::Expand(sVEC2 *vexs, int pn, bool clear){
	if(clear){
		d0 = GN_FLOAT_MIN; d_0 = GN_FLOAT_MAX;
		d1 = GN_FLOAT_MIN; d_1 = GN_FLOAT_MAX;
	}
    sVEC2 *v_end = vexs + pn;
	float d;
	for(; vexs < v_end; ++vexs){
		d = GN_Vec2Dot( vexs -> , n0. );
		if(d > d0)d0 = d;
		else if(d < d_0)d_0 = d;
		d = GN_Vec2Dot( vexs -> , n1. );
		if(d > d1)d1 = d;
		else if(d < d_1)d_1 = d;
	}
}
void GNg::sQUADS_SPACE2::Expand(sQUADS2 *q, bool clear){
	sVEC2 vex[4];
	RangeQuadsGetAnglePoint(*q, vex);
	Expand(vex, 4, clear);
}
bool GNg::sQUADS_SPACE2::HasExpand(sVEC2 *vexs, int pn){
	float d0b = d0, d_0b = d_0, d1b = d1, d_1b = d_1;
    Expand(vexs, pn);
	return d0b > d0 || d_0b < d_0 || d1b > d1|| d_1b < d_1;
}
/**************************************************** 3X3矩阵结构 ****************************************************************/
GNg::sMAT33 GNg::sMAT33::operator *(const sMAT33 &_tm)const{
    sMAT33 tm;
	tm.m11 = this->m11 * _tm.m11 + this->m12 * _tm.m21 + this->m13 * _tm.m31;
	tm.m12 = this->m11 * _tm.m12 + this->m12 * _tm.m22 + this->m13 * _tm.m32;
	tm.m13 = this->m11 * _tm.m13 + this->m12 * _tm.m23 + this->m13 * _tm.m33;

    tm.m21 = this->m21 * _tm.m11 + this->m22 * _tm.m21 + this->m23 * _tm.m31;
	tm.m22 = this->m21 * _tm.m12 + this->m22 * _tm.m22 + this->m23 * _tm.m32;
	tm.m23 = this->m21 * _tm.m13 + this->m22 * _tm.m23 + this->m23 * _tm.m33;

	tm.m31 = this->m31 * _tm.m11 + this->m32 * _tm.m21 + this->m33 * _tm.m31;
	tm.m32 = this->m31 * _tm.m12 + this->m32 * _tm.m22 + this->m33 * _tm.m32;
	tm.m33 = this->m31 * _tm.m13 + this->m32 * _tm.m23 + this->m33 * _tm.m33;
	return tm;
}
GN_API GNg::sMAT33 GNg::Matrix33(true);      //全局默认单位矩阵

/***************************************************** 变换实用函数 **************************************************************/
GN_API void GNg::MultTransf2D(sTRANSF2D *out,const sTRANSF2D *ts1,const sTRANSF2D *ts2){
	out->sx = ts1->sx * ts2->sx;
    out->sy = ts1->sy * ts2->sy;
	out->x  = ts1-> x * ts2->sx + ts2->x;
	out->y  = ts1-> y * ts2->sy + ts2->y;
	out->r  = ts1->r + ts2->r;
}
GN_API void GNg::MultTransf2D(sMAT33 *out, const sTRANSF2D *ts1, const sTRANSF2D *ts2){
	if(ts2->r){ //含有旋转
		sMAT33 tm;
		M33GetTransf2D(&tm, ts2);
		if(ts1->r){
			M33GetTransf2D(out, ts1);
			MultM33T(out, &tm);
		}else{
			out->m11 = tm.m11 * ts1->sx; out->m12 = tm.m12 * ts1->sx;
			out->m21 = tm.m21 * ts1->sy; out->m22 = tm.m22 * ts1->sy;
			out->m31 = GN_Vec21DotM33Col( ts1-> , tm. , 1 );
			out->m32 = GN_Vec21DotM33Col( ts1-> , tm. , 2 );
			out->Col3To001();
		}
	}else{
		M33GetTransf2D(out, ts1);
		out->m11 *= ts2->sx; out->m12 *= ts2->sy;
		out->m21 *= ts2->sx; out->m22 *= ts2->sy;
		out->m31  = out->m31 * ts2->sx + ts2->x;
		out->m32  = out->m32 * ts2->sy + ts2->y;
	}
}
GN_API void GNg::MultTransf2D(sMAT33 *out, const sTRANSF2D *ts1, const sMAT33 *mat2){
	if(mat2->m12 || mat2->m13 || mat2->m21 || mat2->m23){ //含有旋转
		if(ts1->r){
			M33GetTransf2D(out, ts1);
			MultM33L(out, mat2);
		}else{
			out->m11 = mat2->m11 * ts1->sx; out->m12 = mat2->m12 * ts1->sx;  out->m13 = mat2->m13 * ts1->sx;
			out->m21 = mat2->m21 * ts1->sy; out->m22 = mat2->m22 * ts1->sy;  out->m23 = mat2->m23 * ts1->sy;
			out->m31 = GN_Vec21DotM33Col( ts1-> , mat2-> , 1 );
			out->m32 = GN_Vec21DotM33Col( ts1-> , mat2-> , 2 );
			out->m33 = GN_Vec21DotM33Col( ts1-> , mat2-> , 3 );
		}
	}else{
		M33GetTransf2D(out, ts1);
		out->m11 *= mat2->m11; out->m12 *= mat2->m22;
		out->m21 *= mat2->m11; out->m22 *= mat2->m22;
		out->m31  = out->m31 * mat2->m11 + mat2->m31;
		out->m32  = out->m32 * mat2->m22 + mat2->m32;
		out->m33  =						   mat2->m33;
	}
}
GN_API void GNg::MultM33(sMAT33 *out,const sMAT33 *mat1,const sMAT33 *mat2){
	out->m11 = mat1->m11 * mat2->m11 + mat1->m12 * mat2->m21 + mat1->m13 * mat2->m31;
	out->m12 = mat1->m11 * mat2->m12 + mat1->m12 * mat2->m22 + mat1->m13 * mat2->m32;
	out->m13 = mat1->m11 * mat2->m13 + mat1->m12 * mat2->m23 + mat1->m13 * mat2->m33;

    out->m21 = mat1->m21 * mat2->m11 + mat1->m22 * mat2->m21 + mat1->m23 * mat2->m31;
	out->m22 = mat1->m21 * mat2->m12 + mat1->m22 * mat2->m22 + mat1->m23 * mat2->m32;
	out->m23 = mat1->m21 * mat2->m13 + mat1->m22 * mat2->m23 + mat1->m23 * mat2->m33;

	out->m31 = mat1->m31 * mat2->m11 + mat1->m32 * mat2->m21 + mat1->m33 * mat2->m31;
	out->m32 = mat1->m31 * mat2->m12 + mat1->m32 * mat2->m22 + mat1->m33 * mat2->m32;
	out->m33 = mat1->m31 * mat2->m13 + mat1->m32 * mat2->m23 + mat1->m33 * mat2->m33;
}
GN_API void GNg::MultM33T(sMAT33 *inout, const sMAT33 *mat2){
	sVEC2 vx = *((sVEC2*)&inout->m11), vy = *((sVEC2*)&inout->m21), vp = *((sVEC2*)&inout->m31);
	inout->m11 = GN_Vec2DotM33Col( vx. , mat2-> , 1 ); inout->m12 = GN_Vec2DotM33Col( vx. , mat2-> , 2 );
	inout->m21 = GN_Vec2DotM33Col( vy. , mat2-> , 1 ); inout->m22 = GN_Vec2DotM33Col( vy. , mat2-> , 2 );
	inout->m31 = GN_Vec21DotM33Col(vp. , mat2-> , 1 ); inout->m32 = GN_Vec21DotM33Col(vp. , mat2-> , 2 );
}
GN_API void GNg::MultM33L(sMAT33 *inout, const sMAT33 *mat2){
	sVEC2 vx = *((sVEC2*)&inout->m11), vy = *((sVEC2*)&inout->m21), vp = *((sVEC2*)&inout->m31);
	inout->m11 = GN_Vec2DotM33Col( vx. , mat2-> , 1 ); inout->m12 = GN_Vec2DotM33Col( vx. , mat2-> , 2 );  inout->m13 = GN_Vec2DotM33Col( vx. , mat2-> , 3 );
	inout->m21 = GN_Vec2DotM33Col( vy. , mat2-> , 1 ); inout->m22 = GN_Vec2DotM33Col( vy. , mat2-> , 2 );  inout->m23 = GN_Vec2DotM33Col( vy. , mat2-> , 3 );
	inout->m31 = GN_Vec21DotM33Col(vp. , mat2-> , 1 ); inout->m32 = GN_Vec21DotM33Col(vp. , mat2-> , 2 );  inout->m33 = GN_Vec21DotM33Col(vp. , mat2-> , 3 );
}
GN_API void GNg::M33MultMove(sMAT33 *mat, const sVEC2 *ofs, bool is_back){
	if(is_back){
		*((sVEC2*)&(mat->m31)) += *ofs;
	}else{
        mat->m31 += mat->m11 * ofs->x + mat->m21 * ofs->y;
		mat->m32 += mat->m12 * ofs->x + mat->m22 * ofs->y;
		mat->m33 += mat->m13 * ofs->x + mat->m23 * ofs->y;
	}
}
GN_API void GNg::M33MultMove1(sMAT33 *mat, float ofs, uint8 id, bool is_back){
	if(is_back){
		(&mat->m31)[id] += ofs;
	}else{
		id *= 3;
        mat->m31 += (&mat->m11)[id] * ofs;
		mat->m32 += (&mat->m12)[id] * ofs;
		mat->m33 += (&mat->m13)[id] * ofs;
	}
}
GN_API void GNg::M33MultScale(sMAT33 *mat, float sx, float sy, bool is_back){
	if(is_back){
		sVEC2 scl(sx,sy);
		*((sVEC2*)(&mat->m11)) = *((sVEC2*)(&mat->m11)) * scl;
		*((sVEC2*)(&mat->m21)) = *((sVEC2*)(&mat->m21)) * scl;
		*((sVEC2*)(&mat->m31)) = *((sVEC2*)(&mat->m31)) * scl;
	}else{
		mat->m11 *= sx; mat->m12 *= sx; mat->m13 *= sx;
		mat->m21 *= sy; mat->m22 *= sy; mat->m23 *= sy;
	}
}
GN_API void GNg::M33MultScale1(sMAT33 *mat, float scl, uint8 id, bool is_back){
	if(is_back){
		(&mat->m11)[id] *= scl;
		(&mat->m21)[id] *= scl;
		(&mat->m31)[id] *= scl;
	}else{
		float* _m = &mat->m11 + id * 3;
		_m[0] *= scl; _m[1] *= scl; _m[2] *= scl;
	}
}
GN_API void GNg::M33GetTransf2D(sMAT33 *w_out,const sTRANSF2D *ts){
	 w_out->Col3To001();
	 if(ts->r< -0.001f || ts->r > 0.001f){ //结合旋转
         float cosF = cosf(ts->r * GT_ANGLE_UNIT);
	     float sinF = sinf(ts->r * GT_ANGLE_UNIT); 
	     w_out->m11 = ts->sx *  cosF;
         w_out->m12 = ts->sx *  sinF;
	     w_out->m21 = ts->sy * -sinF;
	     w_out->m22 = ts->sy *  cosF;
	 }else{
	     w_out->m11 = ts->sx;
	     w_out->m22 = ts->sy;
		 w_out->m12 = 0.0f;
	     w_out->m21 = 0.0f;
	 }
	 w_out->m31 = ts->x; //结合平移
	 w_out->m32 = ts->y;
}
GN_API void GNg::M33GetMove(sMAT33 *w_out, float x, float y){
    w_out->Init(true);
    w_out->m31 = x;
    w_out->m32 = y;
}
GN_API void GNg::M33GetScale(sMAT33 *w_out, float sx, float sy){
    w_out->Init(true);
    w_out->m11 = sx;
    w_out->m22 = sy;
}
GN_API void GNg::M33GetRot(sMAT33 *w_out, float angle){
    w_out->Init(true);
	float f = angle * GT_ANGLE_UNIT;
	float f_cos = cosf(f), f_sin = sinf(f);
	w_out->m11 = f_cos;
	w_out->m12 = f_sin;
	w_out->m21 =-f_sin;
	w_out->m22 = f_cos;
}
GN_API void GNg::M33GetV2Col(sMAT33 *w_out, const sVEC2 *col1, const sVEC2 *col2){
    w_out->m11 = col1->x;
    w_out->m21 = col1->y;
	w_out->m31 = 0.0f;
	w_out->m12 = col2->x;
	w_out->m22 = col2->y;
	w_out->m32 = 0.0f;
	w_out->m13 = 0.0f;
	w_out->m23 = 0.0f;
	w_out->m33 = 1.0f;
}
GN_API void GNg::M33GetQusdsLocal(const sQUADS_SPACE2 &qs, sMAT33 &mat, bool ndc){
	float u = ndc ? 2.0f : 1.0f;
	float _d0_ = u / (qs.d0 - qs.d_0), _d1_ = u / (qs.d1 - qs.d_1);
	mat.m11 = -qs.n0.x * _d0_;     mat.m12 = -qs.n1.x * _d1_;	   mat.m13 = 0.0f;
	mat.m21 = -qs.n0.y * _d0_;     mat.m22 = -qs.n1.y * _d1_;	   mat.m23 = 0.0f;
	mat.m31 =  qs.d0   * _d0_;	   mat.m32 =  qs.d1   * _d1_;	   mat.m33 = 1.0f;
	if(ndc){ mat.m31 -= 1.0f; mat.m32 -= 1.0f; }
}
GN_API void GNg::M33GetTranspose(sMAT33 &m_out, const sMAT33 &m_in){
    m_out.m11 = m_in.m11, m_out.m12 = m_in.m21, m_out.m13 = m_in.m31;
	m_out.m21 = m_in.m12, m_out.m22 = m_in.m22, m_out.m23 = m_in.m32;
	m_out.m31 = m_in.m13, m_out.m32 = m_in.m23, m_out.m33 = m_in.m33;
}
GN_API void GNg::M33GetInverse(sMAT33 &m_out, sMAT33 &m_in){
    float r1[6], r2[6], r3[6];
	float *s[3], *tmprow;

	s[0] = r1;
	s[1] = r2;
	s[2] = r3;

	register int8 i, j, p, jj;
	for(i = 0; i < 3; ++i){
		for(j = 0;j < 3; ++j){
			s[i][j] = m_in(i,j);
			if(i == j)s[i][j+3] = 1.0f;
			else      s[i][j+3] = 0.0f;
		}
	}
	float scp[3];
	for(i = 0; i < 3; ++i){
		scp[i] = fabs(s[i][0]);
		for(j = 1; j < 3; ++j)
			if(fabs(s[i][j]) > scp[i]) scp[i] = fabs(s[i][j]);
			if(scp[i] == 0.0f) return;
	}
	int8 pivot_to;
	float scp_max, tmpscp, mji;
	for(i = 0; i < 3; ++i){
		pivot_to = i;
		scp_max = fabs(s[i][i]/scp[i]);
		for(p = i+1; p < 3; ++p)
			if(fabs(s[p][i] / scp[p]) > scp_max){ scp_max = fabs(s[p][i] / scp[p]); pivot_to = p; }
			if(pivot_to != i){
				tmprow = s[i];
				s[i] = s[pivot_to];
				s[pivot_to] = tmprow;
				tmpscp = scp[i];
				scp[i] = scp[pivot_to];
				scp[pivot_to] = tmpscp;
			}
			for(j = i+1; j < 3; ++j){
				mji = s[j][i] / s[i][i];
				s[j][i] = 0.0f;
				for(jj = i+1; jj < 6; ++jj)
					s[j][jj] -= mji * s[i][jj];
			}
	}
	if(s[2][2] == 0.0f)return;

	float mij;
	for(i = 2; i > 0; --i){
		for(j = i-1; j > -1; --j){
			mij = s[j][i] / s[i][i];
			for(jj = j+1; jj < 6; ++jj)
				s[j][jj] -= mij * s[i][jj];
		}
	}
	for(i = 0 ; i < 3; ++i)
		for(j = 0; j < 3; ++j)
			m_out(i,j) = s[i][j+3] / s[i][i];
}
GN_API void GNg::M33ScaleFactorGet(const sMAT33 *m_in, float *sx_out, float *sy_out){
	*sx_out = Vec2Length(((sMAT33*)m_in)->Row1()); /*看原标准轴向量的长度是多少就证明在那个方向放大多少陪了*/
	*sy_out = Vec2Length(((sMAT33*)m_in)->Row2());
}
GN_API void GNg::M33ToTransf2D(const sMAT33 *m_in, sTRANSF2D *ts_out){
	ts_out->sx = Vec2Length(((sMAT33*)m_in)->Row1()); /*看原标准轴向量的长度是多少就证明在那个方向放大多少陪了*/
	ts_out->sy = Vec2Length(((sMAT33*)m_in)->Row2());
	
	ts_out->x = m_in->m31, ts_out->y = m_in->m32;			   /*得到位置信息*/
    ts_out->r = atan2f( m_in->m12, m_in->m11) / GT_ANGLE_UNIT; /*得到旋转角度*/
}
GN_API void GNg::M33ToTransf2DMir(const sMAT33 *m_in, sTRANSF2D *ts_out){
	sVEC2 ax = *(((sMAT33*)m_in)->Row1());
	ts_out->sx = Vec2Length(&ax); /*看原标准轴向量的长度是多少就证明在那个方向放大多少陪了*/
	if(ax.x < 0.0f)ts_out->sx = -(ts_out->sx);
	ax /= ts_out->sx;
	ts_out->sy = (-ax.y) * m_in->m21 + (ax.x) * m_in->m22;

	ts_out->x = m_in->m31, ts_out->y = m_in->m32;			   /*得到位置信息*/
    ts_out->r = atan2f( ax.y, ax.x) / GT_ANGLE_UNIT; /*得到旋转角度*/
}
GN_API bool GNg::M44ToTransf2D(const sMAT33 *m_in, sTRANSF2D *ts_out, sMAT33 *residual){
    M33ToTransf2D(m_in, ts_out);
	if(ts_out->sx > 1.00001f || ts_out->sx < 0.99999f || ts_out->sy > 1.00001f || ts_out->sy < 0.99999f){ //有缩放
		M33GetTransf2D(residual, ts_out);
		sMAT33 tm; 
		M33GetInverse(tm, *residual);
        MultM33(residual, m_in , &tm);
        return true;
	}else return false;
}
GN_API void GNg::M33LinearRatio(sMAT33 *m_out, const sMAT33 *m_st, const sMAT33 *m_end, float ratio){
	m_out->m11 = m_st->m11 + (m_end->m11 - m_st->m11) * ratio;
	m_out->m12 = m_st->m12 + (m_end->m12 - m_st->m12) * ratio;
	m_out->m13 = m_st->m13 + (m_end->m13 - m_st->m13) * ratio;
						  
	m_out->m21 = m_st->m21 + (m_end->m21 - m_st->m21) * ratio;
	m_out->m22 = m_st->m22 + (m_end->m22 - m_st->m22) * ratio;
	m_out->m23 = m_st->m23 + (m_end->m23 - m_st->m23) * ratio;
						  
	m_out->m31 = m_st->m31 + (m_end->m31 - m_st->m31) * ratio;
	m_out->m32 = m_st->m32 + (m_end->m32 - m_st->m32) * ratio;
    m_out->m33 = m_st->m33 + (m_end->m33 - m_st->m33) * ratio;
}
GN_API void GNg::M33Linear(sMAT33 *m_out, const sMAT33 *m_st, const sMAT33 *m_end, unsigned int tm_time, unsigned int end_time, bool ign_last_row_col){
	m_out->m11 = GN_Linear( m_st->m11 , m_end->m11 , tm_time , end_time );
	m_out->m12 = GN_Linear( m_st->m12 , m_end->m12 , tm_time , end_time );
	m_out->m21 = GN_Linear( m_st->m21 , m_end->m21 , tm_time , end_time );
	m_out->m22 = GN_Linear( m_st->m22 , m_end->m22 , tm_time , end_time );
   if(!ign_last_row_col){
	m_out->m31 = GN_Linear( m_st->m31 , m_end->m31 , tm_time , end_time );
	m_out->m32 = GN_Linear( m_st->m32 , m_end->m32 , tm_time , end_time );
	m_out->m33 = GN_Linear( m_st->m33 , m_end->m33 , tm_time , end_time );
	m_out->m13 = GN_Linear( m_st->m13 , m_end->m13 , tm_time , end_time );
	m_out->m23 = GN_Linear( m_st->m23 , m_end->m23 , tm_time , end_time );
   }
}
GN_API void GNg::M33TransformCoord(float *out_x,float *out_y,const float in_x,const float in_y,const sMAT33 *mat, bool isCoord){
	if(isCoord){
	    *out_x = in_x * mat->m11 + in_y * mat->m21 + mat->m31;
	    *out_y = in_x * mat->m12 + in_y * mat->m22 + mat->m32;
	}else{
	    *out_x = in_x * mat->m11 + in_y * mat->m21;
	    *out_y = in_x * mat->m12 + in_y * mat->m22;
	}
}
GN_API void GNg::M33TransformCoord(sVEC2 *out,const sVEC2 *in,const sMAT33 *mat, bool isCoord){
    if(isCoord){
	    out->x = in->x * mat->m11 + in->y * mat->m21 + mat->m31;
	    out->y = in->x * mat->m12 + in->y * mat->m22 + mat->m32;
	}else{
	    out->x = in->x * mat->m11 + in->y * mat->m21;
	    out->y = in->x * mat->m12 + in->y * mat->m22;
	}
}
GN_API void GNg::M33TransformCoord(sVEC2 *pv,const sMAT33 *mat, bool isCoord){
    sVEC2 tmp;
	if(isCoord){
	    tmp.x = pv->x * mat->m11 + pv->y * mat->m21 + mat->m31;
	    tmp.y = pv->x * mat->m12 + pv->y * mat->m22 + mat->m32;
	}else{
	    tmp.x = pv->x * mat->m11 + pv->y * mat->m21;
	    tmp.y = pv->x * mat->m12 + pv->y * mat->m22;
	}
    *pv = tmp;
}
GN_API void GNg::M33TransformCoords(sVEC2 *tmp,const sVEC2 *array_in, int num, const sMAT33 *mat, bool isCoord){
	sVEC2  *pv = (sVEC2*)array_in,  *pv_end  =  (sVEC2*)(array_in + num);
	if(isCoord){
		for(; pv < pv_end; ++pv, ++tmp){
		    tmp->x = pv->x * mat->m11 + pv->y * mat->m21 + mat->m31;
	        tmp->y = pv->x * mat->m12 + pv->y * mat->m22 + mat->m32;
		}
	}else{
		for(; pv < pv_end; ++pv, ++tmp){
	        tmp->x = pv->x * mat->m11 + pv->y * mat->m21;
	        tmp->y = pv->x * mat->m12 + pv->y * mat->m22;
		}
	}
}
GN_API void GNg::M33TransformCoords(sVEC2 **ppv_array,int num, const sMAT33 *mat, bool isCoord){
	sVEC2 tmp,  *pv = *ppv_array,  *pv_end  =  *ppv_array + num;
	if(isCoord){
		for(; pv < pv_end; ++pv){
		    tmp.x = pv->x * mat->m11 + pv->y * mat->m21 + mat->m31;
	        tmp.y = pv->x * mat->m12 + pv->y * mat->m22 + mat->m32;
            *pv = tmp;
		}
	}else{
		for(; pv < pv_end; ++pv){
	        tmp.x = pv->x * mat->m11 + pv->y * mat->m21;
	        tmp.y = pv->x * mat->m12 + pv->y * mat->m22;
			*pv = tmp;
		}
	}
}
GN_API void GNg::M33TransformTransf2D(sTRANSF2D *out, const sTRANSF2D *in, const sMAT33 *mat){
   #define GT_TMP_TRANSFORM_TRANSF2D( OUT , IN ) \
	sMAT33 tmp, tmp2;\
	M33GetTransf2D(&tmp, IN );\
    MultM33(&tmp2,&tmp,mat);\
    M33ToTransf2D(&tmp2, OUT )

    GT_TMP_TRANSFORM_TRANSF2D( out , in );
}
GN_API void GNg::M33TransformTransf2D(sTRANSF2D *ts, const sMAT33 *mat){
    GT_TMP_TRANSFORM_TRANSF2D( ts , ts );
}
GN_API void GNg::M33TransformQuads(sQUADS2 &qs,const sMAT33 &mat){
	M33TransformCoord(&qs.p0, &mat);
	M33TransformCoord(&qs.p1, &mat);
	M33TransformCoord(&qs.p2, &mat);
}
GN_API void GNg::M33TransformRect(sRECT2 &rc, sQUADS2 &qs,const sMAT33 &mat, bool has_mirror, const sVEC2 *scl_factor_inv, sVEC2 *ap){
    if(scl_factor_inv){ /*修正一些太扁的范围盒*/
        if(scl_factor_inv->x > 10.0f && (rc.right - rc.left) < scl_factor_inv->x * GN_MIN_RANGE_BOX_SIZE)rc.left = rc.right - scl_factor_inv->x * GN_MIN_RANGE_BOX_SIZE;
		if(scl_factor_inv->y > 10.0f && (rc.bottom - rc.top) < scl_factor_inv->y * GN_MIN_RANGE_BOX_SIZE)rc.top = rc.bottom - scl_factor_inv->y * GN_MIN_RANGE_BOX_SIZE;
	}
	sVEC2 qv1(rc.right, rc.top), qv2;

	//计算模型在3D世界空间中的实时判定范围
	M33TransformCoord(&qs.p0, (sVEC2*)(&rc.left), &mat);
	M33TransformCoord(&qs.p1, &qv1,&mat );
	M33TransformCoord(&qs.p2, (sVEC2*)(&rc.right),&mat);
	if(has_mirror)RangeQuadsTurnOrde( qs );
	
	GNg::RangeQuadsToRect(&rc, &qs ,ap); //计算在3D世界空间中的包围盒范围
}
GN_API void GNg::GetRangeBoxFromVertexs(sRECT2* rb, GNg::sVEC2* vex_data, int vex_num) {
	GNg::sVEC2 *p_vertex = vex_data, *vex_end = vex_data + vex_num;
    rb->left   = p_vertex->x;
    rb->right  = p_vertex->x;
    rb->bottom = p_vertex->y;
    rb->top    = p_vertex->y;
	for(++p_vertex; p_vertex < vex_end; ++p_vertex){ /*统计得出外接盒大小*/
		if     (p_vertex->x < rb->left)   rb->left   = p_vertex->x;
		else if(p_vertex->x > rb->right)  rb->right  = p_vertex->x;
        if     (p_vertex->y < rb->top)    rb->top    = p_vertex->y;
		else if(p_vertex->y > rb-> bottom)rb->bottom = p_vertex->y;
	}
	if (GN_ValueEqualFloat(rb->left, rb->right))rb->left -= GN_MIN_RANGE_BOX_SIZE;
	if (GN_ValueEqualFloat(rb->top, rb->bottom))rb->top  -= GN_MIN_RANGE_BOX_SIZE;
}
#endif // _GN_2DMATH_H_
