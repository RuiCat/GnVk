// GN_mini UI 实现片段：cPALETTE 调色板。
// 2026-11-26 自 GN_UiBase.cpp 第 1085-1545 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
/********************************************* cPALETTE ********************************************************/
P<GNg::sIMAGE> GNu::cPALETTE::zAlphaBgImg;
P<GNg::sIMAGE> GNu::cPALETTE::zColorBarImg;
GN::sLIST<GNg::sCOLOR> GNu::cPALETTE::aColorPasteTable;
unsigned short GNu::cPALETTE::zCreateTotalNum = 0;

GNu::cPALETTE::cPALETTE() : aColorTablePosType(0), aDisableControl(false), zOffsetSheep(1.0f) {
    ++zCreateTotalNum;
    if(zCreateTotalNum == 1) {
        const int img_w = 128, img_h = 4;
        zColorBarImg.New(1);
        zColorBarImg->CreateEmptyToData(img_w, img_h, 1, GNg::CR_RGB); //创建色调条图
        uint8 cr_data[img_h * img_w * 3];
        zComputeColorBarData(cr_data, img_w, img_h, 0, img_w); //计算色调条数据
        zColorBarImg->DataFill(cr_data, GNg::CR_RGB, img_w, img_h);

        int imgw = 16, imgh = 8;
        zAlphaBgImg.New(1);
        zAlphaBgImg->CreateEmptyToData(imgw, imgh, 1, GNg::CR_LUMINANCE); //创建透明区的底图
        memset(cr_data, 80, imgw * imgh);
        uint8 *pd = cr_data;
        pd[0] = pd[1] = pd[3] = pd[4] = pd[7] = pd[8] = pd[9] = pd[11] = pd[12] = pd[13] = pd[14] = 30;
        pd += imgw;
        pd[1] = pd[3] = pd[6] = pd[9] = pd[12] = pd[14] = 80;
        pd += imgw;
        pd[1] = pd[3] = pd[5] = pd[12] = 80;
        pd += imgw;
        pd[2] = pd[5] = pd[8] = pd[9] = pd[10] = pd[12] = pd[13] = 80;
        pd += imgw;
        pd[2] = pd[6] = pd[9] = pd[12] = pd[14] = 80;
        pd += imgw;
        pd[1] = pd[2] = pd[3] = pd[7] = pd[8] = pd[11] = pd[12] = pd[13] = pd[14] = 80;
        zAlphaBgImg->DataFill(cr_data, GNg::CR_LUMINANCE, imgw, imgh);
    }
    aType = eUI_PALETTE;
    c = 0xffffffff /*该类忽略该值*/;
}
GNu::cPALETTE::~cPALETTE() {
    --zCreateTotalNum;
    if(!zCreateTotalNum) {
        zAlphaBgImg.Delete();
        zColorBarImg.Delete();
        aColorPasteTable.clear();
    }
}
void GNu::cPALETTE::zUpdateColorFromOffsetState() {
    float ocx = (1.0f - zColorOffsetX) * 255.0f, ocy = (1.0f - zColorOffsetY);
    zColor.BGRA.r = (uint8)(((float)zColorBase.BGRA.r * zColorOffsetX + ocx) * ocy);
    zColor.BGRA.g = (uint8)(((float)zColorBase.BGRA.g * zColorOffsetX + ocx) * ocy);
    zColor.BGRA.b = (uint8)(((float)zColorBase.BGRA.b * zColorOffsetX + ocx) * ocy);
}
void GNu::cPALETTE::zComputeColorImgData(GNg::sCOLOR base) {
    float ratio;
    uint8 locbuf[65536]; //64K
    GN::sTBUF_INT tbuf;
    int imgw = zColorImg->width, imgh = zColorImg->height;
    uint8 *img_data = (uint8 *)tbuf.GetSpace(imgw * imgh * 3);
    uint8 *pd = img_data, *pd2;
    int i, j, add_v, w2 = imgw - 1, h2 = imgh - 1;
    for(i = 0; i < imgw; ++i) { //首先计算首行的颜色渐变
        add_v = 255 * (w2 - i) / w2;
        *pd = (uint8)((int)base.BGRA.r * i / w2 + add_v);
        ++pd;
        *pd = (uint8)((int)base.BGRA.g * i / w2 + add_v);
        ++pd;
        *pd = (uint8)((int)base.BGRA.b * i / w2 + add_v);
        ++pd;
    }
    pd2 = img_data;
    pd = pd2 + (imgw * 3);
    for(j = 1; j <= h2; ++j, pd2 = img_data) { //按比例来调深第二行以下的行颜色值
        ratio = (float)(h2 - j) / (float)h2;
        for(i = 0; i < imgw; ++i) {
            *pd = (uint8)((float)(*pd2) * ratio);
            ++pd;
            ++pd2;
            *pd = (uint8)((float)(*pd2) * ratio);
            ++pd;
            ++pd2;
            *pd = (uint8)((float)(*pd2) * ratio);
            ++pd;
            ++pd2;
        }
    }
    zColorImg->DataFill(img_data, GNg::CR_RGB, imgw, imgh);
}
void GNu::cPALETTE::zComputeColorBarData(uint8 *pd, int bar_w, int bar_h, int start, int end) {
    uint8 *cr_data = pd;
    int i = start, j, s1 = bar_w / 6;
    int s2 = s1 << 1, s3 = s1 * 3, s4 = s1 << 2, s5 = s1 * 5;
    int se = bar_w - s5;
    for(; i < end; ++i) { //按在不同的色调段落位置来计算颜色3个分量值
        if(i <= s1) {
            *pd = 255;
            ++pd;
            *pd = 0;
            ++pd;
            *pd = (uint8)(i * 255 / s1);
            ++pd;
        } else if(i <= s2) {
            *pd = (uint8)((s2 - i) * 255 / s1);
            ++pd;
            *pd = 0;
            ++pd;
            *pd = 255;
            ++pd;
        } else if(i <= s3) {
            *pd = 0;
            ++pd;
            *pd = (uint8)((i - s2) * 255 / s1);
            ++pd;
            *pd = 255;
            ++pd;
        } else if(i <= s4) {
            *pd = 0;
            ++pd;
            *pd = 255;
            ++pd;
            *pd = (uint8)((s4 - i) * 255 / s1);
            ++pd;
        } else if(i <= s5) {
            *pd = (uint8)((i - s4) * 255 / s1);
            ++pd;
            *pd = 255;
            ++pd;
            *pd = 0;
            ++pd;
        } else {
            *pd = 255;
            ++pd;
            *pd = (uint8)((bar_w - i) * 255 / se);
            ++pd;
            *pd = 0;
            ++pd;
        }
    }
    for(s1 = bar_w * 3, j = 1; j < bar_h; j <<= 1, s1 <<= 1) { //按首行的数据复制到其它行
        if((j << 1) <= bar_h) memcpy(cr_data + s1, cr_data, s1);
        else memcpy(cr_data + s1, cr_data, (bar_h - j) * bar_w * 3);
    }
}
void GNu::cPALETTE::InitToData(short _x, short _y, short _w, short _h, GNg::sCOLOR cr, GNg::sCOLOR arrow, bool arrow_shadow, bool alpha_set, bool alpha_bg_show) {
    x = _x, y = _y, w = _w, h = _h;
    aArrowShadow = arrow_shadow;
    aAlphaSet = alpha_set;
    aAlphaBgShow = alpha_bg_show;

#define GT_TMP_PALETTE_INIT_TO_DATA(CR)                         \
    zAlphaBgUV = 0.0f;                                          \
    zIsMouseFocus = false;                                      \
    zColorClone.Delete();                                       \
    if(!zColorImg) {                                            \
        zColorImg.New(1);                                       \
        zColorImg->CreateEmptyToData(128, 128, 1, GNg::CR_RGB); \
    }                                                           \
    SetColor(CR);

    GT_TMP_PALETTE_INIT_TO_DATA(cr)
    zColorArrow = arrow;
}
void GNu::cPALETTE::InitDefaultTable() {
    GNg::sCOLOR colors[9] = {0xffff0000, 0xffffff00, 0xff00ff00, 0xff00ffff, 0xff0000ff, 0xffff00ff, 0xffffffff, 0xff808080, 0xff000000};
    aTable.New(colors, 9);
}
void GNu::cPALETTE::SetColor(GNg::sCOLOR cr) {
    //比较出最小和最大的颜色分量
    uint8 min_rgb = cr.BGRA.r;
    uint8 max_rgb = min_rgb;
    if(cr.BGRA.g < min_rgb) min_rgb = cr.BGRA.g;
    else if(cr.BGRA.g > max_rgb) max_rgb = cr.BGRA.g;
    if(cr.BGRA.b < min_rgb) min_rgb = cr.BGRA.b;
    else if(cr.BGRA.b > max_rgb) max_rgb = cr.BGRA.b;
    float ratio = 255.0f / (float)(max_rgb - min_rgb), sr = 1.0f / 6.0f;
    zColorBase.Init((uint8)((float)(cr.BGRA.r - min_rgb) * ratio), (uint8)((float)(cr.BGRA.g - min_rgb) * ratio), (uint8)((float)(cr.BGRA.b - min_rgb) * ratio), 255);
    zComputeColorImgData(zColorBase); //按基色计算色谱数据

    //按基色来计算色调条的偏移量
    if(zColorBase.BGRA.r) {
        if(zColorBase.BGRA.g) {
            if(zColorBase.BGRA.r == 255) zBarOffset = 1.0f - ((float)zColorBase.BGRA.g / 255.0f * sr);
            else zBarOffset = sr * (4.0f + (float)zColorBase.BGRA.r / 255.0f);
        } else if(zColorBase.BGRA.b) {
            if(zColorBase.BGRA.r == 255) zBarOffset = sr * ((float)zColorBase.BGRA.b / 255.0f);
            else zBarOffset = sr * (2.0f - (float)zColorBase.BGRA.r / 255.0f);
        } else zBarOffset = 0.0f; //纯红色
    } else {
        if(zColorBase.BGRA.b) {
            if(zColorBase.BGRA.b == 255) {
                if(zColorBase.BGRA.g) zBarOffset = sr * (2.0f + (float)zColorBase.BGRA.g / 255.0f);
                else zBarOffset = sr * 2.0f; //纯蓝色
            } else zBarOffset = sr * (4.0f - (float)zColorBase.BGRA.b / 255.0f);
        } else zBarOffset = sr * 4.0f; //纯绿色
    }
    zColor = cr;
    //按最大和最小颜色分量计算在色谱的偏移量
    zColorOffsetX = 1.0f - (float)min_rgb / (float)max_rgb;
    zColorOffsetY = 1.0f - (float)max_rgb / 255.0f;
    zAlphaBgOffset = (float)cr.BGRA.a / 255.0f;
}
bool GNu::cPALETTE::UpdateRun(unsigned int updatetime, unsigned int dc) {
    if(!zColorImg) {
#ifdef GN_DEBUG
        GN_Log("cPALETTE::UpdateRun() 还没初始化好! 请先调用 cPALETTE::InitToData(...) 来初始化!\n");
#endif
        return false;
    }
    bool ret = false, mouse_cp = !aDisableControl && GN::Mouse1[GM_LEFT];
    short mouse_x = (short)GNg::ViewCur->mouseMX, mouse_y = (short)GNg::ViewCur->mouseMY,
          dlgx = x, dlgy = y, dlgw = w, dlgh = h - 3,
          tbx = x, tby = y, tbx1, tby1, tbw1, tbh1, tbw, tbh, cid,
          tc_x, tc_y, tc_w, tc_h; //用来临时记下复制出的色块要飞过去的目标位置

    unsigned int tn = aTable.Num();
    if(tn) {
        GNg::sCOLOR *pccc = aTable, *_pccc = 0;
        P<GNg::sCOLOR> paste_table;
        unsigned int tn_ = aColorPasteTable.size();
        if(tn_) {
            paste_table(aColorPasteTable);
            if(tn_ > tn) {
                _pccc = (GNg::sCOLOR *)paste_table + (tn_ - tn);
                tn_ = tn;
            } else _pccc = paste_table;
        }
        unsigned int ti = 0, ti2 = tn - 1;
        if(w > h) { //色版在左边		/*模版代码:处理与绘制调色版的颜色表*/
            tbw = dlgw >> 3, tbh = dlgh / tn;
            dlgx += tbw, dlgw -= tbw;
            if(tn_) { //有粘贴色版
                tbw1 = tbw >> 1;
                if(aColorTablePosType) {
                    tbx1 = tbx;
                    tbx = tbx + tbw1;
                } else tbx1 = tbx + tbw1;
            } else {
                tbw1 = tbw;
                tbx1 = tbx;
            }

            if(zColorClone) {
                tc_w = tbw >> 1;
                if(tn_) tc_x = tbx;
                else {
                    tc_x = tbx;
                    if(aColorTablePosType) tc_x += tc_w;
                }
                if(tn_ >= ti2) {
                    tc_y = tby + ti2 * tbh;
                    tc_h = y + h - tc_y;
                } else {
                    tc_y = tby + tn_ * tbh;
                    tc_h = tbh;
                }
            }
            cid = (mouse_y - tby) / tbh;
            if(cid > (short)ti2) cid = (short)ti2;
            if(mouse_cp && mouse_y >= tby && mouse_y <= y + dlgh) {
                if(tn_ && mouse_x >= tbx && mouse_x <= tbx + tbw1) {
                    if(cid < tn_) {
                        SetColor(_pccc[cid]);
                        ret = true;
                    }
                } else if(mouse_x >= tbx1 && mouse_x <= tbx1 + tbw1) {
                    SetColor(pccc[cid]);
                    ret = true;
                }
            }
            for(; ti < tn; ++ti, tby += tbh) {
                if(ti == ti2) tbh = y + h - tby;
                if(ti < tn_) GNg::ImageDraw(0, tbx, tby, tbw1, tbh, _pccc[ti].COLOR); //显示粘贴表各格颜色
                GNg::ImageDraw(0, tbx1, tby, tbw1, tbh, pccc[ti].COLOR); //显示颜色表各格颜色
            }
            dlgx += (tbw >> 3), dlgw -= (tbw >> 3);

        } else { //色版在上边		/*模版代码:处理与绘制调色版的颜色表*/
            tbh = dlgh >> 3, tbw = dlgw / tn;
            dlgy += tbh, dlgh -= tbh;
            if(tn_) { //有粘贴色版
                tbh1 = tbh >> 1;
                if(aColorTablePosType) {
                    tby1 = tby;
                    tby = tby + tbh1;
                } else tby1 = tby + tbh1;
            } else {
                tbh1 = tbh;
                tby1 = tby;
            }

            if(zColorClone) {
                tc_h = tbh >> 1;
                if(tn_) tc_y = tby;
                else {
                    tc_y = tby;
                    if(aColorTablePosType) tc_y += tc_h;
                }
                if(tn_ >= ti2) {
                    tc_x = tbx + ti2 * tbw;
                    tc_w = x + w - tc_x;
                } else {
                    tc_x = tbx + tn_ * tbw;
                    tc_w = tbw;
                }
            }
            cid = (mouse_x - tbx) / tbw;
            if(cid > (short)ti2) cid = (short)ti2;
            if(mouse_cp && mouse_x >= tbx && mouse_x <= x + w) {
                if(tn_ && mouse_y >= tby && mouse_y <= tby + tbh1) {
                    if(cid < tn_) {
                        SetColor(_pccc[cid]);
                        ret = true;
                    }
                } else if(mouse_y >= tby1 && mouse_y <= tby1 + tbh1) {
                    SetColor(pccc[cid]);
                    ret = true;
                }
            }
            for(; ti < tn; ++ti, tbx += tbw) {
                if(ti == ti2) tbw = x + w - tbx;
                if(ti < tn_) GNg::ImageDraw(0, tbx, tby, tbw, tbh1, _pccc[ti].COLOR); //显示粘贴表各格颜色
                GNg::ImageDraw(0, tbx, tby1, tbw, tbh1, pccc[ti].COLOR); //显示颜色表各格颜色
            }
            dlgy += (tbh >> 3), dlgh -= (tbh >> 3);
        }
    }
    short dh = aAlphaSet ? dlgh - 72 : dlgh - 36; //色谱显示高
    short dy = dlgy; //色普显示位置Y
    short ay1 = dy + dh + 4; //色调箭头显示位置Y
    short by = ay1 + 10; //色调条显示位置Y
    short ay2 = by + 28; //透明度箭头显示位置Y
    short by2 = ay2 + 10; //透明条显示位置Y

    GNg::sCOLOR last_color = zColor;
    if(aDisableControl || !GN::Mouse[GM_LEFT]) zSetTpye = 0;
    zIsMouseFocus = false;
    // 判断是对那项设置 ----------------------------------------------
    if(mouse_x >= dlgx && mouse_x <= dlgx + dlgw && mouse_y >= dlgy) {
        if(mouse_y <= dy + dh) {
            zIsMouseFocus = true;
            if(mouse_cp) zSetTpye = 1;
        } else if(mouse_y <= by + 24) {
            zIsMouseFocus = true;
            if(mouse_cp) zSetTpye = 2;
        } else if(mouse_y <= dlgy + dlgh) {
            zIsMouseFocus = true;
            if(mouse_cp) zSetTpye = 3;
        }
    }
    // 选择控制处理 --------------------------------------------------------------------------------------------
    if((mouse_cp && zSetTpye) || (zSetTpye && GN::MouseM)) {
        switch(zSetTpye) {
            case 1:
                zColorOffsetX = (float)(mouse_x - dlgx) / (float)dlgw;
                GN_ValueClamp(zColorOffsetX, 0.0f, 1.0f);
                zColorOffsetY = (float)(mouse_y - dlgy) / (float)dh;
                GN_ValueClamp(zColorOffsetY, 0.0f, 1.0f);

                zUpdateColorFromOffsetState(); //更新一下颜色
                break;
            case 2: {
                zBarOffset = (float)(mouse_x - dlgx) / (float)dlgw;
                GN_ValueClamp(zBarOffset, 0.0f, 1.0f);
                int st_x = (int)((float)zColorBarImg->width * zBarOffset);
                if(st_x == zColorBarImg->width) st_x -= 1;
                uint8 *_rgb = (uint8 *)&zColorBase;
                zComputeColorBarData(_rgb, zColorBarImg->width, 1, st_x, st_x + 1); //计算得到当前偏移位置在色调条的颜色RGB分量
                GN_Swap(uint8, _rgb[0], _rgb[2]);

                zComputeColorImgData(zColorBase); //按新基色重新计算一下色谱数据
                zUpdateColorFromOffsetState(); //更新一下颜色
            } break;
            case 3:
                zAlphaBgOffset = (float)(mouse_x - dlgx) / (float)dlgw;
                GN_ValueClamp(zAlphaBgOffset, 0.0f, 1.0f);
                zColor.BGRA.a = (uint8)(zAlphaBgOffset * 255.0f);
                break;
        }
        if(GN::MouseM) zColorClone.Delete();
        ret = true;
    }
    GNg::ImageDraw(zColorImg, dlgx, dy, dlgw, dh, dc); //显示色谱
    GNg::ImageDraw(zColorBarImg, dlgx, by, dlgw, 24, dc); //显示色调条

    if(aAlphaSet) {
        if(aAlphaBgShow) { //显示透明滚动背景
            float u2 = zAlphaBgUV + ((float)dlgw / (float)zAlphaBgImg->width);
            GNg::ImageDraw(zAlphaBgImg, dlgx, by2, dlgw, 24, 0xffffffff, 0, 1, zAlphaBgUV, zAlphaBgUV, u2, zAlphaBgUV + 3.0f);
            zAlphaBgUV += 0.02f * GN::TimeRatio;
        }
        GNu::eNEAR_TYPE near_type = (GNu::eNEAR_TYPE)GNg::Draw2DAlphaDirectionGet();
        GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_LEFT);
        GNg::ImageDraw(0, dlgx, by2, dlgw, 24, zColor.COLOR | 0xff000000); //显示选中的颜色透明渐变
        GNg::Draw2DAlphaDirectionSet(near_type);

        if(aArrowShadow) GNg::ImageDraw(0, dlgx + 1 + dlgw * zAlphaBgOffset, ay2 - 1, 1, 8, 0xaa000000); //显示透明 游标箭头阴影
        GNg::ImageDraw(0, dlgx - 1 + dlgw * zAlphaBgOffset, ay2, 2, 8, zColorArrow); //显示透明 度游标箭头
    }
    if(aArrowShadow) GNg::ImageDraw(0, dlgx + 1 + dlgw * zBarOffset, ay1 - 1, 1, 8, 0xaa000000); //显示色调 游标箭头阴影
    GNg::ImageDraw(0, dlgx - 1 + dlgw * zBarOffset, ay1, 2, 8, zColorArrow); //显示色调   游标箭头

    uint8 c_ = zColorOffsetY < 0.5f ? 80 : 175;
    short cx = dlgx + dlgw * zColorOffsetX, cy = dlgy + dh * zColorOffsetY;
    GNg::LineFrame2dDraw(cx - 3, cy - 3, 7, 7, 1, 0xff000000 | (c_ << 16) | (c_ << 8) | c_); //显示在色谱上的选点

    // 复制飞出色块 -----------------------------------------------------------------------------------------------------
    if(zColorClone && !zSetTpye) {
        float ls = GN::TimeRatioL * zOffsetSheep;
        zColorCloneMX += ((float)tc_x - zColorCloneMX) * ls;
        zColorCloneMW += ((float)tc_w - zColorCloneMW) * ls;
        zColorCloneMY += ((float)tc_y - zColorCloneMY) * ls;
        zColorCloneMH += ((float)tc_h - zColorCloneMH) * ls;
        GNg::ImageDraw(0, (int)zColorCloneMX, (int)zColorCloneMY, (int)zColorCloneMW, (int)zColorCloneMH, zColorClone[0]);
        short crs_x = (short)zColorCloneMX - tc_x;
        if((crs_x <= 1 && crs_x >= -1) || (GN::TimeGetRunCount() - zLastRunTime > 600)) {
            aColorPasteTable.push_back(zColorClone[0]);
            if(aColorPasteTable.size() > 32) aColorPasteTable.pop_front(); //粘贴版色表多与32个 自动忽略在前面的
            zColorClone.Delete();
        }
    }
    if(zIsMouseFocus && !aDisableControl) {
        bool ctrl_on = GN::Key[GK_LCONTROL] || GN::Key[GK_RCONTROL];
        if((ctrl_on && GN::Key1[GK_C]) || (mouse_cp && last_color == zColor)) {
            if(zOffsetSheep > 0.99f) {
                aColorPasteTable.push_back(zColor);
                if(aColorPasteTable.size() > 32) aColorPasteTable.pop_front(); //粘贴版色表多与32个 自动忽略在前面的
            } else {
                zColorCloneMX = (float)cx, zColorCloneMY = (float)cy;
                zColorCloneMW = 7.0f;
                zColorCloneMH = 7.0f;
                zColorClone.New(1, zColor);
                zLastRunTime = GN::TimeGetRunCount();
            }
        }
        if(ctrl_on && GN::Key1[GK_V] && aColorPasteTable.size()) {
            SetColor(aColorPasteTable.back());
            ret = true;
        }
    }
    return ret;
}
void GNu::cPALETTE::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GNg::sDRAW_AREA2 *_rng = (GNg::sDRAW_AREA2 *)&(this->x);
    GNg::sDRAW_AREA2 rng_bk = *_rng;
    DrawRangeGet(*_rng, parent);
    uint8 last_set_type = zSetTpye;
    unsigned int last_color = zColor;
    if(RecvInputUiObject != this) GNg::ViewCur->mouseMX += 10000;
    UpdateRun(updatetime, c);
    if(RecvInputUiObject != this) GNg::ViewCur->mouseMX -= 10000;
    else if((last_set_type && !zSetTpye) || (last_color != zColor.COLOR && !(GN::TimeGetRunFpsCount() & 31))) {
        int colorBits;
        memcpy(&colorBits, &zColor, sizeof(colorBits));
        UiActionCall(this, colorBits);
    }
    *_rng = rng_bk;
}
P<GNu::iUI_OBJECT> GNu::cPALETTE::Clone() {
    P<cPALETTE> palette(1);
    *palette = *this;
    palette->zColorImg = this->zColorImg->CopyOut(0, 0, 0, 0, false, true);
    return palette;
}
