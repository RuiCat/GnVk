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
#ifdef _GN_2DAREA_H_

GN_API bool GNg::Quads2DAreaAlign(const GNg::sVEC2 *qp1, uint8 pn, GNg::sVEC2 *qp2, GNg::sQUADS_SPACE2 *qs2, const GNg::sVEC2 *_rv2/*[2]*/){
	bool has_chg = false;

	//4边型2对齐靠拢( 4边型2区域面XY序号, 面向左边两个点的序号 ... , 面向右边两个点的序号 )
   #define GT_TMP_ALIGN_QUADS_2D( AId ,   PId1 , PId2 ,   PId_1 , PId_2 ) \
	for(d_min = GN_FLOAT_MAX , d_max = GN_FLOAT_MIN , i = 0; i < pn; ++i){\
		d = GN_Vec2Dot( qs2->n##AId. , qp1[i]. );\
	    if(d > d_max)d_max = d;\
		if(d < d_min)d_min = d;\
	}\
	if(d_max < qs2->d##AId ){\
		has_chg = true;\
	    qs2->d##AId = d_max;\
	    TestRayIntPlane(qp2 + PId1 , _rv2 + AId , &qs2->n##AId , -d_max, qp2 + PId1 );\
		TestRayIntPlane(qp2 + PId2 , _rv2 + AId , &qs2->n##AId , -d_max, qp2 + PId2 );\
	}\
	if(d_min > qs2->d_##AId ){\
		has_chg = true;\
	    qs2->d_##AId = d_min;\
	    TestRayIntPlane(qp2 + PId1 , _rv2 + AId , &qs2->n##AId , -d_min, qp2 + PId_1 );\
		TestRayIntPlane(qp2 + PId2 , _rv2 + AId , &qs2->n##AId , -d_min, qp2 + PId_2 );\
	}
	float d, d_min, d_max;
	uint8 i;
    GT_TMP_ALIGN_QUADS_2D( 0 ,   0 , 3 ,    1 , 2 )  //X面
    GT_TMP_ALIGN_QUADS_2D( 1 ,   0 , 1 ,    3 , 2 )  //Y面
    return has_chg;
}
GN_API void GNg::MakeAreaQuads(sAREA_QUADS2 *qa_out, const sVEC2 *qnNz, const sVEC2 *ps1, int pn1, const sVEC2 *ps2, int pn2){
    /*模版代码:构建能包含两组点的四边形区域*/   
	sQUADS_SPACE2 *qsp = &(qa_out->sp);
	const sVEC2 *_p;
	sVEC2 qnz1 = qnNz[1];
	qsp->n0 = qnNz[0];
	qsp->n1 = qnz1;
    qsp->d0 = qsp->d_0 = GN_Vec2Dot( ps1-> , qnNz-> );
	qsp->d1 = qsp->d_1 = GN_Vec2Dot( ps1-> , qnz1. );
    float _d0, _d1;
	
   #define GT_TMP_QUADS_2D_AREA_SPACE_DIST_CPT( ST , N ) \
    for(i = ST ; i < pn##N; ++i){\
		_p = ps##N + i;\
	    _d0 = GN_Vec2Dot(_p-> , qnNz-> );\
        _d1 = GN_Vec2Dot(_p-> , qnz1.);\
        if     (_d0 > qsp->d0 )qsp->d0  = _d0;\
		else if(_d0 < qsp->d_0)qsp->d_0 = _d0;\
        if     (_d1 > qsp->d1 )qsp->d1  = _d1;\
		else if(_d1 < qsp->d_1)qsp->d_1 = _d1;\
	}
	int i;
	GT_TMP_QUADS_2D_AREA_SPACE_DIST_CPT( 1 ,  1 );
    GT_TMP_QUADS_2D_AREA_SPACE_DIST_CPT( 0 ,  2 );

    qsp->GetAnglePoint(qa_out->p);
}
/*************************************************** 碰撞检测实用函数 *********************************************************/
#define GT_TMP_QUADSAREA_FAR_FACE_VAR_DEF \
    int   i_max = -1, pi, i;\
	float d_max = GN_FLOAT_MIN, *sp_ds = &(qs->d0), *sp_d_s = &(qs->d_0);\
    float  *dbuf = dss

/*按经过四边形区域TestInThereEx计算好的信息来获取另一区域点离得该四边形区域比较开的方位ID(四边形空间区域, 计算过的对应方位号, 计算过的顶点离距离数据)返回比较开的方位号索引*/
static int GT_QuadsAreaFarFaceIndexGet (/*const*/ GNg::sQUADS_SPACE2 *qs, int *nids, float *dss/*[2][4]*/){
    GT_TMP_QUADSAREA_FAR_FACE_VAR_DEF;
	float _d_max;
	for(i = 0; i < 2; ++i, dbuf += 4 ){
		if(nids[i]){/*有同一面就代表所有d已经计算好了*/
			_d_max = dbuf[0];
			if(nids[i] > 3){
                for(pi = 1; pi < 4; ++pi){ if(dbuf[pi] > _d_max)_d_max = dbuf[pi]; }
                _d_max = sp_d_s[nids[i]-1-3] - _d_max;
			}else{
                for(pi = 1; pi < 4; ++pi){ if(dbuf[pi] < _d_max)_d_max = dbuf[pi]; }
                _d_max =  _d_max - sp_ds[nids[i]-1];
			}
			if(_d_max > d_max){ d_max = _d_max; i_max = i; }
	}   }
	return i_max;
}
/*按经过四边形区域TestInThereEx计算好的信息来分析另一区域点离得该四边形区域比较开的是那一边(四边形空间区域, 计算过的顶点离距离数据, 计算过的顶点离距离数 , 另一区域的顶点)返回非0值 其他意义和 TestInThere 一样 这会一起帮您补全计算完所有离距离*/
static int GT_QuadsAreaFarFaceOnThere(/*const*/ GNg::sQUADS_SPACE2 *qs, float *dss/*[2][4]*/, const int *dns/*[2]*/, const GNg::sVEC2 *vexs){
    GT_TMP_QUADSAREA_FAR_FACE_VAR_DEF;
	float _dm, _d_m;
    GNg::sVEC2 *n_ = &(qs->n0);
    for(i = 0; i < 2; ++i, ++n_, dbuf += 4 ){
	    _dm = GN_FLOAT_MIN, _d_m = GN_FLOAT_MAX;
	    for(pi = 0; pi < dns[i]; ++pi){
            if(dbuf[pi] > _dm )_dm  = dbuf[pi];
            if(dbuf[pi] < _d_m)_d_m = dbuf[pi];
	    }
		for(pi = dns[i]; pi < 4; ++pi){
            dbuf[pi] = GN_Vec2Dot( n_-> , vexs[pi]. );
            if(dbuf[pi] > _dm )_dm  = dbuf[pi];
            if(dbuf[pi] < _d_m)_d_m = dbuf[pi];
	    }
        _dm  = sp_d_s[i] - _dm;
        _d_m = _d_m - sp_ds[i];
		if(_dm > _d_m){
            if(_dm  > d_max){ d_max =  _dm; i_max = i + 3; }
		}else{
            if(_d_m > d_max){ d_max = _d_m; i_max = i; }
		}
	}
    return i_max + 1;
}

#define GT_TMP_QUADSAREA_FACE_LOOK_STATE_CPT( D , CMP , S1 , S2 , QA , FB_PROC ) \
	for(i = 0; i < 2; ++i){\
		if(nids[i]){\
            D QA ->sp.GetNormal(_n, nids[i], true);\
			rets[i] = (GN_Vec2Dot(_n-> , pv-> ) CMP ) ? S1 : S2 ;/*在前还是后*/\
			if(ret){\
                if(ret != rets[i]) FB_PROC ;\
			}else ret = rets[i];\
	    }else rets[i] = 0;\
    }

#define GT_TMP_QUADSAREA_RETURN_IS_FB( QA , DBUF ) \
    if(ret){\
	    if(ret == 3){ \
            return rets[GT_QuadsAreaFarFaceIndexGet(&( QA ->sp), nids, DBUF [0] )];\
	    }else return ret;\
    }
#define GT_TMP_QUADSAREA_RETURN_CROSS_FB( D , CMP ) \
    else{ /*相交*/\
	    *_n = (qa1->p[0] + qa1->p[3]) * 0.5f; /*GNg::RangeQuadsGetCentre*/\
	    if(qa2->sp.TestIncPoint(_n))return 2;\
        int nid = GT_QuadsAreaFarFaceOnThere(&(qa2->sp), dbuf[0], dnn, qa1->p);\
        D qa2 ->sp.GetNormal(_n, nid, true);\
	    return (GN_Vec2Dot(_n-> , pv-> ) CMP ) ? 1 : 2 ;/*在前还是后*/\
    }

#define GT_TMP_TEST_QUADSAREA_ON_QUADSAREA( FNAME , PROC1 , PROC2 , PROC3 , PROC4 , PROC5 ) \
GN_API int GNg::TestQuadsAreaOn##FNAME (/*const*/ sAREA_QUADS2 *qa1, /*const*/ sAREA_QUADS2 *qa2, const sVEC2 *pv, bool is_v){\
    float nbuf[2], dbuf[2][4], dbuf2[2][4];\
	GNg::sVEC2 *_n = (GNg::sVEC2 *)nbuf;\
	int dnn[2], dnn2[2], nids[2], rets[2], ret = 0;\
    uint8 i;\
	if(is_v){\
		qa2->sp.TestInThereEx(qa1->p, 4, nids, dnn , dbuf[0]);\
	    GT_TMP_QUADSAREA_FACE_LOOK_STATE_CPT(    ,  > 0.0f , 1 , 2 , qa2 , PROC1 ) \
		PROC2 \
		qa1->sp.TestInThereEx(qa2->p, 4, nids, dnn2 , dbuf2[0]);\
	    GT_TMP_QUADSAREA_FACE_LOOK_STATE_CPT(    ,  > 0.0f , 2 , 1 , qa1 , PROC1) \
		PROC3 \
		PROC4 \
	}else{\
	    qa2->sp.TestInThereEx(qa1->p, 4, nids, dnn , dbuf[0]);\
	    GT_TMP_QUADSAREA_FACE_LOOK_STATE_CPT( float _d =   ,  < _d , 1 , 2 , qa2 , PROC1 ) \
		PROC2 \
		qa1->sp.TestInThereEx(qa2->p, 4, nids, dnn2 , dbuf2[0]);\
	    GT_TMP_QUADSAREA_FACE_LOOK_STATE_CPT( float _d =   ,  < _d , 2 , 1 , qa1 , PROC1 ) \
		PROC3 \
		PROC5 \
	}\
    return ret;\
}

GT_TMP_TEST_QUADSAREA_ON_QUADSAREA( QuadsArea ,         return 3 , if(ret)return ret; , ; , ; , ; )\
GT_TMP_TEST_QUADSAREA_ON_QUADSAREA( QuadsAreaRelative , ret  = 3 , GT_TMP_QUADSAREA_RETURN_IS_FB( qa2 , dbuf ) , GT_TMP_QUADSAREA_RETURN_IS_FB( qa1 , dbuf2 ) ,\
	GT_TMP_QUADSAREA_RETURN_CROSS_FB(  , > 0.0f ) ,\
	GT_TMP_QUADSAREA_RETURN_CROSS_FB( float _d =   ,  < _d ) )

GN_API bool GNg::TestQuadsAreaIntQuadsArea(/*const*/ sAREA_QUADS2 *qa1, /*const*/ sAREA_QUADS2 *qa2){
    if(qa1->sp.TestInThere(qa2->p, 4))return false;
    if(qa2->sp.TestInThere(qa1->p, 4))return false;
	return true;
}
GN_API bool GNg::TestQuadsAreaIntQuadsArea(/*const*/ sAREA_QUADS2 *qa1, /*const*/ sAREA_QUADS2 *qa2, sVEC2 *crossP, sVEC2 *crsNz, float *crsR){
	if(!TestQuadsAreaIntQuadsArea(qa1, qa2))return false;

	sAREA_QUADS2 *qa_min, *qa_max;
	int test_state = qa2->sp.TestInclude(qa1->p, 4);
	if(test_state){  //包含
		if(test_state == 1){
		    qa_min = qa1;
            qa_max = qa2;
		}else{
		    qa_min = qa2;
            qa_max = qa1;
		}
        /*计算区域中心点*/
		RangeQuadsGetCentre(*((sQUADS2 *)(qa_min->p)), *crossP);
	}else{			 //相交
		sQUADS2 *qds1 = (sQUADS2*)qa1->p, *qds2 = (sQUADS2*)qa2->p;
        
		sVEC2 rv1[2], rv2[2]; //计算两四边型边方向
		rv1[0] = qds1->p1 - qds1->p0; Vec2Normalize(rv1  , rv1  );
		rv1[1] = qds1->p2 - qds1->p1; Vec2Normalize(rv1+1, rv1+1);
        rv2[0] = qds2->p1 - qds2->p0; Vec2Normalize(rv2  , rv2  );
		rv2[1] = qds2->p2 - qds2->p1; Vec2Normalize(rv2+1, rv2+1);
      
        sAREA_QUADS2 tmp_qa1 = *qa1, tmp_qa2 = *qa2;
	    sAREA_QUADS2 *_qa1 = qa1 = &tmp_qa1, *_qa2 = qa2 = &tmp_qa2, *_qa_tmp;
		sVEC2 *_rv1 = rv1,   *_rv2 = rv2,   *_rv_tmp;
		Quads2DAreaAlign(_qa1->p, 4 , _qa2->p, &_qa2->sp, _rv2);
		do{ /*循环互相对齐靠拢缩近*/
			/*交换比较和被比较四变形相关数据指针*/
            _qa_tmp = _qa1; _qa1 = _qa2; _qa2 = _qa_tmp;
            _rv_tmp = _rv1; _rv1 = _rv2; _rv2 = _rv_tmp;
		}while(Quads2DAreaAlign(_qa1->p, 4 , _qa2->p, &_qa2->sp, _rv2));
        /*找出两四边型中谁是被击中的*/
        qa_max = _qa1;
		unsigned int i, n = 4 ;
		for(i = 0; i < n; ++i){
			if(qa1->sp.TestInclude(qa2->p + i, 1)){
			    qa_max = qa1;
				n = 0;
		}	}
        for(i = 0; i < n; ++i){
			if(qa2->sp.TestInclude(qa1->p + i, 1)){
			    qa_max = qa2;
				n = 0;
		}	}
		/*计算相交区域中心点*/
		sVEC2 cp;
        RangeQuadsGetCentre(*((sQUADS2 *)(qa1->p)),  cp);
        RangeQuadsGetCentre(*((sQUADS2 *)(qa2->p)), *crossP);
		*crossP = (*crossP + cp) * 0.5f;
	}
	//找出点靠近那条边并获取它的法线来返回
    switch(qa_max->sp.TestNearThere(crossP, crsR)){
	case 1: *crsNz =   qa_max->sp.n0;  break;
	case 2: *crsNz =   qa_max->sp.n1;  break;
	case 4: *crsNz = -(qa_max->sp.n0); break;
	case 5: *crsNz = -(qa_max->sp.n1); break;
	}
	return true;
}
GN_API bool GNg::TestSphereIntQuadsAreaNz(const sSPHERE2D *sph, /*const*/ sAREA_QUADS2 *qaNz){
    /*模版代码:圆球与四边型区域测试*/
	const sQUADS_SPACE2 * qsp = &(qaNz->sp);
	const sVEC2 * sp = &(sph->cp);
	sVEC2 na(0.0f, 0.0f);
	float sd;
	uint8 fn = 0, fids[2];
    sd = GN_Vec2Dot( sp-> , qsp->n0. );
	if      ( sd > qsp->d0 ){
	    if(sd - sph->r > qsp->d0)return false; /*在区域边面外*/
		else { na -= qsp->n0; fids[fn++] = 1; }
	}else if( sd < qsp->d_0 ){
	    if(sd + sph->r < qsp->d_0)return false; /*在区域边面外*/
		else { na += qsp->n0; fids[fn++] = 4; }
	}
	sd = GN_Vec2Dot( sp-> , qsp->n1. );
	if      ( sd > qsp->d1 ){
	    if(sd - sph->r > qsp->d1)return false; /*在区域边面外*/
		else { na -= qsp->n1; fids[fn++] = 2; }
	}else if( sd < qsp->d_1 ){
	    if(sd + sph->r < qsp->d_1)return false; /*在区域边面外*/
		else { na += qsp->n1; fids[fn++] = 5; }
	}
		if(fn > 1){ /*圆心靠区域边角*/
	        uint8 pids[2];
            GNg::RangeQuadsGetNearPIdFromVisualPlane2D(pids, fids, fn);
            Vec2Normalize(&na, &na);
		    const sVEC2 *p = qaNz->p;
			const sVEC2 *_p = p + pids[0];
			float d2 = GN_Vec2Dot( _p-> , na. );
            sd = GN_Vec2Dot( sp-> , na. );
			if( sd + sph->r < d2 )return false; /*在区域边角外*/
		}
    return true;
}
GN_API bool GNg::TestSphereIntQuadsAreaNz(const sSPHERE2D *sph, /*const*/ sAREA_QUADS2 *qaNz, sVEC2 *crossP, sVEC2 *crossNz,float *crossR){
    /*模版代码:圆球与四边型区域测试*/
	const sQUADS_SPACE2 * qsp = &(qaNz->sp);
	const sVEC2 * sp = &(sph->cp);
	sVEC2 na(0.0f, 0.0f), near_n;
	float sd, _d, near_d = GN_FLOAT_MAX;
	uint8 fn = 0, fids[2];
    sd = GN_Vec2Dot( sp-> , qsp->n0. );
	if      ( sd > qsp->d0 ){
	    if(sd - sph->r > qsp->d0)return false; /*在区域边面外*/
		else { na -= qsp->n0; fids[fn++] = 1; _d = sd - qsp->d0; }
	}else if( sd < qsp->d_0 ){
	    if(sd + sph->r < qsp->d_0)return false; /*在区域边面外*/
		else { na += qsp->n0; fids[fn++] = 4; _d = qsp->d_0 - sd; }
	}else if(!fn){
		if(qsp->d0  - sd < near_d){ near_d = qsp->d0  - sd; near_n =  qsp->n0; }
		if(sd - qsp->d_0 < near_d){ near_d = sd - qsp->d_0; near_n = -qsp->n0; }
	}
	sd = GN_Vec2Dot( sp-> , qsp->n1. );
	if      ( sd > qsp->d1 ){
	    if(sd - sph->r > qsp->d1)return false; /*在区域边面外*/
		else { na -= qsp->n1; fids[fn++] = 2; _d = sd - qsp->d1; }
	}else if( sd < qsp->d_1 ){
	    if(sd + sph->r < qsp->d_1)return false; /*在区域边面外*/
		else { na += qsp->n1; fids[fn++] = 5; _d = qsp->d_1 - sd; }
	}else if(!fn){
        if(qsp->d1  - sd < near_d){ near_d = qsp->d1  - sd; near_n =  qsp->n1; } 
		if(sd - qsp->d_1 < near_d){ near_d = sd - qsp->d_1; near_n = -qsp->n1; }
	}
	if(fn){
		if(fn > 1){ /*圆心靠区域边角*/
			uint8 pids[2];
            RangeQuadsGetNearPIdFromVisualPlane2D(pids, fids, fn);
            Vec2Normalize(&na, &na);
		    const sVEC2 *p = qaNz->p;
			const sVEC2 *_p = p + pids[0];
			float d2 = GN_Vec2Dot( _p-> , na. );
            sd = GN_Vec2Dot( sp-> , na. );
			if( sd + sph->r < d2 )return false; /*在区域边角外*/

		    //圆球与区域边角相交
			*crossP = *_p;
			*crossNz = sph->cp - (*crossP);
			Vec2Normalize(crossNz, crossNz);
            *crossR = sd + sph->r - d2;

        }else{ //圆球与区域的一面相交
            *crossNz = -na;
			*crossP  = sph->cp + (na * _d);
            *crossR  = sph->r - _d;
        }
	}else{ //球心在区域里面
        *crossNz = near_n;
        *crossP  = sph->cp + (near_n * near_d);
		*crossR  = sph->r + near_d;
    }
	return true;
}
GN_API bool GNg::TestRectIntQuadsArea(/*const*/ sRECT2 *rc, /*const*/ sAREA_QUADS2 *qa){
	if(rc->TestInThere(qa->p, 4))return false;
	GNg::sVEC2 ap[4];
	RectGetAnglePoint( *rc, ap);
    if(qa->sp.TestInThere(ap, 4))return false;
	return true;
}
GN_API bool GNg::TestPointIntPoint(const sVEC2 *point1,const sVEC2 *point2,float r){
	 if((point1->x - point2->x >= 0.0f ? point1->x - point2->x : point2->x - point1->x) > r 
	 || (point1->y - point2->y >= 0.0f ? point1->y - point2->y : point2->y - point1->y) > r)return false;
	 return true;
}
GN_API bool GNg::TestPointIntRect(int x,int y,int rx, int ry, int rw, int rh){
     if(x < rx || x > rx+rw || y < ry || y > ry+rh)return false;
	 return true;
}
GN_API bool GNg::TestPointIntRect(int x,int y,const sRECT *rect){
     if(x < rect->left || x > rect->right || y < rect->top  || y > rect->bottom) return false;
	 return true;
}
GN_API bool GNg::TestPointIntRect(const sVEC2 *point,const sRECT2 *rect){
     if(point->x < rect->left || point->x > rect->right || point->y < rect->top  || point->y > rect->bottom) return false;
	 return true;
}
GN_API bool GNg::TestPointIntQuads(float x,float y,float p0x,float p0y,float p1x,float p1y,float p2x,float p2y){
     float nx = p1x - p0x;
     float ny = p1y - p0y;  //计算出点1到点2之间的向量
     float npx = -ny;       //向量向右方向转90度
	 float npy =  nx;
	 float d  = x * npx + y * npy;
	 if(d < p0x * npx + p0y * npy || d > p2x * npx + p2y * npy)return false; //在四边面对的第1平面外或在四边面对的第3平面外
	 
		   nx = p2x - p1x;
		   ny = p2y - p1y;
		   npx = -ny;       //向量向右方向转90度
		   npy =  nx;
		   d  = x * npx + y * npy;
     if(d < p1x * npx + p1y * npy || d > p0x * npx + p0y * npy)return false; //在四边面对的第2平面外或在四边面对的第4平面外
	 return true;
}
GN_API bool GNg::TestPointIntQuads(const sVEC2 *point, const sQUADS2 *quads){
	sVEC2 np;
	sVEC2 *_np = &np;

	sVEC2 n = quads->p1 - quads->p0; //计算出点1到点2之间的向量
	Vec2Right(_np,&n);               //向量向右方向转90度
	float d  = Vec2Dot(point,_np);
	if(d < Vec2Dot(&quads->p0,_np) || d > Vec2Dot(&quads->p2,_np))return false; //在四边面对的第1平面外或在四边面对的第3平面外

	n = quads->p2 - quads->p1;
    Vec2Right(_np,&n);               //向量向右方向转90度
	d  = Vec2Dot(point,_np);
    if(d < Vec2Dot(&quads->p1,_np) || d > Vec2Dot(&quads->p0,_np))return false; //在四边面对的第2平面外或在四边面对的第4平面外
	
	return true;
}
GN_API GNg::sVEC2 * GNg::GetTriNearPoint(const sVEC2 *point, const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, sVEC2 *np_out){
	sVEC2  va = (*b) - (*a), vb = (*c) - (*b), vc = (*a) - (*c);
	return GetTriNearPoint(point, a, b, c, va, vb, vc, GN_Vec2Dot(va., va.), GN_Vec2Dot(vb., vb.), GN_Vec2Dot(vc., vc.), np_out);
}
GN_API GNg::sVEC2 * GNg::GetTriNearPoint(const sVEC2 *point, const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, sVEC2 &va, sVEC2 &vb, sVEC2 &vc, float d_a, float d_b, float d_c, sVEC2 *np_out){
	sVEC2  pa = (*point) - (*a), pb = (*point) - (*b), pc = (*point) - (*c), vla, vlb, vlc;
	float da = GN_Vec2Dot(pa., va.),  db = GN_Vec2Dot(pb., vb.), dc = GN_Vec2Dot(pc., vc.);
	
	float l_min = GN_FLOAT_MAX, len;
	sVEC2 *_vl = 0, *_min = 0;

	//计算三角型边关系
   #define GT_TMP_TRI_EDGE_RELATION_CPT( A , C ) \
    if(d##A < 0.0f){\
		if(d##C > d_##C){ *np_out = *A; return (sVEC2*)A; /*点比较靠近边角点*/}\
	}else if(d##A <= d_##A){\
		vl##A = (d##A / d_##A) * v##A - p##A;\
		if(_vl){\
			if(GN_Vec2Dot( _vl-> , vl##A. ) < 0.0f){ *np_out = *point; return (sVEC2*)point; /*点在三角形内*/}\
		}else _vl = &vl##A;\
		p##A = vl##A + (*point);\
		len = GN_Vec2Dot( vl##A. , vl##A. );\
		if(len < l_min){ l_min = len; _min = &p##A; }\
	}
	GT_TMP_TRI_EDGE_RELATION_CPT( a , c ) 
	GT_TMP_TRI_EDGE_RELATION_CPT( b , a ) 
	GT_TMP_TRI_EDGE_RELATION_CPT( c , b ) 
	*np_out = *_min;
	return np_out;
}
GN_API float GNg::GetTriNearDist(const sVEC2 *point, const sTRI2D *tri){
    return GetTriNearDist(point, &(tri->a), &(tri->b), &(tri->c));
}
GN_API float GNg::GetTriNearDist(const sVEC2 *point, const sVEC2 *a, const sVEC2 *b, const sVEC2 *c){
    sTRI_SPACE2D tri_space;
    tri_space.Init(a, b, c, true);
	return tri_space.NearDist(point);
}
GN_API float  GNg::GetLineNearDist(const sVEC2 *point, const sVEC2 *pa, const sVEC2 *pb){
	sVEC2 v = (*pa) - (*pb);
	float pd = GN_Vec2Dot(v. , point->);
	float vd = GN_Vec2Dot(v. , pb->);
	if(pd < vd)v = (*pb) - (*point);
	else{
		vd = GN_Vec2Dot(v. , pa->);
		if(pd > vd)v = (*pa) - (*point);
		else{
			Vec2Normalize(&v, &v);
			sVEC2 v1;
			Vec2Right(&v1, &v);
			vd = GN_Vec2Dot(v1. , pa->) - GN_Vec2Dot(v1. , point->);
			return vd < 0.0f? -vd : vd;
		}
	}
	return Vec2Length(&v);
}
GN_API bool GNg::TestRectIntRect(const sRECT  *rect1,const sRECT  *rect2){
	if(rect1->right  < rect2->left || rect1->left > rect2->right
	 ||rect1->bottom < rect2->top  || rect1->top > rect2->bottom)return false;
	return true;
}
GN_API bool GNg::TestRectIntRect(const sRECT2 *rect1,const sRECT2 *rect2){
	if(rect1->right  < rect2->left || rect1->left > rect2->right
	 ||rect1->bottom < rect2->top  || rect1->top > rect2->bottom)return false;
	return true;
}
GN_API bool GNg::TestQuadsIntQuads(const sQUADS2 *qds1, const sQUADS2 *qds2){
     sVEC2 n,np,p3;
	 sVEC2 *_np = &np;
	 float d,d0,d1,d2,d3;
	 uint8 tn = 0;
	 for(;;++tn){
		 p3 = qds2->p0 + (qds2->p2-qds2->p1);
		 
		 n = qds1->p1 - qds1->p0;				/*计算出4边形1 点0到点1之间的向量*/
		 Vec2Right(_np,&n);                     /*向量向右方向转90度*/
		 d0 = Vec2Dot(&qds2->p0,_np);           /*计算四边形2的4点,对应四边形1的平面1 的关系偏移量*/
		 d1 = Vec2Dot(&qds2->p1,_np);
         d2 = Vec2Dot(&qds2->p2,_np);
		 d3 = Vec2Dot(        &p3,_np);
		 d  = Vec2Dot(&qds1->p0,_np);
		 if(d0<d && d1<d && d2<d && d3<d)return false; /*在四边形1的平面1对面*/
         d  = Vec2Dot(&qds1->p2,_np);
		 if(d0>d && d1>d && d2>d && d3>d)return false; /*在四边形1的平面3对面*/
         
         n = qds1->p2 - qds1->p1;				/*计算出4边形1 点1到点2之间的向量*/
		 Vec2Right(_np,&n);                     /*向量向右方向转90度*/
		 d0 = Vec2Dot(&qds2->p0,_np);           /*计算四边形2的4点,对应四边形1的平面2 的关系偏移量*/
		 d1 = Vec2Dot(&qds2->p1,_np);
         d2 = Vec2Dot(&qds2->p2,_np);
		 d3 = Vec2Dot(        &p3,_np);
		 d  = Vec2Dot(&qds1->p1,_np);
		 if(d0<d && d1<d && d2<d && d3<d)return false; /*在四边形1的平面2对面*/
         d  = Vec2Dot(&qds1->p0,_np);
		 if(d0>d && d1>d && d2>d && d3>d)return false; /*在四边形1的平面4对面*/
		 if(tn)return true;
		 GN_Swap(const sQUADS2* , qds1 , qds2);
	 }
	 return true;
}
GN_API bool GNg::TestQuadsIntQuads(const sQUADS2 *quads1,const sQUADS2 *quads2,sVEC2 *crossP,sVEC2 *crossNz,float *crossR){
    sAREA_QUADS2 qa1(quads1,true), qa2(quads2,true);
    return TestQuadsAreaIntQuadsArea(&qa1, &qa2, crossP, crossNz, crossR);
}
GN_API bool GNg::TestSphereIntSphere(const sSPHERE2D *Op1,const sSPHERE2D *Op2){
	 sVEC2 v = Op2->cp - Op1->cp;
	 float r2 = Op1->r + Op2->r;
     if(GN_Vec2Dot( v. , v. ) > r2 * r2) return false;
     return true;
}
GN_API bool GNg::TestSphereIntSphere(const sSPHERE2D *Op1,const sSPHERE2D *Op2,sVEC2 *crossP,sVEC2 *crossN,float *crossR){
	 sVEC2 v = Op2->cp - Op1->cp;
	 float r2 = Op1->r + Op2->r;
	 float len = Vec2Length(&v);
     if(len > r2) return false;
	 *crossP = Op1->cp + v * (Op1->r / r2);
     *crossN = v;
	 *crossR = r2 - len;  //交叠深度范围 如果想要得到圆切面的半径可用公式 Op1->r -(Op1->r -(v * Op2->r / r2)) / 2 得到
     return true;
}
GN_API bool GNg::TestSphereIntQuads(const sSPHERE2D *sph,const sQUADS2 *quads){
    sAREA_QUADS2 qa(quads,true);
    return TestSphereIntQuadsAreaNz(sph, &qa);
}
GN_API bool GNg::TestSphereIntQuads(const sSPHERE2D *sph,const sQUADS2 *quads,sVEC2 *crossP,sVEC2 *crossNz,float *crossR){
    sAREA_QUADS2 qa(quads,true);
	return TestSphereIntQuadsAreaNz(sph, &qa, crossP, crossNz, crossR);
}
GN_API bool GNg::TestTriIntTri(const sTRI2D *tr1,const sTRI2D *tr2, float r){
    return GNg::TestTriIntTri(&(tr1->a), &(tr1->b), &(tr1->c), &(tr2->a), &(tr2->b), &(tr2->c), r);
}
GN_API bool GNg::TestTriIntTri(const sVEC2 *a1, const sVEC2 *b1, const sVEC2 *c1, const sVEC2 *a2, const sVEC2 *b2, const sVEC2 *c2, float r){
	float d;
	sVEC2 n, v;
	bool has_ofs = false;
	if(r){
        has_ofs = true;
		r *= 2.0f;
	} 
   #define GT_TMP_TEST_TRI_ON_TRI_SIDE( PB , PA , TID ) \
	v = (*PB) - (*PA);\
	if(has_ofs){\
	  GNg::Vec2Normalize(&v, &v);\
      d = GN_Vec2Dot( v. , PB-> ) + r;\
	  if(GN_Vec2Dot( v. , a##TID-> ) > d && GN_Vec2Dot( v. , b##TID-> ) > d && GN_Vec2Dot( v. , c##TID-> ) > d)return false;\
	}\
	  GNg::Vec2Right(&n, &v);\
	  d = GN_Vec2Dot( n. , PA-> ) + r;\
	  if(GN_Vec2Dot( n. , a##TID-> ) > d && GN_Vec2Dot( n. , b##TID-> ) > d && GN_Vec2Dot( n. , c##TID-> ) > d)return false
    
    GT_TMP_TEST_TRI_ON_TRI_SIDE( b1 , a1 , 2 );
	GT_TMP_TEST_TRI_ON_TRI_SIDE( c1 , b1 , 2 );
	GT_TMP_TEST_TRI_ON_TRI_SIDE( a1 , c1 , 2 );
    GT_TMP_TEST_TRI_ON_TRI_SIDE( b2 , a2 , 1 );
	GT_TMP_TEST_TRI_ON_TRI_SIDE( c2 , b2 , 1 );
	GT_TMP_TEST_TRI_ON_TRI_SIDE( a2 , c2 , 1 );
	return true;
}
GN_API bool GNg::TestRayIntPlane(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *pnNz, float pd){
    float t1 = ( (-pd) - GN_Vec2Dot( pnNz-> , rp-> )) / GN_Vec2Dot( pnNz-> , rv-> ); /*射粒点到平面,计算t的有效距离*/
    if(t1 <= 0.0001f)return false; /*3角形在射线的后面*/
	return true;
}
GN_API bool GNg::TestRayIntPlane(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *pnNz, float pd, sVEC2 *crossP){
    float t1 = ( (-pd) - GN_Vec2Dot( pnNz-> , rp-> )) / GN_Vec2Dot( pnNz-> , rv-> ); /*射粒点到平面,计算t的有效距离*/
    *crossP = t1 * (*rv) + (*rp);
	return t1 <= 0.0001f ? false : true;
}
GN_API bool GNg::TestRayIntND(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *Nz, float d){
    float t1 = ( d - GN_Vec2Dot( Nz-> , rp-> )) / GN_Vec2Dot( Nz-> , rv-> ); /*射粒点到平面,计算t的有效距离*/
    if(t1 <= 0.0001f)return false; /*3角形在射线的后面*/
	return true;
}
GN_API bool GNg::TestRayIntND(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *Nz, float d, sVEC2 *crossP){
    float t1 = ( d - GN_Vec2Dot( Nz-> , rp-> )) / GN_Vec2Dot( Nz-> , rv-> ); /*射粒点到平面,计算t的有效距离*/
    *crossP = t1 * (*rv) + (*rp);
	return t1 <= 0.0001f ? false : true;
}
GN_API bool GNg::TestTriInPlaneBack(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, const sVEC2 *pl_n, const float *pl_d, int pln){
	GNg::sVEC2 *_pln, *pl_ns = (GNg::sVEC2 *)pl_n;
	float pld;
	int i = 0;
	for(; i < pln; ++i){
	    _pln = pl_ns + i;
        pld = pl_d[i];
        if(GN_Vec2Dot( a-> , _pln-> ) < pld && GN_Vec2Dot( b-> , _pln-> ) < pld && GN_Vec2Dot( c-> , _pln-> ) < pld)return true;
	}
	return false;
}
#define GT_TMP_TEST_VEXS_ALL_IN_PLANE_BACK( DT ) \
	GNg::sVEC2 *_pln, *pl_ns = (GNg::sVEC2 *)pl_n;\
	float pld;\
	int pi , i = 0;\
	for(; i < pln; ++i){\
	    _pln = pl_ns + i;\
        pld = pl_d[i];\
	    for(pi = 0; pi < pn && (GN_Vec2Dot( points[pi] DT , _pln-> ) < pld); ++pi);\
        if(pi >= pn)return true;\
	}\
    return false

GN_API bool GNg::TestPointsAllInPlaneBack(const sVEC2 *points, int pn, const sVEC2 *pl_n, const float *pl_d, int pln){
    GT_TMP_TEST_VEXS_ALL_IN_PLANE_BACK( . );
}
GN_API bool GNg::TestPointsAllInPlaneBack(const sVEC2 **points, int pn, const sVEC2 *pl_n, const float *pl_d, int pln){
    GT_TMP_TEST_VEXS_ALL_IN_PLANE_BACK( -> );
}
GN_API int GNg::TestPointsAllInPlaneBackOrFront(const sVEC2 *points, int pn, const sVEC2 *pl_n, const float *pl_d, int pln){
	GNg::sVEC2 *_pln, *pl_ns = (GNg::sVEC2 *)pl_n;
	float pld;
	int ret = -1, pi , i = 0;
	for(; i < pln; ++i){
	    _pln = pl_ns + i;
        pld = pl_d[i];
		for(pi = 0; pi < pn && (GN_Vec2Dot( points[pi]. , _pln-> ) < pld); ++pi);
        if(pi >= pn)return i;
		else if(ret == -1){
            if(!pi){
                for(pi = 1; pi < pn && (GN_Vec2Dot( points[pi]. , _pln-> ) >= pld); ++pi);
                if(pi < pn)ret = -2;
			}else ret = -2;
        }
	}
    return ret;
}
GN_API bool GNg::TestPointsNoIntSpace(const sVEC2 *points, int pn, const sRECT2 *rc){
	const sVEC2 *pend = points + pn;
	for(; points < pend; ++points){
		if(points->x >= rc->left && points->x <= rc->right  &&  points->y >= rc->top && points->y <= rc->bottom)return false;
	}
    return true;
}
GN_API int GNg::TestPointsAllInRectThere(const sRECT2 *rect, const sVEC2 *vexs, int pn){
    int i, xn = 0, _xn = 0, yn = 0, _yn = 0;
	for(i = 0; i < pn; ++i){
	    if(vexs[i].x >= rect->left)++xn;
		else if(xn)break;
	}
	if(!xn)return 1;
	for(i = 0; i < pn; ++i){
	    if(vexs[i].x <= rect->right)++_xn;
		else if(_xn)break;
	}
    if(!_xn)return 4;
	for(i = 0; i < pn; ++i){
	    if(vexs[i].y >= rect->top)++yn;
		else if(yn)break;
	}
	if(!yn)return 2;
	for(i = 0; i < pn; ++i){
	    if(vexs[i].y <= rect->bottom)++_yn;
		else if(_yn)break;
	}
	if(!_yn)return 5;
	return (xn == pn && _xn == pn && yn == pn && _yn == pn) ? -1 : 0;
}
GN_API bool GNg::TestLineIntLine(const sVEC2 *p0a, const sVEC2 *p0b, const sVEC2 *p1a, const sVEC2 *p1b, float line_w){
   #define GT_TMP_LINE_INT_LINE_DEF \
	sVEC2 v_0, v_1, v;\
    float d, da, db;\
	bool uw = false;\
	if(line_w){\
        uw = true;\
		line_w *= 1.8f;\
	}
   #define GT_TMP_LINE_INT_LINE_TEST( ID0 , ID1 , ADD_PROC ) \
    v = (*p##ID0##b) - (*p##ID0##a);\
	ADD_PROC \
	d  = GN_Vec2Dot( v. , p##ID0##b-> );\
    da = GN_Vec2Dot( v. , p##ID1##a-> );\
	db = GN_Vec2Dot( v. , p##ID1##b-> );\
	if(uw){\
        if(da - d > line_w && db - d > line_w)return false;\
        d  = GN_Vec2Dot( v. , p##ID0##a-> );\
		if(d - da > line_w && d - db > line_w)return false;\
	}else{\
		if(!(da < d || db < d))return false;\
        d  = GN_Vec2Dot( v. , p##ID0##a-> );\
		if(!(da > d || db > d))return false;\
	}\
	GNg::Vec2Right(&v_##ID0, &v);\
    d  = GN_Vec2Dot( v_##ID0. , p##ID0##b-> );\
    da = GN_Vec2Dot( v_##ID0. , p##ID1##a-> );\
	db = GN_Vec2Dot( v_##ID0. , p##ID1##b-> );\
	if(uw){\
        if(da - d > line_w && db - d > line_w)return false;\
		if(d - da > line_w && d - db > line_w)return false;\
	}else{\
        if(!(da < d || db < d))return false;\
	    if(!(da > d || db > d))return false;\
	}

	GT_TMP_LINE_INT_LINE_DEF
    GT_TMP_LINE_INT_LINE_TEST( 0 , 1 , if(uw)GNg::Vec2Normalize(&v, &v); )
	GT_TMP_LINE_INT_LINE_TEST( 1 , 0 , if(uw)GNg::Vec2Normalize(&v, &v); )
	return true;
}
GN_API bool GNg::TestLineIntLine(const sVEC2 *p0a, const sVEC2 *p0b, const sVEC2 *p1a, const sVEC2 *p1b, sVEC2 *crossP, sVEC2 **rlp_out, float *d2_out/*[2]*/, float line_w){
	GT_TMP_LINE_INT_LINE_DEF
    GT_TMP_LINE_INT_LINE_TEST( 0 , 1 , GNg::Vec2Normalize(&v, &v);  )
	if(da > db){
        d2_out [0] = da - d;
        rlp_out[0] = (sVEC2*)p1a;
	}else{
        d2_out [0] = db - d;
        rlp_out[0] = (sVEC2*)p1b;
	}
	GT_TMP_LINE_INT_LINE_TEST( 1 , 0 , GNg::Vec2Normalize(&v, &v); )
	if(da > db){
        d2_out [1] = da - d;
        rlp_out[1] = (sVEC2*)p0a;
	}else{
        d2_out [1] = db - d;
        rlp_out[1] = (sVEC2*)p0b;
	}
    v = (*p0b) - (*p0a);
    *crossP = (v * ((d - da) / GN_Vec2Dot( v. , v_1. ))) + (*p0a);
	return true;
}
GN_API bool GNg::TestPointIntLine(float fmx, float fmy ,const sVEC2 &_p1, const sVEC2 &_p2, int line_w){
    sVEC2 _n1, _n2, _n3 = _p2 - _p1;
	Vec2Normalize(&_n1,&_n3);
	float fw = (float)line_w *0.5f + 3.0f;
	float d = fmx * _n1.x + fmy * _n1.y;
	if(d < (_p1.x * _n1.x + _p1.y * _n1.y) - fw)return false;
	if(d > (_p2.x * _n1.x + _p2.y * _n1.y) + fw)return false;
	_n2.x = - _n1.y, _n2.y = _n1.x;
    Vec2Normalize(&_n2,&_n2);
    d = fmx * _n2.x + fmy * _n2.y;
    float d2 = _p1.x * _n2.x + _p1.y * _n2.y;
    if(d > d2 + fw || d < d2-fw)return false;
    return true;
}
#endif //_GN_2DAREA_H_