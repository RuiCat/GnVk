// GN_UiPopDlgSetup.cpp — GNu::cSIM_DIALOG 装配/样式/几何/文本量算

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
#include "GN.h"
#include "GN_Def.h"
#include "GN_UiPopDlgInner.h"
#ifdef _GN_UIPOPDLG_H_
char GNu::cSIM_DIALOG::aFuntionButtonName[2][3][9] = {{"取 消", "保 存", "默 认"}, {"CANCEL", "SAVE", "DEFAULT"}};

void GNu::cSIM_DIALOG::zDeleteArrowMap() {
    if(zUsGenArrowMap) {
        GN_Delete(zArrowMap);
        zUsGenArrowMap = 0;
    } else zArrowMap = 0;
}
void GNu::cSIM_DIALOG::zRelease_(bool all) {
    zTextOut.Release();
#ifdef _GN_SOUND_H_
    zSound.Release();
#endif
    zDeleteArrowMap();
    if(all) sSIM_MENU::ReleaseAll();
    else sSIM_MENU::Release();
    GN_DeleteArray(zButtonName);
    GN_DeleteArray(zFileName);
    zSetTool.Delete();

    zIsDetailSet = 0;
    zShowReady = 0;
    zState = 0;
    zFocusMap = 0;
    zTargetMenu = 0;
    zEffect = 0;
    zBackCaption = 0;
    zBackFrame = 0;
    zBackDrawProc = 0;
    aValueLoop = 1;
}
GNu::cSIM_DIALOG::cSIM_DIALOG() : zBackDrawProc(0),
                                  zValueChangeProc(0),
                                  zValueChangeArg(0),
                                  zFun1(0), zFun2(0), zFun3(0),
                                  zButtonName(0),
                                  zFileName(0),
                                  zShowReady(0),
                                  zState(0),
                                  zTargetMenu(0),
                                  zEffect(0),
                                  zFocusFlash(0),
                                  zFocusMap(0),
                                  zArrowMap(0),
                                  zBackCaption(0),
                                  zBackFrame(0),
                                  zUsGenArrowMap(0),
                                  aCaptextOfsY(0),
                                  aValueLoop(1),
                                  aSkipPos(0),
                                  zIsDetailSet(0),
                                  zKeyDOWN(GK_DOWN),
                                  zKeyUP(GK_UP),
                                  zKeyLEFT(0),
                                  zKeyRIGHT(1),
                                  zKeyOK(GK_RETURN),
                                  zUsMouse(0),
#ifdef _GN_SOUND_H_
                                  zSouObj(0)
