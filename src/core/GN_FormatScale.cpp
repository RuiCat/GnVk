// GN_FormatScale.cpp — 图像缩放/位块传送/混合（ImgScale/ImgBlt/ImgBlend 与像素插值/混合内核）。
#include "GN.h"
#include "GN_CoreContext.h" // B7-3：缩放临时边界收敛进 GNg::GN_Core
#ifdef _GN_FORMAT_H_
// B7-3：行尾像素编号（原 sFormatScaleContext.aImgRowEndID）已并入 GNg::GN_Core。
static float GT_ImgPixLinear4(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
#define GT_TMP_IMG_LINEAR_FACTOR_GET(PB, PB1) \
    unsigned int i = (unsigned int)i_;        \
    float i__ = i_ - (float)i;                \
    unsigned char *pd = pdata + (i PB);       \
    if(i__ > 0.0f && i < GNg::GN_Core.aImgRowEndID) {    \
        float inv = 1.0f - i__;               \
        unsigned char *pd1 = pd + PB1;

    GT_TMP_IMG_LINEAR_FACTOR_GET(<< 2, 4)
    *c_add += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
    c_add[1] += ((float)pd[1] * inv + (float)pd1[1] * i__) * wr;
    c_add[2] += ((float)pd[2] * inv + (float)pd1[2] * i__) * wr;
    c_add[3] += ((float)pd[3] * inv + (float)pd1[3] * i__) * wr;
}
else {
    *c_add += (float)(*pd) * wr;
    c_add[1] += (float)pd[1] * wr;
    c_add[2] += (float)pd[2] * wr;
    c_add[3] += (float)pd[3] * wr;
}
return wr;
}
static float GT_ImgPixLinear3(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
    GT_TMP_IMG_LINEAR_FACTOR_GET(*3, 3)
    *c_add += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
    c_add[1] += ((float)pd[1] * inv + (float)pd1[1] * i__) * wr;
    c_add[2] += ((float)pd[2] * inv + (float)pd1[2] * i__) * wr;
}
else {
    *c_add += (float)(*pd) * wr;
    c_add[1] += (float)pd[1] * wr;
    c_add[2] += (float)pd[2] * wr;
}
return wr;
}
static float GT_ImgPixLinear2(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
    GT_TMP_IMG_LINEAR_FACTOR_GET(<< 1, 2)
    *c_add += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
    c_add[1] += ((float)pd[1] * inv + (float)pd1[1] * i__) * wr;
}
else {
    *c_add += (float)(*pd) * wr;
    c_add[1] += (float)pd[1] * wr;
}
return wr;
}
static float GT_ImgPixLinear1(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
    GT_TMP_IMG_LINEAR_FACTOR_GET(, 1)
    *c_add += ((float)(*pd) * inv + (float)(*pd1) * i__) * wr;
}
else {
    *c_add += (float)(*pd) * wr;
}
return wr;
}
static float GT_ImgLinePixMerge4(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
#define GT_TMP_IMG_LINE_PIX_MERGE(PROC, PROC1, PROC2)     \
    unsigned char *pd; (void)pd;                          \
    unsigned int i = (unsigned int)i_; /*合并左端像素*/   \
    float i__ = i_ - (float)i, wei = 0.0f;                \
    if(i__ > 0.0f) {                                      \
        i__ = (1.0f - i__) * wr;                          \
        wei += i__;                                       \
        PROC++ i;                                         \
    }                                                     \
    unsigned int i2 = (unsigned int)i2_; /*合并右端像素*/ \
    i__ = i2_ - (float)i2;                                \
    if(i__ > 0.0f) {                                      \
        i__ = i__ * wr;                                   \
        wei += i__;                                       \
        PROC1                                             \
    }                                                     \
    for(; i < i2; ++i) { /*合并中间连续像素*/             \
        wei += wr;                                        \
        PROC2                                             \
    }                                                     \
    return wei;

#define GT_TMP_ADD_PIX_COLOR4(I, R) \
    pd = pdata + (I << 2);          \
    c_add[0] += (float)*pd * R;     \
    c_add[1] += (float)pd[1] * R;   \
    c_add[2] += (float)pd[2] * R;   \
    c_add[3] += (float)pd[3] * R;

    GT_TMP_IMG_LINE_PIX_MERGE(GT_TMP_ADD_PIX_COLOR4(i, i__), GT_TMP_ADD_PIX_COLOR4(i2, i__), GT_TMP_ADD_PIX_COLOR4(i, wr))
}
static float GT_ImgLinePixMerge3(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
#define GT_TMP_ADD_PIX_COLOR3(I, R) \
    pd = pdata + (I * 3);           \
    c_add[0] += (float)*pd * R;     \
    c_add[1] += (float)pd[1] * R;   \
    c_add[2] += (float)pd[2] * R;

    GT_TMP_IMG_LINE_PIX_MERGE(GT_TMP_ADD_PIX_COLOR3(i, i__), GT_TMP_ADD_PIX_COLOR3(i2, i__), GT_TMP_ADD_PIX_COLOR3(i, wr))
}
static float GT_ImgLinePixMerge2(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
#define GT_TMP_ADD_PIX_COLOR2(I, R) \
    pd = pdata + (I << 1);          \
    c_add[0] += (float)*pd * R;     \
    c_add[1] += (float)pd[1] * R;

    GT_TMP_IMG_LINE_PIX_MERGE(GT_TMP_ADD_PIX_COLOR2(i, i__), GT_TMP_ADD_PIX_COLOR2(i2, i__), GT_TMP_ADD_PIX_COLOR2(i, wr))
}
static float GT_ImgLinePixMerge1(unsigned char *pdata, float i_, float i2_, float wr, float *c_add) {
#define GT_TMP_ADD_PIX_COLOR1(I, R) \
    c_add[0] += (float)pdata[I] * R;

    GT_TMP_IMG_LINE_PIX_MERGE(GT_TMP_ADD_PIX_COLOR1(i, i__), GT_TMP_ADD_PIX_COLOR1(i2, i__), GT_TMP_ADD_PIX_COLOR1(i, wr))
}
static void GT_ImgTgPixColorCpt4(unsigned char *pdata, float *cc, float wr) {
    *pdata = *cc * wr;
    pdata[1] = cc[1] * wr;
    pdata[2] = cc[2] * wr;
    pdata[3] = cc[3] * wr;
}
static void GT_ImgTgPixColorCpt3(unsigned char *pdata, float *cc, float wr) {
    *pdata = *cc * wr;
    pdata[1] = cc[1] * wr;
    pdata[2] = cc[2] * wr;
}
static void GT_ImgTgPixColorCpt2(unsigned char *pdata, float *cc, float wr) {
    *pdata = *cc * wr;
    pdata[1] = cc[1] * wr;
}
static void GT_ImgTgPixColorCpt1(unsigned char *pdata, float *cc, float wr) {
    *pdata = *cc * wr;
}
GN_API void GNg::ImgScale(uint8 *pdesc_data, int desc_w, int desc_h, const uint8 *psrc_data, int src_w, int src_h, short pix_byte, uint8 smooth) {
    unsigned int dw = (unsigned int)desc_w, dh = (unsigned int)desc_h, sw = (unsigned int)src_w, sh = (unsigned int)src_h;
    unsigned int i, j, pb = (unsigned int)pix_byte;
    unsigned int picth = pb * dw, _picth = pb * sw; //行跨字节数
    uint8 *_pdl, *_pd, *pd = pdesc_data, *pd2 = (uint8 *)psrc_data;
    float swf = (float)sw, shf = (float)sh;
    float i_ratio = swf / (float)dw, j_ratio = shf / (float)dh;
    float i_addv, j_addv;

    uint8 loc_buf[131072]; //128K
    GN::sTBUF_INT temp_buf(loc_buf, 131072);
    unsigned int *src_index_i = 0;

    if(smooth) { //线性梯度采样 ---------------------------------------------------------------------------------------------------------------------
        if(smooth > 2 && (desc_w < src_w || desc_h < src_h)) {
            bool is_min_scl = false;
            if(desc_w < src_w) is_min_scl = true;
            else GNg::GN_Core.aImgRowEndID = src_w - 1;
            float (*pf_line_pix_merge)(uint8 *, float, float, float, float *) = is_min_scl ? GT_ImgLinePixMerge4 : GT_ImgPixLinear4;
            void (*pf_tg_pix_color_cpt)(uint8 *, float *, float) = GT_ImgTgPixColorCpt4;
            switch(pix_byte) {
                case 3:
                    pf_line_pix_merge = is_min_scl ? GT_ImgLinePixMerge3 : GT_ImgPixLinear3;
                    pf_tg_pix_color_cpt = GT_ImgTgPixColorCpt3;
                    break;
                case 2:
                    pf_line_pix_merge = is_min_scl ? GT_ImgLinePixMerge2 : GT_ImgPixLinear2;
                    pf_tg_pix_color_cpt = GT_ImgTgPixColorCpt2;
                    break;
                case 1:
                    pf_line_pix_merge = is_min_scl ? GT_ImgLinePixMerge1 : GT_ImgPixLinear1;
                    pf_tg_pix_color_cpt = GT_ImgTgPixColorCpt1;
                    break;
            }
            shf -= j_ratio * 0.5f;
            swf -= i_ratio * 0.5f;
            unsigned int j2, csize = pix_byte << 2 /**sizeof(float)*/;
            uint8 *pd2j, *pd2_j, *pd2_jj, *_pd2j;
            float i_, j_ = 0.0f, i2_, j2_, j__, __j, wei, c4[4];
            if(desc_h < src_h) { //竖缩小
                for(; j_ < shf; j_ = j2_) {
                    j2_ = j_ + j_ratio;
                    j = (unsigned int)j_;
                    j__ = j_ - (float)j;
                    pd2j = pd2 + j * _picth;
                    if(j__ > 0.0f) {
                        j__ = 1.0f - j__;
                        pd2_j = pd2j + _picth;
                    } else pd2_j = pd2j;
                    j2 = (unsigned int)j2_;
                    __j = j2_ - (float)j2;
                    pd2_jj = pd2 + j2 * _picth;

                    for(i_ = 0.0f; i_ < swf; i_ = i2_, pd += pb) {
                        i2_ = i_ + i_ratio;
                        wei = 0.0f;
                        memset(c4, 0, csize);
                        if(j__ > 0.0f)
                            wei += pf_line_pix_merge(pd2j, i_, i2_, j__, c4); /*合并上端行像素*/
                        if(__j > 0.0f)
                            wei += pf_line_pix_merge(pd2_jj, i_, i2_, __j, c4); /*合并下端行像素*/

                        for(_pd2j = pd2_j; _pd2j < pd2_jj; _pd2j += _picth)
                            wei += pf_line_pix_merge(_pd2j, i_, i2_, 1.0f, c4); /*合并中间连续行像素*/

                        pf_tg_pix_color_cpt(pd, c4, 1.0f / wei);
                    }
                }
            } else {
                float inv;
                unsigned int endj = src_h - 1;
                for(; j_ < shf; j_ = j2_) {
                    j2_ = j_ + j_ratio;
                    j = (unsigned int)j_;
                    j__ = j_ - (float)j;
                    pd2j = pd2 + j * _picth;
                    if(j__ > 0.0f && j < endj) {
                        inv = 1.0f - j__;
                        pd2_j = pd2j + _picth;
                    } else pd2_j = 0;

                    for(i_ = 0.0f; i_ < swf; i_ = i2_, pd += pb) {
                        i2_ = i_ + i_ratio;
                        wei = 0.0f;
                        memset(c4, 0, csize);
                        if(pd2_j) {
                            wei += pf_line_pix_merge(pd2_j, i_, i2_, j__, c4);
                            wei += pf_line_pix_merge(pd2j, i_, i2_, inv, c4);
                        } else {
                            wei += pf_line_pix_merge(pd2j, i_, i2_, 1.0f, c4);
                        }
                        pf_tg_pix_color_cpt(pd, c4, 1.0f / wei);
                    }
                }
            }
        } else {
            if(sh == 1) { //如遇到1象素高度的图象使它变为大于1高度来统一更好地处理
                src_index_i = (unsigned int *)temp_buf.GetSpace((((dw + dh) << 2) + _picth) << 2);
                pd2 = (uint8 *)(&(src_index_i[((dw + dh) << 2)]));
                memcpy(pd2, psrc_data, _picth);
                memcpy(pd2 + _picth, psrc_data, _picth);
                sh = 2;
                j_ratio = (float)sh / (float)dh;
            } else {
                src_index_i = (unsigned int *)temp_buf.GetSpace(((dw + dh) << 4)); //一次过分配好各种临时产生数据的容量,以减少性能开销
            }
            unsigned int *src_index_j = src_index_i + dw;
            uint8 *src_index_u1 = (uint8 *)(src_index_j + dh);
            uint8 *src_index_1u = src_index_u1 + dw;
            uint8 *src_index_v1 = src_index_1u + dw;
            uint8 *src_index_1v = src_index_v1 + dh;
            float *src_index_u = (float *)(src_index_1v + dh);
            float *src_index_v = (src_index_u + dw);

            unsigned int _i, _j, pb_picth = pb + _picth;
            float u, v, _u, _v;
            for(i = 0, i_addv = 0.0f; i < dw; ++i, i_addv += i_ratio) {
                _i = (unsigned int)i_addv;
                src_index_i[i] = _i * pb; //预先计算好行插值样本索引, 以提高性能
                src_index_u[i] = u = i_addv - (float)_i;
                if(u < 0.360001f) {
                    src_index_u1[i] = 0;
                    src_index_1u[i] = 8;
                } else if(u < 0.78f) {
                    src_index_u1[i] = 1;
                    src_index_1u[i] = 1;
                } else {
                    src_index_u1[i] = 8;
                    src_index_1u[i] = 0;
                }
            }
            for(_i = (sw - 2) * pb, i = dw - 1; i > 0; --i) { //使不能索引到最右边的一列
                if(src_index_i[i] > _i) src_index_i[i] = _i;
                else break;
            }
            //预先计算好原图象离目标点的相邻点的点索引偏移
            const unsigned int B_0 = 0, B_1 = pb + 0, B_2 = _picth + 0, B_3 = pb_picth + 0;
            const unsigned int G_0 = 1, G_1 = pb + 1, G_2 = _picth + 1, G_3 = pb_picth + 1;
            const unsigned int R_0 = 2, R_1 = pb + 2, R_2 = _picth + 2, R_3 = pb_picth + 2;
            const unsigned int A_0 = 3, A_1 = pb + 3, A_2 = _picth + 3, A_3 = pb_picth + 3;

            uint8 _u1, _1u, _v1, _1v;

            unsigned int ci = (sw - 1) * pb, cj;
            pd = ((dw - 1) * pb) + pdesc_data;

            //计算好最右边一列的象图颜色先 - 不插值
#define GT_TMP_IMG_SCALE_POSE_COMPUTE1                                              \
    for(j = 0, j_addv = 0.0f; j < dh; ++j, j_addv += j_ratio, pd += picth) {        \
        _j = (unsigned int)j_addv;                                                  \
        src_index_j[j] = cj = _j * _picth; /*预先计算好列插值样本索引, 以提高性能*/ \
        src_index_v[j] = v = j_addv - (float)_j;                                    \
        if(v < 0.360001f) {                                                         \
            src_index_v1[j] = 0;                                                    \
            src_index_1v[j] = 8;                                                    \
        } else if(v < 0.78f) {                                                      \
            src_index_v1[j] = 1;                                                    \
            src_index_1v[j] = 1;                                                    \
        } else {                                                                    \
            src_index_v1[j] = 8;                                                    \
            src_index_1v[j] = 0;                                                    \
        }                                                                           \
        _pd = (cj + ci) + pd2;

#define GT_TMP_IMG_SCALE_POSE_COMPUTE1_END                                       \
    }                                                                            \
    --dw;                                                                        \
    --dh;                                                                        \
    for(_j = (sh - 2) * _picth, j = dh; j > 0; --j) { /*使不能索引到最底的一行*/ \
        if(src_index_j[j] > _j) src_index_j[j] = _j;                             \
        else break;                                                              \
    }                                                                            \
    pd = pdesc_data;

            //续个目标图象点计算 - 使用插值
#define GT_TMP_IMG_SCALE_POSE_COMPUTE2      \
    for(j = 0; j < dh; ++j, pd += pb) {     \
        _v1 = src_index_v1[j];              \
        _1v = src_index_1v[j];              \
                                            \
        _pdl = src_index_j[j] + pd2;        \
        for(i = 0; i < dw; ++i, pd += pb) { \
            _pd = _pdl + src_index_i[i];    \
                                            \
            _u1 = src_index_u1[i];          \
            _1u = src_index_1u[i];

//续个目标图象点计算 - 使用线性插值
#define GT_TMP_IMG_SCALE_POSE_COMPUTE2_L    \
    for(j = 0; j < dh; ++j, pd += pb) {     \
        _v = src_index_v[j];                \
        v = 1.0f - _v;                      \
                                            \
        _pdl = src_index_j[j] + pd2;        \
        for(i = 0; i < dw; ++i, pd += pb) { \
            _pd = _pdl + src_index_i[i];    \
                                            \
            _u = src_index_u[i];            \
            u = 1.0f - _u;

            //计算最底一行的象图颜色 - 不插值
#define GT_TMP_IMG_SCALE_POSE_COMPUTE3  \
    }                                   \
    }                                   \
    _pdl = src_index_j[j] + pd2;        \
    for(i = 0; i < dw; ++i, pd += pb) { \
        _pd = _pdl + src_index_i[i];

            if(smooth == 1) {
                switch(pix_byte) {
                    case 4:
                        GT_TMP_IMG_SCALE_POSE_COMPUTE1
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
                        pd[A_0] = _pd[A_0];
                        GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                        GT_TMP_IMG_SCALE_POSE_COMPUTE2

#define GT_TMP_IMG_SCALE_LEVEL_PROC(CID) \
    pd[CID##_0] = (((_pd[CID##_0] >> _u1) + (_pd[CID##_1] >> _1u)) >> _v1) + (((_pd[CID##_2] >> _u1) + (_pd[CID##_3] >> _1u)) >> _1v)

                        GT_TMP_IMG_SCALE_LEVEL_PROC(B);
                        GT_TMP_IMG_SCALE_LEVEL_PROC(G);
                        GT_TMP_IMG_SCALE_LEVEL_PROC(R);
                        GT_TMP_IMG_SCALE_LEVEL_PROC(A);
                        GT_TMP_IMG_SCALE_POSE_COMPUTE3
                        pd[B_0] = _pd[B_0];
                        pd[G_0] = _pd[G_0];
                        pd[R_0] = _pd[R_0];
                        pd[A_0] = _pd[A_0];
                }
                break;
                case 3:
                    GT_TMP_IMG_SCALE_POSE_COMPUTE1
                    pd[B_0] = _pd[B_0];
                    pd[G_0] = _pd[G_0];
                    pd[R_0] = _pd[R_0];
                    GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                    GT_TMP_IMG_SCALE_POSE_COMPUTE2
                    GT_TMP_IMG_SCALE_LEVEL_PROC(B);
                    GT_TMP_IMG_SCALE_LEVEL_PROC(G);
                    GT_TMP_IMG_SCALE_LEVEL_PROC(R);
                    GT_TMP_IMG_SCALE_POSE_COMPUTE3
                    pd[B_0] = _pd[B_0];
                    pd[G_0] = _pd[G_0];
                    pd[R_0] = _pd[R_0];
            }
            break;
            case 1:
                GT_TMP_IMG_SCALE_POSE_COMPUTE1
                pd[B_0] = _pd[B_0];
                GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
                GT_TMP_IMG_SCALE_POSE_COMPUTE2
                GT_TMP_IMG_SCALE_LEVEL_PROC(B);
                GT_TMP_IMG_SCALE_POSE_COMPUTE3
                pd[B_0] = _pd[B_0];
        }
        break;
        case 2:
            GT_TMP_IMG_SCALE_POSE_COMPUTE1
            pd[B_0] = _pd[B_0];
            pd[G_0] = _pd[G_0];
            GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
            GT_TMP_IMG_SCALE_POSE_COMPUTE2
            GT_TMP_IMG_SCALE_LEVEL_PROC(B);
            GT_TMP_IMG_SCALE_LEVEL_PROC(G);
            GT_TMP_IMG_SCALE_POSE_COMPUTE3
            pd[B_0] = _pd[B_0];
            pd[G_0] = _pd[G_0];
    }
    break;
}
}
else {
    switch(pix_byte) {
        case 4:
            GT_TMP_IMG_SCALE_POSE_COMPUTE1
            pd[B_0] = _pd[B_0];
            pd[G_0] = _pd[G_0];
            pd[R_0] = _pd[R_0];
            pd[A_0] = _pd[A_0];
            GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
            GT_TMP_IMG_SCALE_POSE_COMPUTE2_L

#define GT_TMP_IMG_SCALE_LINEAR_PROC(CID) \
    pd[CID##_0] = (_pd[CID##_0] * u + _pd[CID##_1] * _u) * v + (_pd[CID##_2] * u + _pd[CID##_3] * _u) * _v;

            GT_TMP_IMG_SCALE_LINEAR_PROC(B);
            GT_TMP_IMG_SCALE_LINEAR_PROC(G);
            GT_TMP_IMG_SCALE_LINEAR_PROC(R);
            GT_TMP_IMG_SCALE_LINEAR_PROC(A);
            GT_TMP_IMG_SCALE_POSE_COMPUTE3
            pd[B_0] = _pd[B_0];
            pd[G_0] = _pd[G_0];
            pd[R_0] = _pd[R_0];
            pd[A_0] = _pd[A_0];
    }
    break;
    case 3:
        GT_TMP_IMG_SCALE_POSE_COMPUTE1
        pd[B_0] = _pd[B_0];
        pd[G_0] = _pd[G_0];
        pd[R_0] = _pd[R_0];
        GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
        GT_TMP_IMG_SCALE_POSE_COMPUTE2_L
        GT_TMP_IMG_SCALE_LINEAR_PROC(B);
        GT_TMP_IMG_SCALE_LINEAR_PROC(G);
        GT_TMP_IMG_SCALE_LINEAR_PROC(R);
        GT_TMP_IMG_SCALE_POSE_COMPUTE3
        pd[B_0] = _pd[B_0];
        pd[G_0] = _pd[G_0];
        pd[R_0] = _pd[R_0];
}
break;
case 1:
    GT_TMP_IMG_SCALE_POSE_COMPUTE1
    pd[B_0] = _pd[B_0];
    GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
    GT_TMP_IMG_SCALE_POSE_COMPUTE2_L
    GT_TMP_IMG_SCALE_LINEAR_PROC(B);
    GT_TMP_IMG_SCALE_POSE_COMPUTE3
    pd[B_0] = _pd[B_0];
    }
    break;
case 2:
    GT_TMP_IMG_SCALE_POSE_COMPUTE1
    pd[B_0] = _pd[B_0];
    pd[G_0] = _pd[G_0];
    GT_TMP_IMG_SCALE_POSE_COMPUTE1_END
    GT_TMP_IMG_SCALE_POSE_COMPUTE2_L
    GT_TMP_IMG_SCALE_LINEAR_PROC(B);
    GT_TMP_IMG_SCALE_LINEAR_PROC(G);
    GT_TMP_IMG_SCALE_POSE_COMPUTE3
    pd[B_0] = _pd[B_0];
    pd[G_0] = _pd[G_0];
    }
    break;
    }
    }
    }
    }
    else { //邻近点采样 ---------------------------------------------------------------------------------------------------------------------
        src_index_i = (unsigned int *)temp_buf.GetSpace(dw << 2);
        for(i = 0, i_addv = 0.0f; i < dw; ++i, i_addv += i_ratio) {
            src_index_i[i] = (unsigned int)i_addv * pb; //预先计算好相对目标图象行的每个象素在原图象行的第几个位量上, 以提高性能
        }
        //续个目标图象点计算
#define GT_TMP_IMG_SCALE_POSE_COMPUTE                           \
    for(j = 0, j_addv = 0.0f; j < dh; ++j, j_addv += j_ratio) { \
        _pdl = ((unsigned int)j_addv * _picth) + pd2;           \
        for(i = 0; i < dw; ++i, pd += pb) {                     \
            _pd = _pdl + src_index_i[i];

        switch(pix_byte) {
            case 4:
                GT_TMP_IMG_SCALE_POSE_COMPUTE
                pd[0] = _pd[0];
                pd[1] = _pd[1];
                pd[2] = _pd[2];
                pd[3] = _pd[3];
        }
    }
    break;
case 3:
    GT_TMP_IMG_SCALE_POSE_COMPUTE
    pd[0] = _pd[0];
    pd[1] = _pd[1];
    pd[2] = _pd[2];
    }
    }
    break;
