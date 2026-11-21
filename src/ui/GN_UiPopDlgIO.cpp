// GN_UiPopDlgIO.cpp — 显隐/保存/恢复、GNu::cSIM_DIALOGEX、克隆与存档读写、全局设置菜单工厂

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
void GNu::cSIM_DIALOG::SetValueChange(bool value_loop) {
    aValueLoop = value_loop;
}
void GNu::cSIM_DIALOG::zHide() {
    if(zEffect) {
        GT_TMP_SIM_DIALOG_EffectTransTimeSet();
        zState = 3;
    } else {
        zState = 0;
        zTargetMenu = 0; //没父选单就发出结束消息
    }
}
void GNu::cSIM_DIALOG::Hide() {
    if(!zState || zState == 3) return;
    zDetailSetState = eDETAIL_SET_NULL;
    zHide();
}
void GNu::cSIM_DIALOG::Cancel() {
    if(!zState || zState == 3) return;

#define GT_TMP_SIM_DIALOG_Cancel()                                                            \
    zDetailSetState = eDETAIL_SET_NULL;                                                       \
    if(zTargetMenu->zFather) zSetActiveMenu(zTargetMenu->zFather); /*有父层就退回上一层选单*/ \
    else zHide()

    GT_TMP_SIM_DIALOG_Cancel();
}
void GNu::cSIM_DIALOG::zSave(int offset_num) {
    const char *err_str = "调用cSIM_DIALOG::Save ";
    GNf::sFILE *savefile = GNf::Open(zFileName, "r+b", GNf::eFILE_SAFE);
    if(savefile) { //文件存在就保存该层选单的数据
        char file_head[4];
        GNf::Read(file_head, 1, 4, savefile);
        if(!strcmp(file_head, "YGS")) {
            GT_SimMenuTextOffsetPos = 0.0f;
            GNf::Seek(savefile, offset_num, GNf::eSEEK_CUR); /*跳至变量要保存的文件正确位置*/
            GT_sSP_CHILD *_chd;
            short si = 0;
            for(; si < zChildNum; ++si) {
                _chd = &(zTargetMenu->zChild[si]);
                if(_chd->type == GT_SP_VALUE_TEXT) {
                    GNf::Write((char *)&(_chd->maxValue), 4, 1, savefile);
                    GT_SimMenuTextOffsetPos = 1.0f;
                } else if(_chd->type <= GT_SP_VALUE_FLOAT) GNf::Write((char *)(_chd->pointer), 4, 1, savefile); //写入当前选单的变量数据
            }
            if(GT_SimMenuTextOffsetPos) { //写入当前选单的文本内容
                GNf::Close(savefile);
                char *name_end = zFileName + (strlen(zFileName) - 1);
                *name_end = 't';
                savefile = GNf::Open(zFileName, "r+b", GNf::eFILE_SAFE);
                if(savefile) {
                    GNf::Read(file_head, 1, 4, savefile);
                    if(!strcmp(file_head, "YGT")) {
                        for(si = 0; si < zChildNum; ++si) {
                            _chd = &(zTargetMenu->zChild[si]);
                            if(_chd->type == GT_SP_VALUE_TEXT) {
                                GNf::Seek(savefile, 4 + (int)(_chd->addValue), GNf::eSEEK_START); //跳至文本要保存的位置
                                GNf::Write((char *)(_chd->pointer), 1, (int)(_chd->maxValue), savefile);
                            }
                        }
                    } else GN_Log("%s文件 : %s 损坏了!\n", err_str, zFileName);
                } else GN_Log("%s保存文本数据有错! 没找到对应的 ygt文件!\n", err_str);
                *name_end = 's';
            }
        } else GN_Log("%s文件 : %s 损坏了!\n", err_str, zFileName);
        GNf::Close(savefile);
    } else
        GN_Log("%s文件 : %s 没有存档数据!\n", err_str, zFileName);
}
void GNu::cSIM_DIALOG::Save() {
    if(!zState) return;
    if(zFileName) {
        int offset_num = 0; //计算该选单变量存放在文件的偏移位置
        zComputeMenuForDataOffset(offset_num, zTargetMenu);
        zSave(offset_num);
        zSave(offset_num);
    }
    GT_TMP_SIM_DIALOG_Cancel();
}
void GNu::cSIM_DIALOG::Default(bool this_also) {
    if(!zState) return;
    zTargetMenu->zReDefault(false);
    if(this_also && zTargetMenu != (GNu::sSIM_MENU *)this) this->zReDefault(false);
    if(zValueChangeProc) zValueChangeProc(zValueChangeArg);
}
/********************************************************** cSIM_DIALOGEX ******************************************************************/
struct GT_s_VALUE_TS //值转换信息
{
    int chgv; //中转修改值
    uint8 *tg_v; //目标修改值
};
typedef GN::sLIST<GT_s_VALUE_TS, GN::cALLOC_POOL<GT_s_VALUE_TS>> GT_s_VALUE_TS_LIST;
void GT_SimpleDlgExOnValueChange(void *us_arg) {
    ((GNu::cSIM_DIALOGEX *)us_arg)->zOnValueChangeProc(0);
}
void GNu::cSIM_DIALOGEX::zOnValueChangeProc(unsigned short *chg_id_out) {
    GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS_LIST::iterator ititm = _items->begin(), etitm = _items->end();
    if(chg_id_out) {
        unsigned short i = 0;
        *chg_id_out = 0xffff;
        GT_s_VALUE_TS *_vts;
        for(; ititm != etitm; ++ititm, ++i) {
            _vts = &(*ititm);
            if(*(_vts->tg_v) != (uint8)(_vts->chgv)) {
                *(_vts->tg_v) = (uint8)(_vts->chgv);
                *chg_id_out = i;
            }
        }
    } else {
        for(; ititm != etitm; ++ititm) {
            *(ititm->tg_v) = (uint8)(ititm->chgv);
        }
    }
    if(zValueChangeProc2) zValueChangeProc2(zValueChangeArg2);
}
void GNu::cSIM_DIALOGEX::UpValues() {
    GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS_LIST::iterator ititm = _items->begin(), etitm = _items->end();
    for(; ititm != etitm; ++ititm) {
        ititm->chgv = *(ititm->tg_v);
    }
}
GNu::cSIM_DIALOGEX::cSIM_DIALOGEX() : zValueChangeProc2(0), zValueChangeArg2(0) {
    zTsItems = new GT_s_VALUE_TS_LIST;
    cSIM_DIALOG::OnValueChange(GT_SimpleDlgExOnValueChange, this);
}
void GNu::cSIM_DIALOGEX::OnValueChange(void (*proc)(void *), void *us_arg) {
    zValueChangeProc2 = proc;
    zValueChangeArg2 = us_arg;
}
GNu::cSIM_DIALOGEX::~cSIM_DIALOGEX() {
    if(zTsItems) {
        GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
        delete _items;
        zTsItems = 0;
    }
}
void GNu::cSIM_DIALOGEX::zRemoveValueRelatTsItems() {
    GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS_LIST::iterator ititm, etitm = _items->end();
    short ci, ce = zChild.size();
    if(ce && _items->size()) {
        void *_ptr;
        GT_s_VALUE_TS *_vts;
        for(ci = 0; ci < ce; ++ci) {
            _ptr = zChild[ci].pointer;
            for(ititm = _items->begin(); ititm != etitm;) {
                if(_ptr == (void *)&(ititm->chgv)) {
                    ititm = _items->erase(ititm);
                    break;
                } else ++ititm;
            }
        }
    }
}
void GNu::cSIM_DIALOGEX::RemoveValue() {
    zRemoveValueRelatTsItems();
    cSIM_DIALOG::RemoveValue();
}
void GNu::cSIM_DIALOGEX::RemoveValueAll() {
    ((GT_s_VALUE_TS_LIST *)zTsItems)->clear();
    cSIM_DIALOG::RemoveValueAll();
}
void GNu::cSIM_DIALOGEX::Release() {
    zRemoveValueRelatTsItems();
    cSIM_DIALOG::Release();
}
void GNu::cSIM_DIALOGEX::ReleaseAll() {
    ((GT_s_VALUE_TS_LIST *)zTsItems)->clear();
    cSIM_DIALOG::ReleaseAll();
}
void GNu::cSIM_DIALOGEX::InitValueToData1(sSIM_MENU *tg, uint8 *value, const char *name, uint8 max_value, uint8 min_value, uint8 add_value, const char *enum_name, P<GNg::sIMAGE> icon) {
    GT_TMP_sSIM_MENU_HasInitValue(value)
        GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS *_node = _items->add(GT_s_VALUE_TS());
    _node->tg_v = value;
    _node->chgv = *value;
    tg->InitValueToData(&(_node->chgv), name, max_value, min_value, add_value, enum_name, icon);
}
/******************************************************************************************************************************************************************************/
#ifdef _GN_SOUND_H_
void GNu::cUI_SIM_DIALOG::SetKeyToData(P<GNa::sSOUND> &sound, short ok, short down, short up, short left, short right, bool usMouse) {
    zSouObj = (GNa::sSOUND *)sound;
    zSound.InitToData(zSouObj);
    zKeyOK = ok;
    zKeyDOWN = down;
    zKeyUP = up;
    zKeyLEFT = left;
    zKeyRIGHT = right;
    zUsMouse = usMouse;
    zSouObj1 = sound;
}
#endif
void GNu::cUI_SIM_DIALOG::SetEffect(GNg::cFRAME_RC *effect, short focus_flash_time, P<GNg::sIMAGE> focus_style, P<GNg::sIMAGE> arrow_style, bool affect_color) {
    cSIM_DIALOG::SetEffect(effect, focus_flash_time, focus_style, arrow_style, affect_color);
    zFocusMap1 = focus_style;
    zArrowMap1 = arrow_style;
}
void GNu::cUI_SIM_DIALOG::SetBackFrame(P<GNg::sIMAGE> &frame_img, P<GNg::sIMAGE> &caption_img, bool repeat, void (*draw_call)(short, short, short, short, unsigned int, unsigned short, GNg::sIMAGE *)) {
    cSIM_DIALOG::SetBackFrame(frame_img, caption_img, repeat, draw_call);
    zBackCaption1 = caption_img;
    if(zBackFrame) zBackFrame1 = frame_img;
    else zBackFrame1.Delete();
}
GNu::cUI_SIM_DIALOG::~cUI_SIM_DIALOG() {
    if(GT_FullSelectPathDlg == (cSIM_DIALOGEX *)this) GT_FullSelectPathDlg = 0;
    if(zObjsKeep.Us() == 1) {
        GN_Delete(zEffect);
        zObjsKeep.Delete();
    }
}
P<GNu::iUI_OBJECT> GNu::cUI_SIM_DIALOG::Clone() {
    P<cUI_SIM_DIALOG> ui_dlg(1);
    cUI_SIM_DIALOG *_ui_dlg = ui_dlg;
    zTextOut.Release();
    void *ts_items_bk = _ui_dlg->zTsItems;
    *_ui_dlg = *this;
    _ui_dlg->zTsItems = ts_items_bk;
    memset(&(_ui_dlg->zChild), 0, sizeof(_ui_dlg->zChild));
    _ui_dlg->zCaption = 0;
    GN_StrcpyData(_ui_dlg->zCaption, zCaption);
    _ui_dlg->zFileName = 0;
    GN_StrcpyData(_ui_dlg->zFileName, zFileName);
    if(zButtonName) {
        _ui_dlg->zButtonName = new char[GT_DLG_BNAME_SIZE];
        memcpy(_ui_dlg->zButtonName, zButtonName, GT_DLG_BNAME_SIZE); //复制按钮名
    }
    _ui_dlg->zTmpRangeRect.Delete();
    _ui_dlg->zSetTool.Delete();
    _ui_dlg->zIsDetailSet = false;
    memset(&(_ui_dlg->zSound), 0, sizeof(_ui_dlg->zSound));
    if(zSouObj) _ui_dlg->zSound.InitToData(zSouObj);
    _ui_dlg->zValueChangeArg = (cSIM_DIALOGEX *)_ui_dlg;
    _ui_dlg->zFather = 0;
    //_ui_dlg->InitValueToData(&(_ui_dlg->aCaptextColor),				 " 标题颜色         ", 255, 0);
    //_ui_dlg->InitValueToData1(_ui_dlg, (uint8*)&(_ui_dlg->aSkipPos),	 " 锁定筐位置       ", 1,  0,  1, " 否| 是");
    //_ui_dlg->InitValueToData1(_ui_dlg, (uint8*)&(_ui_dlg->aValueLoop), " 值大小调整循环   ", 1,  0,  1, " 否| 是");
    _ui_dlg->zSetActiveMenu(_ui_dlg);
    return ui_dlg;
}
unsigned int GNu::cUI_SIM_DIALOG::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[1500];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = zTextOut.zDataSave(wpos, share_objs->image_list);
    GT_cUI_SHARE_DATA_W *sdw = (GT_cUI_SHARE_DATA_W *)share_objs;
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_frame_rc, (GNg::cFRAME *)zEffect);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_keep_obj, zObjsKeep);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zIcon);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zFocusMap1);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zArrowMap1);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zBackFrame1);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zBackCaption1);
    pointer_id = 0xffffffff;