#endif
{
}
GNu::cSIM_DIALOG::~cSIM_DIALOG() {
    zDeleteArrowMap();
    GN_DeleteArray(zButtonName);
    GN_DeleteArray(zFileName);

    aValueLoop = 1;
    zIsDetailSet = 0;
    zShowReady = 0;
    zState = 0;
    zTargetMenu = 0;
    zEffect = 0;
    zFocusMap = 0;
    zBackCaption = 0;
    zBackFrame = 0;
    zBackDrawProc = 0;
}
void GNu::cSIM_DIALOG::BeginInitToData(const char *fname) {
    if(fname && fname[0]) { //拷贝保存名称
        int len = strlen(fname);
        int di = len - 1;
        int de = (di - 6) > 0 ? (di - 6) : 0;
        for(; di > de; --di)
            if(fname[di] == '.') break;
        if(di == de) di = len;
        GN_DeleteArray(zFileName);
        zFileName = new char[di + 5];
        strncpy(zFileName, fname, di);
        // 已核（B5f）：new char[di+5]，".ygs"+NUL 恰好 5 字节。
        strcpy(zFileName + di, ".ygs");
    }
}
void GNu::cSIM_DIALOG::EndInitToData() {
    if(!zFileName) { //没指定文件名就为它创建一个默认文件
        char *def_file_name = "GameSetting.ygs";
        zFileName = new char[strlen(def_file_name) + 1];
        // 已核（B5f）：new char[strlen+1]，精确容量。
        strcpy(zFileName, def_file_name);
    }
    /*- ygs文件格式数据布局 --------\   /- ygt文件格式数据布局 ----------*\   
	|	char 文件类型标识4个字节    |	|	char 文件类型标识4个字节      |
    |	void *数据 每个占4个字节    |	|	void *数据 紧接的多段字符串   |
	\*------------------------------/   \--------------------------------*/

    // 读取文件数据初始化选单变量值 ---------------------------------------------------------------------------------------------------
    int file_var_num = GN_INT_MIN;
    GNf::sFILE *pfile = GNf::Open(zFileName, "rb", GNf::eFILE_SAFE);
    if(pfile) { //确认文件存在
        const char *err_str = "调用cSIM_DIALOG::EndInitToData 失败! %s: %s %s!\n";
        char file_type[4];
        int file_size = GNf::GetSize(pfile);
        file_var_num = (file_size >> 2) - 1; //计算得到变量值数
        GNf::Read(file_type, 1, 4, pfile);
        if(!strcmp(file_type, "YGS")) { //确认是该文件类型
            GN_UiPopDlgContext().aSimMenuTextOffsetPos = 0.0f;
            zReadData(pfile, file_var_num); //递归读入数据 不够的保持默认值
            const char *err_str1 = "读取简单对话筐存档文件数据有错! 该.yg%c文件是过期的! 与当前对话筐变量条目或层次结构匹配不上!\n";
            if(file_var_num == GN_INT_MIN) {
                zReDefault(true); //出错时就恢复为默认值
                GN_Log(err_str1, 's'); //文件不对应 这可能在生成该文件后又在代码里修改过变量条目或文件搞错或损坏

            } else if(GN_UiPopDlgContext().aSimMenuTextOffsetPos) { //有文本要读
                GNf::Close(pfile);
                char *name_end = zFileName + (strlen(zFileName) - 1);
                *name_end = 't';
                pfile = GNf::Open(zFileName, "rb", GNf::eFILE_SAFE);
                if(pfile) {
                    file_size = GNf::GetSize(pfile);
                    GNf::Read(file_type, 1, 4, pfile);
                    if(!strcmp(file_type, "YGT")) { //确认是该文件类型
                        file_var_num = file_size - 4;
                        zReadText(pfile, file_var_num); //递归读入文本 不够的保持默认值
                        if(file_var_num == GN_INT_MIN) {
                            zReDefault(true); //出错时就恢复为默认值
                            GN_Log(err_str1, 't'); //文件不对应 这可能在生成该文件后又在代码里修改过变量条目或文件搞错或损坏
                        }
                    } else {
                        file_var_num = GN_INT_MIN;
                        GN_Log(err_str, "文件", zFileName, "损坏");
                    }
                } else {
                    file_var_num = GN_INT_MIN;
                    GN_Log(err_str, "找不到文件", zFileName, "文本数据读取失败");
                }
                *name_end = 's';
            }
        } else {
            file_var_num = GN_INT_MIN;
            GN_Log(err_str, "文件", zFileName, "损坏");
        }
        GNf::Close(pfile);
    }
    if(file_var_num == GN_INT_MIN) { //当文件不存在时 将数据保存成一个新文件
        const char *err_str2 = "调用cSIM_DIALOG::EndInitToData 安全创建文件: %s 失败!\n";
        uint8 ygi = 0;
        for(; ygi < 2; ++ygi) { //安全保存数据会创建两个文件
            pfile = GNf::Open(zFileName, "wb", GNf::eFILE_SAFE);
            if(pfile) {
                char file_type2[] = "YGS";
                GNf::Write(file_type2, 1, 4, pfile); //写文件头
                GN_UiPopDlgContext().aSimMenuTextOffsetPos = 0.0f;
                zWriteData(pfile); //递归写入数据
                GNf::Close(pfile);

                if(GN_UiPopDlgContext().aSimMenuTextOffsetPos) { //有文本要写就另创建个ygt文件
                    char *name_end = zFileName + (strlen(zFileName) - 1);
                    *name_end = 't';
                    pfile = GNf::Open(zFileName, "wb", GNf::eFILE_SAFE);
                    if(pfile) {
                        file_type2[2] = 'T';
                        GNf::Write(file_type2, 1, 4, pfile); //写文件头
                        zWriteText(pfile); //递归写入文本内容
                        GNf::Close(pfile);
                        *name_end = 's';
                    } else {
                        *name_end = 's';
                        GN_Log(err_str2, zFileName);
                        break;
                    }
                }
            } else {
                GN_Log(err_str2, zFileName);
                break;
            }
        }
    }
}
bool GNu::cSIM_DIALOG::SetStyleToData(const char *caption, const GNg::cTEXT_ART *text_style, int8 type, unsigned int capt_color,
                                      unsigned int text_color, unsigned int back_color, unsigned int line_color, unsigned int focus_color,
                                      unsigned short text_blend, unsigned short back_blend, unsigned short line_blend, unsigned short focus_blend,
                                      unsigned short line_width, P<GNg::sIMAGE> icon) {
    GN_StrcpyData(zCaption, caption);
    if(!zCaption) {
        zShowReady = false;
        return false;
    }
    zShowReady = true;
    if(text_style) {
        ((GNg::cTEXT_ART *)text_style)->Release();
        zTextOut.Release();
        zTextOut = *text_style;
    } else if(GN::RunRec.zGWPrintReady) {
        GN::RunRec.aFont.Release();
        zTextOut.Release();
        zTextOut = GN::RunRec.aFont;
    } else {
        zShowReady = false;
        return false;
    }
    GNg::sARRANGE_FMT *p_arrg = zTextOut.GetArrangeFormat();
    if(p_arrg->erect_print || p_arrg->overturn_print) GN_Log("%s 文字式样参数的排列方法必须是从左到右的横方向输出!\n", "调用cSIM_DIALOG::SetStyleToData");

    // 记下文本式样起始状态
    GNg::sARRANGE_FMT &tfmt = zTextOut.zParame;
    tfmt.near_pointV = 0.5f;
    zTwidth = tfmt.width;
    zTi_compartment = tfmt.i_compartment;
    zTheight = tfmt.height;
    zTj_compartment = tfmt.j_compartment;

    // 颜色赋值
    aCaptextColor = capt_color ? capt_color : 0xffffffff;
    aTextColor = text_color ? text_color : GT_AppDefaultColorFont;
    aTextBlend = text_blend;
    aBackColor = back_color ? back_color : GT_AppDefaultColorBack;
    aBackBlend = back_blend;
    aLineColor = line_color ? line_color : GT_AppDefaultColorFont;
    aLineBlend = line_blend;
    aFocusColor = focus_color;
    aFocusBlend = focus_blend;
    aLineWidth = line_width;

    GN_ValueClamp(type, 0, 3);
    zType = type;
    zIcon = icon;

    //生成小箭头图
    if(!zArrowMap) {
        zArrowMap = new GNg::sIMAGE;


        GT_TMP_MAKE_SIM_ARROW_ICON
        zUsGenArrowMap = true;
    }
    return zShowReady;
}
void GNu::cUI_SIM_DIALOG::ArrowMapTransfer() {
    if(zUsGenArrowMap && zArrowMap) {
        zArrowMap1.New(1);
        *zArrowMap1 = *zArrowMap;
        zArrowMap1->cloneImg = false;
        zArrowMap->cloneImg = true;
        GN_Delete(zArrowMap);
        zUsGenArrowMap = 0;
        zArrowMap = zArrowMap1;
    }
}
void GNu::cSIM_DIALOG::zSetActiveMenu(const sSIM_MENU *menu) {
    if(menu) {
        zTargetType = menu->zType;
        zTargetCaption = menu->zCaption;
        zTargetMenu = (sSIM_MENU *)menu;
        zTargetIcon = zTargetMenu->zIcon;
    }
    zStatisticsDlgTextNum(zTargetMenu, zTargetCaption); //预先计算好各项文本的格位个数与大小
    zStatisticsDlgSize(1.0f, 1.0f); //计算好各项文本的格位实际显示大小
    zDlgx = (short)(GNu::NearCx() - (zDlgw >> 1)) + zDlaOffsetX; //计算对话筐显示坐标 没效果时默认居中显示
    zDlgy = (short)(GNu::NearCy() - (zDlgh >> 1)) + zDlaOffsetY;
}
void GNu::cSIM_DIALOG::Pop() {
    if(!zShowReady) SetStyleToData(); //如还未设置显示式样就自动设置一下 若已开启了调试信息即可以借助它的文字显示
    if(zEffect) {
        zEffect->aFrame_ts.x = zT_ef_x;
        zEffect->aFrame_ts.y = zT_ef_y;
    }
    zDlaOffsetX = 0; //恢复原设置的对话筐弹出位置
    zDlaOffsetY = 0;
    zSetActiveMenu(this);
}
void GNu::cSIM_DIALOG::Pop(const char *new_cap) {
    GN_StrcpyData(zCaption, new_cap);
    Pop();
}
void GNu::cSIM_DIALOG::RePop() {
    zSetActiveMenu();
}
void GNu::cSIM_DIALOG::OffsetPos(int ofsx, int ofsy) {
    if(zEffect) {
        zEffect->aFrame_ts.x += ofsx;
        zEffect->aFrame_ts.y += ofsy;
        zDlgx += ofsx;
        zDlgy += ofsy;
    } else {
        zDlaOffsetX += ofsx;
        zDlaOffsetY += ofsy;
        zDlgx += ofsx;
        zDlgy += ofsy;
    }
}
void GNu::cSIM_DIALOG::SetFileSelectOpenDirectoryID(uint8 did) {
    if(zSetTool) zSetTool->fb_open_did = did;
}
unsigned int GNu::cSIM_DIALOG::GetEffectLevelColor() {
    return (zEffect ? zEffect->aLevel_color : 0xffffffff);
}
void GNu::cSIM_DIALOG::GetDlgPos(int &_x, int &_y, int &_w, int &_h) {
    _x = zDlgx, _y = zDlgy, _w = zDlgw, _h = zDlgh;
}
void GNu::cSIM_DIALOG::GetDlgRect(GNg::sRECT *rc_out) {
    rc_out->left = zDlgx;
    rc_out->top = zDlgy;
    rc_out->right = (zDlgx + zDlgw);
    rc_out->bottom = (zDlgy + zDlgh);
}
int GNu::cSIM_DIALOG::zGetLastDetailSetCY() {
    if(zSetTool) return zSetTool->fy + (zSetTool->fh >> 1);
    return -100000;
}
void GNu::cSIM_DIALOG::GetDetailSetPos(int &_x, int &_y, int &_w, int &_h) {
    if(!zDetailSetState == eDETAIL_SET_NULL) return;
    GT_sVALUE_SET_TOOL *_set_tool = zSetTool;
    _x = _set_tool->fx;
    _w = _set_tool->fw;
    _y = _set_tool->fy;
    _h = _set_tool->fh;
}
void GNu::cSIM_DIALOG::GetDetailSetPos(GNg::sRECT *rc_out) {
    if(!zDetailSetState == eDETAIL_SET_NULL) return;
    GT_sVALUE_SET_TOOL *_set_tool = zSetTool;
    rc_out->left = _set_tool->fx;
    rc_out->top = _set_tool->fy;
    rc_out->right = _set_tool->fx + _set_tool->fw;
    rc_out->bottom = _set_tool->fy + _set_tool->fh;
}
void GNu::cSIM_DIALOG::GetChildPos(int &_x, int &_y, int &_w, int &_h, short chd_id, eCHILD_CONTENT_TYPE content) {
    if(!zState) {
#ifdef GN_DEBUG
        GN_Log("调用cSIM_DIALOG::GetChildPos 还没准备好! 请先调用 Pop();\n");
#endif
        return;
    }
    if(!zChildNum) {
#ifdef GN_DEBUG
        GN_Log("调用cSIM_DIALOG::GetChildPos 失败! 由于没有子选项! 请先调用 InitValue___(...);\n");
#endif
        _x = 0;
        _y = 0;
        _w = 0;
        _h = 0;
        return;
    }
    if(chd_id < 0) chd_id = 0;
    else if(chd_id >= zChildNum) chd_id = zChildNum - 1; //防止出轨
    uint8 type = zTargetMenu->zChild[chd_id].type;


    GT_TMP_SIM_DIALOG_DRAW_COMPARTENT_CPT //计算好绘制间距
        short th_ofs = (tj - tj_) >> 1; //输入文本筐显示高度相对该项显示高度的偏移量

    short draw_x = zDlgx + zBorderW + ti2,
          draw_y = zDlgy + zBorderW + tj2 + tj_,
          draw_w = zDlgw - (zBorderW << 1) - (ti2 << 1); //焦点缩至内边筐
    short dx, dw; //绘制焦点的当前位置
    if(type <= GT_SP_VALUE_FLOAT) {
        dx = draw_x + ti;
        dw = draw_w - ti2;
        dx += (short)((float)(dw - zSubValueW) * tfmt.near_pointU); //显示变量栏的焦点范围
    } else {
        dx = draw_x;
        dw = draw_w;
        if(content > eCHILD_CONTENT_NAME) content = eCHILD_CONTENT_NAME;
    }
    switch(content) {
        case eCHILD_CONTENT_ALL: //整个子选项范围
            _x = (dx - (ti >> 1));
            _w = (dw + ti);
            _y = (draw_y + chd_id * tj);
            _h = tj;
            return;
        case eCHILD_CONTENT_ICON: //子选项图标范围
            _x = dx;
            _w = zIconW;
            _y = (draw_y + chd_id * tj);
            _h = tj;
            return;
        case eCHILD_CONTENT_NAME: //子选项标题名范围
            _x = dx + iw;
            _w = type <= GT_SP_VALUE_FLOAT ? zNameW : (draw_x + draw_w - _x);
            _y = draw_y + chd_id * tj + th_ofs;
            _h = tj_;
            return;
    }
        // 剩下其它可调值的子选项内容 --------------------------------------------------------------------------------------------

    GT_TMP_CHILD_DRAW_POSX_CPT //计算子选项各个内容显示位置X
        short arr_w = (short)((float)zArrowMap->width * (float)ti / (float)zTi_compartment); //计算经比例变换后的箭头宽
    if(type >= GT_SP_VALUE_COLOR) {
        short arr_h = (short)((float)zArrowMap->height * (float)ti / (float)zTi_compartment); //计算经比例变换后的箭头高
        short ah_ofs = (tj - arr_h) >> 1; //箭头显示高度偏移
        short ch_ofs = (tj - tfmt.height) >> 1; //颜色格显示高度偏移

        switch(content) {
            case eCHILD_CONTENT_VALUE: //子选项标题名范围
                _x = vvx;
                _w = zValueW;
                _y = draw_y + chd_id * tj + ch_ofs;
                _h = zTextOut.zParame.height;
                return;
            case eCHILD_CONTENT_ARWL: //子选项标题名范围
                _x = va1x - arr_w;
                _w = arr_w;
                _y = draw_y + chd_id * tj + ah_ofs;
                _h = arr_h;
                return;
            case eCHILD_CONTENT_ARWR: //子选项标题名范围
                _x = va2x;
                _w = arr_w;
                _y = draw_y + chd_id * tj + ah_ofs;
                _h = arr_h;
                return;
            default: { //eCHILD_CONTENT_LABLE 子选项标题名范围
                _x = va2x + ti + ti + (ti >> 1); //临时附加注息显示X
                _w = zEnumNameW;
                _y = draw_y + chd_id * tj + th_ofs;
                _h = tj_;
            }
                return;
        }
    } else if(type == GT_SP_VALUE_TEXT) { //子选项文本输入筐范围
        short text_in_w = va2x - va1x; //输入文本筐的宽
        _x = va1x - (arr_w >> 1);
        _w = text_in_w + arr_w;
        _y = draw_y + chd_id * tj + th_ofs;
        _h = tj_;
    }
}
void GNu::cSIM_DIALOG::GetChildRect(GNg::sRECT *rect_out, short chd_id, eCHILD_CONTENT_TYPE content) {
    int _x, _y, _w, _h;
    GetChildPos(_x, _y, _w, _h, chd_id, content);
    rect_out->left = _x;
    rect_out->right = _x + _w;
    rect_out->top = _y;
    rect_out->bottom = _y + _h;
}
#ifdef _GN_SOUND_H_
void GNu::cSIM_DIALOG::SetKeyToData(const GNa::sSOUND *sound, short ok, short down, short up, short left, short right, bool usMouse) {
    zSouObj = (GNa::sSOUND *)sound;
    zSound.InitToData(zSouObj);
#else
void GNu::cSIM_DIALOG::SetKeyToData(const void *key_sound, short ok, short down, short up, short left, short right, bool usMouse) {
#endif
    zKeyOK = ok;
    zKeyDOWN = down;
    zKeyUP = up;
    zKeyLEFT = left;
    zKeyRIGHT = right;
    zUsMouse = usMouse;
}
void GNu::cSIM_DIALOG::SetBackFrame(const GNg::sIMAGE *frame_img, const GNg::sIMAGE *caption_img, bool repeat, void (*draw_call)(short, short, short, short, unsigned int, unsigned short, GNg::sIMAGE *)) {
    zBackFrameRepeat = repeat;
    zBackCaption = (GNg::sIMAGE *)caption_img;
    zBackDrawProc = draw_call;
    if(draw_call) zBackFrame = (GNg::sIMAGE *)frame_img;
    else if(frame_img) {
        if(frame_img->texnum == 9) {
            zBackFrame = (GNg::sIMAGE *)frame_img;
        } else {
            zBackFrame = 0;
#ifdef GN_DEBUG
            GN_Log("调用cSIM_DIALOG::SetBackFrame 背景筐图不是有效的9宫格图!\n");
#endif
        }
    } else zBackFrame = 0;
}
void GNu::cSIM_DIALOG::SetEffect(GNg::cFRAME_RC *effect, short focus_flash_time, const GNg::sIMAGE *focus_style, const GNg::sIMAGE *arrow_style, bool affect_color) {
    if(effect) {
        zEffect = effect;
        GNg::sRECT2 *_rc = &(effect->zRcLoc);
        float ratio_w = (float)effect->zPlay_obj->width / (_rc->right - _rc->left);
        float ratio_h = (float)effect->zPlay_obj->height / (_rc->bottom - _rc->top);
        zT_effect_rect.left = _rc->left * ratio_w;
        zT_effect_rect.top = _rc->top * ratio_h;
        zT_effect_rect.right = _rc->right * ratio_w;
        zT_effect_rect.bottom = _rc->bottom * ratio_h;
        zT_ef_x = effect->aFrame_ts.x;
        zT_ef_y = effect->aFrame_ts.y;
        aAffectColor = affect_color;
    }
    zFocusMap = (GNg::sIMAGE *)focus_style;
    zFocusFlash = focus_flash_time;
    if(arrow_style) {
        zDeleteArrowMap();
        zArrowMap = (GNg::sIMAGE *)arrow_style;
    }
}
void GNu::cSIM_DIALOG::EnableDetailSetting(bool detail_set, unsigned int input_bg_color, unsigned short input_bg_blend, eSET_PATH_TYPE set_path_type) {
    eSET_PATH_TYPE set_path_type1 = zSetPathType;
    zSetPathType = set_path_type;
    zIsDetailSet = detail_set;
    aInputBackColor = input_bg_color;
    aInputBackBlend = input_bg_blend;

    if(zIsDetailSet) {
        GT_sVALUE_SET_TOOL *_set_tool;
        if(zSetTool) _set_tool = zSetTool;
        else {
            zSetTool.New(1);
            _set_tool = zSetTool;
            _set_tool->text_in->InitStyle(&zTextOut, 0, 0, 256, 32, aCaptextColor, aFocusColor, aTextBlend, aFocusBlend);
            _set_tool->palette->InitToData(0, 0, 256, 256);
            _set_tool->palette->InitDefaultTable();
        }
        if(set_path_type != eSET_PATH_INPUT) {
            cFILE_BROWSER::GetIcon();
            if(!_set_tool->dir_strs) {
                _set_tool->dir_strs = GNf::EnumSystemDirectory();

                _set_tool->ftf_strs.New(7);
                P<char> *_ft_strs = _set_tool->ftf_strs;
                (_ft_strs[0])("*", 0);
                (_ft_strs[1])("txt,html,json,ini,inf", 0);
                (_ft_strs[2])(GNg::GetSupportImgFmtFileExtName(), 0);
                (_ft_strs[3])(GNa::GetSupportAudioFmtFileExtName(), 0);
                (_ft_strs[4])("avi,flv,mp4,mgp,mkv", 0);
                (_ft_strs[5])("obj,fbx,gmh", 0);
                (_ft_strs[6])("zip,rar,7z", 0);

                _set_tool->fts_strs.New(4);
                _ft_strs = _set_tool->fts_strs;
                if(zTextOut.zFontMapFormat < GNg::eFONT_FREETYPE) {
                    (_ft_strs[0])("Arranged by name", 0);
                    (_ft_strs[1])("Arranged by size", 0);
                    (_ft_strs[2])("Arranged by type", 0);
                    (_ft_strs[3])("Arranged by modified time", 0);
                } else {
                    (_ft_strs[0])("按名称排列", 0);
                    (_ft_strs[1])("按大小排列", 0);
                    (_ft_strs[2])("按类型排列", 0);
                    (_ft_strs[3])("按修改时间排列", 0);
                }
            }
            if(set_path_type != set_path_type1) {
                _set_tool->file_browser.Delete();
                _set_tool->fb_select.Delete();
            }
        }
        _set_tool->info[0] = 0;
    } else zSetTool.Delete();
}
void GNu::cSIM_DIALOG::PathSelectDirectoryAdd(P<P<char>> dirs) {
    if(zSetTool && zSetTool->dir_strs) {
        dirs.Insert(zSetTool->dir_strs);
        zSetTool->dir_strs = dirs;
    }
}
void GNu::cSIM_DIALOG::ReplaceFuntion(char button_name[2][3][9], void (*fun1)(), void (*fun2)(), void (*fun3)()) {
    if(fun1) zType = 1;
    zFun1 = fun1;
    if(fun2) zType = 2;
    zFun2 = fun2;
    if(fun3) zType = 3;
    zFun3 = fun3;

    GN_DeleteArray(zButtonName);

    unsigned int size = GT_DLG_BNAME_SIZE;
    zButtonName = new char[size];
    memcpy(zButtonName, button_name, size); //保存按钮名
}
void GNu::cSIM_DIALOG::OnValueChange(void (*proc)(void *), void *us_arg) {
    zValueChangeProc = proc;
    zValueChangeArg = us_arg;
}
void GNu::cSIM_DIALOG::zStatisticsDlgTextNum(const sSIM_MENU *menu, const char *caption) {
    //- 统计正文区内各项要显示的最大字数 -----------------------------------------------------------
    zIconNW = 0, zIconNH = 0, zTotalLenNum = 0, zSubMenuNum = 0, zNameNum = 0, zEnumNameNum = 0;
    zValueNum = 10; //变量值预留10位
    short len, si, li;
    zChildNum = menu->zChild.size();
    GT_sSP_CHILD *_chd;
    for(si = 0; si < zChildNum; ++si) {
        _chd = (GT_sSP_CHILD *)&(((sSIM_MENU *)menu)->zChild[si]);
        if(_chd->icon) {
            len = (short)_chd->icon->width;
            if(zIconNW < len) zIconNW = len;
            len = (short)_chd->icon->height;
            if(zIconNH < len) zIconNH = len;
        }
        switch(_chd->type) {
            case GT_SP_VALUE_INT:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len; //比较找出最长的名称是字符数
                if(_chd->enumName) { //有列举名
                    if(_chd->enumIds) { //且有分割
                        len = _chd->enumIds[_chd->enumIdnum - 1];
                        if(zValueNum < len) zValueNum = len;
                    } else { //没有分割
                        len = strlen(_chd->enumName);
                        if(zEnumNameNum < len) zEnumNameNum = len;
                    }
                }
                break;
            case GT_SP_VALUE_FLOAT:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len; //比较找出最长的名称是字符数
                break;
            case GT_SP_VALUE_TEXT:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len;
                len = (short)_chd->minValue;
                if(zValueNum < len) zValueNum = len;
                break;
            case GT_SP_VALUE_COLOR:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len;
                break;
            case GT_SP_MENU:
                len = strlen(_chd->name);
                if(zSubMenuNum < len) zSubMenuNum = len;
                break;
            case GT_SP_TEXT:
                len = (short)_chd->maxValue;
                if(zSubMenuNum < len) zSubMenuNum = len;
                break;
        }
    }
    //- 找出横方向最大可能排的字符数 ---------------------------------------------------------------
    zTotalLenNum = zSubMenuNum + (2 << 1); //子选单边距两字宽
    if(caption) {
        zCaptionNum = strlen(caption);
        len = zCaptionNum + 2; //标提边距1字宽
        if(zTotalLenNum < len) zTotalLenNum = len;
    }
    zSubValueNum = zNameNum + zValueNum + zEnumNameNum + (3 << 1); //箭头占用3字宽
    len = zSubValueNum + (3 << 1); //子变量项边距3字宽
    if(zTotalLenNum < len) zTotalLenNum = len;

    if(zTargetType) {
        if(zTextOut.zFontMapFormat < GNg::eFONT_FREETYPE) {
            zFuncButtonNum = 9; //" DEFAULT ";
            len = strlen("  SAVE    CANCEL   DEFAULT ") + ((2 + 3) << 1); //功能按纽项间隔2字宽 边距3字宽
        } else {
            zFuncButtonNum = 9; //"  默 认  ";
            len = strlen("  保 存    取 消    默 认  ") + ((2 + 3) << 1);
        }
        if(zTotalLenNum < len) zTotalLenNum = len;
    }
    //重置一些其他内容变量值
    if(zSetTool) zSetTool->info[0] = 0;
    zDetailSetState = eDETAIL_SET_NULL;
    zDrawBackFrame.right = 0;
    zSelectLast = 0;
    zSelect = 0;
    _chd = zTargetMenu->zChild.data;
    while(zSelect < zChildNum && _chd[zSelect].type == GT_SP_TEXT) ++zSelect; //跳过文本提示
    if(zEffect) {
        zSelectLast = zSelect;
        zState = 8;
    } else zState = 2;
    zSelectLockID = 30000;
    zIsMoveDlg = 0;
    zfNowtime = 0;
    zNowtime = 0;
    zfAm_s_e = 0;
}
void GNu::cSIM_DIALOG::zStatisticsDlgSize(float ratio_w, float ratio_h) {
    // 计算出各项宽的实际尺寸 ----------------------------------------------------------------
    float char_w = (float)zTi_compartment * ratio_w;
    float char_h = (float)zTj_compartment * ratio_h;

    zSubValueW = (short)((float)zSubValueNum * char_w);
    zValueW = (short)((float)zValueNum * char_w);
    zNameW = (short)((float)zNameNum * char_w);
    zEnumNameW = (short)((float)zEnumNameNum * char_w);
    zFuncButtonW = (short)((float)zFuncButtonNum * char_w);
    zBorderW = aLineWidth >> 1 + 2; //标题距线筐缝隙2个象素宽

    zIconW = (short)((float)zIconNW * ratio_w);
    zIconH = (short)((float)zIconNH * ratio_h);
    zIconW2 = zTargetIcon ? (short)((float)zTargetIcon->width * ratio_w) : 0;
    zIconH2 = zTargetIcon ? (short)((float)zTargetIcon->height * ratio_h) : 0;

    short max_icn_w = zIconW > zIconW2 ? zIconW : zIconW2;
    if(max_icn_w) zDlgw = (short)((float)(zTotalLenNum + 1) * char_w) + (zBorderW << 1) + max_icn_w;
    else zDlgw = (short)((float)zTotalLenNum * char_w) + (zBorderW << 1);

    float icn1_h = (char_h < (float)zIconH) ? (float)zIconH : char_h;
    float icn1_h2 = (char_h < (float)zIconH2) ? (float)zIconH2 : char_h;
    //主内容顶离对话筐顶1字高与1标题栏高 主内容底离对话筐底2.5字高
    zDlgh = (short)(icn1_h2 + icn1_h * (float)(zChildNum) + char_h * (1.0f + 2.5f) + (float)(zBorderW << 1));
    if(zTargetType) zDlgh += char_h;

    GNg::sARRANGE_FMT &tfmt = zTextOut.zParame;
    tfmt.width = (short)((float)zTwidth * ratio_w); //字体大小也按比例缩放一下
    tfmt.height = (short)((float)zTheight * ratio_h);
    tfmt.i_compartment = (short)char_w;
    tfmt.j_compartment = (short)char_h;

    if(!tfmt.width) tfmt.width = 1; //确保排版值至少为1
    if(!tfmt.height) tfmt.height = 1;
    if(!tfmt.i_compartment) tfmt.i_compartment = 1;
    if(!tfmt.j_compartment) tfmt.j_compartment = 1;
}
void GNu::cSIM_DIALOG::zDetailSetReturnLongLinear(GT_sVALUE_SET_TOOL *_set_tool, const GNg::sRECT *rng) {
    if(zEffect) {
        _set_tool->fy = rng->top; //计算插值焦点显示位置
        _set_tool->fh = rng->bottom;
        _set_tool->linear_ing = true;
        float lr = 160.0f / (float)zEffect->zEndtime;
        GN::ValueChange(&(_set_tool->fx), (short)(rng->left), 0, 0, 0.5f, lr);
        GN::ValueChange(&(_set_tool->fw), (short)(rng->right), 0, &_set_tool->linear_ing, 0.5f, lr);
        zDetailSetState = 0xff;
    } else zDetailSetState = eDETAIL_SET_NULL;
}
void GNu::cSIM_DIALOG::zDetailSetReturnRangeLinear(GT_sVALUE_SET_TOOL *_set_tool, const GNg::sRECT *rng, unsigned int tg_color) {
    if(zEffect) {
        float lr = 130.0f / (float)zEffect->zEndtime;
        _set_tool->linear_ing = true;
        GN::ValueChange(&_set_tool->fy, (short)(rng->top), 0, 0, 0.5f, lr);
        GN::ValueChange(&_set_tool->fh, (short)(rng->bottom), 0, &_set_tool->linear_ing, 0.5f, lr);
        GN::ValueChange(&_set_tool->fx, (short)(rng->left), 0, 0, 0.5f, lr);
        GN::ValueChange(&_set_tool->fw, (short)(rng->right), 0, 0, 0.5f, lr);
        GN::ValueChangeColor(&_set_tool->fc, tg_color, 0, 0, 0.5f, lr);
        zDetailSetState = 0xfe;
    } else zDetailSetState = eDETAIL_SET_NULL;
}
/********************************************************************************************************/
//对话框设置工具提示信息状态重置(变量设置工具集, 动画效果框, 不透明度色)
void GT_DlgSetToolInfoStateReset(GT_sVALUE_SET_TOOL *_set_tool, const GNg::cFRAME_RC *_ef, unsigned int ca) {
    if(_ef) {
        float lr = 160.0f / (float)_ef->zEndtime;
        _set_tool->ic = ca & 0x00ffffff;
        GN::ValueChangeColor(&_set_tool->ic, ca, 0, 0, 0.5f, lr * 0.36f);
    } else _set_tool->ic = ca;
}
//对话框设置工具提示信息状态更新(变量设置工具集, 动画效果框, 不透明度色, 提示时间)
void GT_DlgSetToolInfoStateUpdate(GT_sVALUE_SET_TOOL *_set_tool, const GNg::cFRAME_RC *_ef, unsigned int ca, unsigned int t) {
    if(t > 1600) { //显示到时间要结束了
        if(_ef) {
            if((t - 1600) <= (unsigned int)_ef->zEndtime) {
                _set_tool->ic = ((_ef->zEndtime - (t - 1600)) * ((ca & 0xff000000) >> 24) / _ef->zEndtime << 24) | (_set_tool->ic & 0x00ffffff);
            } else {
                _set_tool->info[0] = 0;
            }
        } else _set_tool->info[0] = 0;
    }
}
#endif //_GN_UIPOPDLG_H_