case 1:
    GT_TMP_IMG_SCALE_POSE_COMPUTE
    pd[0] = _pd[0];
    }
    }
    break;
case 2:
    GT_TMP_IMG_SCALE_POSE_COMPUTE
    pd[0] = _pd[0];
    pd[1] = _pd[1];
    }
    }
    break;
    }
    }
    }
    GN_API uint8 *GNg::ImgCopyToData(const uint8 *pdata, int pitch, int h, bool col_turn) {
        int size = pitch * h;
        uint8 *t_data = new uint8[size];
        if(col_turn) {
            pdata += pitch * (h - 1);
            uint8 *ps, *pe = t_data + size;
            for(ps = t_data; ps < pe; ps += pitch, pdata -= pitch) memcpy(ps, pdata, pitch);

        } else memcpy(t_data, pdata, size);
        return t_data;
    }
    GN_API void GNg::ImgBlt(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, COLOR_FORMAT srcFormat) {
        int pb = (int)ImgFmtGetByteNum(srcFormat);
        int src_pitch = src_w * pb, desc_pitch = desc_w * pb, row_size = _src_w * pb;

        uint8 *_psrc_data = (uint8 *)psrc_data + (src_pitch * _src_y + _src_x * pb);
        pdesc_data += desc_pitch * _desc_y + _desc_x * pb;
        for(_src_h += _src_y; _src_y < _src_h; ++_src_y, _psrc_data += src_pitch, pdesc_data += desc_pitch) {
            memcpy(pdesc_data, _psrc_data, row_size);
        }
    }
    GN_API void GNg::ImgTsBlt(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, COLOR_FORMAT srcFormat, COLOR_FORMAT descFormat, bool RGB_turn) {
        int src_pb = (int)ImgFmtGetByteNum(srcFormat), desc_pb = (int)ImgFmtGetByteNum(descFormat);
        int src_pitch = src_w * src_pb, desc_pitch = desc_w * desc_pb;

        uint8 *_psrc_data = (uint8 *)psrc_data + (src_pitch * _src_y + _src_x * src_pb);
        pdesc_data += desc_pitch * _desc_y + _desc_x * desc_pb;
        for(_src_h += _src_y; _src_y < _src_h; ++_src_y, _psrc_data += src_pitch, pdesc_data += desc_pitch) {
            ImgFmtConvertTs(pdesc_data, descFormat, _psrc_data, srcFormat, _src_w, RGB_turn);
        }
    }
    GN_API void GNg::ImgScaleBlt(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, int _desc_w, int _desc_h, COLOR_FORMAT srcFormat, uint8 *proc_buf) {
        if(_src_w == _desc_w && _src_h == _desc_h) {
            ImgBlt(psrc_data, src_w, _src_x, _src_y, _src_w, _src_h, pdesc_data, desc_w, _desc_x, _desc_y, srcFormat);
        } else {
            int pb = (int)ImgFmtGetByteNum(srcFormat);
            int src_size = _src_w * _src_h * pb;
            bool buf_is_new = false;
            if(!proc_buf) {
                proc_buf = new uint8[(src_size + (_desc_w * _desc_h * pb)) << 1];
                buf_is_new = true;
            }
            uint8 *__proc_buf = proc_buf + src_size;

            ImgBlt(psrc_data, src_w, _src_x, _src_y, _src_w, _src_h, proc_buf, _src_w, 0, 0, srcFormat); //先把原数据块移到一个完整的小区划里
            ImgScale(__proc_buf, _desc_w, _desc_h, proc_buf, _src_w, _src_h, (short)pb); //从小区划缩放到另一小区划
            ImgBlt(__proc_buf, _desc_w, 0, 0, _desc_w, _desc_h, pdesc_data, desc_w, _desc_x, _desc_y, srcFormat); //再从另一小区划里般到实际想要的地方

            if(buf_is_new) delete[] proc_buf;
        }
    }
    GN_API void GNg::ImgMultMultiplyRatio(uint8 *pdata, COLOR_FORMAT cfmt, int img_w, int x, int y, int w, int h, uint8 ratio, bool to_alpha) {
        if(to_alpha && cfmt != CR_RGBA /*&& cfmt != CR_RGBA4444 && cfmt != CR_RGBA5551*/ && cfmt != CR_LUMINANCE_ALPHA && cfmt != CR_ALPHA) to_alpha = false;
        unsigned int r_ = (unsigned int)ratio;
        int pb = (int)ImgFmtGetByteNum(cfmt);
        int pitch = img_w * pb, row_size = w * pb, j;
        uint8 *_data = pdata + (y * pitch + x * pb), *_data_sta, *_data_end;
        if(to_alpha) {
            uint8 aid = 0;
            switch(cfmt) {
                case CR_RGBA:
                    aid = 3;
                    break;
                    /*case CR_RGBA4444:        aid = 3; break;
		case CR_RGBA5551:        aid = 3; break;*/
                case CR_LUMINANCE_ALPHA: aid = 1; break;
                            default: break; // B9-3: 未列举枚举值兜底（保行为）
}
#define GT_TMP_IMG_RANGE_LOOP                \
    for(j = 0; j < h; ++j, _data += pitch) { \
        for(_data_sta = _data, _data_end = _data + row_size; _data_sta < _data_end; _data_sta += pb) {
            GT_TMP_IMG_RANGE_LOOP
            _data_sta[aid] = (uint8)((unsigned int)_data_sta[aid] * r_ >> 8);
        }
    }
    }
    else {
        switch(cfmt) {
            case CR_RGBA:
            case CR_RGB:
                GT_TMP_IMG_RANGE_LOOP
                _data_sta[0] = (uint8)((unsigned int)_data_sta[0] * r_ >> 8);
                _data_sta[1] = (uint8)((unsigned int)_data_sta[1] * r_ >> 8);
                _data_sta[2] = (uint8)((unsigned int)_data_sta[2] * r_ >> 8);
                    default: break; // B9-3: 未列举枚举值兜底（保行为）
}
    }
    break;
case CR_RGB565: {
    unsigned short *tcolor;
    GT_TMP_IMG_RANGE_LOOP
    tcolor = (unsigned short *)_data_sta;
    tcolor[0] = (unsigned short)(((unsigned int)(tcolor[0] & 31) * r_ >> 8) | ((unsigned int)((tcolor[0] & 2016) >> 5) * r_ >> 8 << 5) | ((unsigned int)((tcolor[0] & 63488) >> 11) * r_ >> 8 << 11));
}
    }
    }
    break;
    /*case CR_RGBA4444:
		case CR_RGBA5551:
		     break;*/