#ifdef _GN_SOUND_H_
    if(zSouObj1) pointer_id = sdw->list_sound.add_no_eq2(zSouObj1);
#endif
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;

    memcpy(wpos, &zT_ef_x, 4);
    wpos += 4;
    memcpy(wpos, &zT_ef_y, 4);
    wpos += 4;
    memcpy(wpos, &zT_effect_rect, 16);
    wpos += 16;
    memcpy(wpos, &zTwidth, 2);
    wpos += 2;
    memcpy(wpos, &zTheight, 2);
    wpos += 2;
    memcpy(wpos, &zTi_compartment, 2);
    wpos += 2;
    memcpy(wpos, &zTj_compartment, 2);
    wpos += 2;
    memcpy(wpos, &zDlaOffsetX, 2);
    wpos += 2;
    memcpy(wpos, &zDlaOffsetY, 2);
    wpos += 2;
    memcpy(wpos, &zSetPathType, 2);
    wpos += 2;
    memcpy(wpos, &zKeyOK, 2);
    wpos += 2;
    memcpy(wpos, &zKeyDOWN, 2);
    wpos += 2;
    memcpy(wpos, &zKeyUP, 2);
    wpos += 2;
    memcpy(wpos, &zKeyLEFT, 2);
    wpos += 2;
    memcpy(wpos, &zKeyRIGHT, 2);
    wpos += 2;
    memcpy(wpos, &zUsMouse, 1);
    wpos += 1;
    memcpy(wpos, &zState, 1);
    wpos += 1;
    memcpy(wpos, &zType, 1);
    wpos += 1;
    memcpy(wpos, &zBackFrameRepeat, 1);
    wpos += 1;
    memcpy(wpos, &zIsDetailSet, 1);
    wpos += 1;
    memcpy(wpos, &aBackBlurLv, 1);
    wpos += 1;
    memcpy(wpos, &aAffectColor, 1);
    wpos += 1;
    memcpy(wpos, &aValueLoop, 1);
    wpos += 1;
    memcpy(wpos, &aSkipPos, 1);
    wpos += 1;
    memcpy(wpos, &aCaptextColor, 4);
    wpos += 4;
    memcpy(wpos, &aTextColor, 4);
    wpos += 4;
    memcpy(wpos, &aBackColor, 4);
    wpos += 4;
    memcpy(wpos, &aLineColor, 4);
    wpos += 4;
    memcpy(wpos, &aFocusColor, 4);
    wpos += 4;
    memcpy(wpos, &aInputBackColor, 4);
    wpos += 4;
    memcpy(wpos, &aInputBackBlend, 2);
    wpos += 2;
    memcpy(wpos, &aTextBlend, 2);
    wpos += 2;
    memcpy(wpos, &aBackBlend, 2);
    wpos += 2;
    memcpy(wpos, &aLineBlend, 2);
    wpos += 2;
    memcpy(wpos, &aFocusBlend, 2);
    wpos += 2;
    memcpy(wpos, &aLineWidth, 2);
    wpos += 2;
    memcpy(wpos, &aCaptextOfsY, 2);
    wpos += 2;
    memcpy(wpos, &zFocusFlash, 2);
    wpos += 2;

    unsigned short len = zButtonName ? GT_DLG_BNAME_SIZE : 0;
    memcpy(wpos, &len, 2);
    wpos += 2;
    if(len) {
        memcpy(wpos, zButtonName, len);
        wpos += len;
    }

    len = zFileName ? strlen(zFileName) + 1 : 0;
    memcpy(wpos, &len, 2);
    wpos += 2;
    if(len) {
        memcpy(wpos, zFileName, len);
        wpos += len;
    }

    len = zCaption ? strlen(zCaption) + 1 : 0;
    memcpy(wpos, &len, 2);
    wpos += 2;
    if(len) {
        memcpy(wpos, zCaption, len);
        wpos += len;
    }
    return wpos - data_out;
}
unsigned int GNu::cUI_SIM_DIALOG::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    rpos = zTextOut.zDataLoad(rpos, share_objs->image_refs);
    GT_cUI_SHARE_DATA_R *sdr = (GT_cUI_SHARE_DATA_R *)share_objs;
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zEffect = sdr->pframes_rc[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zObjsKeep = sdr->pkeep_objs[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zIcon = sdr->pimages[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zFocusMap1 = sdr->pimages[pointer_id];
    zFocusMap = zFocusMap1;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zArrowMap1 = sdr->pimages[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zBackFrame1 = sdr->pimages[pointer_id];
    zBackFrame = zBackFrame1;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zBackCaption1 = sdr->pimages[pointer_id];
    zBackCaption = zBackCaption1;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
#ifdef _GN_SOUND_H_
    if(pointer_id != 0xffffffff) {
        zSouObj1 = sdr->psounds[pointer_id];
        zSouObj = zSouObj1;
        zSound.InitToData(zSouObj);
    }
#endif
    eSET_PATH_TYPE set_path_type;
    memcpy(&zT_ef_x, rpos, 4);
    rpos += 4;
    memcpy(&zT_ef_y, rpos, 4);
    rpos += 4;
    memcpy(&zT_effect_rect, rpos, 16);
    rpos += 16;
    memcpy(&zTwidth, rpos, 2);
    rpos += 2;
    memcpy(&zTheight, rpos, 2);
    rpos += 2;
    memcpy(&zTi_compartment, rpos, 2);
    rpos += 2;
    memcpy(&zTj_compartment, rpos, 2);
    rpos += 2;
    memcpy(&zDlaOffsetX, rpos, 2);
    rpos += 2;
    memcpy(&zDlaOffsetY, rpos, 2);
    rpos += 2;
    memcpy(&set_path_type, rpos, 2);
    rpos += 2;
    memcpy(&zKeyOK, rpos, 2);
    rpos += 2;
    memcpy(&zKeyDOWN, rpos, 2);
    rpos += 2;
    memcpy(&zKeyUP, rpos, 2);
    rpos += 2;
    memcpy(&zKeyLEFT, rpos, 2);
    rpos += 2;
    memcpy(&zKeyRIGHT, rpos, 2);
    rpos += 2;
    memcpy(&zUsMouse, rpos, 1);
    rpos += 1;
    memcpy(&zState, rpos, 1);
    rpos += 1;
    memcpy(&zType, rpos, 1);
    rpos += 1;
    memcpy(&zBackFrameRepeat, rpos, 1);
    rpos += 1;
    memcpy(&zIsDetailSet, rpos, 1);
    rpos += 1;
    memcpy(&aBackBlurLv, rpos, 1);
    rpos += 1;
    memcpy(&aAffectColor, rpos, 1);
    rpos += 1;
    memcpy(&aValueLoop, rpos, 1);
    rpos += 1;
    memcpy(&aSkipPos, rpos, 1);
    rpos += 1;
    memcpy(&aCaptextColor, rpos, 4);
    rpos += 4;
    memcpy(&aTextColor, rpos, 4);
    rpos += 4;
    memcpy(&aBackColor, rpos, 4);
    rpos += 4;
    memcpy(&aLineColor, rpos, 4);
    rpos += 4;
    memcpy(&aFocusColor, rpos, 4);
    rpos += 4;
    memcpy(&aInputBackColor, rpos, 4);
    rpos += 4;
    memcpy(&aInputBackBlend, rpos, 2);
    rpos += 2;
    memcpy(&aTextBlend, rpos, 2);
    rpos += 2;
    memcpy(&aBackBlend, rpos, 2);
    rpos += 2;
    memcpy(&aLineBlend, rpos, 2);
    rpos += 2;
    memcpy(&aFocusBlend, rpos, 2);
    rpos += 2;
    memcpy(&aLineWidth, rpos, 2);
    rpos += 2;
    memcpy(&aCaptextOfsY, rpos, 2);
    rpos += 2;
    memcpy(&zFocusFlash, rpos, 2);
    rpos += 2;
    unsigned short len;
#define GT_TMP_LOAD_SIM_DLG_NAME(NAME) \
    memcpy(&len, rpos, 2);             \
    rpos += 2;                         \
    GN_DeleteArray(NAME);              \
    if(len) {                          \
        NAME = new char[len];          \
        memcpy(NAME, rpos, len);       \
        rpos += len;                   \
    }
    GT_TMP_LOAD_SIM_DLG_NAME(zButtonName)
    GT_TMP_LOAD_SIM_DLG_NAME(zFileName)
    GT_TMP_LOAD_SIM_DLG_NAME(zCaption)
    if(!zArrowMap1) {
        zArrowMap1.New(1);
        zArrowMap = zArrowMap1;
        GT_TMP_MAKE_SIM_ARROW_ICON
    } else zArrowMap = zArrowMap1;

    zShowReady = zCaption && zTextOut.IsInited();
    if(zIsDetailSet) EnableDetailSetting(zIsDetailSet, aInputBackColor, aInputBackBlend, set_path_type);
    else zSetPathType = set_path_type;
    if(zState) Pop();
    return rpos - data;
}
#include "Player/GN_EdPlugin.h"
GN_API void (*GP_SettingMenuInsert)(GP_eSETTING_CLASS set_class, void *_set_obj, P<P_> &other_objs, GNu::sSIM_MENU *pmenu) = 0;
GN_API GNu::iSETTING_MENU *(*GP_BaseSetMenuCreate)(GP_eMENU_TYPE menu_type, void *args) = 0;

#endif //_GN_UIPOPDLG_H_
