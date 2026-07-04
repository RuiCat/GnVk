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
#include"GN_Def.h"
#ifdef _GN_UILISTGUI_H_
/***************************************************** cSCROLL_LIST_GUI *************************************************************************/
void GNu::cVALUE_EXTRACT_FILE::StatisticSize(cUI_GROUP *parent){
	cSCROLL_LIST_GUI *list_gui = (cSCROLL_LIST_GUI*)parent;
	//list_gui->aSubUiCallBack = SubUiUpdateSyncProc;
}
//以新区块ID来重加载图像(图像, 区块ID)
void GT_ImageReLoadForZID(GNg::sIMAGE *_img, unsigned int zid){
	char str_buf[384];
	strncpy(str_buf, _img->fileName, sizeof(str_buf)-1); str_buf[sizeof(str_buf)-1]=0;
	char *number_pos = str_buf + (strlen(str_buf) - 1);
	if(number_pos[0] == '_')sprintf(number_pos-8, "%08d_", zid);
	else					sprintf(number_pos-7, "%08d" , zid);
	// TODO: verify buffer size
	if(!_img->add_0000)strcpy(number_pos+1, ".png");
	number_pos = _img->fileName;  _img->fileName = 0;
	_img->InitToData(str_buf, 0, _img->cFormat, /*_img->hasMipmap*/false, 1);
	delete [] _img->fileName;
	_img->fileName = number_pos;
}
GN::sVALUE_LAYOUT2 *GT_SubUiUpdateSyncProc(GNu::eSUBUI_FUNC func, unsigned int zid, GNu::iUI_OBJECT *_ui, GN::sVALUE_LAYOUT2 *vdescs, GNu::cVALUE_EXTRACT_FILE *_vef){
	if(vdescs->stride > -1){
		uint8 *tg_ptr;
		switch(func){
		case GNu::eSUBUI_LOAD:
			tg_ptr = (uint8*)_ui + vdescs->pos_tg;
			GNf:Seek(_vef->fp, (int)(_vef->data_pos + vdescs->pos) + (int)vdescs->stride * zid, GNf::eSEEK_START);
			short str_size = (vdescs->size & GNu::eLAYOUT_TARGET_STR_SIZE_BIT) >> 8;
			if(vdescs->size > 0){
				if(str_size){
					char value_str[32];
					short _size = vdescs->size & 0xff, value_len;
					if(_size == GNu::eLAYOUT_VALUE_FLOAT){
						float valuef;
						GNf::Read((char*)&valuef, 4, 1, _vef->fp);
						value_len = sprintf(value_str, "%f", valuef);
					}else{
						int valuei = 0;
						GNf::Read((char*)&valuei, _size, 1, _vef->fp);
						value_len = sprintf(value_str, "%d", valuei);
					}
					char **ppstr = (char**)tg_ptr;
					char *pstr = *ppstr, *pstr2 = new char[value_len+str_size];
					// TODO: verify buffer size
					strncpy(pstr2, value_str, value_len); strcpy(pstr2+value_len, pstr + (strlen(pstr)-(str_size-1)));
					delete [] pstr;
					*ppstr = pstr2;
				
				}else GNf::Read((char*)tg_ptr, vdescs->size, 1, _vef->fp);
			}else{
				switch(vdescs->size){
				case GNu::eLAYOUT_VALUE_MIN_STR:
				case GNu::eLAYOUT_VALUE_MIN_PSTR:{
					char str_buf[512];
					if(vdescs->size == GNu::eLAYOUT_VALUE_MIN_STR){
						if(_ui->aType == GNg::eOBJ_FRAME_LAUNCHER_FONT){
							GNu::cUI_FRAME_LAUNCHER_FONT *_ffm = (GNu::cUI_FRAME_LAUNCHER_FONT*)_ui;
							if(_ffm->zIsFontMap){
								GNf::Gets(str_buf, 512, _vef->fp);
								_ffm->SetText(str_buf,0);
							}else{
								GNf::Read(str_buf, 1, 256, _vef->fp);
								char *pstr = str_buf, *pend = str_buf + 256;
								while(*pstr >= 0 && pstr < pend)++pstr;
								_ffm->SetText(str_buf, pstr - str_buf);
							}
						}else{
							GNf::Gets(str_buf, 512, _vef->fp);
							short value_len = strlen(str_buf);
							int str_size2 = value_len + str_size;
							char **ppstr = (char**)tg_ptr;
							char *pstr = *ppstr, *pstr2 = new char[str_size2?str_size2:1];
							if(str_size){
								// TODO: verify buffer size
								strncpy(pstr2, str_buf, value_len); strcpy(pstr2+value_len, pstr + (strlen(pstr)-(str_size-1)));
							// TODO: verify buffer size
							}else strcpy(pstr2 , str_buf);
							delete [] pstr;
							*ppstr = pstr2;
						}
					}else{
						GNf::Gets(str_buf, 512, _vef->fp);
						if(str_size){
							short value_len = strlen(str_buf);
							char *pstr = *((P<char>*)tg_ptr);
							P<char> pstr2(value_len+str_size);
							// TODO: verify buffer size
							strncpy(pstr2, str_buf, value_len); strcpy(pstr2+value_len, pstr + (strlen(pstr)-(str_size-1)));
							*((P<char>*)tg_ptr) = pstr2;
						}else (*((P<char>*)tg_ptr))(str_buf,0);
					}
					}break;
				case GNu::eLAYOUT_VALUE_PIMG0000:{
					P<GNg::sIMAGE> &p_img = *((P<GNg::sIMAGE>*)tg_ptr);
					GNg::sIMAGE *_pimg = p_img;
					if(_pimg){
						if(p_img.Us() != 1){
							char *file_name = new char[strlen(_pimg->fileName)+1];
							// TODO: verify buffer size
						strcpy(file_name, _pimg->fileName);
							p_img.New(1);
							p_img->fileName = file_name;
							p_img->add_0000 = _pimg->add_0000;
						}
						GT_ImageReLoadForZID(p_img, zid);
					}
					}break;
				case GNu::eLAYOUT_VALUE_IMG0000:
					GT_ImageReLoadForZID(*((GNg::sIMAGE**)tg_ptr), zid);
					break;
				case GNu::eLAYOUT_VALUE_FRAME_IMG0000:
					GT_ImageReLoadForZID(((GNg::cFRAME**)tg_ptr)[0]->zPlay_obj, zid);
					break;
				case GNu::eLAYOUT_VALUE_PFFRAME_IMG0000:
					GT_ImageReLoadForZID(((P<GNg::cFRAME_LAUNCHER_FONT>*)tg_ptr)[0]->zFontFrame->zPlay_obj, zid);
					break;
				}
			}
			break;


		}
	}
	++vdescs; 
	if(_ui->aType == GNg::eOBJ_GROUP){
		GN::sLIST< P<GNu::iUI_OBJECT> >::iterator itu = ((GNu::cUI_GROUP*)_ui)->aSubs.begin(), etu = ((GNu::cUI_GROUP*)_ui)->aSubs.end();
		for(; itu != etu; ++itu){
			vdescs = GT_SubUiUpdateSyncProc(func, zid, (*itu), vdescs, _vef);
		}
	}
	return vdescs;
}
GN_API void GNu::SubUiUpdateSyncProc(eSUBUI_FUNC func, unsigned int zid, P<iUI_OBJECT> &sub_ui, P<iUI_OBJECT> &extract_file){
	cVALUE_EXTRACT_FILE *_vef = CVP(cVALUE_EXTRACT_FILE)extract_file;
	GT_SubUiUpdateSyncProc(func, zid, sub_ui, _vef->extract_descs, _vef);
}
void GT_SubUiEmptyProc(GNu::eSUBUI_FUNC func, unsigned int zid, P<GNu::iUI_OBJECT> &sub_ui, P<GNu::iUI_OBJECT> &args){}