case CR_LUMINANCE_ALPHA:
case CR_LUMINANCE:
case CR_RED:
case CR_GREEN:
case CR_BLUE:
    GT_TMP_IMG_RANGE_LOOP
    _data_sta[0] = (uint8)((unsigned int)_data_sta[0] * r_ >> 8);
    }
    }
    break;
    }
    }
    }
    GN_API void GNg::ImgBlendRGBAtoRGBAusNormal(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y) {
        int src_px_size = (int)ImgFmtGetByteNum(GNg::CR_RGBA);
        int desc_px_size = src_px_size;
        unsigned int r_, _r;

#define GT_TMP_IMAGE_PIXEL_RANGE_LOOP                                                                         \
    int src_pitch = src_w * src_px_size, desc_pitch = desc_w * desc_px_size, row_size = _src_w * src_px_size; \
    uint8 *_psrc_data = (uint8 *)psrc_data + (src_pitch * _src_y + _src_x * src_px_size);                     \
    pdesc_data += desc_pitch * _desc_y + _desc_x * desc_px_size;                                              \
    uint8 *_pdesc_pix, *_psrc_pix, *_psrc_pix_end;                                                            \
    for(_src_h += _src_y; _src_y < _src_h; ++_src_y, _psrc_data += src_pitch, pdesc_data += desc_pitch) {     \
        for(_pdesc_pix = pdesc_data, _psrc_pix = _psrc_data, _psrc_pix_end = _psrc_data + row_size; _psrc_pix < _psrc_pix_end; _pdesc_pix += desc_px_size, _psrc_pix += src_px_size) {

        GT_TMP_IMAGE_PIXEL_RANGE_LOOP
        r_ = (unsigned int)_psrc_pix[3];
        _r = 255 - r_;
        _pdesc_pix[0] = (uint8)(((unsigned int)_pdesc_pix[0] * _r >> 8) + ((unsigned int)_psrc_pix[0] * r_ >> 8));
        _pdesc_pix[1] = (uint8)(((unsigned int)_pdesc_pix[1] * _r >> 8) + ((unsigned int)_psrc_pix[1] * r_ >> 8));
        _pdesc_pix[2] = (uint8)(((unsigned int)_pdesc_pix[2] * _r >> 8) + ((unsigned int)_psrc_pix[2] * r_ >> 8));
        _pdesc_pix[3] = (unsigned int)_pdesc_pix[3] + r_ < 255 ? _pdesc_pix[3] + _psrc_pix[3] : 255;
    }
    }
    }
    GN_API void GNg::ImgBlend(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, int src_px_size, int desc_px_size, void (*blend_func)(const uint8 *src_pix, uint8 *desc_pix)) {
        GT_TMP_IMAGE_PIXEL_RANGE_LOOP
        blend_func(_psrc_pix, _pdesc_pix);
    }
    }
    }
#endif //_GN_FORMAT_H_
