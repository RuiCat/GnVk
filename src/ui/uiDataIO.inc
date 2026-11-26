// GN_mini UI 实现片段：各控件 zDataSave/zDataLoad/zSpecClone 序列化。
// 2026-11-26 自 GN_UiBase.cpp 第 2149-2669 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
/*************************************************************************************************************************************************/
uint8 *GNu::cSCROLL_CTRL::zDataSave(uint8 *wpos, GN::sLIST<P<GNg::sIMAGE>> &img_collect) {
    memcpy(wpos, &zOffsetSheep, 4);
    wpos += 4;
    memcpy(wpos, &zTextOffset, 4);
    wpos += 4;
    memcpy(wpos, &aFocusColor, 4);
    wpos += 4;
    memcpy(wpos, &aFocusBlend, 2);
    wpos += 2;
    memcpy(wpos, &zRepeatNum, 1);
    wpos += 1;
    memcpy(wpos, &(zFev.segment_time), 4);
    wpos += 4;
    unsigned int pointer_id;
    if(zOffsetArrow && zOffsetArrow != GN::RunRec.aGV_OffsetArrow) {
        pointer_id = img_collect.add_no_eq2(zOffsetArrow);
    } else pointer_id = 0xffffffff;
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;
    return wpos;
}
uint8 *GNu::cSCROLL_CTRL::zDataLoad(uint8 *rpos, P<P<GNg::sIMAGE>> &img_refs) {
    memcpy(&zOffsetSheep, rpos, 4);
    rpos += 4;
    memcpy(&zTextOffset, rpos, 4);
    rpos += 4;
    zTextOffsettg = zTextOffset;
    memcpy(&aFocusColor, rpos, 4);
    rpos += 4;
    memcpy(&aFocusBlend, rpos, 2);
    rpos += 2;
    memcpy(&zRepeatNum, rpos, 1);
    rpos += 1;
    memcpy(&(zFev.segment_time), rpos, 4);
    rpos += 4;
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id == 0xffffffff) zOffsetArrow = GN::RunRec.aGV_OffsetArrow;
    else zOffsetArrow = img_refs[pointer_id];
    GT_UiInitArrowAm();
    return rpos;
}
unsigned int GNu::cSCROLL_RECT::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    unsigned int gsize = cUI_GROUP::DataSave(share_objs, data_out);
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    else data_out += gsize;
    uint8 *wpos = data_out;
    wpos = cSCROLL_CTRL::zDataSave(wpos, *(share_objs->pimage_list));
    memcpy(wpos, &zMaxOffset, 4);
    wpos += 4;
    return (wpos - data_out) + gsize;
}
unsigned int GNu::cSCROLL_RECT::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    if(rpos[0] > 1) return 0;
    rpos += cUI_GROUP::DataLoad(share_objs, rpos);
    rpos = cSCROLL_CTRL::zDataLoad(rpos, *(share_objs->pimage_refs));
    memcpy(&zMaxOffset, rpos, 4);
    rpos += 4;
    return rpos - data;
}
void GNu::cSCROLL_LIST::zSpecClone() {
    cSCROLL_CTRL::zSpecClone();
    zDrawRangeData.Delete();
    P<GNg::cFRAME_LAUNCHER_FONT> fframes2(1);
    fframes2->PerfectCopy(*((GNg::cFRAME_LAUNCHER_FONT *)zFFrames));
    zFFrames = fframes2;
    zReArrange = true;
}
void GNu::cSELECT_LIST_ICON::zSpecClone() {
    cSCROLL_LIST::zSpecClone();
    zUsArg = this;
}
uint8 *GNu::cSCROLL_LIST::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs) {
    GT_TMP_SCROLL_LIST_DATA_SAVE(zDefaultFrameAct)
    memcpy(wpos, &aShowBG, 1);
    wpos += 1;
    memcpy(wpos, &aShowInFrame, 1);
    wpos += 1;
    return wpos;
}
uint8 *GNu::cSCROLL_LIST::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs) {
    GT_TMP_SCROLL_LIST_DATA_LOAD
    GNg::cFRAME *_fm_icon = zFrameIcon;
    if(!_fm_icon->zObjsKeep && _fm_icon->zFEvent) {
        zFev2.New(_fm_icon->zFEvent, _fm_icon->zEventcount);
        _fm_icon->zFEvent = zFev2;
        delete[](sdr->pframes[pointer_id]->zFEvent); /*这个通常不共享 复制数据后就可以删除*/
        sdr->pframes[pointer_id]->zFEvent = 0;
    }
    GT_TMP_LOAD_FONT_FRAME_TRANSFER
    memcpy(&aShowBG, rpos, 1);
    rpos += 1;
    memcpy(&aShowInFrame, rpos, 1);
    rpos += 1;
    return rpos;
}
uint8 *GNu::cSELECT_LIST_ICON::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs) {
    wpos = cSCROLL_LIST::zDataSave(wpos, share_objs);
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), zIcons);
    memcpy(wpos, &zMultSelect, 1);
    wpos += 1;
    memcpy(wpos, &aSelected, 4);
    wpos += 4;
    memcpy(wpos, &aSelectFrameStyle, 2);
    wpos += 2;
    memcpy(wpos, &aLineWidth, 2);
    wpos += 2;
    return wpos;
}
uint8 *GNu::cSELECT_LIST_ICON::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs) {
    rpos = cSCROLL_LIST::zDataLoad(rpos, share_objs);
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zIcons = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&zMultSelect, rpos, 1);
    rpos += 1;
    memcpy(&aSelected, rpos, 4);
    rpos += 4;
    zSelectedLast = aSelected;
    memcpy(&aSelectFrameStyle, rpos, 2);
    rpos += 2;
    memcpy(&aLineWidth, rpos, 2);
    rpos += 2;
    return rpos;
}
void GNu::cSELECT_LIST_TEXT::zSpecClone() {
    cSELECT_LIST_ICON::zSpecClone();
}
uint8 *GNu::cSELECT_LIST_TEXT::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs) {
    wpos = cSELECT_LIST_ICON::zDataSave(wpos, share_objs);
    memcpy(wpos, &aTextColor, 4);
    wpos += 4;
    memcpy(wpos, &aBackColor, 4);
    wpos += 4;
    memcpy(wpos, &aBackStyle, 2);
    wpos += 2;
    memcpy(wpos, &aAffectColor, 1);
    wpos += 1;
    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_WRITE_INDEX(share_objs->text_style_list, zTextOut)
    GT_TMP_OBJECT_WRITE_INDEX(((GT_cUI_SHARE_DATA_W *)share_objs)->list_texts, zLabels);
    return wpos;
}
uint8 *GNu::cSELECT_LIST_TEXT::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs) {
    rpos = cSELECT_LIST_ICON::zDataLoad(rpos, share_objs);
    memcpy(&aTextColor, rpos, 4);
    rpos += 4;
    memcpy(&aBackColor, rpos, 4);
    rpos += 4;
    memcpy(&aBackStyle, rpos, 2);
    rpos += 2;
    memcpy(&aAffectColor, rpos, 1);
    rpos += 1;
    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_READ_INDEX(share_objs->text_style_refs, zTextOut)
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zLabels = ((GT_cUI_SHARE_DATA_R *)share_objs)->ptexts_s[pointer_id];
    return rpos;
}
unsigned int GNu::cSELECT_LIST_ICON::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    GT_TMP_PACK_DATA_SAVE(1160, cSELECT_LIST_ICON)
    return wpos - data_out;
}
unsigned int GNu::cSELECT_LIST_ICON::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    GT_TMP_PACK_DATA_LOAD(cSELECT_LIST_ICON)
    if(zIcons) {
        zZoningNum = zIcons->texnum;
        zCheckResetZoningCount(GNg::cFRAME_LAUNCHER_FONT::aPicIndex, zFFrames);
        cSCROLL_LIST::SetZoningDrawCall(GT_SelectListIconDrawCall, this);
    } else {
        zZoningNum = 0;
        zFFrames->zMax_num = 0;
    }
    return rpos - data;
}
unsigned int GNu::cSELECT_LIST_TEXT::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    GT_TMP_PACK_DATA_SAVE(1200, cSELECT_LIST_TEXT)
    return wpos - data_out;
}
unsigned int GNu::cSELECT_LIST_TEXT::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    GT_TMP_PACK_DATA_LOAD(cSELECT_LIST_TEXT)
    if(zLabels) {
        zZoningNum = zLabels.Num();
        GT_TMP_SCROLL_LIST_ZONING_RESET
        cSCROLL_LIST::SetZoningDrawCall(GT_SelectListTextDrawCall, this);
        zArrowAm.aFrame_ts.sx = (float)zTextOut->zParame.height / (float)zOffsetArrow->width;
    } else {
        zZoningNum = 0;
        zFFrames->zMax_num = 0;
    }
    return rpos - data;
}
uint8 *GNu::cSELECT_CALL::zDataSave(uint8 *wpos) {
    memcpy(wpos, &aSelect, 2);
    wpos += 2;
    unsigned short num = zHots.Num();
    memcpy(wpos, &num, 2);
    wpos += 2;
    return wpos;
}
uint8 *GNu::cSELECT_CALL::zDataLoad(uint8 *rpos) {
    memcpy(&aSelect, rpos, 2);
    rpos += 2;
    unsigned short num;
    memcpy(&num, rpos, 2);
    rpos += 2;
    if(num) Init(num);
    return rpos;
}
uint8 *GT_sUI_STYLE_BASE::zDataSave(uint8 *wpos) {
    memcpy(wpos, &aAmSheep, 4);
    wpos += 4;
    memcpy(wpos, &aFocusColor, 4);
    wpos += 4;
    memcpy(wpos, &aStride, 2);
    wpos += 2;
    return wpos;
}
uint8 *GT_sUI_STYLE_BASE::zDataLoad(uint8 *rpos) {
    memcpy(&aAmSheep, rpos, 4);
    rpos += 4;
    memcpy(&aFocusColor, rpos, 4);
    rpos += 4;
    memcpy(&aStride, rpos, 2);
    rpos += 2;
    return rpos;
}
unsigned int GNu::cUI_PAGE_BAR::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[360];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = GT_sUI_STYLE_BASE::zDataSave(wpos);
    wpos = cSELECT_CALL::zDataSave(wpos);
    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_WRITE_INDEX(share_objs->text_style_list, zTextOut)
    GT_TMP_OBJECT_WRITE_INDEX(((GT_cUI_SHARE_DATA_W *)share_objs)->list_texts, zNames);
    memcpy(wpos, &zL, 2);
    wpos += 2;
    memcpy(wpos, &zH, 2);
    wpos += 2;
    memcpy(wpos, &aAutoLayerObjShow, 1);
    wpos += 1;
    memcpy(wpos, &aFontLineW, 1);
    wpos += 1;
    memcpy(wpos, &aBackColor, 4);
    wpos += 4;
    memcpy(wpos, &aLineColor, 4);
    wpos += 4;
    return wpos - data_out;
}
unsigned int GNu::cUI_PAGE_BAR::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
#define GT_TMP_UI_PAGE_BAR_DATALOAD_           \
    uint8 *rpos = (uint8 *)data;               \
    uint8 version = *rpos;                     \
    ++rpos;                                    \
    if(version > 1) return 0; /*版本*/         \
    rpos = zNearAreaDataLoad(rpos);            \
    rpos = GT_sUI_STYLE_BASE::zDataLoad(rpos); \
    rpos = cSELECT_CALL::zDataLoad(rpos);

    GT_TMP_UI_PAGE_BAR_DATALOAD_
    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_READ_INDEX(share_objs->text_style_refs, zTextOut)
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zNames = ((GT_cUI_SHARE_DATA_R *)share_objs)->ptexts_s[pointer_id];
    memcpy(&zL, rpos, 2);
    rpos += 2;
    memcpy(&zH, rpos, 2);
    rpos += 2;
    memcpy(&aAutoLayerObjShow, rpos, 1);
    rpos += 1;
    memcpy(&aFontLineW, rpos, 1);
    rpos += 1;
    memcpy(&aBackColor, rpos, 4);
    rpos += 4;
    memcpy(&aLineColor, rpos, 4);
    rpos += 4;
    return rpos - data;
}
unsigned int GNu::cUI_SELECT_BUTTON::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[320];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = GT_sUI_STYLE_BASE::zDataSave(wpos);
    wpos = cSELECT_CALL::zDataSave(wpos);
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), zImg);
    memcpy(wpos, &aIsErectArrange, 1);
    wpos += 1;
    return wpos - data_out;
}
unsigned int GNu::cUI_SELECT_BUTTON::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    GT_TMP_UI_PAGE_BAR_DATALOAD_
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zImg = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&aIsErectArrange, rpos, 1);
    rpos += 1;
    return rpos - data;
}
unsigned int GNu::cPALETTE::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[8192];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    memcpy(wpos, &zOffsetSheep, 4);
    wpos += 4;
    memcpy(wpos, &zColor, 4);
    wpos += 4;
    memcpy(wpos, &zColorArrow, 4);
    wpos += 4;
    memcpy(wpos, &aAlphaSet, 1);
    wpos += 1;
    memcpy(wpos, &aArrowShadow, 1);
    wpos += 1;
    memcpy(wpos, &aAlphaBgShow, 1);
    wpos += 1;
    memcpy(wpos, &aColorTablePosType, 1);
    wpos += 1;
    unsigned int csize = aTable.Size();
    *((unsigned int *)wpos) = csize;
    wpos += 4;
    if(csize) {
        memcpy(wpos, (GNg::sCOLOR *)aTable, csize);
        wpos += csize;
    }
    return wpos - data_out;
}
unsigned int GNu::cPALETTE::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    memcpy(&zOffsetSheep, rpos, 4);
    rpos += 4;
    memcpy((void *)&zColor, rpos, 4); //sCOLOR 是 4 字节 POD 布局的 union
    rpos += 4;
    memcpy((void *)&zColorArrow, rpos, 4);
    rpos += 4;
    memcpy(&aAlphaSet, rpos, 1);
    rpos += 1;
    memcpy(&aArrowShadow, rpos, 1);
    rpos += 1;
    memcpy(&aAlphaBgShow, rpos, 1);
    rpos += 1;
    memcpy(&aColorTablePosType, rpos, 1);
    rpos += 1;
    unsigned int csize;
    memcpy(&csize, rpos, 4);
    rpos += 4;
    if(csize) {
        aTable.New(csize >> 2);
        memcpy((void *)(GNg::sCOLOR *)aTable, rpos, csize);
        rpos += csize;
    }
    GT_TMP_PALETTE_INIT_TO_DATA(zColor)
    return rpos - data;
}
unsigned int GNu::cSLIDER::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    unsigned int size = cUI_IMAGE::DataSave(share_objs, data_out);
    if(data_out) {
        data_out += size;
        memcpy(data_out, &aAmSheep, 4);
        data_out += 4;
        memcpy(data_out, &aFocusColor, 4);
        data_out += 4;
        memcpy(data_out, &aPressColor, 4);
        data_out += 4;
        memcpy(data_out, &aIconColor, 4);
        data_out += 4;
        memcpy(data_out, &aIconBlend, 2);
        data_out += 2;
        memcpy(data_out, &aIconW, 2);
        data_out += 2;
        memcpy(data_out, &aIconH, 2);
        data_out += 2;
        memcpy(data_out, &zOfs, 2);
        data_out += 2;
    }
    return size + 24;
}
unsigned int GNu::cSLIDER::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    unsigned int size = cUI_IMAGE::DataLoad(share_objs, data);
    if(!size) return 0;
    uint8 *rpos = (uint8 *)data + size;
    memcpy(&aAmSheep, rpos, 4);
    rpos += 4;
    memcpy(&aFocusColor, rpos, 4);
    rpos += 4;
    memcpy(&aPressColor, rpos, 4);
    rpos += 4;
    memcpy(&aIconColor, rpos, 4);
    rpos += 4;
    memcpy(&aIconBlend, rpos, 2);
    rpos += 2;
    memcpy(&aIconW, rpos, 2);
    rpos += 2;
    memcpy(&aIconH, rpos, 2);
    rpos += 2;
    memcpy(&zOfs, rpos, 2);
    rpos += 2;
    return size + 24;
}
unsigned int GNu::cBUTTON_TOUCH::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    unsigned int size = cUI_IMAGE::DataSave(share_objs, data_out);
    if(data_out) {
        data_out[size] = zTid;
        data_out[size + 1] = aTouchZ;
        data_out[size + 2] = aMappingKey;
    }
    return size + 3;
}
unsigned int GNu::cBUTTON_TOUCH::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    unsigned int size = cUI_IMAGE::DataLoad(share_objs, data);
    if(!size) return 0;
    zTid = data[size];
    aTouchZ = data[size + 1];
    aMappingKey = data[size + 2];
    return size + 3;
}
unsigned int GNu::cJOYSTICK::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    *((unsigned int *)wpos) = aType;
    wpos += 4;
    *((short *)wpos) = zRngL;
    wpos += 2;
    *((short *)wpos) = zRngT;
    wpos += 2;
    *((short *)wpos) = zRngR;
    wpos += 2;
    *((short *)wpos) = zRngB;
    wpos += 2;
    memcpy(wpos, &zVpUV, 8);
    wpos += 8;
    memcpy(wpos, &zVectorScl, 4);
    wpos += 4;
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), aBackDiskImg);
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), aArrowImg);
    memcpy(wpos, &aBackDiskColor, 4);
    wpos += 4;
    memcpy(wpos, &aArrowColor, 4);
    wpos += 4;
    memcpy(wpos, &aVectorMaxLen, 4);
    wpos += 4;
    memcpy(wpos, &aVectorMappingRef, 4);
    wpos += 4;
    memcpy(wpos, &aPressMappingRef, 4);
    wpos += 4;
    memcpy(wpos, &aVecMappingKeyType, 1);
    wpos += 1;
    memcpy(wpos, &aPressMappingKey, 1);
    wpos += 1;
    return wpos - data_out;
}
unsigned int GNu::cJOYSTICK::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; /*版本*/
    aType = *((unsigned int *)rpos);
    rpos += 4;
    zRngL = *((short *)rpos);
    rpos += 2;
    zRngT = *((short *)rpos);
    rpos += 2;
    zRngR = *((short *)rpos);
    rpos += 2;
    zRngB = *((short *)rpos);
    rpos += 2;
    memcpy(&zVpUV, rpos, 8);
    rpos += 8;
    memcpy(&zVectorScl, rpos, 4);
    rpos += 4;
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) aBackDiskImg = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) aArrowImg = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&aBackDiskColor, rpos, 4);
    rpos += 4;
    memcpy(&aArrowColor, rpos, 4);
    rpos += 4;
    memcpy(&aVectorMaxLen, rpos, 4);
    rpos += 4;
    memcpy(&aVectorMappingRef, rpos, 4);
    rpos += 4;
    memcpy(&aPressMappingRef, rpos, 4);
    rpos += 4;
    memcpy(&aVecMappingKeyType, rpos, 1);
    rpos += 1;
    memcpy(&aPressMappingKey, rpos, 1);
    rpos += 1;
    if(zVpUV.x > 0.0f) zReCptNearRange();
    zInit(-1.0f);
    return rpos - data;
}