void GT_ScrollListGuiDrawCall(int _x, int _y, int _w, int _h, unsigned int color, unsigned int zid, unsigned int vid, bool has_focus, void *us_arg){
	GNu::cSCROLL_LIST_GUI *list_gui = (GNu::cSCROLL_LIST_GUI*)us_arg;
	list_gui->aCurDrawZID = zid;
	P<GNu::iUI_OBJECT> &uig = list_gui->zUiGroups[zid % list_gui->zUiGroups.Num()];
	GNu::cUI_GROUP uig0;
	uig->StatisticSize(&uig0);
	GNu::sUI_LEVEL uilv0;
	uilv0.x = _x; uilv0.y = _y; uilv0.w = _w; uilv0.h = _h; uilv0.c = color;
	uilv0.parent = 0; uilv0.node = 0;
	uig->UpdateRun(GT_RunTotalTimeApp - GT_LastRunTotalTimeApp, &uilv0);
	list_gui->aCurDrawZID = 0xffffffff;
	if(has_focus)list_gui->aSubUiCallBack(GNu::eSUBUI_BEOPT, zid, uig, list_gui->aSubUiArg);
}
GNu::cSCROLL_LIST_GUI::cSCROLL_LIST_GUI():/*zZoningTimes(0)*/aCurDrawZID(0xffffffff),aSubUiCallBack(GT_SubUiEmptyProc){
	aType = GNu::eUI_SCROLL_LIST_GUI; 
}
void GNu::cSCROLL_LIST_GUI::zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am){
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;	
	unsigned short stzid = _fframe->zDrawRngData->strid;
	if(stzid > zLastStartZID){
		P<iUI_OBJECT> *_uigs = zUiGroups;
		unsigned short zn = zUiGroups.Num();
		unsigned short zid1 = zLastStartZID + _fframe->zMax_num + zPreLoadNum, zid2 = stzid + _fframe->zMax_num + zPreLoadNum;
		if(zid1 >= zZoningNum)zid1 = zZoningNum - 1;
		if(zid2 >= zZoningNum)zid2 = zZoningNum - 1;
		for(; zid2 > zid1; --zid2)aSubUiCallBack(eSUBUI_LOAD, zid2, _uigs[zid2 % zn], aSubUiArg);
		zLastStartZID = stzid;
	}else if(stzid < zLastStartZID){
		P<iUI_OBJECT> *_uigs = zUiGroups;
		unsigned short zn = zUiGroups.Num();
		unsigned short zid1 = zLastStartZID > zPreLoadNum? zLastStartZID - zPreLoadNum : 0, zid2 = stzid > zPreLoadNum? stzid - zPreLoadNum : 0;
		for(; zid2 < zid1; ++zid2)aSubUiCallBack(eSUBUI_LOAD, zid2, _uigs[zid2 % zn], aSubUiArg);
		zLastStartZID = stzid;
	}
}
bool GNu::cSCROLL_LIST_GUI::InitStyleToData(unsigned short	pre_loan_num, short _x,short _y,short _w,short _h,
bool default_am ,short dw, short dh, unsigned int zoning_num, unsigned int  focus_color,unsigned short focus_blend){
//	this->cSCROLL_LIST_GUI::~cSCROLL_LIST_GUI();
	bool ret = cSCROLL_LIST::InitStyleToData(0, _x,_y,_w,_h, default_am,dw,dh,zoning_num, focus_color,focus_blend);
    cSCROLL_LIST::SetZoningDrawCall(GT_ScrollListGuiDrawCall, this);
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	if(_fframe->zActFrame == (GNg::cFRAME*)zDefaultFrameAct){
		_fframe->ControlActFromMouseAuto(false);
		_fframe->zActFrame = 0;
		_fframe->aColor_range = 127;
	}
	//zZoningTimes = new unsigned int[zoning_num];
	//memset(zZoningTimes, 0, zoning_num << 2);
	zPreLoadNum = pre_loan_num;
	return ret;
}
void GNu::cSCROLL_LIST_GUI::zInitSubUis(P<iUI_OBJECT> sub_ui){
	zLastStartZID = zFFrames->zDrawRngData->strid;
	unsigned int zi = 0, zn = zFFrames->zMax_num + (zPreLoadNum<<1);
	if(zn > zZoningNum)zn = zZoningNum;
	zUiGroups.New(zn);
	P<iUI_OBJECT> *_uigs = zUiGroups;
	iUI_OBJECT *_uig = sub_ui;
	for(;zi < zn; ++zi){
		_uigs[zi] = _uig->Clone();
		aSubUiCallBack(eSUBUI_LOAD, zi, _uigs[zi], aSubUiArg);
	}
}
void GNu::cSCROLL_LIST_GUI::SubUiExecCallBack(eSUBUI_FUNC func){
	P<iUI_OBJECT> *_uigs = zUiGroups;
	unsigned int zi = 0, zn = zUiGroups.Num();
	for(;zi < zn; ++zi)aSubUiCallBack(func, zi, _uigs[zi], aSubUiArg);
}
void GNu::cSCROLL_LIST_GUI::SetSubUiCallBack(P<iUI_OBJECT> sub_ui, P<iUI_OBJECT> us_arg, void (*callback)(eSUBUI_FUNC func, unsigned int zid, P<iUI_OBJECT> &sub_ui, P<iUI_OBJECT> &args)){
	aSubUiArg = us_arg;
	aSubUiCallBack = callback;
	if(us_arg)us_arg->StatisticSize((cUI_GROUP*)this);
	else if(callback == SubUiUpdateSyncProc)aSubUiCallBack = GT_SubUiEmptyProc;
	zInitSubUis(sub_ui);
}
void GNu::cSCROLL_LIST_GUI::SetZoningCount(unsigned int zoning_num){
	cSCROLL_LIST::SetZoningCount(zoning_num);
	if(zUiGroups){
		zInitSubUis(zUiGroups[0]);
		zLastStartZID = 0;
	}
}
void GNu::cSCROLL_LIST_GUI::SetPreLoadNum(unsigned short pre_loan_num){
	zPreLoadNum = pre_loan_num;
	if(zUiGroups)zInitSubUis(zUiGroups[0]);
}
P<GNu::iUI_OBJECT> GNu::cSCROLL_LIST_GUI::Clone(){
	P<cSCROLL_LIST_GUI> ui_list_gui(1);
	*ui_list_gui = *this;
	ui_list_gui->zSpecClone();
	return ui_list_gui;
}
unsigned int GNu::cVALUE_EXTRACT_FILE::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	if(!data_out)return 1+4+4+extract_descs.Size()+4;

	uint8 *wpos = data_out;
	*wpos = 1; ++wpos; /*版本*/
	*((unsigned int*)wpos) = aType;	wpos += 4;
	unsigned int pointer_id = extract_descs.Size();
	memcpy(wpos, &pointer_id, 4); wpos += 4;
	if(pointer_id){ 
		memcpy(wpos, (GN::sVALUE_LAYOUT2*)extract_descs, pointer_id); wpos += pointer_id;
	}
	if(fp && fp->type == GNf::eFILE_STD && filename){\
		pointer_id = share_objs->text_list.add_no_eq2( filename );\
	}else pointer_id = 0xffffffff; /*该值代表没引用指针*/\
    memcpy(wpos, &pointer_id, 4); wpos += 4;\
	return wpos - data_out;
}
unsigned int GNu::cVALUE_EXTRACT_FILE::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	uint8 *rpos = (uint8*)data;\
	uint8 version = *rpos; ++rpos; if(version > 1)return 0; /*版本*/\
	aType = *((unsigned int*)rpos); rpos += 4;
	unsigned int pointer_id;
	memcpy(&pointer_id, rpos, 4); rpos += 4;
	if(pointer_id){
		extract_descs.New(pointer_id / sizeof(GN::sVALUE_LAYOUT2));
		memcpy((GN::sVALUE_LAYOUT2*)extract_descs, rpos, pointer_id); rpos += pointer_id;
	}
	memcpy(&pointer_id, rpos, 4); rpos += 4;
	if(pointer_id != 0xffffffff){
		filename = share_objs->text_refs[pointer_id];
		fp = GNf::Open(filename, "rb");
		unsigned int sld_head[3]; //[0]文件标识 [1]嵌套UI区块个数 [2]数值提取描述数
		if(GNf::Read((char*)sld_head, 1, 12, fp) == 12 && !strcmp((char*)sld_head, "sld"))data_pos = 12 + sizeof(GN::sVALUE_LAYOUT2) * sld_head[2];
		else{
			GN_Log("加载UI数值提取文件 %s 失败!\n", (char*)filename);
			GNf::Close(fp); fp = 0;
			filename.Delete();
		}
	}
	return rpos - data;
}
void GNu::cSCROLL_LIST_GUI::zSpecClone(){
	cSCROLL_LIST::zSpecClone();
	zUsArg = this;
	if(zUiGroups)zInitSubUis(zUiGroups[0]);
}
uint8* GNu::cSCROLL_LIST_GUI::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs){
	wpos = cSCROLL_LIST::zDataSave(wpos, share_objs);
	memcpy(wpos, &zLastStartZID,	2);	  wpos += 2;
	memcpy(wpos, &zPreLoadNum,		2);	  wpos += 2;
	memcpy(wpos, &zZoningNum,		4);	  wpos += 4;
	unsigned int pointer_id;
	GT_TMP_OBJECT_WRITE_INDEX( ((GT_cUI_SHARE_DATA_W*)share_objs)->list_uobj , aSubUiArg );
	P<iUI_OBJECT> ug;
	if(zUiGroups)ug = zUiGroups[0];
	GT_TMP_OBJECT_WRITE_INDEX( ((GT_cUI_SHARE_DATA_W*)share_objs)->list_uobj , ug );
	return wpos;
}
uint8* GNu::cSCROLL_LIST_GUI::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs){
	rpos = cSCROLL_LIST::zDataLoad(rpos, share_objs);
	memcpy(&zLastStartZID,	rpos,	2);	  rpos += 2;
	memcpy(&zPreLoadNum,	rpos, 	2);	  rpos += 2;
	memcpy(&zZoningNum,		rpos, 	4);	  rpos += 4;
	if(zZoningNum){
		GT_TMP_SCROLL_LIST_ZONING_RESET
		cSCROLL_LIST::SetZoningDrawCall(GT_ScrollListGuiDrawCall, this);
	}else zFFrames->zMax_num = 0; 
	unsigned int pointer_id;
	memcpy(&pointer_id,    rpos, 4);	  rpos += 4;
	if(pointer_id != 0xffffffff)aSubUiArg = ((GT_cUI_SHARE_DATA_R*)share_objs)->puobjs[pointer_id];
	memcpy(&pointer_id,    rpos, 4);	  rpos += 4;
	if(pointer_id != 0xffffffff)zInitSubUis(((GT_cUI_SHARE_DATA_R*)share_objs)->puobjs[pointer_id]);
	return rpos;
}
unsigned int GNu::cSCROLL_LIST_GUI::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	GT_TMP_PACK_DATA_SAVE( 1148 , cSCROLL_LIST_GUI )
	return wpos - data_out;
}
unsigned int GNu::cSCROLL_LIST_GUI::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	GT_TMP_PACK_DATA_LOAD( cSCROLL_LIST_GUI )
	return rpos - data;
}
#endif //_GN_UILISTGUI_H_
