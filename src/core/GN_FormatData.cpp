// GN_FormatData.cpp — 程序化图像/几何数据生成（圆柱/球/矩形/棋盘/箭头与数字位图，原 GN_Format.cpp 拆分）。
#include "GN.h"
#ifdef _GN_FORMAT_H_
    GN_API void GNg::GenCylinderDataToMem(uint8 *p_memory, int w, int h, float value) {
        float cp = (float)w * 0.5f - 0.5f;
        float r = (float)(w) * 0.5f, _r, _l;
        int i, j, id = 0;
        //if(is_solid){
        _r = r * value;
        float r_ = r - _r;
        for(j = 0; j < h; ++j) {
            for(i = 0; i < w; ++i, ++id) {
                _l = (float)i - cp;
                if(_l < 0.0f) _l = -_l;
                if(_l > r) p_memory[id] = 0;
                else if(_l > _r) p_memory[id] = (uint8)((1.0f - (_l - _r) / r_) * 255.0f);
                else p_memory[id] = 255;
            }
        }
        //}
    }
    GN_API void GNg::GenSphereDataToMem(uint8 *p_memory, int wh, int b, float value, bool is_solid) {
        float cp = (float)wh * 0.5f - 0.5f;
        float r = (float)(wh - (b << 1)) * 0.5f, _r, _x, _y, _l;
        int i, j, id = 0;
        if(is_solid) {
            _r = r * value;
            float r_ = r - _r;
            for(j = 0; j < wh; ++j) {
                for(i = 0; i < wh; ++i, ++id) {
                    _x = (float)i - cp;
                    _y = (float)j - cp;
                    _l = sqrtf(_x * _x + _y * _y);
                    if(_l > r) p_memory[id] = 0;
                    else if(_l > _r) p_memory[id] = (uint8)((1.0f - (_l - _r) / r_) * 255.0f);
                    else p_memory[id] = 255;
                }
            }
        } else {
            r -= 0.5f;
            for(j = 0; j < wh; ++j) {
                for(i = 0; i < wh; ++i, ++id) {
                    _x = (float)i - cp;
                    _y = (float)j - cp;
                    _l = sqrtf(_x * _x + _y * _y);
                    _r = _l > r ? _l - r : r - _l;
                    if(_r >= 1.0f) p_memory[id] = 0;
                    else p_memory[id] = 255 - (uint8)(255.0f * _r);
                }
            }
        }
    }
    GN_API float GNg::GenSphereDataToMem(uint8 *p_memory, int wh, int b, const float *_keys, int kn) {
        GNg::sVEC2 *keys = (GNg::sVEC2 *)_keys;
        float cp = (float)wh * 0.5f - 0.5f;
        float r = (float)(wh - (b << 1)) * 0.5f, _x, _y, _l;
        int i, j, id = 0;

        uint8 loc_buf[4096];
        GN::sTBUF_INT temp_buf(loc_buf, 4096);
        sVEC3 *keys2 = (sVEC3 *)temp_buf.GetSpace(sizeof(sVEC3) * kn);
        /*缩放值来适配图像空间*/
        float max_v = keys->y, max_t = keys[kn - 1].x;
        for(i = 1; i < kn; ++i) {
            if(keys[i].y > max_v) max_v = keys[i].y;
        }
        int ki1, ki2, ki;
        float tscl = r / max_t, vscl = 255.0f / max_v;
        for(i = 0; i < kn; ++i) {
            keys2[i].x = keys[i].x * tscl;
            keys2[i].y = keys[i].y * vscl;
            if(i) {
                ki = i - 1;
                keys2[ki].z = (keys2[i].y - keys2[ki].y) / (keys2[i].x - keys2[ki].x); /*预先计算好插值因子*/
            }
        }
        --kn;
        keys2[kn].z = 0.0f;

        for(j = 0; j < wh; ++j) {
            for(i = 0; i < wh; ++i, ++id) {
                _x = (float)i - cp;
                _y = (float)j - cp;
                _l = sqrtf(_x * _x + _y * _y);
                if(_l > r) p_memory[id] = 0;
                else if(_l < 0.76f) p_memory[id] = (uint8)keys2->y; //使中心像素不插值
                else {
                    ki1 = 0;
                    ki2 = kn;
                    GN_FindNearDataID2(_l < keys2[ki].x, ki1, ki2, ki);
                    p_memory[id] = (uint8)(((_l - keys2[ki1].x) * keys2[ki1].z + keys2[ki1].y));
                }
            }
        }
        return max_v;
    }
    void GNg::GenSphereDataToMem3D(uint8 *p_memory, int whd, int b, float value, float sqv, bool is_half) {
        bool us_sq = sqv > 0.0f ? true : false;
        float cp = (float)whd * 0.5f - 0.5f, max_value = sqv > 0.0f ? sqv : 255.0f;
        float r = (float)(whd - (b << 1)) * 0.5f, _r, _x, _y, _z, _l;
        _r = r * value;
        float r_ = r - _r;
        int i, j, l, id = 0, w = is_half ? (whd >> 1) : whd;
        if(is_half) {
            int _w = w - b;
            for(l = 0; l < whd; ++l) {
                for(j = 0; j < whd; ++j) {
                    for(i = 0; i < b; ++i, ++id) p_memory[id] = 0;
                    for(; i < w; ++i, ++id) {
                        _x = (float)(i + _w) - cp;

#define GT_TMP_COMPUTE_SPHERE3D_VALUE                                         \
    _y = (float)j - cp;                                                       \
    _z = (float)l - cp;                                                       \
    _l = sqrtf(_x * _x + _y * _y + _z * _z);                                  \
    if(_l > r) p_memory[id] = 0;                                              \
    else if(_l > _r) {                                                        \
        _l = 1.0f - (_l - _r) / r_;                                           \
        p_memory[id] = (uint8)(us_sq ? _l * _l * max_value : _l * max_value); \
    } else p_memory[id] = max_value

                        GT_TMP_COMPUTE_SPHERE3D_VALUE;
                    }
                }
            }
        } else {
            for(l = 0; l < whd; ++l) {
                for(j = 0; j < whd; ++j) {
                    for(i = 0; i < w; ++i, ++id) {
                        _x = (float)i - cp;
                        GT_TMP_COMPUTE_SPHERE3D_VALUE;
                    }
                }
            }
        }
    }
    GN_API void GNg::GenRectDataToMem(uint8 *p_memory, int w, int h, float px, float py, float vx, float vy) {
        float _x, _y, _l;
        int i, j, id = 0;
        for(j = 0; j < h; ++j) {
            for(i = 0; i < w; ++i, ++id) {
                _x = (float)i - px;
                _y = (float)j - py;
                _l = _x * vx + _y * vy;
                if(_l < 0.01f) p_memory[id] = 255;
                else if(_l > 0.99f) p_memory[id] = 0;
                else p_memory[id] = (uint8)((1.0f - _l) * 255.0f);
            }
        }
    }
    GN_API void GNg::GenCheckerboardDataToMem(uint8 *p_memory, int cb_size, int wn, int hn, uint8 lum) {
        int row_size = cb_size * wn;
        int layer_size = row_size * cb_size;
        uint8 *pdata = p_memory, *pdata1 = p_memory + row_size;
        uint8 a, ai, ai2 = hn > 1 ? 2 : 1;
        int i, i2, j;
        for(ai = 0; ai < ai2; ++ai) {
            for(i = 0, j = 0; j < wn; ++j) {
                a = (j & 1) == ai ? lum : 0xff;
                for(i2 = i + cb_size; i < i2; ++i) pdata[i] = a;
            }
            for(i = 1; i < cb_size; ++i) {
                memcpy(pdata1, pdata, row_size);
                pdata1 += row_size;
            }
            pdata += layer_size;
            pdata1 = pdata + row_size;
        }
        uint8 *pdata2[2] = {p_memory, p_memory + layer_size};
        for(j = 2; j < hn; ++j) {
            memcpy(pdata, pdata2[j & 1], layer_size);
            pdata += layer_size;
        }
    }
    GN_API void GNg::GenArrowDataToMem(uint8 *p_memory, int w, int h, bool style, int frame_w) {
        int j, x, pw = w - 1, ph = h >> 1;
        uint8 *psm; /*历遍开始地址*/
        uint8 *pem = p_memory; /*历遍结束地址*/
        uint8 *pm, *px; /*当前地址 和 边界点地址*/
        uint8 *pm1, *px_;

#define GT_TMP_DRAW_ARROW_TO_MEM(func1, func2, cmp, add_proc)                \
    for(j = 0; j < h; ++j) { /*历遍每行*/                                    \
        if(j < ph) x = (int)func1(0, pw, j, ph); /*计算找到箭头每行边界点*/  \
        else x = (int)func2(pw, 0, (j - ph), ph); /*计算找到箭头每行边界点*/ \
        psm = pem;                                                           \
        pem = pem + w;                                                       \
        px = psm + x;                                                        \
        add_proc for(pm = psm; pm < pem; ++pm) {                             \
            if(cmp) *pm = 255;                                               \
            else *pm = 0;                                                    \
        }                                                                    \
    }
        if(frame_w) {
            if(style) { //尖头
                GT_TMP_DRAW_ARROW_TO_MEM(GN_Linear, GN_Linear, pm < pm1 || (pm > px_ && pm <= px), pm1 = psm + frame_w; px_ = px - frame_w;);
            } else { //圆头
                GT_TMP_DRAW_ARROW_TO_MEM(GN_SLinearStart, GN_SLinearEnd, pm < pm1 || (pm > px_ && pm <= px), pm1 = psm + frame_w; px_ = px - frame_w;);
            }
        } else {
            if(style) { //尖头
                GT_TMP_DRAW_ARROW_TO_MEM(GN_Linear, GN_Linear, pm <= px, );
            } else { //圆头
                GT_TMP_DRAW_ARROW_TO_MEM(GN_SLinearStart, GN_SLinearEnd, pm <= px, );
            }
        }
    }
    GN_API uint8 *GNg::GenNumberImgData(int &fw_out, int &fh_out) {
        const uint8 number_bits[] = /*0123456789 号码图像的位数据*/
            {0x78, 0x30, 0x3c, 0x3c, 0x0c, 0x7e, 0x38, 0x7e, 0x3c, 0x3c,
             0xcc, 0x38, 0x66, 0x66, 0x0c, 0x06, 0x18, 0x60, 0x66, 0x66,
             0xec, 0x3e, 0x66, 0x66, 0x6c, 0x06, 0x0c, 0x30, 0x66, 0x66,
             0xec, 0x30, 0x60, 0x60, 0x6c, 0x06, 0x3e, 0x30, 0x6e, 0x66,
             0xcc, 0x30, 0x30, 0x38, 0x6c, 0x3e, 0x66, 0x18, 0x3c, 0x66,
             0xdc, 0x30, 0x18, 0x60, 0x66, 0x60, 0x66, 0x18, 0x76, 0x7c,
             0xdc, 0x30, 0x0c, 0x66, 0xfe, 0x60, 0x66, 0x0c, 0x66, 0x30,
             0xcc, 0x30, 0x06, 0x66, 0x60, 0x30, 0x66, 0x0c, 0x66, 0x18,
             0x78, 0x30, 0x7e, 0x3c, 0x60, 0x1e, 0x3c, 0x0c, 0x3c, 0x1c};

        const int fw = 8; //号码图像单字符宽
        const int fh = 9; //号码图像单字符高

        const int pn = fw * fh * 10;
        uint8 *number_data = new uint8[pn];
        memset(number_data, 0, pn);
        int pi = 0, bi;
        for(; pi < pn; ++pi) {
            bi = pi >> 3;
            if(number_bits[bi] & (1 << (pi - (bi << 3)))) number_data[pi] = 255; /*模板代码:把位记载的数据释放为字节的数据*/
        }
        fw_out = fw;
        fh_out = fh;
        return number_data;
    }
#endif //_GN_FORMAT_H_
