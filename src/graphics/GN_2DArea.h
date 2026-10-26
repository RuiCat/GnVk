#ifndef _GN_2DAREA_H_
#define _GN_2DAREA_H_
/**************************************************** 空间区划产生实用函数 **********************************************************/
namespace GNg {
struct sAREA_QUADS2 //四边形区域2D
{
    GNg::sVEC2 p[4]; //四边形区域的4个角点 前四个点序和四边型一样
    GNg::sQUADS_SPACE2 sp; //四边形区域空间面

    sAREA_QUADS2() {}
    sAREA_QUADS2(const sQUADS2 *q, bool to_unit = true) : sp(q, to_unit) {
        *((sQUADS2 *)p) = *q;
        GNg::RangeQuadsGetAnglePointOther(*q, p[3]);
    }
};
//使四边型向一组点对齐靠拢(点, 点数, 四边形区域的4个角点, 四边形区域空间面, 四边型边筐线的两个方向) 返回是否有对齐更变过点
GN_API bool Quads2DAreaAlign(const GNg::sVEC2 *qp1, uint8 pn, GNg::sVEC2 *qp2, GNg::sQUADS_SPACE2 *qs2, const GNg::sVEC2 *_rv2 /*[2]*/);

//按给定四边形左上两面方向来构建一个能包含所有指定点的四边形区域(四边形区域输出, 四边形左上两面方向 需标准化好, 点组1, 点组1数, 点组2, 点组2数)
GN_API void MakeAreaQuads(sAREA_QUADS2 *qa_out, const GNg::sVEC2 *qnNz /*[2]*/, const GNg::sVEC2 *points1, int pn1, const GNg::sVEC2 *points2, int pn2);

/***************************************************** 碰撞检测实用功能 *************************************************************/

// !!!通常与区域测试会比图元和图元测试效率更高 !!!

//检测相对于观察点向四边型区域1和四边型区域2那个在前面(四边形区域1, 四边形区域2, 视线方向或视点位置, 参数pv是否为方向向量) 返回1:四边型区域1在前面 返回2:四边型区域2在前面 返回3:不确定可能比较远或许1在2前面同时2也相对在1前面  返回0两四边型或者相交 !若要快速准确判断是否相交请调用 TestQuadsAreaIntQuadsArea
GN_API int TestQuadsAreaOnQuadsArea(/*const*/ sAREA_QUADS2 *qa1, /*const*/ sAREA_QUADS2 *qa2, const sVEC2 *pv, bool is_v);
//检测相对于观察点向四边型区域1和四边型区域2那个相对在前面(四边形区域1, 四边形区域2, 视线方向或视点位置, 参数pv是否为方向向量) 返回1:四边型区域1相对在前面 返回2:四边型区域2相对在前面 !若要快速准确判断是否相交请调用 TestQuadsAreaIntQuadsArea
GN_API int TestQuadsAreaOnQuadsAreaRelative(/*const*/ sAREA_QUADS2 *qa1, /*const*/ sAREA_QUADS2 *qa2, const sVEC2 *pv, bool is_v);

//检测四边型区域是否与另一个四边型区域相交(四边形区域1, 四边形区域2)返回是否相交 !通常大的四边形放在1 性能会比较好
GN_API bool TestQuadsAreaIntQuadsArea(/*const*/ sAREA_QUADS2 *qa1, /*const*/ sAREA_QUADS2 *qa2);
//检测四边型区域是否与另一个四边型区域相交(四边形区域1, 四边形区域2, 返回交点位置, 返回交点法线, 返回交叠深度)返回是否相交 !通常大的四边形放在1 性能会比较好
GN_API bool TestQuadsAreaIntQuadsArea(/*const*/ sAREA_QUADS2 *qa1, /*const*/ sAREA_QUADS2 *qa2, sVEC2 *crossP, sVEC2 *crossNz, float *crossR);
//检测圆球是否与四边型区域相交(圆球, 四边形区域)返回是否相交 !注意与圆球相交判断的四边型区域面法线必须先标准化好
GN_API bool TestSphereIntQuadsAreaNz(const sSPHERE2D *sph, /*const*/ sAREA_QUADS2 *qaNz);
//检测圆球是否与四边型区域相交(圆球, 四边形区域, 返回交点位置, 返回交点法线, 返回交叠深度)返回是否相交 !注意与圆球相交判断的四边型区域面法线必须先标准化好
GN_API bool TestSphereIntQuadsAreaNz(const sSPHERE2D *sph, /*const*/ sAREA_QUADS2 *qaNz, sVEC2 *crossP, sVEC2 *crossNz, float *crossR);
//检测矩型筐是否与四边型区域相交(矩型筐, 四边形区域)返回是否相交
GN_API bool TestRectIntQuadsArea(/*const*/ sRECT2 *rc, /*const*/ sAREA_QUADS2 *qa);

/******************************************************** 图元之间的检测系列 **************************************************************/

//检测点与点是否相交(点1, 点2, 误差范围)返回是否相交
GN_API bool TestPointIntPoint(const sVEC2 *point1, const sVEC2 *point2, float range = 2.0f);
//检测点是否在图筐内(坐标x, 坐标y, 筐x, 筐y, 筐w, 筐h)返回是否
GN_API bool TestPointIntRect(int x, int y, int rx, int ry, int rw, int rh);
//检测点是否在图筐内(坐标x, 坐标y, 筐)返回是否
GN_API bool TestPointIntRect(int x, int y, const sRECT *rect);
//检测点是否在图筐内(点, 筐)返回是否
GN_API bool TestPointIntRect(const sVEC2 *point, const sRECT2 *rect);

//检测点是否在四边形内(坐标x, 坐标y, 四边形点0x, 四边形点0y, 四边形点1x, 四边形点1y, 四边形点2x, 四边形点2y)返回是否
GN_API bool TestPointIntQuads(float x, float y, float p0x, float p0y, float p1x, float p1y, float p2x, float p2y);
//检测点是否在四边形内(点, 四边形)返回是否
GN_API bool TestPointIntQuads(const sVEC2 *point, const sQUADS2 *quads);

//获得三角型比较靠近同平面上指定点的点(指定点, 3角形点a, 3角形点b, 3角形点c, 返回靠近点)返回靠近点指针 !如果指针值为 point 表明点在三角型内 如果为a b c其中一个表明比较靠近对应的角点 如果为 np_out 表明返回的点是在三角形里插值得到的
GN_API sVEC2 *GetTriNearPoint(const sVEC2 *point, const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, sVEC2 *np_out);
//获得三角型比较靠近同平面上指定点的点(指定点, 3角形点a, 3角形点b, 3角形点c, 边a, 边b, 边c, 边a点积边a, 边b点积边b, 边c点积边c, 返回靠近点)返回靠近点指针 !如果指针值为 point 表明点在三角型内 如果为a b c其中一个表明比较靠近对应的角点 如果为 np_out 表明返回的点是在三角形里插值得到的 如果你已经计算好三角形边 调用这个会比较快
GN_API sVEC2 *GetTriNearPoint(const sVEC2 *point, const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, /*const*/ sVEC2 &va, /*const*/ sVEC2 &vb, /*const*/ sVEC2 &vc, float d_a, float d_b, float d_c, sVEC2 *np_out);
//获得点与3角形的靠近距离(点, 3角形)返回距离 !注意这不是绝对距离
GN_API float GetTriNearDist(const sVEC2 *point, const sTRI2D *tri);
//获得点与3角形的靠近距离(点, 3角形点a, 3角形点b, 3角形点c)返回距离 !注意这不是绝对距离
GN_API float GetTriNearDist(const sVEC2 *point, const sVEC2 *a, const sVEC2 *b, const sVEC2 *c);

//获得点与线的最近距离(点, 线点A, 线点B)返回距离
GN_API float GetLineNearDist(const sVEC2 *point, const sVEC2 *pa, const sVEC2 *pb);
//检测点是否与线相交(点, 线点A, 线点B, 误差范围 您也可通过这个来模拟带有宽度的线或点)返回是否相交
inline bool TestPointIntLine(const sVEC2 *point, const sVEC2 *pa, const sVEC2 *pb, float range = 2.0f) {
    return GetLineNearDist(point, pa, pb) < range;
}

//检测两图筐是否相交(筐1, 筐2)返回是否相交
GN_API bool TestRectIntRect(const sRECT *rect1, const sRECT *rect2);
//检测两图筐是否相交(筐1, 筐2)返回是否相交
GN_API bool TestRectIntRect(const sRECT2 *rect1, const sRECT2 *rect2);

//检测两四边形是否相交(四边形1, 四边形2)返回是否相交
GN_API bool TestQuadsIntQuads(const sQUADS2 *quads1, const sQUADS2 *quads2);
//检测两四边形是否相交(四边形1, 四边形2, 返回交点位置, 返回交点法线, 返回交叠深度)返回是否相交
GN_API bool TestQuadsIntQuads(const sQUADS2 *quads1, const sQUADS2 *quads2, sVEC2 *crossP, sVEC2 *crossNz, float *crossR);

//检测圆和圆 圆和点是否相交(圆1, 圆2)返回是否相交
GN_API bool TestSphereIntSphere(const sSPHERE2D *Op1, const sSPHERE2D *Op2);
//检测圆和圆 圆和点是否相交(圆1, 圆2, 返回交点位置, 返回交点法线, 返回交叠深度)返回是否相交
GN_API bool TestSphereIntSphere(const sSPHERE2D *Op1, const sSPHERE2D *Op2, sVEC2 *crossP, sVEC2 *crossN, float *crossR);

//检测圆是否和四边形相交(圆, 四边形)返回是否相交
GN_API bool TestSphereIntQuads(const sSPHERE2D *sph, const sQUADS2 *quads);
//检测圆是否和四边形相交(圆, 四边形, 返回交点位置, 返回交点法线, 返回交叠深度)返回是否相交
GN_API bool TestSphereIntQuads(const sSPHERE2D *sph, const sQUADS2 *quads, sVEC2 *crossP, sVEC2 *crossNz, float *crossR);

//检测3角形和3角形是否相交(3角形1, 3角形2, 三角形边半径大小)返回是否相交
GN_API bool TestTriIntTri(const sTRI2D *tr1, const sTRI2D *tr2, float r = 0.0f);
//检测3角形和3角形是否相交(3角形1点a, 3角形1点b, 3角形1点c, 3角形2点a, 3角形2点b, 3角形2点c, 三角形边半径大小)返回是否相交
GN_API bool TestTriIntTri(const sVEC2 *a1, const sVEC2 *b1, const sVEC2 *c1, const sVEC2 *a2, const sVEC2 *b2, const sVEC2 *c2, float r = 0.0f);

//检测射线是否和平面相交(射线发射点, 射线方向, 平面的法线, 平面离距离)返回是否相交
GN_API bool TestRayIntPlane(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *pn, float pd);
//检测射线是否和平面相交(射线发射点, 射线方向, 平面的法线, 平面离距离, 交点输出)返回是否相交
GN_API bool TestRayIntPlane(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *pn, float pd, sVEC2 *crossP);

//检测射线是否和平面相交(射线发射点, 射线方向, 面法线, 面离距离)返回是否相交 !这个和上面的射线平面不同在于参数 d 为正值的(P dot N)
GN_API bool TestRayIntND(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *n, float d);
//检测射线是否和平面相交(射线发射点,射线方向, 面法线, 面离距离, 交点输出)返回是否相交 !这个和上面的射线平面不同在于参数 d 为正值的(P dot N)
GN_API bool TestRayIntND(const sVEC2 *rp, const sVEC2 *rv, const sVEC2 *n, float d, sVEC2 *crossP);

//检测三角型三点是否都在其中一个面的后面(三角形点a, 三角形点b, 三角形点c, 所有平面的法线数据, 所有平面的离距离数据, 平面数)返回是否
GN_API bool TestTriInPlaneBack(const sVEC2 *a, const sVEC2 *b, const sVEC2 *c, const sVEC2 *pl_n, const float *pl_d, int pln);
//检测一组点是否都在其中一个面的后面(点数据, 点数, 所有平面的法线数据, 所有平面的离距离数据, 平面数)返回是否
GN_API bool TestPointsAllInPlaneBack(const sVEC2 *points, int pn, const sVEC2 *pl_n, const float *pl_d, int pln);
//检测一组点是否都在其中一个面的后面(点指针数据, 点数, 所有平面的法线数据, 所有平面的离距离数据, 平面数)返回是否
GN_API bool TestPointsAllInPlaneBack(const sVEC2 **points, int pn, const sVEC2 *pl_n, const float *pl_d, int pln);

//检测一组点是否都在其中一个面的后面或都在所有面的正面(点数据, 点数, 所有平面的法线数据, 所有平面的离距离数据, 平面数) 返回正数表示点都在那一个面后面的面索引 返回-1:表示都在所有面的正面 如果都不是返回-2
GN_API int TestPointsAllInPlaneBackOrFront(const sVEC2 *points, int pn, const sVEC2 *pl_n, const float *pl_d, int pln);

//检测一组点是否都不在一个矩型空间内(点数指针, 点数, 矩型空间)返回是否
GN_API bool TestPointsNoIntSpace(const sVEC2 *points, int pn, const sRECT2 *rect);

//检测指定的所有点都在矩型筐的那一边或里面(矩型筐, 点数据, 点数)返回-1:都在矩型筐里面 返回1:矩型筐自身-x方向 2:自身-y方向 4:自身x方向 5:自身y方向 如果都不是返回0
GN_API int TestPointsAllInRectThere(const sRECT2 *rect, const sVEC2 *vexs, int pn);

//检测直线是否与直线相交(线1点1, 线1点2, 线2点1, 线2点2, 线宽度)返回是否相交
GN_API bool TestLineIntLine(const sVEC2 *p0a, const sVEC2 *p0b, const sVEC2 *p1a, const sVEC2 *p1b, float line_w = 0.0f);
//检测直线是否与直线相交(线1点1, 线1点2, 线2点1, 线2点2, 返回交点, 返回对应两线右则的另一线插入点指针, 返回对应两线右则的插入深度, 线宽度)返回是否相交
GN_API bool TestLineIntLine(const sVEC2 *p0a, const sVEC2 *p0b, const sVEC2 *p1a, const sVEC2 *p1b, sVEC2 *crossP, sVEC2 **rlp_out /*[2]*/, float *d2_out /*[2]*/, float line_w = 0.0f);

//检测点是否与直线相交(点X, 点Y, 线点1, 线点2, 线宽度)返回是否相交
GN_API bool TestPointIntLine(float fmx, float fmy, const sVEC2 &_p1, const sVEC2 &_p2, int line_w);

}; //end namespace GNg
#endif //_GN_2DAREA_H_