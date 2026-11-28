// GN_mini UI 实现片段：sCTRL_AXIS / sBUBBLE 坐标轴与气泡。
// 2026-11-26 自 GN_UiBase.cpp 第 1546-1994 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
/**************************************************** sCTRL_AXIS 2D *************************************************/
P<GNg::sIMAGE> GNu::sCTRL_AXIS::zCtrlAxisImg;
unsigned short GNu::sCTRL_AXIS::zCreateTotalNum = 0;

GNu::sCTRL_AXIS::sCTRL_AXIS() : zPos(0.0f, 0.0f), zHideMove(false), zHideRot(false), zHideScl(false), zIsLocalMode(false), aUnitCtrlMov(0.0f), aUnitCtrlRot(0.0f), aUnitCtrlScl(0.0f), aDisableControl(false), aIsVisual(true), aShowSize(128) {
    ++zCreateTotalNum;
    if(zCreateTotalNum == 1) {
        GNf_LoadFileStatePush2(0, GNf::eFILE_STD); //首次创建该对象就读入2D轴图片文件
        zCtrlAxisImg.New(3);
        zCtrlAxisImg[0].InitToData(GN_SysDir("axis/2Dctrl_mov"), 1, GNg::CR_ALPHA);
#ifdef GT_TMP_COMPATI_DX_DDRAW
        if(GN_Has3DAccelerate())
#endif
        {
            zCtrlAxisImg[1].InitToData(GN_SysDir("axis/2Dctrl_rot"), 1, GNg::CR_ALPHA);
        }
        zCtrlAxisImg[2].InitToData(GN_SysDir("axis/2Dctrl_scl"), 1, GNg::CR_ALPHA);
        GNf_LoadFileStatePop2();
    }
}
GNu::sCTRL_AXIS::~sCTRL_AXIS() {
    --zCreateTotalNum;
    if(!zCreateTotalNum) zCtrlAxisImg.Delete(); //已没这类对象就删除共享内容
}
void GNu::sCTRL_AXIS::InitToData(GNg::sCOLOR move, GNg::sCOLOR rot, GNg::sCOLOR scl) {
    zGroup.New(1);
    zGroup->Init(0, 0, 0, GNg::Ts2D, 0.5f, 0.5f, 1, 0xffffffff, GNg::eAM_NOAM);
    GNg::sIMAGE *ctrl_img = zCtrlAxisImg;
    zAxis.New(3);
    zAxis->Init(ctrl_img, 0, 0, GNg::sTRANSF2D(0, 0, (float)aShowSize / (float)ctrl_img->width, (float)aShowSize / (float)ctrl_img->height), 0.5f, 0.5f, 1, move.COLOR, GNg::eAM_NOAM);
#ifdef GT_TMP_COMPATI_DX_DDRAW
    if(GN_Has3DAccelerate())
#endif
    {
        zAxis[1].Init(ctrl_img + 1, 0, 0, GNg::sTRANSF2D(0, 0, (float)aShowSize / (float)ctrl_img[1].width, (float)aShowSize / (float)ctrl_img[1].height), 0.5f, 0.5f, 1, rot.COLOR, GNg::eAM_NOAM);
    }
    zAxis[2].Init(ctrl_img + 2, 0, 0, GNg::sTRANSF2D(0, 0, (float)aShowSize / (float)ctrl_img[2].width, (float)aShowSize / (float)ctrl_img[2].height), 0.5f, 0.5f, 1, scl.COLOR, GNg::eAM_NOAM);

    zAxisType = 0;
    zSelectAxis = 0;
}
void GNu::sCTRL_AXIS::SetPos(int x, int y, float rot, bool local_mode) {
    zIsLocalMode = local_mode;
    zTs.Init(true);
    zTsBak = zTs;
    zPos.x = (float)x;
    zPos.y = (float)y;
    zRot = rot;
}
uint8 GNu::sCTRL_AXIS::Show(float *rx, float *ry) {
    if(!aIsVisual) return 0;
#ifdef GN_DEBUG
    if(!zGroup) { GN_Log("2D控制轴没初始化好! 请先调用sCTRL_AXIS::InitToData(...)\n"); }
#endif
    uint8 ret = 0, aid = 0;
    bool is_mouse_down = GN::Mouse1[GM_LEFT];
    float mouse_x = (float)GNg::ViewCur->mouseMX, mouse_y = (float)GNg::ViewCur->mouseMY;
    GNg::cFRAME *gp = zGroup, *axis = zAxis;
    GNg::sIMAGE *ctrl_img = zCtrlAxisImg;
    GNg::sMAT33 tm;
#ifdef GT_TMP_COMPATI_DX_DDRAW
    bool spt_rot = GN_Has3DAccelerate(); //图形环境是否支持旋转硬件加速
    if(spt_rot) GNg::M33GetRot(&tm, zRot);
    else tm.Init(true);
#define GT_DEVICE_SPT_ROT spt_rot &&
#else
#define GT_DEVICE_SPT_ROT
    GNg::M33GetRot(&tm, zRot);
#endif
    tm.m31 = zPos.x, tm.m32 = zPos.y; //设置原摆放位置
    gp->aLevel_world = tm * zTsBak; //并调整为适合大小后再组合修改矩阵来设置它的世界矩阵
    if(!zHideMove) {
        axis->PlayFrame(16, gp);
        aid = 0;
    } //更新显示各控制轴
    if(GT_DEVICE_SPT_ROT !zHideRot) {
        axis[1].PlayFrame(16, gp);
        aid = 1;
    }
    if(!zHideScl) {
        axis[2].PlayFrame(16, gp);
        aid = 2;
    }
    if(aDisableControl) {
        if(zSelectAxis) {
            zSelectAxis->aBlendmode = 1;
            zAxisType = 0;
            zSelectAxis = 0;
        }
        return ret;
    }
    if(is_mouse_down) { //判断选到那种控制轴 与点下去的位置
        GNg::M33GetInverse(tm, axis[aid].aLevel_world);
        float loc_x, loc_y, img_w = (float)(ctrl_img[2].width >> 1), img_h = (float)(ctrl_img[2].height >> 1);
        GNg::M33TransformCoord(&loc_x, &loc_y, mouse_x, mouse_y, &tm);
        if(loc_x >= -img_w && loc_x <= img_w && loc_y >= -img_h && loc_y <= img_h) {
            if(!zHideScl && loc_x >= img_w - 13.0f && loc_x <= img_w && loc_y >= img_h - 13.0f && loc_y <= img_h) {
                zSelectAxis = axis + 2;
                zAxisType = 7; //xy整体缩放
            } else if(!zHideScl && (loc_x < -img_w + 10.0f || loc_x > img_w - 10.0f)) {
                zSelectAxis = axis + 2;
                zAxisType = 5; //x缩放
            } else if(!zHideScl && (loc_y < -img_h + 10.0f || loc_y > img_h - 10.0f)) {
                zSelectAxis = axis + 2;
                zAxisType = 6; //y缩放
            } else if(!zHideMove && loc_x >= -3.0f && loc_x <= 3.0f && loc_y >= -3.0f && loc_y <= 3.0f) {
                zSelectAxis = axis;
                zAxisType = 3; //xy偏移
                GNg::M33GetMove(&zTsBak, gp->aLevel_world.m31 - zPos.x, gp->aLevel_world.m32 - zPos.y);
                zRot = 0.0f;
#ifdef GT_TMP_COMPATI_DX_DDRAW
                if(spt_rot) GNg::M33GetRot(&tm, zRot);
                else tm.Init(true);
#else
                GNg::M33GetRot(&tm, zRot);
#endif
                tm.m31 = zPos.x, tm.m32 = zPos.y;
                gp->aLevel_world = tm * zTsBak;
                ret = 0xff;
            } else if(!zHideMove && loc_y >= -3.0f && loc_y <= 3.0f) {
                zSelectAxis = axis;
                zAxisType = 1; //x偏移
            } else if(!zHideMove && loc_x >= -3.0f && loc_x <= 3.0f) {
                zSelectAxis = axis;
                zAxisType = 2; //y偏移
            } else {
                GNg::sVEC2 t_v(loc_x, loc_y);
                float r = GNg::Vec2Length(&t_v);
                if(GT_DEVICE_SPT_ROT !zHideRot && r <= 54.0f && r >= 49.0f) {
                    zSelectAxis = axis + 1;
                    zAxisType = 4; //旋转
                } else if(!zHideMove) {
                    zSelectAxis = axis;
                    zAxisType = 3; //xy偏移
                }
            }
            if(zSelectAxis) {
                zSelectAxis->aBlendmode = 2;
                zCp.x = mouse_x;
                zCp.y = mouse_y;
            }
        }
    }
    if(rx) *rx = 0.0f;
    if(ry) *ry = 0.0f;

    if(GN::Mouse[GM_LEFT]) { //按住鼠左键拖动修改变换 -------------------------------------------------------------------------------------------
        if(zSelectAxis && (is_mouse_down || GN::MouseM)) {
            GNg::sMAT33 add_ts, to_centre_mat;
            GNg::sVEC2 front_x1, left_y1, front_x, crs_point(mouse_x, mouse_y);
            GNg::sVEC2 *_front_x = gp->aLevel_world.Row1(), *_left_y = gp->aLevel_world.Row2();
            float len_x = GNg::Vec2Length(_front_x), len_y = GNg::Vec2Length(_left_y);
            front_x1 = (*_front_x) / len_x; //计算出当前控制轴原X坐标原轴的方向
            left_y1 = (*_left_y) / len_y; //计算出当前控制轴原Y坐标原轴的方向
            int uc = 1;
            if(zAxisType < 3) { // x或y偏移 --------------------------------------------
                GNg::sVEC2 tmpv = zAxisType == 1 ? front_x1 : left_y1;
                float ofs = 0.0f;
                if(!is_mouse_down) {
                    ofs = GN_Vec2Dot(crs_point., tmpv.) - GN_Vec2Dot(zCp., tmpv.);
                    if(aUnitCtrlMov) {
                        uc = (int)(ofs / aUnitCtrlMov);
                        ofs = (float)uc * aUnitCtrlMov;
                    }
                    if(uc) {
                        zCp = crs_point;
                        tmpv *= ofs;
                        GNg::M33MultMove(&zTs, &tmpv, true);
                        GNg::M33MultMove(&zTsBak, &tmpv, true);
                    }
                    ret = zAxisType;
                }
                if(zAxisType == 1) {
                    if(rx) *rx = ofs; //x偏移量
                } else {
                    if(ry) *ry = ofs; //y偏移量
                }
            } else if(zAxisType == 3) { // XY偏移 ----------------------------------------------------
                if(!is_mouse_down) {
                    float ofs_x = GN_Vec2Dot(crs_point., front_x1.) - GN_Vec2Dot(zCp., front_x1.);
                    float ofs_y = GN_Vec2Dot(crs_point., left_y1.) - GN_Vec2Dot(zCp., left_y1.);
                    int uc2 = 1;
                    if(aUnitCtrlMov) {
                        uc = (int)(ofs_x / aUnitCtrlMov);
                        ofs_x = (float)uc * aUnitCtrlMov;
                        uc2 = (int)(ofs_y / aUnitCtrlMov);
                        ofs_y = (float)uc2 * aUnitCtrlMov;
                    }
                    if(uc || uc2) {
                        crs_point = front_x1 * ofs_x + left_y1 * ofs_y;
                        zCp = crs_point + zCp;
                        GNg::M33MultMove(&zTs, &crs_point, true);
                        GNg::M33MultMove(&zTsBak, &crs_point, true);
                    }
                    ret = zAxisType;
                    if(rx) *rx = ofs_x;
                    if(ry) *ry = ofs_y;
                }
            } else if(GT_DEVICE_SPT_ROT zAxisType == 4) { // 旋转 ------------------------------------
                if(!is_mouse_down) {
                    GNg::sVEC2 *_wrd_pos = gp->aLevel_world.GetPos();
                    GNg::sVEC2 n1 = zCp - (*_wrd_pos), n2 = crs_point - (*_wrd_pos);
                    float dot;
                    GNg::Vec2ToRot(&dot, &n1, &n2); //计算出旋转角度
                    if(aUnitCtrlRot) {
                        uc = (int)(dot / aUnitCtrlRot);
                        dot = (float)uc * aUnitCtrlRot;
                    }
                    if(uc) {
                        GNg::M33GetRot(&add_ts, dot);
                        n1 = -(*_wrd_pos);
                        GNg::M33MultMove(&add_ts, &n1, false);
                        add_ts.m31 += _wrd_pos->x, add_ts.m32 += _wrd_pos->y;
                        if(!GN_ValueIsInvalidFloat(add_ts.m11)) {
                            zCp = crs_point;
                            zTs = zTs * add_ts;
                            zTsBak = zTsBak * add_ts;
                            if(rx) *rx = dot;
                            if(ry) *ry = dot;
                        }
                    }
                    ret = zAxisType;
                }
            } else if(zAxisType == 5 || zAxisType == 6) { //x或y缩放 ------------------------------------
                GNg::sVEC2 posi(gp->aLevel_world.m31, gp->aLevel_world.m32), tmpv;
                float vlen, *r_;
                if(zAxisType == 5) {
                    tmpv = front_x1;
                    vlen = len_x;
                    r_ = rx;
                } else {
                    tmpv = left_y1;
                    vlen = len_y;
                    r_ = ry;
                }
                float dot = GN_Vec2Dot(crs_point., tmpv.) - GN_Vec2Dot(posi., tmpv.); //计算该点到离控制轴中心的距离
                if(!is_mouse_down) {
                    float ax_len = vlen * 0.5f * (float)ctrl_img[2].width;
                    if(zScaleIsFront) dot /= ax_len;
                    else dot /= -ax_len;
                    if(aUnitCtrlScl) {
                        uc = (dot - 1.0f) * zCurTsTotalSize / aUnitCtrlScl;
                        dot = 1.0f + (uc * aUnitCtrlScl) / zCurTsTotalSize;
                    }
                    if(uc) {
                        zCurTsTotalSize *= dot;
                        GNg::M33GetInverse(to_centre_mat, gp->aLevel_world);
                        GNg::M33MultScale1(&to_centre_mat, dot, zAxisType == 6, true);
                        add_ts = to_centre_mat * gp->aLevel_world;
                        zTs = zTs * add_ts;
                        zTsBak = zTsBak * add_ts;
                    }
                    ret = zAxisType;
                    if(r_) *r_ = dot;
                } else {
                    zScaleIsFront = dot > 0.0f;
                    zCurTsTotalSize = 1.0f;
                }
            } else if(zAxisType == 7) { // xy整体缩放 ----------------------------------------------------
                GNg::sVEC2 posi(gp->aLevel_world.m31, gp->aLevel_world.m32);
                float dot = GN_Vec2Dot(crs_point., front_x1.) - GN_Vec2Dot(posi., front_x1.);
                if(!is_mouse_down) {
                    float dot2 = GN_Vec2Dot(crs_point., left_y1.) - GN_Vec2Dot(posi., left_y1.);
                    float ax_len;
                    if((dot < 0.0f ? -dot : dot) < (dot2 < 0.0f ? -dot2 : dot2)) { //比较两方向轴的距离
                        ax_len = len_x * 0.5f * (float)ctrl_img[2].width;
                    } else {
                        ax_len = len_y * 0.5f * (float)ctrl_img[2].width;
                        dot = dot2;
                    }
                    if(zScaleIsFront) dot /= ax_len;
                    else dot /= -ax_len;
                    if(aUnitCtrlScl) {
                        uc = (dot - 1.0f) * zCurTsTotalSize / aUnitCtrlScl;
                        dot = 1.0f + (uc * aUnitCtrlScl) / zCurTsTotalSize;
                    }
                    if(uc) {
                        zCurTsTotalSize *= dot;
                        GNg::M33GetInverse(to_centre_mat, gp->aLevel_world);
                        GNg::M33MultScale(&to_centre_mat, dot, dot, true);
                        add_ts = to_centre_mat * gp->aLevel_world;
                        zTs = zTs * add_ts;
                        zTsBak = zTsBak * add_ts;
                    }
                    ret = zAxisType;
                    if(rx) *rx = dot;
                    if(ry) *ry = dot;
                } else {
                    zScaleIsFront = dot > 0.0f;
                    zCurTsTotalSize = 1.0f;
                }
            }
        }
    } else {
        if(zSelectAxis) {
            if(!zIsLocalMode) //如是按世界坐标轴方向来变换的时 每次操作完一步就把控制轴的方向清除
                GNg::M33GetMove(&zTsBak, gp->aLevel_world.m31 - zPos.x, gp->aLevel_world.m32 - zPos.y);
            zSelectAxis->aBlendmode = 1;
            zAxisType = 0;
            zSelectAxis = 0;
        } else if(GN::Key1[GK_T]) { //按左或右Shift键都强制按世界坐标轴方向来变换
            GNg::M33GetMove(&zTsBak, gp->aLevel_world.m31 - zPos.x, gp->aLevel_world.m32 - zPos.y);
            zRot = 0.0f;
            return 0xff;
        }
    }
    return ret;
}
void GNu::sCTRL_AXIS::GetLastTranslate(GNg::sMAT33 *m_out, bool clear) {
    *m_out = zTs;
    if(clear) zTs.Init(true);
}
void GNu::sCTRL_AXIS::GetCoordForSXY(float &scx, float &scy) {
    GNg::sVEC2 pos((float)((zCtrlAxisImg[2].width >> 1) - 7), (float)((zCtrlAxisImg[2].height >> 1) - 7));
    GNg::M33TransformCoord(&pos, &(zAxis[2].aLevel_world));
    scx = pos.x, scy = pos.y;
}
GNg::sMAT33 *GNu::sCTRL_AXIS::GetWorldMatrix() {
#ifdef GN_DEBUG
    if(!zGroup) GN_Log("2D控制轴还没初始化好! 请先调用 sCTRL_AXIS::InitToData(...)\n");
#endif
    return &(zGroup->aLevel_world);
}
/************************************************************* 对话汽泡 *************************************************************************************/
void GNu::sBUBBLE::InitToData(P<GNg::sIMAGE> image9, P<GNg::sIMAGE> arrow, unsigned int color, unsigned short blend) {
    zImage9 = image9;
    zArrow = arrow;
    aColor = color;
    aBlend = blend;
}
void GNu::sBUBBLE::SetFrameStyle(P<GNg::sIMAGE> image9, P<GNg::sIMAGE> arrow, unsigned int colorf, unsigned short blend, int fw) {
    zImage9Frame = image9;
    zArrowFrame = arrow;
    aColorFrame = colorf;
    aBlendFrame = blend;
    aFw = fw;
}
void GNu::sBUBBLE::Show(int ax, int ay, int x, int y, int w, int h) {
    GNg::sVEC2 av((float)(ax - x) - ((float)w * 0.5f), (float)(ay - y) - ((float)h * 0.5f));
    Show(av, x, y, w, h);
}
void GNu::sBUBBLE::Show(GNg::sVEC2 &v, int x, int y, int w, int h) {
#ifdef GN_DEBUG
    if(!zImage9 || zArrow) GN_Log("对话汽泡还没初始化好! 请先调用 sBUBBLE::InitToData(...)\n");
#endif
    GNg::sIMAGE *arrw = zArrow, *arrwframe = zArrowFrame;
    bool show_frame = arrwframe && zImage9Frame && ((aColor & 0xff000000) == 0xff000000) && (aBlend <= 1);
    GNg::ImageDraw9Square(zImage9, x, y, w, h, aColor, aBlend);
    if(show_frame) GNg::ImageDraw9Square(zImage9Frame, x, y, w, h, aColorFrame, aBlendFrame);
    GNg::sVEC2 vnz, vnz2;
    GNg::Vec2Normalize(&vnz, &v);
    GNg::Vec2Right(&vnz2, &vnz);

    float vexs[8];
    float ah = (float)arrw->height, ac;
    float _ah = ah * 0.5f, ah2 = ah * 2.0f,
          _w = (float)w * 0.5f, _h = (float)h * 0.5f;
    float cx = (float)x + _w, cy = (float)y + _h;
    GNg::cVIEW2D *view = GNg::ViewCur;
    GNg::sVEC2 v1_ = vnz2 * (-ah), _1v = vnz * (float)arrw->width;
    int st; //计算绘制旋转箭头的首点在那里
    switch(GNg::ComputeRectNearFromDirection(w, h, vnz)) { //分析箭头靠那边
        case GNu::eNEAR_LEFT:
            st = x + aFw;
            ac = ((-_w) / vnz.x) * vnz.y;
            if(vnz.y < 0.0f) {
                ac += _ah;
                if(ac < -_h + ah2) ac = -_h + ah2;
                vexs[0] = (float)st;
                vexs[1] = cy + ac;
            } else {
                ac -= _ah;
                if(ac > _h - ah2) ac = _h - ah2;
                vexs[0] = (float)st + v1_.x;
                vexs[1] = cy + ac + v1_.y;
            }
            view->ScissorRectSetup(st - arrw->width, view->ortho_top, arrw->width, view->orthoH);
            break;
        case GNu::eNEAR_RIGHT:
            st = x + w - aFw;
            ac = (_w / vnz.x) * vnz.y;
            if(vnz.y < 0.0f) {
                ac += _ah;
                if(ac < -_h + ah2) ac = -_h + ah2;
                vexs[0] = (float)st + v1_.x;
                vexs[1] = cy + ac + v1_.y;
            } else {
                ac -= _ah;
                if(ac > _h - ah2) ac = _h - ah2;
                vexs[0] = (float)st;
                vexs[1] = cy + ac;
            }
            view->ScissorRectSetup(st, view->ortho_top, arrw->width, view->orthoH);
            break;
        case GNu::eNEAR_TOP:
            st = y + aFw;
            ac = ((-_h) / vnz.y) * vnz.x;
            if(vnz.x < 0.0f) {
                ac += _ah;
                if(ac < -_w + ah2) ac = -_w + ah2;
                vexs[0] = cx + ac + v1_.x;
                vexs[1] = (float)st + v1_.y;
            } else {
                ac -= _ah;
                if(ac > _w - ah2) ac = _w - ah2;
                vexs[0] = cx + ac;
                vexs[1] = (float)st;
            }
            view->ScissorRectSetup(view->ortho_left, st - arrw->width, view->orthoW, arrw->width);
            break;
        case GNu::eNEAR_BOTTOM:
            st = y + h - aFw;
            ac = (_h / vnz.y) * vnz.x;
            if(vnz.x < 0.0f) {
                ac += _ah;
                if(ac < -_w + ah2) ac = -_w + ah2;
                vexs[0] = cx + ac;
                vexs[1] = (float)st;
            } else {
                ac -= _ah;
                if(ac > _w - ah2) ac = _w - ah2;
                vexs[0] = cx + ac + v1_.x;
                vexs[1] = (float)st + v1_.y;
            }
            view->ScissorRectSetup(view->ortho_left, st, view->orthoW, arrw->width);
            break;
    }
    v1_ = -(v1_);
    vexs[2] = vexs[0] + _1v.x;
    vexs[3] = vexs[1] + _1v.y;
    vexs[4] = vexs[2] + v1_.x;
    vexs[5] = vexs[3] + v1_.y;
    vexs[6] = vexs[0] + v1_.x;
    vexs[7] = vexs[1] + v1_.y;
    GNg::ImageDraw4P(arrw, vexs, aColor, 0, aBlend, 0.0f, 0.0f, arrw->uRange, arrw->vRange);
    if(show_frame) GNg::ImageDraw4P(arrwframe, vexs, aColorFrame, 0, aBlendFrame, 0.0f, 0.0f, arrwframe->uRange, arrwframe->vRange);
    view->ScissorRectRemove();
}
bool GNu::sBUBBLE::LoadDefStyle(P<GNg::sIMAGE> &image9, P<GNg::sIMAGE> &arrow, P<GNg::sIMAGE> &image9f, P<GNg::sIMAGE> &arrowf) {
    image9.New(1);
    arrow.New(1);
    image9f.New(1);
    arrowf.New(1);
    if(!GNg::ImageLoad(GN_SysDir("ui/bubbleB"), 9, image9, GNg::CR_ALPHA)) return false;
    if(!GNg::ImageLoad(GN_SysDir("ui/bubbleBF"), 9, image9f, GNg::CR_ALPHA)) return false;
    if(!GNg::ImageLoad(GN_SysDir("ui/bubbleA"), 1, arrow, GNg::CR_ALPHA)) return false;
    if(!GNg::ImageLoad(GN_SysDir("ui/bubbleAF"), 1, arrowf, GNg::CR_ALPHA)) return false;
    return true;
}
