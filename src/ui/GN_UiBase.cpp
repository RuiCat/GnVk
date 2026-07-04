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
#ifdef _GN_UIBASE_H_
/************************************************************** cSCROLL_RECT *********************************************************************/
GNu::cSCROLL_CTRL::cSCROLL_CTRL():zOffsetSheep(0.1f),zRepeatNum(0),zIsMouseFocus(false){ zFev.InitColor(0,0,0x50ffffff,0xffffffff); }

void GNu::cSCROLL_CTRL::SetOffsetBorderEF(short focus_flash_time, P<GNg::sIMAGE> offset_arrow, int8 repeat_num){
    zFev.segment_time = (unsigned int)focus_flash_time;
	if(offset_arrow)zOffsetArrow = offset_arrow;
    zRepeatNum = repeat_num;
	GT_UiInitArrowAm();
}
void GNu::cSCROLL_RECT::zResetOffsetAndAm(){
	zTextOffset   = 0.0f;
	zTextOffsettg = 0.0f;
	zOffsetSheep  = 0.1f;
    GT_TMP_UI_OffsetArrowInit();
}
void GNu::cSCROLL_RECT::InitStyleToData(short _x, short _y, short _w, short _h, short total_h, unsigned int focus_color,unsigned short focus_blend){
	x = _x, y = _y, w = _w, h = _h; lw = _w; lh = _h;

   #define GT_TMP_SCROLL_RECT_INIT_STYLE \
	zMaxOffset   = (unsigned int)total_h;\
	aFocusColor  = focus_color;\
	aFocusBlend  = focus_blend;\
    zResetOffsetAndAm();
	GT_TMP_SCROLL_RECT_INIT_STYLE
}
void GT_CloneUiGroupAttrb(GNu::cUI_GROUP *_ug, /*const*/ GNu::cUI_GROUP *src);

void GNu::cSCROLL_RECT::InitStyleToData(const cUI_GROUP *ui_group, short total_h, unsigned int  focus_color, unsigned short focus_blend){
	GT_CloneUiGroupAttrb((cUI_GROUP*)this, (cUI_GROUP*)ui_group);
	GT_TMP_SCROLL_RECT_INIT_STYLE
}
unsigned int GNu::cSCROLL_RECT::UpdateRun(unsigned int updatatime){
	if((short)zMaxOffset <= h)return 1;
	int tx,ty,tw,th, mouse_x = GNg::ViewCur->mouseMX, mouse_y = GNg::ViewCur->mouseMY;
	unsigned int focus_color = zArrowAm.aLevel_color;

	bool ddraw_mode = !GN_Has3DAccelerate(); //滚动控制 ---------------------------------------------------------------------------------------------
	zIsMouseFocus = (short)mouse_x > x && (short)mouse_x < x+w && (short)mouse_y > y && (short)mouse_y < y+h;
	if(!GN::Mouse[GM_LEFT])zKeyOnDown = false;
	if(zIsMouseFocus && GN::Mouse1[GM_LEFT])zKeyOnDown = true;

	zArrowAm.aBlendmode = 0;
	short move_dist = 0;
	if(zKeyOnDown && GN::MouseM){ //按紧鼠标左键拖动
		 move_dist = (short)GNg::ViewCur->mouseMSY;
		 if(move_dist < -38 || move_dist >  38)move_dist <<= 1;
		 else zTextOffset += (zTextOffsettg - zTextOffset) * GN::TimeRatioL * 0.8f;

	}else if(zIsMouseFocus){
		if	   (GN::Mouse1[GM_TOP]	 )move_dist =   h >> 2 ;  //鼠标滚轮上
		else if(GN::Mouse1[GM_BOTTOM])move_dist = -(h >> 2);  //鼠标滚轮下
		else{ //鼠标移动到筐边沿的拖动处理
			tx = (int)x; ty = (int)y; tw = (int)w;  th = zOffsetArrow->width;
			zArrowAm.aFrame_ts.sy = (float)w / (float)zOffsetArrow->height;
            if(zRepeatNum)zArrowAm.aFrame_ts.sy /= (float)zRepeatNum;
			if(GNg::TestPointIntRect(mouse_x,mouse_y, tx,ty,tw,th)){
				if(ddraw_mode)
                    GNg::ImageDraw(0, tx,ty,tw,th, focus_color,0,aFocusBlend); //绘制拖动条
				else{
                    zArrowAm.aFrame_ts.x = (float)(tx+(tw>>1));
					zArrowAm.aFrame_ts.y = (float)ty;
					zArrowAm.aFrame_ts.r = 270.0f; /*模版代码:旋转一下拖动条*/
					zArrowAm.aBlendmode = aFocusBlend;
					zArrowAm.PlayFrame(updatatime);
				}
				move_dist = (short)((float)(h * 12 / 600) * GN::TimeRatioL);
			}
			ty = y + h - zOffsetArrow->width;

			if(!move_dist && GNg::TestPointIntRect(mouse_x,mouse_y, tx,ty,tw,th)){
			    if(ddraw_mode)
                    GNg::ImageDraw(0, tx,ty,tw,th, focus_color,0,aFocusBlend); //绘制拖动条
			    else{
				    zArrowAm.aFrame_ts.x = (float)(tx+(tw>>1));
				    zArrowAm.aFrame_ts.y = (float)(ty+th);
				    zArrowAm.aFrame_ts.r = 90.0f;  /*模版代码:旋转一下拖动条*/
				    zArrowAm.aBlendmode = aFocusBlend;
				    zArrowAm.PlayFrame(updatatime);
			    }
				move_dist = (short)((float)(h * 12 / 600) *-GN::TimeRatioL);
			}
		}
	}
	if(!ddraw_mode){
	    if(!zArrowAm.aBlendmode)zArrowAm.PlayFrame(updatatime); //如箭头没有被显示也运行更新一下 以便有动画颜色变化
	}
	if      (move_dist > 0){
		if(zTextOffsettg < 0.0f){
            if(zTextOffsettg + (float)move_dist > 0.0f)zTextOffsettg = 0.0f;
			else zTextOffsettg += (float)move_dist;
		}
	}else if(move_dist < 0){
		if((float)((short)zMaxOffset - h) > (-(zTextOffsettg + (float)move_dist)))zTextOffsettg += (float)move_dist;
		else zTextOffsettg = -((float)((short)zMaxOffset - h));
	}
    zTextOffset += (zTextOffsettg - zTextOffset) * zOffsetSheep * GN::TimeRatioL;
	return 1;
}
void GNu::cSCROLL_RECT::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	GNg::sDRAW_AREA2 rng, *_rng = (GNg::sDRAW_AREA2*)&(this->x);
	DrawRangeGet(rng, parent);
	bool not_ctrl = (RecvInputUiObject != this);
	short py = parent->y;
	parent->y += GetColOffset();
	if(aUsScissor){
		aUsScissor = false;
		GNg::ViewCur->ScissorRectSetup(rng.x, rng.y, rng.w, rng.h);
		cUI_GROUP::UpdateRun(updatetime, parent);
		GNg::ViewCur->ScissorRectRemove();
		aUsScissor = true;
	}else
		cUI_GROUP::UpdateRun(updatetime, parent);
	parent->y = py;
	
	GNg::sDRAW_AREA2 rng_bk = *_rng;
	*_rng = rng;  
	zArrowAm.aColor = GNg_MultiplyColorRatio(aFocusColor, parent->c);
	if(not_ctrl){
		GNg::ViewCur->mouseMX += 10000;
		GNu::cSCROLL_RECT::UpdateRun(updatetime);
		GNg::ViewCur->mouseMX -= 10000;
	}else
		GNu::cSCROLL_RECT::UpdateRun(updatetime);
	*_rng = rng_bk;
}
void GNu::cSCROLL_CTRL::zSpecClone(){
	if(zArrowAm.zFEvent)zArrowAm.zFEvent = &zFev;
}
P<GNu::iUI_OBJECT> GNu::cSCROLL_RECT::Clone(){
	P<GNu::cSCROLL_RECT> ui_srect(1);
	GNu::cSCROLL_RECT *_ui_srect = ui_srect;
	GT_CloneUiGroupAttrb(_ui_srect, this);
	*((cSCROLL_CTRL*)_ui_srect) = *((cSCROLL_CTRL*)this);
	_ui_srect->zSpecClone();
	_ui_srect->zMaxOffset = zMaxOffset;
	return ui_srect;
}
void  GNu::cSCROLL_RECT::SetPos(short _x,short _y,short _w,short _h){
    x = _x, y = _y, w = _w, h = _h; lw = _w; lh = _h;
}
void GNu::cSCROLL_RECT::GetPos(short *_x,short *_y,short *_w,short *_h){
    *_x = x, *_y = y, *_w = w, *_h = h;
}
void GNu::cSCROLL_RECT::SetOffsetRatio(float value){
	GN_ValueClamp(value , 0.0f, 1.0f);
    zTextOffsettg = (-((float)((short)zMaxOffset - h))) * value;
}
void GNu::cSCROLL_RECT::StatisticFitMaxOffset(){
	GN::sLIST< P<iUI_OBJECT> >::iterator itu = aSubs.begin(), etu = aSubs.end();
	w = lw; h = lh; 
	for(; itu != etu; ++itu)(*itu)->StatisticSize(this);
	zMaxOffset = h;
}
/***************************************************** cSCROLL_LIST **************************************************************/
P<GNg::cFRAME> GNu::cSCROLL_LIST::zDefaultFrameAct;
GNg::sIMAGE   *GNu::cSCROLL_LIST::zInternalImg = 0;
unsigned short GNu::cSCROLL_LIST::zCreateTotalNum = 0;

GNg::cFRAME *GNu::cSCROLL_LIST::CheckInitDefaultFrameAct(){
	if(zDefaultFrameAct)return zDefaultFrameAct;
	GNg::sAM_EVENT *fev = new GNg::sAM_EVENT[1];
	fev->Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,0,120,1.0f,1.12f);
	zDefaultFrameAct.New(1);
	zDefaultFrameAct->Init(0,1,fev GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
	return zDefaultFrameAct;
}
GNu::cSCROLL_LIST::cSCROLL_LIST():aShowBG(false),aShowInFrame(true),zDrawCall(0),zIsErectArrange(true),zUsArg(0){
	aType = GNu::eUI_SCROLL_LIST; c = 0xffffffff /*该类忽略该值*/;
	++zCreateTotalNum; /*创建增加计数*/
}
GNu::cSCROLL_LIST::~cSCROLL_LIST(){
    --zCreateTotalNum;
	if(!zCreateTotalNum){ //已没其他对象存活就删除共享内容
		if(zDefaultFrameAct){
		    GN_DeleteArray(zDefaultFrameAct->zFEvent);
		    zDefaultFrameAct.Delete();
	}	}
}
void GNu::cSCROLL_LIST::SetZoningCount(unsigned int zoning_num){
	zZoningNum = zoning_num;
	RePos();
}
void GNu::cSCROLL_LIST::SetZoningDrawCall(void (*draw_func)(int, int, int, int, unsigned int, unsigned int, unsigned int, bool, void*), void *usarg){
	zUsArg = usarg;
	zDrawCall = (void(*)(int,int,int,int,unsigned int ,unsigned int ,unsigned int ,bool))draw_func;
	if(zFFrames->zMax_num != (unsigned int )zDrawRangeData.Num())zDrawRangeData.New((unsigned int)zFFrames->zMax_num);
}
bool GNu::cSCROLL_LIST::InitStyleToData(void (*draw_func)(int, int , int, int, unsigned int, unsigned int, unsigned int, bool),
short _x,short _y,short _w,short _h,
bool default_am ,short dw, short dh, unsigned int zoning_num,
unsigned int  focus_color,unsigned short focus_blend)
{
    zDrawCall = draw_func;
	zZoningNum    = zoning_num;
   #ifndef GN_USING_ADDITIONAL_LIB //GN_ADD_CONTEN_PRIVATE
    if(!zoning_num)return false;
   #endif
	aFocusColor   = focus_color;
	aFocusBlend   = focus_blend;
    zReArrange    = false;
	zOffsetSheep = 0.1f;
    zTextOffset  = 0.0f;
	zTextOffsettg = 0.0f;
	if(!dw)dw = _w;
	if(!dh)dh = _h >> 1;
    GT_TMP_UI_OffsetArrowInit();

	uint8 locbuf[2048];
	GN::sTBUF_INT tbuf(locbuf, 2048);
	char *pic_id;
	if(zInternalImg)pic_id = GNg::cFRAME_LAUNCHER_FONT::aPicIndex;
	else{
	    pic_id = (char*)tbuf.GetSpace(zZoningNum);
	    memset(pic_id, 0, zZoningNum);
	}
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	if(_fframe){
		_fframe->aGenRngX = (float)_x;
		_fframe->aGenRngY = (float)_y;
		_fframe->aGenRngW = (float)_w;
		_fframe->aGenRngH = (float)_h;
		GNg::sARRANGE_FMT &ffmt = _fframe->zParame;
		ffmt.i_compartment += (dw - ffmt.width);
        ffmt.j_compartment += (dh - ffmt.height);
		ffmt.width  = dw;
		ffmt.height = dh;
		
		if(zInternalImg){
			GNg::cFRAME *ffam = _fframe->zFontFrame;
			ffam->zRcS.right  *= zInternalImg->uRange / ffam->zPlay_obj->uRange;
			ffam->zRcS.bottom *= zInternalImg->vRange / ffam->zPlay_obj->vRange;
			_fframe->zTu_w = ffam->zRcS.right;
		    _fframe->zTv_h = ffam->zRcS.bottom;
			if(ffam->zObjsKeep.Us() == 1 && ffam->zPlay_obj && ffam->zPlay_obj != &GNg::Image)delete (ffam->zPlay_obj);
		    ffam->zPlay_obj = zInternalImg;
		}
	}else{
		if(!zFrameIcon){
			zFrameIcon.New(1);
			if(default_am){
                zFev2.New(2);
				zFev2->InitColor(0,260,0x00ffffff,0xffffffff);
				zFev2[1].Init(GNg::eEVENT_SIZE_Y,0,260,0.01f,1.0f);
				  zFrameIcon->Init(zInternalImg,2,zFev2 GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
			}else zFrameIcon->Init(zInternalImg,0,    0 GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
		}
		if(default_am)CheckInitDefaultFrameAct();

		GNg::sRECT2 arc2d((float)_x, (float)_y, (float)(_x+_w), (float)(_y+_h));
		GNg::sARRANGE_FMT afmt(false,0.0f,0.0f, 1,dh>>4, dw,dh, zIsErectArrange?GNg::eENTER_LEFT_RIGHT:GNg::eENTER_TOP_BOTTOM, !zIsErectArrange, false);
		zFFrames.New(1);
		zFFrames->InitToData(zFrameIcon,pic_id,(unsigned short)zZoningNum, &arc2d, &afmt, 0.0f,100,default_am?1:0,127,0,127, zInternalImg?127:5, 0,0,0,default_am?zDefaultFrameAct.Begin():0,true);
		zFFrames->ControlActFromMouseAuto(default_am);
	}
	zCheckResetZoningCount(pic_id, _fframe);
	if(zDrawCall && zFFrames->zMax_num != (unsigned int )zDrawRangeData.Num())zDrawRangeData.New((unsigned int)zFFrames->zMax_num);
	x = _x; y = _y; w = _w; h = _h;
	return true;
}
void GNu::cSCROLL_LIST::zCheckResetZoningCount(char *pic_id, void *last_fframe){
	zLastStartPos = 0;
    unsigned int arng_num = (unsigned int)GetParalleShowCount(), set_count = zZoningNum;
	if(set_count / arng_num + 1 > 254)set_count = arng_num * 254; //检测并裁短一下SetText的范围 以防超出SetText最大256行的限制
    if(last_fframe || set_count != zZoningNum)zFFrames->SetText(pic_id, (unsigned short)set_count, 0,0);
}
void GNu::cSCROLL_LIST::Transpose(){
   #ifdef GN_DEBUG
	if(!zFFrames){ GN_Log("调用 cSCROLL_LIST::Transpose(...)失败! 还没初始化好!!\n"); }
   #endif
	GNg::sARRANGE_FMT &ffmt = zFFrames->zParame;
	ffmt.erect_print = !ffmt.erect_print;
	switch(ffmt.auto_enter){
	case GNg::eENTER_LEFT_RIGHT: ffmt.auto_enter = GNg::eENTER_TOP_BOTTOM; break;
	case GNg::eENTER_TOP_BOTTOM: ffmt.auto_enter = GNg::eENTER_LEFT_RIGHT; break;
	}
    zReArrange = true;
}
void GNu::cSCROLL_LIST::SetEffect(short focus_flash_time, P<GNg::cFRAME_LAUNCHER_FONT> effect, P<GNg::sIMAGE> focus_style, P<GNg::sIMAGE> offset_arrow,int8 repeat_num){
	GT_TMP_UI_SET_EFFECT;
}
unsigned int GNu::cSCROLL_LIST::UpdateRun(unsigned int updatatime, bool bg_eq, unsigned int dc){
	if(!zZoningNum){
	   #ifdef GN_DEBUG
		GN_Log("调用 cSCROLL_LIST::UpdateRun(...) 被停止! 请先调用它的初始化函数\n");
       #endif
		return 0;
	}
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sARRANGE_FMT *pfmt = &(_fframe->zParame);
	if(zReArrange){
		bool ret_am = GN_ValueCompareFloat(zTextOffset,zTextOffsettg,0.5f);
		GT_TMP_ScrollSetTextRangeBegin( zZoningNum )
		uint8 locbuf[2048];
		GN::sTBUF_INT tbuf(locbuf, 2048);
		char *pic_id;
		if(bg_eq){
            pic_id = (char*)tbuf.GetSpace(zZoningNum);
			memset(pic_id, 0, zZoningNum);
		}else{
            #ifdef GN_DEBUG
			if(zZoningNum > 128){ GN_Log("调用cSCROLL_LIST::UpdateRun(...) 不同的图索引号不能大于128个!\n"); }
            #endif
			pic_id = GNg::cFRAME_LAUNCHER_FONT::aPicIndex;
		}
		if(pfmt->erect_print)_fframe->SetText(pic_id + pid_start,set_count, _icon_offset,0, ret_am);
		else				 _fframe->SetText(pic_id + pid_start,set_count, 0,_icon_offset, ret_am);
		if(pid_start){
			GNg::sDRAW_RANGE *_arrd = _fframe->zDrawRngData;
			GNg::sDRAW_RANGE *arrd_end = _arrd + _fframe->zMax_num;
            unsigned short _pid_start = (unsigned short)pid_start;
			for(; _arrd < arrd_end; ++_arrd)_arrd->strid += _pid_start;
		}
		zOnReArranged(pid_start, zLastStartPos, ret_am);
        zLastStartPos = pid_start;
		zReArrange = false;
		if(zDrawCall && _fframe->zMax_num != zDrawRangeData.Num())zDrawRangeData.New(_fframe->zMax_num);
	}
	// 更新显示区划 ---------------------------------------------------------------
	unsigned short blmode = _fframe->zFontFrame->aBlendmode;
	if(!aShowBG)_fframe->zFontFrame->aBlendmode = 0;
	_fframe->zUsScissor = aShowInFrame;
	_fframe->zDrawRangeInfo = zDrawRangeData;
	unsigned int ret = _fframe->PlayFrame(updatatime, dc);
	_fframe->zDrawRangeInfo = 0;
    _fframe->zFontFrame->aBlendmode = blmode;
	GT_TMP_FFRAME_RANGE_GET;
	zIsMouseFocus = mouse_x > zx && mouse_x < zr && mouse_y > zy  && mouse_y < zb;
	if(!ret)return 0;

	// 处理绘制回调 -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	if(zDrawCall){
		if(aShowInFrame)GNg::ViewCur->ScissorRectSetup(zx,zy,zw,zh);
		GNg::sDRAW_RANGE *_text_data = _fframe->zDrawRngData;
		unsigned int *time_data = _fframe->zNowtime_s, cmp_time = _fframe->zSrcAmMode ? _fframe->zEnd_time : 0xffffffff;
		GNg::sDRAW_AREA *rng_datas = zDrawRangeData, *_rng_info;
		unsigned int zvi = 0, zvn = _fframe->zMax_num;
		if(zUsArg){
			void (*draw_callex)(int,int,int,int,unsigned int,unsigned int,unsigned int,bool,void*) = (void (*)(int,int,int,int,unsigned int,unsigned int,unsigned int,bool,void*))zDrawCall;

		   #define GT_TMP_SCROLL_RECT_DRAW_ZONING \
			for(; zvi < zvn; ++zvi){\
				if(time_data[zvi] < cmp_time){\
                    _rng_info = rng_datas + zvi;\
					tx = _rng_info->x, ty = _rng_info->y, tw = _rng_info->w, th = _rng_info->h;

			GT_TMP_SCROLL_RECT_DRAW_ZONING
			     draw_callex(tx,ty,tw,th, _rng_info->c, (unsigned int)_text_data[zvi].strid, zvi, (mouse_x >= tx && mouse_x <= tx+tw && mouse_y >= ty && mouse_y <= ty+th), zUsArg);
			}   }
		}else{
			GT_TMP_SCROLL_RECT_DRAW_ZONING
			       zDrawCall(tx,ty,tw,th, _rng_info->c, (unsigned int)_text_data[zvi].strid, zvi, (mouse_x >= tx && mouse_x <= tx+tw && mouse_y >= ty && mouse_y <= ty+th) );
			}   }
		}
		if(aShowInFrame)GNg::ViewCur->ScissorRectRemove();
	}
	unsigned int focus_color = zArrowAm.aLevel_color;

	// 处理滚屏 ---------------------------------------------------------------------------------------------------------
	const unsigned int add_row = ((zw >= pfmt->i_compartment) ? ((zZoningNum % (zw / pfmt->i_compartment)) ? 1 : 0) : 0);
	GT_TMP_UI_SCROLL_VIEW( 0 ,  add_row , zZoningNum , if(zOnAddScroolOffset(move_dist));else );
	return zReArrange? 1 : ret;
}
void GNu::cSCROLL_LIST::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	GT_TMP_SCROLL_LIST_UPDATE( UpdateRun(updatetime, true, parent->c) ,   ) 
}
P<GNu::iUI_OBJECT> GNu::cSCROLL_LIST::Clone(){
	P<cSCROLL_LIST> ui_scr_list(1);
	*ui_scr_list = *this;
	ui_scr_list->zSpecClone();
	return ui_scr_list;
}
int GNu::cSCROLL_LIST::GetTotalOffset(){
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sARRANGE_FMT &ffmt = _fframe->zParame;
	int col_num = (_fframe->zRowNum > 1) ? (zZoningNum / _fframe->zRowSIDs[1]) : 1;
    if(ffmt.erect_print && ffmt.overturn_print){
							return col_num *   ffmt.i_compartment;
	}else{
	    if(ffmt.erect_print)return col_num * (-ffmt.i_compartment);
		else                return col_num * (-ffmt.j_compartment);
	}
}
void GNu::cSCROLL_LIST::SetOffsetRatio(float value, float flow_r){
    if(!zZoningNum)return;
	GN_ValueClamp(value, 0.0f, 1.0f);
	zTextOffsettg = (float)GetTotalOffset() * value;
	if(flow_r)zTextOffset += (zTextOffsettg - zTextOffset) * flow_r;
}
void GNu::cSCROLL_LIST::RePos(){
    zTextOffset = 0.0f;
	zTextOffsettg = 0.0f;
	zReArrange = true;
}
void GNu::cSCROLL_LIST::OkHide(){
    zFFrames->ToHide();
}
GT_TMP_UI_GET_SET_POS_AND_OTHER_FUNC_DEF( cSCROLL_LIST )
#if 0
void GNu:cSCROLL_LIST::StatisticSize(cUI_GROUP *parent){\
	GNg::cFRAME_LAUNCHER_FONT * _fframe = zFFrames;\
\
	/*存在问题:该段之后可以移到预计算*/\
	w = (short)(_fframe->aGenRngW);\
	h = (short)(_fframe->aGenRngH);\
	x = (short)(_fframe->aGenRngX);\
	y = (short)(_fframe->aGenRngY);\
	\
	EnsureInclude(parent);\
}
#endif
/***************************************************** cSELECT_LIST_ICON *************************************************************************/
/*static*/ void GT_SelectListIconDrawCall(int x, int y, int w, int h, unsigned int  color, unsigned int  zid, unsigned int  vid, bool has_focus, void *arg){
	GNu::cSELECT_LIST_ICON *sl = (GNu::cSELECT_LIST_ICON*)arg;
    GT_SELECT_LIST_ProcAndDrawFocus( zSelectProc(zid,has_focus) , zArrowAm.aLevel_color, zFocusImg );
}
bool GNu::cSELECT_LIST_ICON::zSelectProc(unsigned int zid, bool has_focus){
	bool draw;
	if(has_focus){ /*模版代码:选择列表选择处理*/
		if(zFocusLastID != zid){ zFocusLastID = zid; zIsFocusFrameShow = true; }
		draw = zIsFocusFrameShow;
		if(GN::MouseI[GM_LEFT]){  //选择处理
			if(zMultSelect){ //多选
				if(zid < 32){
				    if(zMsLastState && (GN::Key[GK_LSHIFT] || GN::Key[GK_RSHIFT])){ //快捷键段选
	                    if     (zMsLastState == 1) //加段
						    aSelected |=   (zMsLastID < zid) ? (((1<<(zMsLastID))-1)^((1<<(zid+1))-1)) : (((1<<(zid))-1)^((1<<(zMsLastID+1))-1));
	                    else if(zMsLastState == 2) //减段
							aSelected &= ~((zMsLastID < zid) ? (((1<<(zMsLastID))-1)^((1<<(zid+1))-1)) : (((1<<(zid))-1)^((1<<(zMsLastID+1))-1)));
						
						zMsLastState = 0;
					}else{
						unsigned int inv = 1 << zid;
						if( aSelected &  inv){
						    aSelected -= inv;
                            zIsFocusFrameShow = false;
							zMsLastState = 2;
						}else{
						    aSelected += inv;
                            zIsFocusFrameShow = true;
							zMsLastState = 1;
						}
					}
					zMsLastID = zid;
				}
			}else aSelected = zid + 1; //单选
		}
		return draw;
	}else{ //高亮显示其它已被选中的选项
		if(zMultSelect)return aSelected & (1 << zid);
		else           return aSelected == zid+1;
	}
}
void GNu::cSELECT_LIST_ICON::zReSelectState(){
	if(zFFrames->zActFrame)zFFrames->ControlActFromMouseAuto(false); /*如是可多选并图标有动态变化就取消自动变化*/
	zMsLastState = 0;
	zSelectedLast = 0;
	aSelected = 0;
	zIsFocusFrameShow = true;
	zFocusLastID = 0xffffffff;
}
void GNu::cSELECT_LIST_ICON::SetMultSelect(bool yes){
	zMultSelect = yes;
	zReSelectState();
	if(zMultSelect && zZoningNum > 32){
		zZoningNum = 32;
		GNu::cSCROLL_LIST::RePos();
	}
}
bool GNu::cSELECT_LIST_ICON::InitStyleToData(P<GNg::sIMAGE>	icons, bool  mult_select, short _x ,short _y, short _w, short _h, bool default_am, short dw, short dh, unsigned int   focus_color, unsigned short focus_blend){
	GNg::cFRAME_LAUNCHER_FONT * _fframe = zFFrames;
	GNg::sIMAGE *_icons = icons;
	if(_icons)zZoningNum = (unsigned int)_icons->texnum;
	else if(_fframe && _fframe->zFontFrame && _fframe->zFontFrame->zPlay_obj)
		zZoningNum = (unsigned int)_fframe->zFontFrame->zPlay_obj->texnum;
	else{
       #ifdef GN_DEBUG
        GN_Log("调用 cSELECT_LIST_ICON::InitStyleToData 失败! 还没有指定图标图象!\n");
       #endif
		zZoningNum = 0;
	    return false;
	}
	zIcons = icons;
	zMultSelect = mult_select;
	
	if(zZoningNum > 128){ //检察图标数是否超出限制
	    GN_Log("调用 cSELECT_LIST_ICON::InitStyleToData 失败! 图标数不能 > 128!!\n" /*"cSELECT_LIST_ICON::InitStyleToData faile icon count > 128!!\n"*/);
	    zZoningNum = 128;
	}
	if(zMultSelect && zZoningNum > 32){
        GN_Log("调用 cSELECT_LIST_ICON::InitStyleToData 失败! 多选模式时图标数不能 > 32!!\n");
		zZoningNum = 32;
	}
	if(!_w)_w = (short)(_icons ? _icons->width : _fframe->zFontFrame->zPlay_obj->width);
    if(!dw)dw = _w;
	if(!dh)dh = _w;
    zInternalImg = _icons;
	bool ret = cSCROLL_LIST::InitStyleToData(0, _x,_y,_w,_h, default_am,dw,dh,zZoningNum, focus_color,focus_blend);
    cSCROLL_LIST::SetZoningDrawCall(GT_SelectListIconDrawCall, this);
	zInternalImg = 0;
    aShowBG = true;
    zReSelectState();
	return ret;
}
unsigned int GNu::cSELECT_LIST_ICON::UpdateRun(unsigned int updatatime, unsigned int dc){
   #ifdef GT_TMP_COMPATI_DX_DDRAW
	if(!GN_Has3DAccelerate() && aSelectFrameStyle == eRECT_DRAW_FILL)aSelectFrameStyle = eRECT_DRAW_WIRE; //在不带透明的图形环境下不使用实色填充
   #endif
	if(zMultSelect && zIsMouseFocus && GN::Key1[GK_A] && (GN::Key[GK_LCONTROL] || GN::Key[GK_RCONTROL])){
		aSelected = (1 << zZoningNum) - 1; // 快捷键全选
		zIsFocusFrameShow = true;
	}
	unsigned int ret = cSCROLL_LIST::UpdateRun(updatatime, aType == eUI_SELECT_LIST_TEXT, dc);
	if(ret && zFFrames->zActFrame){ //处理图标活动变化
		GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
		GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
		unsigned int fi = 0, fn = _fframe->zMax_num, inv;
		if(zReArrange || zSelectedLast != aSelected){
            if(zMultSelect){
				if(zReArrange){
				    for(; fi < fn; ++fi){
                        inv = (unsigned int)(1<<rng_data[fi].strid);
						if(inv & aSelected)_fframe->SelectActChange(fi);
						else               _fframe->CancelActChange(fi);
					}
				}else{
	                for(; fi < fn; ++fi){
                        inv = (unsigned int)(1<<rng_data[fi].strid);
						if ((inv & zSelectedLast) != (inv & aSelected)){
						    if(inv & aSelected)_fframe->SelectActChange(fi);
							else               _fframe->CancelActChange(fi);
					}	}
				}
			}else{
				for(; fi < fn; ++fi){
                    inv = (unsigned int)(rng_data[fi].strid+1);
					if     (inv == aSelected    )_fframe->SelectActChange(fi);
					else if(inv == zSelectedLast)_fframe->CancelActChange(fi);
				}
			}
		}
	}
	zSelectedLast = aSelected;
	return ret;
}
void GT_SendSelectChangeAction(GNu::cSELECT_LIST_ICON *sel_icon, unsigned int last_sel, unsigned int sel){
	if(sel_icon->IsMultSelect()){
		unsigned int i = 1, bit = 1;
		for(; i <= 32; ++i, bit <<= 1){
			if((last_sel & bit) != (sel & bit))GNu::UiActionCall(sel_icon, (sel & bit)?i:-i);
		}
	}else{
		if(sel)GNu::UiActionCall(sel_icon, sel);
		if(last_sel)GNu::UiActionCall(sel_icon, -last_sel);
	}
}
void GNu::cSELECT_LIST_ICON::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	unsigned int last_sel = aSelected; 
	GT_TMP_SCROLL_LIST_UPDATE( UpdateRun(updatetime, parent->c) , if(last_sel != aSelected)GT_SendSelectChangeAction(this,last_sel,aSelected); ) 
}
P<GNu::iUI_OBJECT> GNu::cSELECT_LIST_ICON::Clone(){
	P<cSELECT_LIST_ICON> ui_scr_list(1);
	*ui_scr_list = *this;
	ui_scr_list->zSpecClone();
	return ui_scr_list;
}
/***************************************************** cSELECT_LIST_TEXT *************************************************************************/
void GNu::cSELECT_LIST_TEXT::zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am){
    GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	if(_fframe->zActFrame && sp != sp_last && !ret_am){
		GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
		unsigned int   act_end_time = _fframe->zActFrame->zEndtime, fi = 0, fn = _fframe->zMax_num;
		unsigned char *act_state_d = _fframe->zActState_s;
		unsigned int  *act_time_d  = _fframe->zActTime_s;
		for(; fi < fn; ++fi){
			if(aSelected == (unsigned int)(rng_data[fi].strid+1)){ //满腹激活动画
                _fframe->SelectActChange(fi);
				act_time_d[fi] = act_end_time;
			}else{ //取消激活动画
				act_time_d[fi]  = 0;
				act_state_d[fi] = 0;
			}
		}
	}
}
/*static*/ void GT_SelectListTextDrawCall(int x, int y, int w, int h, unsigned int color, unsigned int  zid, unsigned int  vid, bool has_focus, void *arg){
    GNu::cSELECT_LIST_TEXT *sl = (GNu::cSELECT_LIST_TEXT*)arg;
	GT_SELECT_LIST_DrawText( sl->zTextOut , sl->zLabels[zid] );
	GT_SELECT_LIST_ProcAndDrawFocus( zSelectProc(zid,has_focus) , zArrowAm.aLevel_color, zFocusImg );
}
static const char *GT_ERR_SelListTextInitFail = "调用 cSELECT_LIST_TEXT::InitStyleToData 失败! %s!\n";

bool GNu::cSELECT_LIST_TEXT::InitStyleToData(P< P<char> > labels, const GNg::cTEXT_ART *text_style,unsigned int  text_color, bool  mult_select, short _x ,short _y, short _w, short _h, bool default_am, short dw, short dh, unsigned int   focus_color, unsigned short focus_blend){
	if(text_style)zTextOut = (GNg::cTEXT_ART*)text_style;
	else if(GN::RunRec.zGWPrintReady){
		GN::RunRec.aFont.Release(); //如调试信息已开启 也可用调试信息的文本式样来显示
		zTextOut = &GN::RunRec.aFont;
	}else{
		zZoningNum = 0;
		GN_Log(GT_ERR_SelListTextInitFail, "还没指定文本式样");
	    return false;
	}
	if(labels){
		zLabels = labels;  zZoningNum = labels.Num();
	}else{
		zZoningNum = 0;
       #ifdef GN_DEBUG
        GN_Log(GT_ERR_SelListTextInitFail, "还没指定标签");
       #endif
	    return false;
	}
	aTextColor = text_color ? text_color : GT_AppDefaultColorFont;
	zMultSelect = mult_select;
	if(mult_select && zZoningNum > 32){ //检测图标数是否超出限制
        GN_Log(GT_ERR_SelListTextInitFail, "多选模式下图标数不能 > 32");
		zZoningNum = 32;
	}
	// 使能自动适应宽高 ------------------------------------------------------------
	GNg::sARRANGE_FMT &tfmt = zTextOut->zParame;
	if(!_w || !_h){
		unsigned int str_num , max_str_num = 0, li = 0, ln = zLabels.Num();
		for(; li < ln; ++li){ //统计最长的标签有几个字符
			str_num = zLabels[li].Num();
			if(str_num > max_str_num)max_str_num = str_num;
		}
		if(tfmt.erect_print){
		    if(!_w)_w = tfmt.i_compartment * (short)zLabels.Num() + 1;
			if(!_h)_h = tfmt.j_compartment * (short)max_str_num + 1;
		}else{
			if(!_w)_w = tfmt.i_compartment * (short)max_str_num + 1;
			if(!_h)_h = tfmt.j_compartment * (short)zLabels.Num() + 1;
		}
	}
	if(!dw){
	    if(tfmt.erect_print)dw = tfmt.i_compartment + 1;
		else                dw = _w;
	}
	if(!dh){
	    if(tfmt.erect_print)dh = _h;
		else                dh = tfmt.j_compartment + 1;
	}
	// 初始化继承类相关数据 -----------------------------------------------------------------------------------
	bool ret = cSCROLL_LIST::InitStyleToData(0, _x,_y,_w,_h, default_am,dw,dh,zZoningNum, focus_color,focus_blend);
	cSCROLL_LIST::SetZoningDrawCall(GT_SelectListTextDrawCall, this);
	//zArrowAm.zFrame_ts.x -= 50;
	zArrowAm.aFrame_ts.sx = (float)tfmt.height / (float)zOffsetArrow->width;
	zReSelectState();
	return ret;
}
void GNu::cSELECT_LIST_TEXT::SetLables(P< P<char> > labels){
	zLabels = labels;
	zZoningNum = labels.Num();
	if(zMultSelect && zZoningNum > 32)zZoningNum = 32; //检测图标数是否超出限制
	zReSelectState();
	RePos();
}
P<GNu::iUI_OBJECT> GNu::cSELECT_LIST_TEXT::Clone(){
	P<cSELECT_LIST_TEXT> ui_scr_list(1);
	*ui_scr_list = *this;
	ui_scr_list->zSpecClone();
	return ui_scr_list;
}
/***************************************************** cSELECT_CALL *************************************************************************/
void GNu::cSELECT_CALL::Init(short count){
	zHots.New(count);
	float *_host = zHots;
	short i = 0;
	for(; i < count; ++i)_host[i] = (i == aSelect ? 1.0f : 0.0f);
}
void GNu::cSELECT_CALL::UpdateRun(void (*proc)(short, short, short, float hot, void*), void *us_arg, bool cp, short dx, short dy, short size, short dist, bool erect, float time_raito){
	if(zHots.Us() > 1)Init(zHots.Num());
	float *_host = zHots;
	short i = 0, count = zHots.Num(), dx2, dy2, selid = aSelect;
	short mouse_mx = GNg::ViewCur->mouseMX, mouse_my = GNg::ViewCur->mouseMY;
	if(erect){ /*模板代码:检测和处理每个项的选择*/
		bool m_inl = cp && mouse_mx >= dx && mouse_mx <= dx + size;
		for(; i < count; ++i){
			if(m_inl && mouse_my >= dy && mouse_my <= dy + dist){
				UiActionCall((cUI_SELECT_BUTTON*)((cSELECT_BUTTON*)this), i+1);
				if(i != selid)UiActionCall((cUI_SELECT_BUTTON*)((cSELECT_BUTTON*)this), -(selid+1));
				aSelect = i;
			}
			if(i == selid)dy2 = dy;
			else proc(dx, dy, i, _host[i], us_arg);
			dy += dist;
		}
		if(selid < count)proc(dx, dy2, selid, _host[selid], us_arg);
	}else{     /*模板代码:检测和处理每个项的选择*/
		bool m_inl = cp && mouse_my >= dy && mouse_my <= dy + size;
		for(; i < count; ++i){
			if(m_inl && mouse_mx >= dx && mouse_mx <= dx + dist){
				UiActionCall((cUI_SELECT_BUTTON*)((cSELECT_BUTTON*)this), i+1);
				if(i != selid)UiActionCall((cUI_SELECT_BUTTON*)((cSELECT_BUTTON*)this), -(selid+1));
				aSelect = i;
			}
			if(i == selid)dx2 = dx;
			else proc(dx, dy, i, _host[i], us_arg);
			dx += dist;
		}
		if(selid < count)proc(dx2, dy, selid, _host[selid], us_arg);
	}
	time_raito *= 0.2f;
	for(i = 0; i < count; ++i)_host[i] += ((i == aSelect? 1.0f : 0.0f) - _host[i]) * time_raito;
}
void GNu::cPAGE_BAR::Init(P< P<char> >  names, const GNg::cTEXT_ART *text_style, short l, short h){
	unsigned int ci = 0, cn = names.Num();
	cSELECT_CALL::Init(cn);
	zNames = names;
	if(text_style)zTextOut = (GNg::cTEXT_ART*)text_style;
	else zTextOut = &GN::RunRec.aFont;
	GNg::sARRANGE_FMT *tfmt = zTextOut->GetArrangeFormat();
	if(l)zL = l;
	else{
		P<char> *_names = names;
		unsigned int max_len = 0, len;
		for(; ci < cn; ++ci){
			len = strlen(_names[ci]);
			if(len > max_len)max_len = len;
		}
		max_len += 2;
		zL = max_len * (tfmt->erect_print ? (zTextOut->zFontMapFormat==GNg::eFONT_FREETYPE?(tfmt->j_compartment*3/5):tfmt->j_compartment) : tfmt->i_compartment);
	}
	if(h)zH = h;
	else zH =		   (tfmt->erect_print ? (zTextOut->zFontMapFormat==GNg::eFONT_FREETYPE?(tfmt->i_compartment<<1):tfmt->i_compartment) : tfmt->j_compartment);
}
void GT_PageDrawCall(short dx, short dy, short id, float hot, void *us_arg){
	unsigned int  *ccs = (unsigned int *)(((void**)us_arg)[0]);
	GNu::cPAGE_BAR *_pb = (GNu::cPAGE_BAR *)(((void**)us_arg)[1]);
	int tw = *((int*)(((void**)us_arg)[2])), tfw, tfh;
	GNg::sARRANGE_FMT *tfmt = _pb->zTextOut->GetArrangeFormat();
	if(tfmt->erect_print){
		tfw = _pb->zH, tfh = _pb->zL;
	  if(tfmt->overturn_print){
		short h2 = (short)((       hot) * (float)tfw);
		if(ccs[2])GNg::ImageDraw(0, dx,    dy, h2,			 tfh, ccs[2]);
				  GNg::ImageDraw(0, dx+h2, dy, tfw - h2, tfh, ccs[1]);
	  }else{
		short h2 = (short)((1.0f - hot) * (float)tfw);
				  GNg::ImageDraw(0, dx,    dy, h2,			 tfh, ccs[1]);
		if(ccs[2])GNg::ImageDraw(0, dx+h2, dy, tfw - h2, tfh, ccs[2]);
	  }
		if(ccs[3])GNg::LineFrame2dDraw(dx, dy, tfw, tfh, 1, ccs[3]);

		dy += 5; tfh -= 10;
	}else{
		tfw = _pb->zL, tfh = _pb->zH;
		short h2 = (short)((1.0f - hot) * (float)tfh);
				  GNg::ImageDraw(0, dx, dy,    tfw, h2,           ccs[1]);
		if(ccs[2])GNg::ImageDraw(0, dx, dy+h2, tfw, tfh - h2, ccs[2]);
		if(ccs[3])GNg::LineFrame2dDraw(dx, dy, tfw, tfh, 1, ccs[3]);

		dx += 5; tfw -= 10;
	}
	for(tw += dx; dx < tw; ++dx)_pb->zTextOut->OutText(_pb->zNames[id], 0, dx, dy, tfw, tfh, ccs[0]);
}
void GNu::cPAGE_BAR::UpdateRun(bool cp, short dx, short dy, short dist, int tw, unsigned int  text_color, unsigned int  back_color, unsigned int  focus_color, unsigned int  line_color, float time_raito){
	unsigned int  ccs[4] = { text_color , back_color, focus_color , line_color };
	void *args[3] = { ccs, this , &tw };
	GNg::sARRANGE_FMT *tfmt = zTextOut->GetArrangeFormat();
	float nu = tfmt->near_pointU, nv = tfmt->near_pointV;
	if(!(tfmt->erect_print && tfmt->overturn_print))tfmt->near_pointU = 0.5f;
	tfmt->near_pointV = 0.5f;
	cSELECT_CALL::UpdateRun(GT_PageDrawCall, (void*)args, cp, dx, dy, zH, dist ? dist : zL, tfmt->erect_print, time_raito);
	tfmt->near_pointU = nu; tfmt->near_pointV = nv;
}
void GNu::cPAGE_BAR::zUpdateRun(bool cp, short dx, short dy, short dist, int tw, unsigned int  text_color, unsigned int  back_color, unsigned int  focus_color, unsigned int  line_color, float time_raito){
	unsigned int  ccs[4] = { text_color , back_color, focus_color , line_color };
	void *args[3] = { ccs, this , &tw };
	cSELECT_CALL::UpdateRun(GT_PageDrawCall, (void*)args, cp, dx, dy, zH, dist ? dist : zL, zTextOut->GetArrangeFormat()->erect_print, time_raito);
}
void GNu::cPAGE_BAR::GetOnecWH(short &w, short &h){
	if(zTextOut->GetArrangeFormat()->erect_print){
		w = zH; h = zL;
	}else{
		h = zH; w = zL;
	}
}
void GNu::cSELECT_BUTTON::Init(P<GNg::sIMAGE> button_style){
	cSELECT_CALL::Init(button_style->texnum);
	zImg = button_style;
}
void GT_SelbuttonDrawCall(short dx, short dy, short id, float hot, void *us_arg){
	unsigned int  *ccs = (unsigned int *)(((void**)us_arg)[0]);
	GNg::sIMAGE *_img = (GNg::sIMAGE *)(((void**)us_arg)[1]);
	if(hot < 0.0005f || ccs[0] == ccs[1])GNg::ImageDraw(_img, dx, dy,  0, 0, ccs[0], id);
	else{
		float add_r = 0.15f;
		unsigned int  c = ccs[1];
		if(hot < 0.9995f){
			add_r *= hot;
			if(c != ccs[0])c = GNg::BlendColor(ccs[0], c, hot);
		}
		int dw = (float)_img->width * add_r, dh = (float)_img->height * add_r;
		GNg::ImageDraw(_img, dx - (dw >> 1), dy - (dh >> 1), _img->width + dw, _img->height + dh, c, id);
	}
}
void GNu::cSELECT_BUTTON::UpdateRun(bool cp, short dx, short dy, short dist, unsigned int  color, unsigned int  focus_color, bool erect, float time_raito){
	unsigned int  ccs[2] = { color , focus_color };
	void *args[2] = { ccs, zImg.Begin() };
	cSELECT_CALL::UpdateRun(GT_SelbuttonDrawCall, (void*)args, cp, dx, dy, erect?zImg->width:zImg->height, dist ? dist : (erect?zImg->height:zImg->width), erect, time_raito);
}
void GNu::cSELECT_BUTTON::GetOnecWH(short &w, short &h){
	w = zImg->width;   h = zImg->height;
}
/*------------------------------------------------------------------------------------------------------------------------------------------*/
void GNu::cUI_PAGE_BAR::GetSize(short &_w, short &_h){
	if(zHots){
		short _l = (aStride ? (zHots.Num() - 1) * aStride + zL  : zHots.Num() * zL);
		if(zTextOut->GetArrangeFormat()->erect_print){
			_h = _l;
			_w = zH;
		}else{
			_w = _l;
			_h = zH;
		}
	}else{  _w = 0;  _h = 0; }
}
void GNu::cUI_PAGE_BAR::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	short dx, dy;
	unsigned int dc = DrawBeginPosGet(dx, dy, parent);
	cPAGE_BAR::UpdateRun(GN::MouseI[GM_LEFT] && RecvInputUiObject == this, dx, dy, aStride, aFontLineW, dc, GNg_MultiplyColorRatio(aBackColor, parent->c), GNg_MultiplyColorRatio(aFocusColor, parent->c), GNg_MultiplyColorRatio(aLineColor, parent->c), aAmSheep);
	GetSize(w, h);
	if(aAutoLayerObjShow){
		GN::sLIST< P<iUI_OBJECT> >::iterator itu = parent->node->aSubs.begin(), etu = parent->node->aSubs.end();
		for(; itu != etu; ++itu){
			if((iUI_OBJECT*)(*itu) == (iUI_OBJECT*)this){
				++itu;
				iUI_OBJECT *_uobj;
				float *_hots = zHots;
				unsigned int ci = 0, cn = zHots.Num();
				for(; ci < cn && itu != etu; ++itu){
					_uobj = *itu;
					if(_uobj->aType == GNg::eOBJ_GROUP || _uobj->aType == GNu::eUI_SCROLL_RECT){
						((cUI_GROUP*)_uobj)->c = ((uint8)(255.0f * _hots[ci])<<24) | (((cUI_GROUP*)_uobj)->c&0x00ffffff);
						++ci;
					}
				}
				break;
		}	}
	}
}
void GNu::cUI_PAGE_BAR::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent){
	out.w = w; out.h = h;
	out.c = DrawBeginPosGet(out.x, out.y, (GNg::sDRAW_AREA2*)&(parent->gx)); 
}
P<GNu::iUI_OBJECT> GNu::cUI_PAGE_BAR::Clone(){
	P<cUI_PAGE_BAR> ui_page(1);
	*ui_page = *this;
	ui_page->zHots.Copy(zHots);
	return ui_page;
}
void GNu::cUI_SELECT_BUTTON::GetSize(short &_w, short &_h){
	if(zHots){
		GNg::sIMAGE *_img = zImg;
		if(aIsErectArrange){
			_h = (aStride ? (zHots.Num() - 1) * aStride + _img->height : zHots.Num() * _img->height);
			_w = _img->width;
		}else{
			_w = (aStride ? (zHots.Num() - 1) * aStride + _img->width  : zHots.Num() * _img->width);
			_h = _img->height;
		}
	}else{  _w = 0;  _h = 0; }
}
void GNu::cUI_SELECT_BUTTON::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	short dx, dy;
	unsigned int dc = DrawBeginPosGet(dx, dy, parent);
	cSELECT_BUTTON::UpdateRun(GN::MouseI[GM_LEFT] && RecvInputUiObject == this, dx, dy, aStride, dc, GNg_MultiplyColorRatio(aFocusColor, parent->c), aIsErectArrange, aAmSheep);
	GetSize(w, h);
}
void GNu::cUI_SELECT_BUTTON::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent){
	out.w = w; out.h = h;
	out.c = DrawBeginPosGet(out.x, out.y, (GNg::sDRAW_AREA2*)&(parent->gx)); 
}
P<GNu::iUI_OBJECT> GNu::cUI_SELECT_BUTTON::Clone(){
	P<cUI_SELECT_BUTTON> ui_sb(1);
	*ui_sb = *this;
	ui_sb->zHots.Copy(zHots);
	return ui_sb;
}
/***************************************************** cSELECT_RECT *************************************************************************/
GNu::cSELECT_RECT::cSELECT_RECT():zP2(0,0,0,0),zRect(0,0,0,0),zHideSleep(1),zHideTime(0){}

void GNu::cSELECT_RECT::Init(P<GNg::sIMAGE> bg_img, int hide_sleep){
    zBg = bg_img;
	zHideSleep = hide_sleep + 1;
}
GNg::sRECT *GNu::cSELECT_RECT::UpdateRun(unsigned int updatatime,unsigned int  color,unsigned short blendmode,unsigned int  line_color,unsigned short line_blendmode,int line_w){
	// 处理鼠标选择 ------------------------------------------------------------
	GNg::sRECT *_rc = 0;
	if(GN::Mouse[GM_LEFT] && !GN::Mouse1[GM_LEFT]){
	    zP2.right = GNg::ViewCur->mouseMX;  zP2.bottom = GNg::ViewCur->mouseMY;
		if(zP2.right != zP2.left && zP2.bottom != zP2.top){
		    GNg::RectMakeFromP2(zRect,zP2.left,zP2.top,zP2.right,zP2.bottom);
	        _rc = &zRect;
		    zHideTime = zHideSleep;
		}
	}else{
	    zP2.left = zP2.right  = GNg::ViewCur->mouseMX;
	    zP2.top  = zP2.bottom = GNg::ViewCur->mouseMY;
	}
    // 显示选择筐 ----------------------------------------------------------------------------------------------------------------------
	if(zHideTime > 0){
	    int rx = zRect.left, ry = zRect.top, rw = zRect.right-zRect.left, rh = zRect.bottom-zRect.top;
        uint8 a = (uint8)(((color & 0xff000000)>>24) * zHideTime / zHideSleep);
		if(a > 2){
		    if(zBg.Has() && zBg->texnum == 9)GNg::ImageDraw9Square(zBg, rx,ry,rw,rh, (a<<24)|(color&0x00ffffff),blendmode); //九格边筐图
		    else GNg::ImageDraw(zBg, rx,ry,rw,rh, (a<<24)|(color&0x00ffffff),0,blendmode);
		}
        if(!zBg && !line_w)line_w = 1;
		if(line_w){
			a = (uint8)(((line_color & 0xff000000)>>24) * zHideTime / zHideSleep);
		    if(a > 2)GNg::LineFrame2dDraw(rx,ry,rw,rh, line_w, (a<<24)|(line_color&0x00ffffff),line_blendmode);
		}
		zHideTime -= (int)updatatime;
	}
	return _rc;
}
void GNu::cSELECT_RECT::Hide(){
    zP2.left  = zP2.right;
    zP2.top   = zP2.bottom;
    zHideTime = 0;
}
/********************************************* cSLIDER ********************************************************/
P<GNu::iUI_OBJECT> GNu::cSLIDER::Clone(){
	P<cSLIDER> u_slider(1);
	*u_slider = *this;
	return u_slider;
}
void GNu::cSLIDER::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent){
	DrawRangeGet(out, (GNg::sDRAW_AREA2*)&(parent->gx)); 
}
void GNu::cSLIDER::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	GNg::sDRAW_AREA2 rng;
	DrawRangeGet(rng, parent);
	short dx = rng.x, dy = rng.y;
	unsigned int dc = rng.c;
	GNg::ImageDraw(0,dx,dy,rng.w,rng.h,dc,0,bmode);
	if(img){
		short iw = aIconW?aIconW:img->width, ih = aIconH?aIconH:img->height;
		if(zState == 2){
			short oo = zOfs;
			if(w > h){
				zOfs = GNg::ViewCur->mouseMX - dx;
				GN_ValueClamp(zOfs, 0, rng.w);
				dx += zOfs       - (iw>>1);
				dy += (rng.h>>1) - (ih>>1);
			}else{
				zOfs = GNg::ViewCur->mouseMY - dy;
				GN_ValueClamp(zOfs, 0, rng.h);
				dy += zOfs       - (ih>>1);
				dx += (rng.w>>1) - (iw>>1);
			}
		   #define GT_TMP_SLIDER_PRESS_PROC \
			if(!GN::Mouse[GM_LEFT]){ zState = 1;			 UiActionCall(this, (unsigned int)zOfs * 65535 / (w>h?w:h)); }\
			else if(zOfs != oo){\
				short crs = zOfs > oo ? zOfs - oo : oo - zOfs;\
				if(crs > 5 || !(GN::TimeGetRunFpsCount()&31))UiActionCall(this, (unsigned int)zOfs * 65535 / (w>h?w:h));  \
			}\
			dc = aPressColor;

			GT_TMP_SLIDER_PRESS_PROC
		}else{
			short iw2 = iw<0?-iw:iw, ih2 = ih<0?-ih:ih;
			zState = 0;
			if(RecvInputUiObject == this && ((w > h? GNg::ViewCur->HitRange(dx-(iw2>>1), dy+(rng.h-ih2>>1), rng.w+iw2, ih2): GNg::ViewCur->HitRange(dx+(rng.w-iw2>>1), dy-(ih2>>1), iw2, rng.h+ih2)))){
				if(GN::Mouse1[GM_LEFT])zState = 2;
				else zState = 1;
			}
			if(aAmSheep < 1.0f){
				zFocusRatio += ((zState ? 1.0f : 0.0f) - zFocusRatio) * aAmSheep;
				dc = GNg::BlendColor(aIconColor, aFocusColor, zFocusRatio);
			}else{
				if(zState){
					zFocusRatio = 1.0f;
					dc = aFocusColor;
				}else{
					zFocusRatio = 0.0f;
					dc = aIconColor;	
				}
			}
			if(w > h){
				dx += zOfs       - (iw>>1);
				dy += (rng.h>>1) - (ih>>1);
			}else{
				dy += zOfs       - (ih>>1);
				dx += (rng.w>>1) - (iw>>1);
			}
		}
		GNg::ImageDraw(img,dx,dy,aIconW,aIconH,GNg_MultiplyColorRatio( dc , parent->c ),0,aIconBlend);
	}else{
		if(zState == 2){
			short oo = zOfs;
			if(w > h){
				zOfs = GNg::ViewCur->mouseMX - dx;
				GN_ValueClamp(zOfs, 0, rng.w);
			}else{
				zOfs = GNg::ViewCur->mouseMY - dy;
				GN_ValueClamp(zOfs, 0, rng.h);
			}
			GT_TMP_SLIDER_PRESS_PROC
		}else{
			zState = 0;
			if(RecvInputUiObject == this && GNg::ViewCur->HitRange(dx,dy,rng.w,rng.h)){
				if(GN::Mouse1[GM_LEFT])zState = 2;
				else zState = 1;
			}
			dc = aFocusColor;
		}
		dc = GNg_MultiplyColorRatio( dc , parent->c );
		short len = zOfs-(aIconW<<1);
		if(len < 0)len = 0;
		if(w > h)GNg::ImageDraw(0,dx+aIconW,dy+aIconW, len,rng.h-(aIconW<<1), dc,0,bmode);
		else	 GNg::ImageDraw(0,dx+aIconW,dy+aIconW, rng.w-(aIconW<<1),len, dc,0,bmode);
	}
}
/********************************************* cPALETTE ********************************************************/
P<GNg::sIMAGE> GNu::cPALETTE::zAlphaBgImg;
P<GNg::sIMAGE> GNu::cPALETTE::zColorBarImg;
GN::sLIST<GNg::sCOLOR> GNu::cPALETTE::aColorPasteTable;
unsigned short  GNu::cPALETTE::zCreateTotalNum = 0;

GNu::cPALETTE::cPALETTE():aColorTablePosType(0),aDisableControl(false),zOffsetSheep(1.0f){
	++zCreateTotalNum;
	if(zCreateTotalNum == 1){
        const int img_w = 128, img_h = 4;
		zColorBarImg.New(1);
		zColorBarImg->CreateEmptyToData(img_w,img_h,1, GNg::CR_RGB); //创建色调条图
		uint8 cr_data[img_h * img_w * 3];
        zComputeColorBarData(cr_data, img_w, img_h , 0, img_w);		 //计算色调条数据
		zColorBarImg->DataFill(cr_data, GNg::CR_RGB, img_w, img_h);

        int imgw = 16, imgh = 8;
        zAlphaBgImg.New(1);
		zAlphaBgImg->CreateEmptyToData(imgw,imgh,1,GNg::CR_LUMINANCE); //创建透明区的底图
		memset(cr_data, 80, imgw * imgh);
		uint8 *pd = cr_data;
        pd[0] = pd[1] = pd[3] = pd[4] = pd[7] = pd[8] = pd[9] = pd[11] = pd[12] = pd[13] = pd[14] = 30; pd += imgw;
		pd[1] = pd[3] = pd[6] = pd[9] = pd[12] = pd[14] = 80; pd += imgw;
		pd[1] = pd[3] = pd[5] = pd[12] = 80; pd += imgw;
        pd[2] = pd[5] = pd[8] = pd[9] = pd[10] = pd[12] = pd[13] = 80; pd += imgw;
		pd[2] = pd[6] = pd[9] = pd[12] = pd[14] = 80; pd += imgw;
        pd[1] = pd[2] = pd[3] = pd[7] = pd[8] = pd[11] = pd[12] = pd[13] = pd[14] = 80;
		zAlphaBgImg->DataFill(cr_data, GNg::CR_LUMINANCE, imgw, imgh);
	}
	aType = eUI_PALETTE; c = 0xffffffff/*该类忽略该值*/;
}
GNu::cPALETTE::~cPALETTE(){
    --zCreateTotalNum;
	if(!zCreateTotalNum){
		zAlphaBgImg.Delete();
        zColorBarImg.Delete();
        aColorPasteTable.clear();
	}
}
void GNu::cPALETTE::zUpdateColorFromOffsetState(){
	float ocx = (1.0f-zColorOffsetX) * 255.0f, ocy = (1.0f-zColorOffsetY);
	zColor.BGRA.r = (uint8)(((float)zColorBase.BGRA.r * zColorOffsetX  + ocx) * ocy);
	zColor.BGRA.g = (uint8)(((float)zColorBase.BGRA.g * zColorOffsetX  + ocx) * ocy);
	zColor.BGRA.b = (uint8)(((float)zColorBase.BGRA.b * zColorOffsetX  + ocx) * ocy);
}
void GNu::cPALETTE::zComputeColorImgData(GNg::sCOLOR base){
	float ratio;
	uint8 locbuf[65536]; //64K
	GN::sTBUF_INT tbuf;
	int imgw = zColorImg->width, imgh = zColorImg->height;
    uint8 *img_data = (uint8*)tbuf.GetSpace(imgw * imgh * 3);
	uint8 * pd = img_data, *pd2;
	int i,j, add_v, w2 = imgw - 1, h2 = imgh - 1;
	for(i = 0; i < imgw; ++i){ //首先计算首行的颜色渐变
        add_v = 255 * (w2-i)/w2;
	   *pd = (uint8)((int)base.BGRA.r * i/w2 + add_v) ; ++pd;
       *pd = (uint8)((int)base.BGRA.g * i/w2 + add_v) ; ++pd;
	   *pd = (uint8)((int)base.BGRA.b * i/w2 + add_v) ; ++pd;
	}
	pd2 = img_data;
	pd  = pd2 + (imgw * 3);
	for(j = 1; j <= h2; ++j, pd2 = img_data){ //按比例来调深第二行以下的行颜色值
		ratio = (float)(h2 - j) / (float)h2;
	    for(i = 0; i < imgw; ++i){
		   *pd = (uint8)((float)(*pd2) * ratio); ++pd; ++pd2;
		   *pd = (uint8)((float)(*pd2) * ratio); ++pd; ++pd2;
		   *pd = (uint8)((float)(*pd2) * ratio); ++pd; ++pd2;
		}
	}
    zColorImg->DataFill(img_data, GNg::CR_RGB, imgw, imgh);
}
void GNu::cPALETTE::zComputeColorBarData(uint8 *pd, int bar_w, int bar_h, int start, int end){
	uint8 *cr_data = pd;
	int i = start, j, s1 = bar_w / 6;
	int s2 = s1 <<1, s3 = s1 * 3, s4 = s1 <<2, s5 = s1 * 5;
	int se = bar_w - s5;
	for(; i < end; ++i){ //按在不同的色调段落位置来计算颜色3个分量值
		if      (i <= s1){
		    *pd = 255;                      ++pd;
            *pd = 0;                        ++pd;
			*pd = (uint8)(i*255/s1);        ++pd;
		}else if(i <= s2){
            *pd = (uint8)((s2-i)*255/s1);   ++pd;
            *pd = 0;					    ++pd;
			*pd = 255;					    ++pd;
		}else if(i <= s3){
            *pd = 0;                        ++pd;
            *pd = (uint8)((i-s2)*255/s1);   ++pd;
			*pd = 255;					    ++pd;
		}else if(i <= s4){
            *pd = 0;                        ++pd;
            *pd = 255;					    ++pd;
			*pd = (uint8)((s4-i)*255/s1);   ++pd;
		}else if(i <= s5){
            *pd = (uint8)((i-s4)*255/s1);   ++pd;
            *pd = 255;					    ++pd;
			*pd = 0;					    ++pd;
		}else{
            *pd = 255;                      ++pd;
            *pd = (uint8)((bar_w-i)*255/se);++pd;
			*pd = 0;					    ++pd;
		}
	}
	for(s1 = bar_w*3, j = 1;  j < bar_h;  j <<= 1, s1 <<= 1){ //按首行的数据复制到其它行
        if((j<<1) <= bar_h)memcpy(cr_data+s1, cr_data, s1);
		else               memcpy(cr_data+s1, cr_data, (bar_h-j) * bar_w*3);
	}
}
void GNu::cPALETTE::InitToData(short _x,short _y,short _w,short _h,GNg::sCOLOR cr, GNg::sCOLOR arrow, bool arrow_shadow, bool alpha_set, bool alpha_bg_show){
	x = _x, y = _y, w = _w, h = _h;
	aArrowShadow = arrow_shadow;
	aAlphaSet    = alpha_set;
	aAlphaBgShow = alpha_bg_show;

   #define GT_TMP_PALETTE_INIT_TO_DATA( CR ) \
	zAlphaBgUV = 0.0f;\
    zIsMouseFocus = false;\
    zColorClone.Delete();\
	if(!zColorImg){\
	    zColorImg.New(1);\
		zColorImg->CreateEmptyToData(128,128,1,GNg::CR_RGB);\
	}\
	SetColor( CR );

	GT_TMP_PALETTE_INIT_TO_DATA( cr )
	zColorArrow = arrow;
}
void GNu::cPALETTE::InitDefaultTable(){
	GNg::sCOLOR colors[9] = { 0xffff0000,0xffffff00,0xff00ff00,0xff00ffff,0xff0000ff,0xffff00ff,0xffffffff,0xff808080,0xff000000 };
	aTable.New (colors,9);
}
void  GNu::cPALETTE::SetColor(GNg::sCOLOR cr){
	//比较出最小和最大的颜色分量
	uint8 min_rgb = cr.BGRA.r;
    uint8 max_rgb = min_rgb;
	if	   (cr.BGRA.g < min_rgb)min_rgb = cr.BGRA.g;
	else if(cr.BGRA.g > max_rgb)max_rgb = cr.BGRA.g;
    if	   (cr.BGRA.b < min_rgb)min_rgb = cr.BGRA.b;
    else if(cr.BGRA.b > max_rgb)max_rgb = cr.BGRA.b;
    float ratio = 255.0f / (float)(max_rgb - min_rgb), sr = 1.0f / 6.0f;
	zColorBase.Init((uint8)((float)(cr.BGRA.r-min_rgb)*ratio), (uint8)((float)(cr.BGRA.g-min_rgb)*ratio), (uint8)((float)(cr.BGRA.b-min_rgb)*ratio), 255);
	zComputeColorImgData(zColorBase); //按基色计算色谱数据

	//按基色来计算色调条的偏移量
	if(zColorBase.BGRA.r){
		if(zColorBase.BGRA.g){
			if(zColorBase.BGRA.r==255)zBarOffset = 1.0f - ((float)zColorBase.BGRA.g/255.0f*sr);
		    else					  zBarOffset = sr * (4.0f + (float)zColorBase.BGRA.r/255.0f);
		}else if(zColorBase.BGRA.b){
		    if(zColorBase.BGRA.r==255)zBarOffset = sr * ((float)zColorBase.BGRA.b/255.0f);
		    else					  zBarOffset = sr * (2.0f - (float)zColorBase.BGRA.r/255.0f);
		}else zBarOffset = 0.0f;	  //纯红色
	}else{
		if(zColorBase.BGRA.b){
			if(zColorBase.BGRA.b==255){
				if(zColorBase.BGRA.g)zBarOffset = sr * (2.0f + (float)zColorBase.BGRA.g/255.0f);
                else				 zBarOffset = sr *  2.0f; //纯蓝色
			}else zBarOffset = sr * (4.0f - (float)zColorBase.BGRA.b/255.0f);
		}else zBarOffset = sr * 4.0f; //纯绿色
	}
	zColor = cr;
	//按最大和最小颜色分量计算在色谱的偏移量
	zColorOffsetX  = 1.0f - (float)min_rgb / (float)max_rgb;
	zColorOffsetY  = 1.0f - (float)max_rgb / 255.0f;
	zAlphaBgOffset = (float)cr.BGRA.a / 255.0f;
}
bool GNu::cPALETTE::UpdateRun(unsigned int updatetime, unsigned int dc){
	if(!zColorImg){
       #ifdef GN_DEBUG
        GN_Log("cPALETTE::UpdateRun() 还没初始化好! 请先调用 cPALETTE::InitToData(...) 来初始化!\n");
       #endif
	    return false;
	}
	bool ret = false, mouse_cp = !aDisableControl && GN::Mouse1[GM_LEFT];
	short mouse_x = (short)GNg::ViewCur->mouseMX, mouse_y = (short)GNg::ViewCur->mouseMY,
		dlgx = x, dlgy = y, dlgw = w, dlgh = h - 3,
		tbx  = x, tby  = y, tbx1, tby1, tbw1, tbh1, tbw, tbh,  cid,
		tc_x, tc_y, tc_w, tc_h; //用来临时记下复制出的色块要飞过去的目标位置

    unsigned int tn = aTable.Num();
	if(tn){
		GNg::sCOLOR *pccc = aTable, *_pccc = 0;
		P<GNg::sCOLOR> paste_table;
		unsigned int tn_ = aColorPasteTable.size();
		if(tn_){
			paste_table(aColorPasteTable);
			if(tn_ > tn){
				  _pccc = (GNg::sCOLOR*)paste_table + (tn_ - tn);
                  tn_ = tn;
			}else _pccc = paste_table;
		}
		unsigned int ti = 0, ti2 = tn - 1;
		if(w > h){ //色版在左边		/*模版代码:处理与绘制调色版的颜色表*/
            tbw = dlgw >> 3, tbh = dlgh / tn;
		    dlgx += tbw, dlgw -= tbw;
			if(tn_){ //有粘贴色版
				tbw1 = tbw >> 1;
				if(aColorTablePosType){
					  tbx1 = tbx; tbx = tbx + tbw1;
				}else tbx1 = tbx + tbw1;
			}else{    tbw1 = tbw; tbx1 = tbx; }

            if(zColorClone){
				tc_w = tbw >> 1;
				if(tn_)tc_x = tbx;
				else{
					tc_x = tbx;
					if(aColorTablePosType)tc_x += tc_w;
				}
				if(tn_ >= ti2){
                    tc_y = tby + ti2 * tbh;
                    tc_h = y + h - tc_y;
				}else{
					tc_y = tby + tn_ * tbh;
                    tc_h = tbh;
				}
			}
			cid = (mouse_y - tby) / tbh;
			if(cid > (short)ti2)cid = (short)ti2;
			if(mouse_cp && mouse_y >= tby && mouse_y <= y + dlgh){
				if      (tn_ && mouse_x >= tbx  && mouse_x <= tbx  + tbw1){
				  if(cid < tn_){
					SetColor(_pccc[cid]); ret = true;
				  }
				}else if(       mouse_x >= tbx1 && mouse_x <= tbx1 + tbw1){
					SetColor( pccc[cid]); ret = true;
				}
			}
			for(; ti < tn; ++ti, tby += tbh){
                if(ti == ti2)tbh = y + h - tby;
				if(ti < tn_)GNg::ImageDraw(0, tbx,  tby, tbw1, tbh, _pccc[ti].COLOR); //显示粘贴表各格颜色
                            GNg::ImageDraw(0, tbx1, tby, tbw1, tbh,  pccc[ti].COLOR); //显示颜色表各格颜色
			}
            dlgx += (tbw >> 3), dlgw -= (tbw >> 3);

		}else{     //色版在上边		/*模版代码:处理与绘制调色版的颜色表*/
			tbh = dlgh >> 3, tbw = dlgw / tn;
		    dlgy += tbh, dlgh -= tbh;
			if(tn_){ //有粘贴色版
                tbh1 = tbh >> 1;
				if(aColorTablePosType){
                      tby1 = tby; tby = tby + tbh1;
				}else tby1 = tby + tbh1;
			}else{    tbh1 = tbh; tby1 = tby; }

			if(zColorClone){
				tc_h = tbh >> 1;
				if(tn_)tc_y = tby;
				else{
					tc_y = tby;
					if(aColorTablePosType)tc_y += tc_h;
				}
				if(tn_ >= ti2){
                    tc_x = tbx + ti2 * tbw;
                    tc_w = x + w - tc_x;
				}else{
					tc_x = tbx + tn_ * tbw;
                    tc_w = tbw;
				}
			}
			cid = (mouse_x - tbx) / tbw;
			if(cid > (short)ti2)cid = (short)ti2;
			if(mouse_cp && mouse_x >= tbx && mouse_x <= x + w){
				if      (tn_ && mouse_y >= tby  && mouse_y <= tby  + tbh1){
				  if(cid < tn_){ 
					SetColor(_pccc[cid]); ret = true;
				  }
				}else if(       mouse_y >= tby1 && mouse_y <= tby1 + tbh1){
                    SetColor( pccc[cid]); ret = true;
				}
			}
			for(; ti < tn; ++ti, tbx += tbw){
				if(ti == ti2)tbw = x + w - tbx;
				if(ti < tn_)GNg::ImageDraw(0, tbx, tby,  tbw, tbh1,  _pccc[ti].COLOR); //显示粘贴表各格颜色
				            GNg::ImageDraw(0, tbx, tby1, tbw, tbh1,   pccc[ti].COLOR); //显示颜色表各格颜色
			}
            dlgy += (tbh >> 3), dlgh -= (tbh >> 3);
		}
	}
	short dh  = aAlphaSet ? dlgh - 72 : dlgh - 36;   //色谱显示高
	short dy  = dlgy;              //色普显示位置Y
	short ay1 = dy  + dh + 4;      //色调箭头显示位置Y
    short by  = ay1 + 10;          //色调条显示位置Y
    short ay2 = by  + 28;          //透明度箭头显示位置Y
    short by2 = ay2 + 10;          //透明条显示位置Y

	GNg::sCOLOR last_color = zColor;
	if(aDisableControl || !GN::Mouse[GM_LEFT])zSetTpye = 0;
	zIsMouseFocus = false;
	// 判断是对那项设置 ----------------------------------------------
	if(mouse_x >= dlgx && mouse_x <= dlgx + dlgw && mouse_y >= dlgy){
		if      (mouse_y <= dy + dh){
		    zIsMouseFocus = true;  if(mouse_cp)zSetTpye = 1;
		}else if(mouse_y <= by + 24){
		    zIsMouseFocus = true;  if(mouse_cp)zSetTpye = 2;
		}else if(mouse_y <= dlgy + dlgh){
		    zIsMouseFocus = true;  if(mouse_cp)zSetTpye = 3;
		}
	}
	// 选择控制处理 --------------------------------------------------------------------------------------------
	if((mouse_cp && zSetTpye) || (zSetTpye && GN::MouseM)){
		switch(zSetTpye){
		case 1:
			zColorOffsetX = (float)(mouse_x - dlgx) / (float)dlgw;
			GN_ValueClamp(zColorOffsetX, 0.0f, 1.0f);
			zColorOffsetY = (float)(mouse_y - dlgy) / (float)dh;
			GN_ValueClamp(zColorOffsetY, 0.0f, 1.0f);
            
			zUpdateColorFromOffsetState();				//更新一下颜色
			break;
		case 2:{
            zBarOffset = (float)(mouse_x - dlgx) / (float)dlgw;
			GN_ValueClamp(zBarOffset, 0.0f, 1.0f);
			int st_x = (int)((float)zColorBarImg->width * zBarOffset);
			if(st_x == zColorBarImg->width)st_x -= 1;
			uint8 *_rgb = (uint8*)&zColorBase;
			zComputeColorBarData(_rgb, zColorBarImg->width, 1 , st_x , st_x+1); //计算得到当前偏移位置在色调条的颜色RGB分量
			GN_Swap(uint8, _rgb[0] ,  _rgb[2]);
            
			zComputeColorImgData(zColorBase);			//按新基色重新计算一下色谱数据
            zUpdateColorFromOffsetState();				//更新一下颜色
			}break;
		case 3:
            zAlphaBgOffset = (float)(mouse_x - dlgx) / (float)dlgw;
			GN_ValueClamp(zAlphaBgOffset, 0.0f, 1.0f);
			zColor.BGRA.a = (uint8)(zAlphaBgOffset * 255.0f);
			break;
		}
		if(GN::MouseM)zColorClone.Delete();
		ret = true;
	}
	GNg::ImageDraw(zColorImg,    dlgx, dy, dlgw, dh, dc); //显示色谱
    GNg::ImageDraw(zColorBarImg, dlgx, by, dlgw, 24, dc); //显示色调条
	
	if(aAlphaSet){
		if(aAlphaBgShow){ //显示透明滚动背景
			float u2 = zAlphaBgUV + ((float)dlgw / (float)zAlphaBgImg->width);
	        GNg::ImageDraw(zAlphaBgImg, dlgx, by2, dlgw,24,0xffffffff,0,1,zAlphaBgUV,zAlphaBgUV, u2, zAlphaBgUV+3.0f);
		    zAlphaBgUV += 0.02f * GN::TimeRatio;
		}
		GNu::eNEAR_TYPE near_type = GNg::Draw2DAlphaDirectionGet();
		GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_LEFT);
		GNg::ImageDraw(0, dlgx, by2, dlgw, 24, zColor.COLOR|0xff000000); //显示选中的颜色透明渐变
		GNg::Draw2DAlphaDirectionSet(near_type);

		if(aArrowShadow)GNg::ImageDraw(0, dlgx+1+dlgw*zAlphaBgOffset, ay2-1, 1 , 8, 0xaa000000);  //显示透明 游标箭头阴影
		                GNg::ImageDraw(0, dlgx-1+dlgw*zAlphaBgOffset, ay2  , 2 , 8, zColorArrow); //显示透明 度游标箭头
	}
		if(aArrowShadow)GNg::ImageDraw(0, dlgx+1+dlgw*zBarOffset,	  ay1-1, 1 , 8, 0xaa000000);  //显示色调 游标箭头阴影
						GNg::ImageDraw(0, dlgx-1+dlgw*zBarOffset,	  ay1  , 2 , 8, zColorArrow); //显示色调   游标箭头
	
	uint8 c_ = zColorOffsetY < 0.5f? 80 : 175;
    short cx = dlgx+dlgw*zColorOffsetX, cy = dlgy+dh*zColorOffsetY;
	GNg::LineFrame2dDraw(cx-3, cy-3, 7, 7, 1, 0xff000000|(c_<<16)|(c_<<8)|c_); //显示在色谱上的选点

	// 复制飞出色块 -----------------------------------------------------------------------------------------------------
	if(zColorClone && !zSetTpye){
        float ls = GN::TimeRatioL * zOffsetSheep;
		zColorCloneMX += ((float)tc_x - zColorCloneMX) * ls;  zColorCloneMW += ((float)tc_w - zColorCloneMW) * ls;
		zColorCloneMY += ((float)tc_y - zColorCloneMY) * ls;  zColorCloneMH += ((float)tc_h - zColorCloneMH) * ls;
		GNg::ImageDraw(0, (int)zColorCloneMX, (int)zColorCloneMY, (int)zColorCloneMW, (int)zColorCloneMH, zColorClone[0]);
		short crs_x = (short)zColorCloneMX - tc_x;
		if((crs_x <= 1 && crs_x >= -1) || (GN::TimeGetRunCount() - zLastRunTime > 600)){
			aColorPasteTable.push_back(zColorClone[0]);
			if(aColorPasteTable.size() > 32)aColorPasteTable.pop_front(); //粘贴版色表多与32个 自动忽略在前面的
            zColorClone.Delete();
		}
	}
	if(zIsMouseFocus && !aDisableControl){
		bool ctrl_on = GN::Key[GK_LCONTROL] || GN::Key[GK_RCONTROL];
		if((ctrl_on && GN::Key1[GK_C]) || (mouse_cp && last_color == zColor)){
			if(zOffsetSheep > 0.99f){
				aColorPasteTable.push_back(zColor);
				if(aColorPasteTable.size() > 32)aColorPasteTable.pop_front(); //粘贴版色表多与32个 自动忽略在前面的
			}else{
                zColorCloneMX = (float)cx, zColorCloneMY = (float)cy;
				zColorCloneMW = 7.0f;	   zColorCloneMH = 7.0f;
				zColorClone.New(1,zColor);
                zLastRunTime = GN::TimeGetRunCount();
			}
		}
		if(ctrl_on && GN::Key1[GK_V] && aColorPasteTable.size()){
			SetColor(aColorPasteTable.back()); ret = true;
		}
	}
	return ret;
}
void GNu::cPALETTE::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	GNg::sDRAW_AREA2 *_rng = (GNg::sDRAW_AREA2*)&(this->x);
	GNg::sDRAW_AREA2 rng_bk = *_rng;
	DrawRangeGet(*_rng, parent);
	uint8 last_set_type = zSetTpye; 
	unsigned int last_color = zColor;
	if(RecvInputUiObject != this)GNg::ViewCur->mouseMX += 10000;
	UpdateRun(updatetime, c);
	if(RecvInputUiObject != this)GNg::ViewCur->mouseMX -= 10000;
	else if((last_set_type && !zSetTpye) || (last_color != zColor.COLOR && !(GN::TimeGetRunFpsCount()&31))){ int colorBits; memcpy(&colorBits, &zColor, sizeof(colorBits)); UiActionCall(this, colorBits); }
	*_rng = rng_bk;
}
P<GNu::iUI_OBJECT> GNu::cPALETTE::Clone(){
	P<cPALETTE> palette(1);
	*palette = *this;
	palette->zColorImg = this->zColorImg->CopyOut(0, 0, 0, 0, false, true);
	return palette;
}
/**************************************************** sCTRL_AXIS 2D *************************************************/
P<GNg::sIMAGE> GNu::sCTRL_AXIS::zCtrlAxisImg;
unsigned short   GNu::sCTRL_AXIS::zCreateTotalNum = 0;

GNu::sCTRL_AXIS::sCTRL_AXIS():zHideMove(false), zHideRot(false), zHideScl(false),aIsVisual(true),aDisableControl(false),aShowSize(128),zPos(0.0f,0.0f),zIsLocalMode(false),aUnitCtrlMov(0.0f),aUnitCtrlRot(0.0f),aUnitCtrlScl(0.0f){
    ++zCreateTotalNum;
	if(zCreateTotalNum == 1){
		GNf_LoadFileStatePush2( 0, GNf::eFILE_STD ); //首次创建该对象就读入2D轴图片文件
		zCtrlAxisImg.New(3); 
		    zCtrlAxisImg[0].InitToData(GN_SysDir("axis/2Dctrl_mov"),1,GNg::CR_ALPHA); 
	   #ifdef GT_TMP_COMPATI_DX_DDRAW
		if(GN_Has3DAccelerate())
	   #endif
		{
		    zCtrlAxisImg[1].InitToData(GN_SysDir("axis/2Dctrl_rot"),1,GNg::CR_ALPHA);
		}
		    zCtrlAxisImg[2].InitToData(GN_SysDir("axis/2Dctrl_scl"),1,GNg::CR_ALPHA);
		GNf_LoadFileStatePop2();
	}
}
GNu::sCTRL_AXIS::~sCTRL_AXIS(){
    --zCreateTotalNum;
	if(!zCreateTotalNum)zCtrlAxisImg.Delete(); //已没这类对象就删除共享内容
}
void GNu::sCTRL_AXIS::InitToData(GNg::sCOLOR move, GNg::sCOLOR rot, GNg::sCOLOR scl){
	zGroup.New(1);
	zGroup->Init(0,0,0,GNg::Ts2D,0.5f,0.5f,1,0xffffffff,GNg::eAM_NOAM);
	GNg::sIMAGE *ctrl_img = zCtrlAxisImg;
	zAxis.New(3);
	    zAxis -> Init(ctrl_img  ,0,0,GNg::sTRANSF2D(0,0,(float)aShowSize/(float)ctrl_img -> width,(float)aShowSize/(float)ctrl_img -> height),0.5f,0.5f,1, move.COLOR,GNg::eAM_NOAM);
	#ifdef GT_TMP_COMPATI_DX_DDRAW
	if(GN_Has3DAccelerate())
	#endif
	{
	    zAxis[1].Init(ctrl_img+1,0,0,GNg::sTRANSF2D(0,0,(float)aShowSize/(float)ctrl_img[1].width,(float)aShowSize/(float)ctrl_img[1].height),0.5f,0.5f,1, rot.COLOR, GNg::eAM_NOAM);
	}
	    zAxis[2].Init(ctrl_img+2,0,0,GNg::sTRANSF2D(0,0,(float)aShowSize/(float)ctrl_img[2].width,(float)aShowSize/(float)ctrl_img[2].height),0.5f,0.5f,1, scl.COLOR, GNg::eAM_NOAM);
	
	zAxisType = 0; zSelectAxis = 0;
}
void GNu::sCTRL_AXIS::SetPos(int x, int y,float rot, bool local_mode){
	zIsLocalMode = local_mode;
	zTs.Init(true);
	zTsBak = zTs;
    zPos.x = (float)x;
	zPos.y = (float)y;
    zRot   = rot;
}
uint8 GNu::sCTRL_AXIS::Show(float *rx, float *ry){
    if(!aIsVisual)return 0;
   #ifdef GN_DEBUG
	if(!zGroup){ GN_Log("2D控制轴没初始化好! 请先调用sCTRL_AXIS::InitToData(...)\n"); }
   #endif
	uint8 ret = 0, aid = 0;
	bool  is_mouse_down = GN::Mouse1[GM_LEFT];
	float mouse_x = (float)GNg::ViewCur->mouseMX, mouse_y = (float)GNg::ViewCur->mouseMY;
	GNg::cFRAME *gp = zGroup, *axis = zAxis;
	GNg::sIMAGE *ctrl_img = zCtrlAxisImg;
	GNg::sMAT33 tm;
   #ifdef GT_TMP_COMPATI_DX_DDRAW
	bool spt_rot = GN_Has3DAccelerate(); //图形环境是否支持旋转硬件加速
	if(spt_rot)GNg::M33GetRot(&tm, zRot);
	else tm.Init(true);
    #define GT_DEVICE_SPT_ROT spt_rot&&
   #else
	#define GT_DEVICE_SPT_ROT  
	GNg::M33GetRot(&tm, zRot);
   #endif
    tm.m31 = zPos.x,  tm.m32 = zPos.y; //设置原摆放位置
	gp->aLevel_world = tm * zTsBak;    //并调整为适合大小后再组合修改矩阵来设置它的世界矩阵
	if(                  !zHideMove){ axis -> PlayFrame(16, gp); aid = 0; } //更新显示各控制轴
	if(GT_DEVICE_SPT_ROT !zHideRot ){ axis[1].PlayFrame(16, gp); aid = 1; }
	if(                  !zHideScl ){ axis[2].PlayFrame(16, gp); aid = 2; } 
	if(aDisableControl){
		if(zSelectAxis){
			zSelectAxis->aBlendmode = 1;
			zAxisType = 0; zSelectAxis = 0;
		}
		return ret;
	}
	if(is_mouse_down){ //判断选到那种控制轴 与点下去的位置
		GNg::M33GetInverse(tm, axis[aid].aLevel_world);
		float loc_x, loc_y, img_w = (float)(ctrl_img[2].width>>1), img_h = (float)(ctrl_img[2].height>>1);
		GNg::M33TransformCoord(&loc_x,&loc_y, mouse_x,mouse_y, &tm);
		if(loc_x >= -img_w && loc_x <= img_w && loc_y >= -img_h && loc_y <= img_h){
			if(!zHideScl && loc_x >= img_w-13.0f && loc_x <= img_w && loc_y >= img_h-13.0f && loc_y <= img_h){
			    zSelectAxis = axis + 2;
                zAxisType = 7; //xy整体缩放
			}else if(!zHideScl && (loc_x < -img_w+10.0f || loc_x > img_w-10.0f)){
			    zSelectAxis = axis + 2;
                zAxisType = 5; //x缩放
			}else if(!zHideScl && (loc_y < -img_h+10.0f || loc_y > img_h-10.0f)){
			    zSelectAxis = axis + 2;
                zAxisType = 6; //y缩放
			}else if(!zHideMove && loc_x >= -3.0f && loc_x <= 3.0f && loc_y >= -3.0f && loc_y <= 3.0f){
			    zSelectAxis = axis;
                zAxisType = 3; //xy偏移
				GNg::M33GetMove(&zTsBak, gp->aLevel_world.m31 - zPos.x, gp->aLevel_world.m32 - zPos.y);
				zRot = 0.0f;
			   #ifdef GT_TMP_COMPATI_DX_DDRAW
	            if(spt_rot)GNg::M33GetRot(&tm, zRot);
                else tm.Init(true);
			   #else
				GNg::M33GetRot(&tm, zRot);
			   #endif
                tm.m31 = zPos.x,  tm.m32 = zPos.y;
	            gp->aLevel_world = tm * zTsBak;
                ret = 0xff;
			}else if(!zHideMove && loc_y >= -3.0f && loc_y <= 3.0f){
			    zSelectAxis = axis;
                zAxisType = 1; //x偏移
			}else if(!zHideMove && loc_x >= -3.0f && loc_x <= 3.0f){
			    zSelectAxis = axis;
                zAxisType = 2; //y偏移
			}else{
			    GNg::sVEC2 t_v(loc_x,loc_y);
				float r = GNg::Vec2Length(&t_v);
				if(GT_DEVICE_SPT_ROT !zHideRot && r <= 54.0f && r >= 49.0f){
				    zSelectAxis = axis + 1;
                    zAxisType = 4; //旋转
				}else if(!zHideMove){
				    zSelectAxis = axis;
                    zAxisType = 3; //xy偏移
				}
			}
			if(zSelectAxis){
			   zSelectAxis->aBlendmode = 2;
		       zCp.x = mouse_x;
			   zCp.y = mouse_y;
			}
		}
	}
	if(rx)*rx = 0.0f;  if(ry)*ry = 0.0f;

	if(GN::Mouse[GM_LEFT]){ //按住鼠左键拖动修改变换 -------------------------------------------------------------------------------------------
		if(zSelectAxis && (is_mouse_down || GN::MouseM)){
			GNg::sMAT33  add_ts, to_centre_mat;
			GNg::sVEC2 front_x1,left_y1, front_x, crs_point(mouse_x, mouse_y);
			GNg::sVEC2 *_front_x = gp->aLevel_world.Row1(), *_left_y = gp->aLevel_world.Row2();
			float len_x = GNg::Vec2Length(_front_x), len_y = GNg::Vec2Length(_left_y);
			front_x1 = (*_front_x) / len_x;	//计算出当前控制轴原X坐标原轴的方向
			left_y1 =  (*_left_y ) / len_y; //计算出当前控制轴原Y坐标原轴的方向
			int uc = 1;
			if(zAxisType < 3){ // x或y偏移 --------------------------------------------
				GNg::sVEC2 tmpv = zAxisType == 1 ? front_x1 : left_y1;
				float ofs = 0.0f;
				if(!is_mouse_down){
					ofs = GN_Vec2Dot(crs_point. , tmpv.) - GN_Vec2Dot(zCp. , tmpv.);
					if(aUnitCtrlMov){
					    uc =  (int)(ofs / aUnitCtrlMov);
						ofs = (float)uc * aUnitCtrlMov;
					}
					if(uc){
                        zCp = crs_point;
						tmpv *= ofs;
						GNg::M33MultMove(&zTs,    &tmpv, true);
						GNg::M33MultMove(&zTsBak, &tmpv, true);
					}
				    ret = zAxisType ;
				}
				if(zAxisType == 1){ 
					if(rx)*rx = ofs;	//x偏移量
				}else{
					if(ry)*ry = ofs;	//y偏移量
				}
			}else if(zAxisType == 3){ // XY偏移 ----------------------------------------------------
				if(!is_mouse_down){
					float ofs_x = GN_Vec2Dot(crs_point. , front_x1.) - GN_Vec2Dot(zCp. , front_x1.);
                    float ofs_y = GN_Vec2Dot(crs_point. , left_y1.)  - GN_Vec2Dot(zCp. , left_y1.);
					int uc2 = 1;
					if(aUnitCtrlMov){
					    uc  = (int)(ofs_x  / aUnitCtrlMov);
						ofs_x = (float)uc  * aUnitCtrlMov;
						uc2 = (int)(ofs_y  / aUnitCtrlMov);
						ofs_y = (float)uc2 * aUnitCtrlMov;
					}
					if(uc || uc2){
						crs_point = front_x1 * ofs_x + left_y1 * ofs_y;
                        zCp = crs_point + zCp;
						GNg::M33MultMove(&zTs,    &crs_point, true);
						GNg::M33MultMove(&zTsBak, &crs_point, true);
					}
					ret = zAxisType;
					if(rx)*rx = ofs_x;  if(ry)*ry = ofs_y;
				}
			}else if(GT_DEVICE_SPT_ROT zAxisType == 4){ // 旋转 ------------------------------------
					if(!is_mouse_down){
						GNg::sVEC2 *_wrd_pos = gp->aLevel_world.GetPos();
						GNg::sVEC2  n1 = zCp - (*_wrd_pos), n2 = crs_point - (*_wrd_pos);
						float dot;
						GNg::Vec2ToRot(&dot,&n1,  &n2); //计算出旋转角度
						if(aUnitCtrlRot){
					        uc = (int)(dot  / aUnitCtrlRot);
					        dot = (float)uc * aUnitCtrlRot;
						}
						if(uc){
							GNg::M33GetRot(&add_ts, dot);
							n1 = -(*_wrd_pos);
							GNg::M33MultMove(&add_ts, &n1, false);
                            add_ts.m31 += _wrd_pos->x, add_ts.m32 += _wrd_pos->y;
							if(!GN_ValueIsInvalidFloat(add_ts.m11)){
				                zCp = crs_point;
				                zTs   = zTs * add_ts;
								zTsBak = zTsBak * add_ts;
								if(rx)*rx = dot;  if(ry)*ry = dot;
							}
						}
						ret =  zAxisType;
					}
			}else if(zAxisType == 5 || zAxisType == 6){ //x或y缩放 ------------------------------------
				GNg::sVEC2 posi(gp->aLevel_world.m31, gp->aLevel_world.m32), tmpv;
				float vlen, *r_;
				if(zAxisType == 5){
					tmpv = front_x1; vlen = len_x; r_ = rx;
				}else{
					tmpv = left_y1;  vlen = len_y; r_ = ry;
				}
				float dot = GN_Vec2Dot(crs_point. , tmpv.) - GN_Vec2Dot(posi. , tmpv.); //计算该点到离控制轴中心的距离
				if(!is_mouse_down){
					float ax_len = vlen * 0.5f * (float)ctrl_img[2].width;
					if(zScaleIsFront)dot /=  ax_len;
					else             dot /= -ax_len;
					if(aUnitCtrlScl){
						uc  = (dot - 1.0f) * zCurTsTotalSize / aUnitCtrlScl;
					    dot = 1.0f + (uc * aUnitCtrlScl)/zCurTsTotalSize;
					}
					if(uc){
						zCurTsTotalSize *= dot;
						GNg::M33GetInverse( to_centre_mat, gp->aLevel_world);
						GNg::M33MultScale1(&to_centre_mat, dot, zAxisType == 6, true);
                        add_ts = to_centre_mat * gp->aLevel_world;
                        zTs = zTs * add_ts;
						zTsBak = zTsBak * add_ts;
					}
					ret = zAxisType;
					if(r_) *r_ = dot;
				}else{
                    zScaleIsFront = dot > 0.0f;
					zCurTsTotalSize = 1.0f;
				}
			}else if(zAxisType == 7){ // xy整体缩放 ----------------------------------------------------
			    GNg::sVEC2 posi(gp->aLevel_world.m31, gp->aLevel_world.m32);
				float dot = GN_Vec2Dot(crs_point. , front_x1.) - GN_Vec2Dot( posi. , front_x1.);
				if(!is_mouse_down){
					float dot2 = GN_Vec2Dot(crs_point. , left_y1.) - GN_Vec2Dot(posi. , left_y1.);
					float ax_len;
					if((dot<0.0f?-dot:dot) < (dot2<0.0f?-dot2:dot2)){ //比较两方向轴的距离
					    ax_len = len_x * 0.5f * (float)ctrl_img[2].width;
					}else{
					    ax_len = len_y * 0.5f * (float)ctrl_img[2].width;
                        dot = dot2;
					}
					if(zScaleIsFront)dot /=  ax_len;
					else             dot /= -ax_len;
                    if(aUnitCtrlScl){
						uc  = (dot - 1.0f) * zCurTsTotalSize / aUnitCtrlScl;
					    dot = 1.0f + (uc * aUnitCtrlScl) / zCurTsTotalSize;
					}
					if(uc){
						zCurTsTotalSize *= dot;
					    GNg::M33GetInverse( to_centre_mat, gp->aLevel_world);
						GNg::M33MultScale(&to_centre_mat, dot, dot, true);
                        add_ts = to_centre_mat * gp->aLevel_world;
                        zTs = zTs * add_ts;
						zTsBak = zTsBak * add_ts;
					}
					ret = zAxisType;
					if(rx)*rx = dot;  if(ry)*ry = dot;
				}else {
					zScaleIsFront = dot > 0.0f;
					zCurTsTotalSize = 1.0f;
				}
			}
		}
	}else{
		if(zSelectAxis){
			if(!zIsLocalMode) //如是按世界坐标轴方向来变换的时 每次操作完一步就把控制轴的方向清除
				GNg::M33GetMove(&zTsBak, gp->aLevel_world.m31 - zPos.x, gp->aLevel_world.m32 - zPos.y);
			zSelectAxis->aBlendmode = 1;
			zAxisType = 0; zSelectAxis = 0;
		}else if(GN::Key1[GK_T]){ //按左或右Shift键都强制按世界坐标轴方向来变换
				GNg::M33GetMove(&zTsBak, gp->aLevel_world.m31 - zPos.x, gp->aLevel_world.m32 - zPos.y);
			zRot = 0.0f;
			return 0xff;
		}
	}
	return ret;
}
void GNu::sCTRL_AXIS::GetLastTranslate(GNg::sMAT33 *m_out, bool clear){
	*m_out = zTs;
	if(clear)zTs.Init(true);
}
void GNu::sCTRL_AXIS::GetCoordForSXY(float &scx, float &scy){
	GNg::sVEC2 pos((float)((zCtrlAxisImg[2].width>>1)-7), (float)((zCtrlAxisImg[2].height>>1)-7));
	GNg::M33TransformCoord(&pos, &(zAxis[2].aLevel_world));
    scx = pos.x, scy = pos.y;
}
GNg::sMAT33 *GNu::sCTRL_AXIS::GetWorldMatrix(){
   #ifdef GN_DEBUG
	if(!zGroup)GN_Log("2D控制轴还没初始化好! 请先调用 sCTRL_AXIS::InitToData(...)\n");
   #endif
	return &(zGroup->aLevel_world);
}
/************************************************************* 对话汽泡 *************************************************************************************/
void GNu::sBUBBLE::InitToData(P<GNg::sIMAGE> image9, P<GNg::sIMAGE> arrow, unsigned int  color, unsigned short blend){
    zImage9 = image9;
	zArrow  = arrow;
	aColor  = color; aBlend  = blend;
}
void GNu::sBUBBLE::SetFrameStyle(P<GNg::sIMAGE> image9, P<GNg::sIMAGE> arrow, unsigned int colorf, unsigned short blend, int fw){
    zImage9Frame = image9;
    zArrowFrame  = arrow;
	aColorFrame  = colorf;
	aBlendFrame  = blend;
	aFw          = fw;
}
void GNu::sBUBBLE::Show(int ax, int ay, int x,int y,int w,int h){
	GNg::sVEC2 av((float)(ax - x) - ((float)w * 0.5f), (float)(ay - y) - ((float)h * 0.5f));
    Show(av, x, y, w, h);
}
void GNu::sBUBBLE::Show(GNg::sVEC2 &v, int x,int y,int w,int h){
   #ifdef GN_DEBUG
    if(!zImage9 || zArrow)GN_Log("对话汽泡还没初始化好! 请先调用 sBUBBLE::InitToData(...)\n");
   #endif
	GNg::sIMAGE *arrw = zArrow, *arrwframe = zArrowFrame;
    bool show_frame = arrwframe && zImage9Frame && ((aColor&0xff000000)==0xff000000) && (aBlend <= 1);
	GNg::ImageDraw9Square(zImage9, x,y,w,h, aColor,aBlend);
    if(show_frame)GNg::ImageDraw9Square(zImage9Frame, x,y,w,h, aColorFrame,aBlendFrame);
	GNg::sVEC2 vnz, vnz2;
	GNg::Vec2Normalize(&vnz, &v);
	GNg::Vec2Right(&vnz2, &vnz);

	float vexs[8];
	float  ah = (float)arrw->height, ac;
	float _ah = ah * 0.5f, ah2 = ah * 2.0f,
		  _w = (float)w * 0.5f, _h = (float)h * 0.5f;
	float cx = (float)x + _w, cy = (float)y + _h;
	GNg::cVIEW2D *view = GNg::ViewCur;
	GNg::sVEC2 v1_ = vnz2 * (-ah), _1v = vnz * (float)arrw->width;
	int st; //计算绘制旋转箭头的首点在那里
	switch(GNg::ComputeRectNearFromDirection(w, h, vnz)){ //分析箭头靠那边
	case GNu::eNEAR_LEFT:
		st = x + aFw;
        ac = ((- _w) / vnz.x) * vnz.y;
		if(vnz.y < 0.0f){
		    ac += _ah;
            if(ac < -_h + ah2)ac = -_h + ah2;
            vexs[0] = (float)st;
			vexs[1] = cy + ac;
		}else{
		    ac -= _ah;
			if(ac >  _h - ah2)ac =  _h - ah2;
			vexs[0] = (float)st + v1_.x;
			vexs[1] = cy + ac + v1_.y;
		}
        view->ScissorRectSetup(st - arrw->width, view->ortho_top ,arrw->width,view->orthoH);
		break;
	case GNu::eNEAR_RIGHT:
		st = x + w - aFw;
        ac = (_w / vnz.x) * vnz.y;
		if(vnz.y < 0.0f){
		    ac += _ah;
            if(ac < -_h + ah2)ac = -_h + ah2;
			vexs[0] = (float)st + v1_.x;
			vexs[1] = cy + ac + v1_.y;
		}else{
		    ac -= _ah;
			if(ac >  _h - ah2)ac =  _h - ah2;
			vexs[0] = (float)st;
			vexs[1] = cy + ac;
		}
		view->ScissorRectSetup(st, view->ortho_top ,arrw->width,view->orthoH);
		break;
	case GNu::eNEAR_TOP:
		st = y + aFw;
        ac = ((- _h) / vnz.y) * vnz.x;
		if(vnz.x < 0.0f){
		    ac += _ah;
            if(ac < -_w + ah2)ac = -_w + ah2;
			vexs[0] = cx + ac + v1_.x;
			vexs[1] = (float)st + v1_.y;
		}else{
		    ac -= _ah;
			if(ac >  _w - ah2)ac =  _w - ah2;
			vexs[0] = cx + ac;
			vexs[1] = (float)st;
		}
		view->ScissorRectSetup(view->ortho_left, st - arrw->width ,view->orthoW,arrw->width);
		break;
	case GNu::eNEAR_BOTTOM:
		st = y + h - aFw;
        ac = (_h / vnz.y) * vnz.x;
		if(vnz.x < 0.0f){
		    ac += _ah;
            if(ac < -_w + ah2)ac = -_w + ah2;
			vexs[0] = cx + ac;
			vexs[1] = (float)st;
		}else{
		    ac -= _ah;
			if(ac >  _w - ah2)ac =  _w - ah2;
			vexs[0] = cx + ac + v1_.x;
			vexs[1] = (float)st + v1_.y;
		}
		view->ScissorRectSetup(view->ortho_left, st ,view->orthoW,arrw->width);
		break;
	}
    v1_ = -(v1_);
    vexs[2] = vexs[0] + _1v.x;
	vexs[3] = vexs[1] + _1v.y;
	vexs[4] = vexs[2] + v1_.x;
	vexs[5] = vexs[3] + v1_.y;
	vexs[6] = vexs[0] + v1_.x;
	vexs[7] = vexs[1] + v1_.y;
	GNg::ImageDraw4P(arrw, vexs, aColor,0,aBlend, 0.0f,0.0f, arrw->uRange,arrw->vRange);
    if(show_frame)GNg::ImageDraw4P(arrwframe, vexs, aColorFrame,0,aBlendFrame, 0.0f,0.0f, arrwframe->uRange,arrwframe->vRange);
    view->ScissorRectRemove();
}
bool GNu::sBUBBLE::LoadDefStyle(P<GNg::sIMAGE> &image9,  P<GNg::sIMAGE> &arrow, P<GNg::sIMAGE> &image9f, P<GNg::sIMAGE> &arrowf){
	image9.New(1); arrow.New(1); image9f.New(1); arrowf.New(1);
	if(!GNg::ImageLoad(GN_SysDir("ui/bubbleB"),  9, image9,  GNg::CR_ALPHA))return false;
    if(!GNg::ImageLoad(GN_SysDir("ui/bubbleBF"), 9, image9f, GNg::CR_ALPHA))return false;
	if(!GNg::ImageLoad(GN_SysDir("ui/bubbleA"),  1, arrow,   GNg::CR_ALPHA))return false;
	if(!GNg::ImageLoad(GN_SysDir("ui/bubbleAF"), 1, arrowf,  GNg::CR_ALPHA))return false;
	return true;
}
/****************************************************************************************************************************/
P<GNu::iUI_OBJECT> GNu::cBUTTON_TOUCH::Clone(){
	P<cBUTTON_TOUCH> ui_bt(1);
	*ui_bt = *this;

	return ui_bt;
}
void GNu::cBUTTON_TOUCH::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	GNg::sDRAW_AREA2 rng;
	cUI_IMAGE::DrawRangeGet(rng, parent);
	short aRngL = rng.x, aRngT = rng.y, aRngR, aRngB;
	if(rng.w < 0){ aRngR = rng.x; aRngL = rng.x + rng.w; }
	else aRngR = rng.x + rng.w;
	if(rng.h < 0){ aRngB = rng.y; aRngT = rng.y + rng.h; }
	else aRngB = rng.y + rng.h;

	#define GT_TMP_BUTTON_T_UPDATE_RUN( CMP ) \
    if(zTid == 255){\
        CMP zTid = GNg::ViewCur->GetRangeTouchDownId(aRngL, aRngT, aRngR, aRngB);\
        if(zTid != 255){\
            if(aMappingKey)GF_SetKeyState(aMappingKey, 1);\
            aTouchZ = 255;\
        }else aTouchZ = 0;\
    }else{\
        aTouchZ = GN::TouchZ[zTid];\
        GNg::cVIEW2D *_view = GNg::ViewCur;\
        if(aTouchZ <= 1 || _view->touchX[zTid] < aRngL || _view->touchX[zTid] > aRngR || _view->touchY[zTid] < aRngT || _view->touchY[zTid] > aRngB){\
            if(aMappingKey)GF_SetKeyState(aMappingKey, 0);\
            zTid = 255;\
        }\
    }
	GT_TMP_BUTTON_T_UPDATE_RUN( if(RecvInputUiObject == this) )
	GNg::ImageDraw(img,rng.x,rng.y,rng.w,rng.h,rng.c);
}
void GNu::cBUTTON_T::UpdateRun(unsigned int updatetime){
	GT_TMP_BUTTON_T_UPDATE_RUN(  )
    GNg::ImageDraw(aIcon, aRngL, aRngT, aRngR-aRngL, aRngB-aRngT, aColor, 0, aBlend);
}
void GNu::cJOYSTICK::zReCptNearRange(){
    GNg::cVIEW2D *_view = GNg::ViewCur;
    if(zVpUV.x < 0.5f){ zRngR = GNu::NearUx(zVpUV.x); zRngL = _view->ortho_left;   }
    else              { zRngL = GNu::NearUx(zVpUV.x); zRngR = _view->ortho_right;  }
    if(zVpUV.y < 0.5f){ zRngB = GNu::NearVy(zVpUV.y); zRngT = _view->ortho_top;    }
    else              { zRngT = GNu::NearVy(zVpUV.y); zRngB = _view->ortho_bottom; }
}
void GNu::cJOYSTICK::zInit(float back_disk_r){
    if(back_disk_r){
        if(back_disk_r > 0.0f)zVectorScl = 1.0f / back_disk_r;
    }else{
        short w = zRngR - zRngL, h = zRngB - zRngT;
        zVectorScl = 1.0f / (float)((w<h?w:h)>>2);
    }
    zTid = 255;
    aVector = GNg::Vector2D;
    aPressure = 0.0f;
}
P<GNu::iUI_OBJECT> GNu::cJOYSTICK::Clone(){
	P<cJOYSTICK> ui_joy(1);
	*ui_joy = *this;
	return ui_joy;
}
void GNu::cJOYSTICK::UpdateRun(unsigned int updatetime, unsigned int color){
    GNg::cVIEW2D *_view = GNg::ViewCur;
    if(zVpUV.x > 0.0f && GN::ViewReSized)zReCptNearRange();
    //GNg::ImageDraw(0, zRngL, zRngT, zRngR - zRngL, zRngB - zRngT, 0x6600ff00);
    if(zTid == 255){
        zTid = _view->GetRangeTouchDownId(zRngL, zRngT, zRngR, zRngB);
        if(zTid != 255){ 
            zCpX = _view->touchX[zTid];
            zCpY = _view->touchY[zTid];
        }
    }else{
        if(GN::TouchZ[zTid] <= 1){
            zTid = 255;
            aVector = GNg::Vector2D;
            aPressure = 0.0f;
        }else{
            aVector.x = (float)(_view->touchX[zTid] - zCpX) * zVectorScl;
            aVector.y = (float)(_view->touchY[zTid] - zCpY) * zVectorScl;
            aPressure = (float)GN::TouchZ[zTid] / 254.0f;
            
            float vlen = GNg::Vec2Length(&aVector);
            if(vlen > 1.0f)aVector /= vlen;
            if(vlen > aVectorMaxLen){
                GNg::sVEC2 cpofs = aVector * ((vlen - aVectorMaxLen) / (vlen*zVectorScl));
                zCpX += (short)cpofs.x;
                zCpY += (short)cpofs.y;
            }
			unsigned int bc = GNg_MultiplyColorRatio(aBackDiskColor,color);
            if(bc&0xff000000){
				GNg::sIMAGE *back_img = aBackDiskImg ? aBackDiskImg : GN::RunRec.aGV_Point32;
                GNg::ImageDraw(back_img,zCpX-(back_img->width>>1),zCpY-(back_img->height>>1),0,0,bc);
            }
            if(aArrowImg){
                GNg::sIMAGE *arrow_img = aArrowImg;
                GNg::sVEC2 ax, ay = (vlen < 1.0f? aVector / vlen : aVector) * (float)arrow_img->width;
                GNg::Vec2Right(&ax, &ay);
                GNg::sVEC2 cp1 = (aVector / zVectorScl) + GNg::sVEC2((float)zCpX,(float)zCpY);
                GNg::sVEC2 vts[4];
                vts[0] = cp1 + ((ay - ax) * 0.5f);
                vts[1] = vts[0] + ax;
                vts[2] = vts[1] - ay;
                vts[3] = vts[2] - ax;
                GNg::ImageDraw4P(arrow_img, (float*)vts, GNg_MultiplyColorRatio(aArrowColor,color), 0, 1, 0.0f, 0.0f, arrow_img->uRange, arrow_img->vRange);
            }
        }
        if(aVecMappingKeyType){
            short kl, kt, kr, kb;
            if(aVecMappingKeyType == 1){ kl = GK_A, kt = GK_W, kr = GK_D, kb = GK_S; }
            else                       { kl = GK_LEFT, kt = GK_UP, kr = GK_RIGHT, kb = GK_DOWN; }
            bool pres = aVector.x < -aVectorMappingRef;
            if(pres != GN::Key[kl])GF_SetKeyState(kl,pres);
                 pres = aVector.x >  aVectorMappingRef;
            if(pres != GN::Key[kr])GF_SetKeyState(kr,pres);
                 pres = aVector.y < -aVectorMappingRef;
            if(pres != GN::Key[kt])GF_SetKeyState(kt,pres);
                 pres = aVector.y >  aVectorMappingRef;
            if(pres != GN::Key[kb])GF_SetKeyState(kb,pres);
            
            if(aPressMappingKey){
                pres = aPressure > aPressMappingRef;
                if(pres != GN::Key[aPressMappingKey])GF_SetKeyState(aPressMappingKey,pres);
            }
        }
    }
}										
void GNu::cJOYSTICK::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	UpdateRun(updatetime, parent->c);
	if(RecvInputUiObject != this)zTid = 255;
}
void GNu::cJOYSTICK::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent){
	out.x = zRngL; out.y = zRngT; out.w = zRngR - zRngL; out.h = zRngB - zRngT;
	out.c = parent?parent->gc:0xffffffff;
}
/*************************************************************************************************************************************************/
uint8* GNu::cSCROLL_CTRL::zDataSave(uint8 *wpos, GN::sLIST< P<GNg::sIMAGE> > &img_collect){
	memcpy(wpos, &zOffsetSheep,		  4);	wpos += 4;
	memcpy(wpos, &zTextOffset,		  4);	wpos += 4;
	memcpy(wpos, &aFocusColor,		  4);	wpos += 4;
	memcpy(wpos, &aFocusBlend,		  2);	wpos += 2;
	memcpy(wpos, &zRepeatNum,		  1);	wpos += 1;
	memcpy(wpos, &(zFev.segment_time),4);	wpos += 4;
	unsigned int pointer_id;
	if( zOffsetArrow && zOffsetArrow != GN::RunRec.aGV_OffsetArrow){
		pointer_id = img_collect.add_no_eq2( zOffsetArrow );
	}else pointer_id = 0xffffffff;
    memcpy(wpos, &pointer_id, 4); wpos += 4;
	return wpos;
}
uint8* GNu::cSCROLL_CTRL::zDataLoad(uint8 *rpos, P< P<GNg::sIMAGE> >		  &img_refs){
	memcpy(&zOffsetSheep,		rpos, 4);	rpos += 4;
	memcpy(&zTextOffset,		rpos, 4);	rpos += 4;  zTextOffsettg = zTextOffset;
	memcpy(&aFocusColor,		rpos, 4);	rpos += 4;
	memcpy(&aFocusBlend,		rpos, 2);	rpos += 2;
	memcpy(&zRepeatNum,			rpos, 1);	rpos += 1;
	memcpy(&(zFev.segment_time),rpos, 4);	rpos += 4;
	unsigned int pointer_id;
	memcpy(&pointer_id,    rpos, 4);	rpos += 4;
	if(pointer_id == 0xffffffff)zOffsetArrow = GN::RunRec.aGV_OffsetArrow;
	else zOffsetArrow = img_refs[pointer_id];
	GT_UiInitArrowAm();
	return rpos;
}
unsigned int GNu::cSCROLL_RECT::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	unsigned int gsize = cUI_GROUP::DataSave(share_objs, data_out);
	uint8 loc_buf[64];
	if(!data_out)data_out = loc_buf;
	else data_out += gsize;
	uint8 *wpos = data_out;
	wpos = cSCROLL_CTRL::zDataSave(wpos, *(share_objs->pimage_list)); 
	memcpy(wpos, &zMaxOffset,	4);	  wpos += 4;
	return (wpos - data_out) + gsize;
}
unsigned int GNu::cSCROLL_RECT::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	uint8 *rpos = (uint8*)data; if(rpos[0] > 1)return 0;
	rpos += cUI_GROUP::DataLoad(share_objs, rpos);
	rpos = cSCROLL_CTRL::zDataLoad(rpos, *(share_objs->pimage_refs)); 
	memcpy(&zMaxOffset, rpos,	4);	  rpos += 4;
	return rpos - data;
}
void GNu::cSCROLL_LIST::zSpecClone(){
	cSCROLL_CTRL::zSpecClone();
	zDrawRangeData.Delete();
	P<GNg::cFRAME_LAUNCHER_FONT> fframes2(1);
	fframes2->PerfectCopy(*((GNg::cFRAME_LAUNCHER_FONT*)zFFrames));
	zFFrames = fframes2;
	zReArrange = true;
}
void GNu::cSELECT_LIST_ICON::zSpecClone(){
	cSCROLL_LIST::zSpecClone();
	zUsArg = this;
}
uint8* GNu::cSCROLL_LIST::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs){
	GT_TMP_SCROLL_LIST_DATA_SAVE( zDefaultFrameAct )
	memcpy(wpos, &aShowBG,		1);	wpos += 1;
	memcpy(wpos, &aShowInFrame, 1);	wpos += 1;
	return wpos;
}
uint8* GNu::cSCROLL_LIST::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs){
	GT_TMP_SCROLL_LIST_DATA_LOAD
		GNg::cFRAME *_fm_icon = zFrameIcon;
		if(!_fm_icon->zObjsKeep && _fm_icon->zFEvent){
			zFev2.New(_fm_icon->zFEvent, _fm_icon->zEventcount);
			_fm_icon->zFEvent = zFev2;
			delete [] (sdr->pframes[pointer_id]->zFEvent); /*这个通常不共享 复制数据后就可以删除*/
			sdr->pframes[pointer_id]->zFEvent = 0;
		}
	GT_TMP_LOAD_FONT_FRAME_TRANSFER
	memcpy(&aShowBG,	  rpos,  1);	rpos += 1;
	memcpy(&aShowInFrame, rpos,  1);	rpos += 1;
	return rpos;
}
uint8* GNu::cSELECT_LIST_ICON::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs){
	wpos = cSCROLL_LIST::zDataSave(wpos, share_objs);
	unsigned int pointer_id;
	GT_TMP_OBJECT_WRITE_INDEX( (*share_objs->pimage_list) , zIcons );
	memcpy(wpos, &zMultSelect,		1);	  wpos += 1;
	memcpy(wpos, &aSelected,		4);	  wpos += 4;
	memcpy(wpos, &aSelectFrameStyle,2);	  wpos += 2;
	memcpy(wpos, &aLineWidth,		2);	  wpos += 2;
	return wpos;
}
uint8* GNu::cSELECT_LIST_ICON::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs){
	rpos = cSCROLL_LIST::zDataLoad(rpos, share_objs);
	unsigned int pointer_id;
	memcpy(&pointer_id,    rpos, 4);	rpos += 4;
	if(pointer_id != 0xffffffff)zIcons  = (*(share_objs->pimage_refs))[pointer_id];
	memcpy(&zMultSelect,	  rpos, 1);	  rpos += 1;
	memcpy(&aSelected,		  rpos, 4);	  rpos += 4;  zSelectedLast = aSelected;
	memcpy(&aSelectFrameStyle,rpos, 2);	  rpos += 2;
	memcpy(&aLineWidth,		  rpos, 2);	  rpos += 2;
	return rpos;
}
void GNu::cSELECT_LIST_TEXT::zSpecClone(){
	cSELECT_LIST_ICON::zSpecClone();
}
uint8* GNu::cSELECT_LIST_TEXT::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs){
	wpos = cSELECT_LIST_ICON::zDataSave(wpos, share_objs);
	memcpy(wpos, &aTextColor,		4);	  wpos += 4;
	memcpy(wpos, &aBackColor,		4);	  wpos += 4;
	memcpy(wpos, &aBackStyle,		2);	  wpos += 2;
	memcpy(wpos, &aAffectColor,		1);	  wpos += 1;
	unsigned int pointer_id;
	GT_TMP_TEXTSTYLE_WRITE_INDEX( share_objs->text_style_list , zTextOut )
	GT_TMP_OBJECT_WRITE_INDEX( ((GT_cUI_SHARE_DATA_W*)share_objs)->list_texts , zLabels );
	return wpos;
}
uint8* GNu::cSELECT_LIST_TEXT::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs){
	rpos = cSELECT_LIST_ICON::zDataLoad(rpos, share_objs);
	memcpy(&aTextColor,	  rpos, 	4);	  rpos += 4;
	memcpy(&aBackColor,	  rpos, 	4);	  rpos += 4;
	memcpy(&aBackStyle,	  rpos, 	2);	  rpos += 2;
	memcpy(&aAffectColor, rpos, 	1);	  rpos += 1;
	unsigned int pointer_id;
	GT_TMP_TEXTSTYLE_READ_INDEX(share_objs->text_style_refs , zTextOut )
	memcpy(&pointer_id,   rpos,		4);	  rpos += 4;
	if(pointer_id != 0xffffffff)zLabels  = ((GT_cUI_SHARE_DATA_R*)share_objs)->ptexts_s[pointer_id];
	return rpos;
}
unsigned int GNu::cSELECT_LIST_ICON::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	GT_TMP_PACK_DATA_SAVE( 1160 , cSELECT_LIST_ICON )
	return wpos - data_out;
}
unsigned int GNu::cSELECT_LIST_ICON::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	GT_TMP_PACK_DATA_LOAD( cSELECT_LIST_ICON )
	if(zIcons){
		zZoningNum = zIcons->texnum;
		zCheckResetZoningCount(GNg::cFRAME_LAUNCHER_FONT::aPicIndex, zFFrames);
		cSCROLL_LIST::SetZoningDrawCall(GT_SelectListIconDrawCall, this);
	}else{ zZoningNum = 0; zFFrames->zMax_num = 0; }
	return rpos - data;
}
unsigned int GNu::cSELECT_LIST_TEXT::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	GT_TMP_PACK_DATA_SAVE( 1200 , cSELECT_LIST_TEXT )
	return wpos - data_out;
}
unsigned int GNu::cSELECT_LIST_TEXT::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	GT_TMP_PACK_DATA_LOAD( cSELECT_LIST_TEXT ) 
	if(zLabels){
		zZoningNum = zLabels.Num();
		GT_TMP_SCROLL_LIST_ZONING_RESET
		cSCROLL_LIST::SetZoningDrawCall(GT_SelectListTextDrawCall, this);
		zArrowAm.aFrame_ts.sx = (float)zTextOut->zParame.height / (float)zOffsetArrow->width;
	}else{ zZoningNum = 0; zFFrames->zMax_num = 0; }
	return rpos - data;
}
uint8* GNu::cSELECT_CALL::zDataSave(uint8 *wpos){
	memcpy(wpos, &aSelect,   2);	wpos += 2;
	unsigned short num = zHots.Num();
	memcpy(wpos, &num,		 2);	wpos += 2;
	return wpos;
}
uint8* GNu::cSELECT_CALL::zDataLoad(uint8 *rpos){
	memcpy(&aSelect,   rpos, 2);	rpos += 2;
	unsigned short num;
	memcpy(&num,	   rpos, 2);	rpos += 2;
	if(num)Init(num);
	return rpos;
}
uint8* GT_sUI_STYLE_BASE::zDataSave(uint8 *wpos){
	memcpy(wpos, &aAmSheep,		   4);	 wpos += 4;
	memcpy(wpos, &aFocusColor,     4);	 wpos += 4;
	memcpy(wpos, &aStride,		   2);	 wpos += 2;
	return wpos;
}
uint8* GT_sUI_STYLE_BASE::zDataLoad(uint8 *rpos){
	memcpy(&aAmSheep,		 rpos, 4);   rpos += 4;
	memcpy(&aFocusColor,     rpos, 4);   rpos += 4;
	memcpy(&aStride,		 rpos, 2);   rpos += 2;
	return rpos;
}
unsigned int GNu::cUI_PAGE_BAR::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	uint8 loc_buf[360];
	if(!data_out)data_out = loc_buf;
	uint8 *wpos = data_out;
	*wpos = 1; ++wpos; //版本
	wpos = zNearAreaDataSave(wpos);
	wpos = GT_sUI_STYLE_BASE::zDataSave(wpos);
	wpos = cSELECT_CALL::zDataSave(wpos);
	unsigned int pointer_id;
	GT_TMP_TEXTSTYLE_WRITE_INDEX( share_objs->text_style_list , zTextOut )
	GT_TMP_OBJECT_WRITE_INDEX( ((GT_cUI_SHARE_DATA_W*)share_objs)->list_texts , zNames );
	memcpy(wpos, &zL,		  2);	wpos += 2;
	memcpy(wpos, &zH,		  2);	wpos += 2;
	memcpy(wpos, &aAutoLayerObjShow, 1);  wpos += 1;
	memcpy(wpos, &aFontLineW, 1);	wpos += 1;
	memcpy(wpos, &aBackColor, 4);	wpos += 4;
	memcpy(wpos, &aLineColor, 4);	wpos += 4;
	return wpos - data_out;
}
unsigned int GNu::cUI_PAGE_BAR::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
   #define GT_TMP_UI_PAGE_BAR_DATALOAD_ \
	uint8 *rpos = (uint8*)data;\
	uint8 version = *rpos; ++rpos;  if(version > 1)return 0; /*版本*/\
	rpos = zNearAreaDataLoad(rpos);\
	rpos = GT_sUI_STYLE_BASE::zDataLoad(rpos);\
	rpos = cSELECT_CALL::zDataLoad(rpos);

	GT_TMP_UI_PAGE_BAR_DATALOAD_
	unsigned int pointer_id;
	GT_TMP_TEXTSTYLE_READ_INDEX(share_objs->text_style_refs , zTextOut )
	memcpy(&pointer_id,   rpos,		4);	  rpos += 4;
	if(pointer_id != 0xffffffff)zNames = ((GT_cUI_SHARE_DATA_R*)share_objs)->ptexts_s[pointer_id];
	memcpy(&zL,				   rpos, 2);  rpos += 2;
	memcpy(&zH,				   rpos, 2);  rpos += 2;
	memcpy(&aAutoLayerObjShow, rpos, 1);  rpos += 1;
	memcpy(&aFontLineW,		   rpos, 1);  rpos += 1;
	memcpy(&aBackColor,		   rpos, 4);  rpos += 4;
	memcpy(&aLineColor,		   rpos, 4);  rpos += 4;
	return rpos - data;
}
unsigned int GNu::cUI_SELECT_BUTTON::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	uint8 loc_buf[320];
	if(!data_out)data_out = loc_buf;
	uint8 *wpos = data_out;
	*wpos = 1; ++wpos; //版本
	wpos = zNearAreaDataSave(wpos);
	wpos = GT_sUI_STYLE_BASE::zDataSave(wpos);
	wpos = cSELECT_CALL::zDataSave(wpos);
	unsigned int pointer_id;
	GT_TMP_OBJECT_WRITE_INDEX( (*share_objs->pimage_list) , zImg );
	memcpy(wpos, &aIsErectArrange, 1);	wpos += 1;
	return wpos - data_out;
}
unsigned int GNu::cUI_SELECT_BUTTON::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	GT_TMP_UI_PAGE_BAR_DATALOAD_
	unsigned int pointer_id;
	memcpy(&pointer_id,    rpos, 4);	rpos += 4;
	if(pointer_id != 0xffffffff)zImg  = (*(share_objs->pimage_refs))[pointer_id];
	memcpy(&aIsErectArrange, rpos, 1);	rpos += 1;
	return rpos - data;
}
unsigned int GNu::cPALETTE::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	uint8 loc_buf[8192];
	if(!data_out)data_out = loc_buf;
	uint8 *wpos = data_out;
	*wpos = 1; ++wpos; //版本
	wpos = zNearAreaDataSave(wpos);
	memcpy(wpos, &zOffsetSheep,	  4);  wpos += 4;
	memcpy(wpos, &zColor,		  4);  wpos += 4;
	memcpy(wpos, &zColorArrow,	  4);  wpos += 4;
	memcpy(wpos, &aAlphaSet,	  1);  wpos += 1;
	memcpy(wpos, &aArrowShadow,	  1);  wpos += 1;
	memcpy(wpos, &aAlphaBgShow,	  1);  wpos += 1;
	memcpy(wpos, &aColorTablePosType,1); wpos += 1;
	unsigned int csize = aTable.Size();
	*((unsigned int*)wpos) = csize; wpos += 4;
	if(csize){
		memcpy(wpos, (GNg::sCOLOR*)aTable, csize); wpos += csize;
	}
	return wpos - data_out;
}
unsigned int GNu::cPALETTE::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	uint8 *rpos = (uint8*)data;
	uint8 version = *rpos; ++rpos;  if(version > 1)return 0; //版本
	rpos = zNearAreaDataLoad(rpos);
	memcpy(&zOffsetSheep, rpos,   4);  rpos += 4;
	memcpy(&zColor,		  rpos,   4);  rpos += 4;
	memcpy(&zColorArrow,  rpos,   4);  rpos += 4;
	memcpy(&aAlphaSet,	  rpos,   1);  rpos += 1;
	memcpy(&aArrowShadow, rpos,   1);  rpos += 1;
	memcpy(&aAlphaBgShow, rpos,   1);  rpos += 1;
	memcpy(&aColorTablePosType, rpos,1); rpos += 1;
	unsigned int csize;
	memcpy(&csize,		  rpos,   4);  rpos += 4;
	if(csize){
		aTable.New(csize>>2);
		memcpy((GNg::sCOLOR*)aTable, rpos, csize); rpos += csize;
	}
	GT_TMP_PALETTE_INIT_TO_DATA( zColor )
	return rpos - data;
}
unsigned int GNu::cSLIDER::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	unsigned int size = cUI_IMAGE::DataSave(share_objs, data_out);
	if(data_out){
		data_out += size;
		memcpy(data_out, &aAmSheep,	   4);  data_out += 4;
		memcpy(data_out, &aFocusColor, 4);  data_out += 4;
		memcpy(data_out, &aPressColor, 4);  data_out += 4;
		memcpy(data_out, &aIconColor,  4);  data_out += 4;
		memcpy(data_out, &aIconBlend,  2);  data_out += 2;
		memcpy(data_out, &aIconW,	   2);  data_out += 2;
		memcpy(data_out, &aIconH,	   2);  data_out += 2;
		memcpy(data_out, &zOfs,		   2);  data_out += 2;
	}
	return size + 24;
}
unsigned int GNu::cSLIDER::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	unsigned int size = cUI_IMAGE::DataLoad(share_objs, data);
	if(!size)return 0;
	uint8 *rpos = (uint8*)data + size;
	memcpy(&aAmSheep,	 rpos, 4);  rpos += 4;
	memcpy(&aFocusColor, rpos, 4);  rpos += 4;
	memcpy(&aPressColor, rpos, 4);  rpos += 4;
	memcpy(&aIconColor,  rpos, 4);  rpos += 4;
	memcpy(&aIconBlend,  rpos, 2);  rpos += 2;
	memcpy(&aIconW,		 rpos, 2);  rpos += 2;
	memcpy(&aIconH,		 rpos, 2);  rpos += 2;
	memcpy(&zOfs,		 rpos, 2);  rpos += 2;
	return size + 24;
}
unsigned int GNu::cBUTTON_TOUCH::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	unsigned int size = cUI_IMAGE::DataSave(share_objs, data_out);
	if(data_out){
		data_out[size]   = zTid;
		data_out[size+1] = aTouchZ;
		data_out[size+2] = aMappingKey;
	}
	return size + 3;
}
unsigned int GNu::cBUTTON_TOUCH::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	unsigned int size = cUI_IMAGE::DataLoad(share_objs, data);
	if(!size)return 0;
	zTid		= data[size];
	aTouchZ     = data[size+1];
	aMappingKey = data[size+2];
	return size + 3;
}
unsigned int GNu::cJOYSTICK::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	uint8 loc_buf[64];
	if(!data_out)data_out = loc_buf;
	uint8 *wpos = data_out;
	*wpos = 1; ++wpos; //版本
	*((unsigned int*)wpos) = aType;	wpos += 4;
	*((short	   *)wpos) = zRngL;	wpos += 2;
	*((short	   *)wpos) = zRngT;	wpos += 2;
	*((short	   *)wpos) = zRngR;	wpos += 2;
	*((short	   *)wpos) = zRngB;	wpos += 2;
	memcpy(wpos, &zVpUV,		8);	wpos += 8;
	memcpy(wpos, &zVectorScl,   4); wpos += 4;
	unsigned int pointer_id;
	GT_TMP_OBJECT_WRITE_INDEX( (*share_objs->pimage_list) , aBackDiskImg );
	GT_TMP_OBJECT_WRITE_INDEX( (*share_objs->pimage_list) , aArrowImg );
	memcpy(wpos, &aBackDiskColor,	 4); wpos += 4;
	memcpy(wpos, &aArrowColor,		 4); wpos += 4;
	memcpy(wpos, &aVectorMaxLen,     4); wpos += 4;
	memcpy(wpos, &aVectorMappingRef, 4); wpos += 4;
	memcpy(wpos, &aPressMappingRef,  4); wpos += 4;
	memcpy(wpos, &aVecMappingKeyType,1); wpos += 1;
	memcpy(wpos, &aPressMappingKey,  1); wpos += 1;
	return wpos - data_out;
}
unsigned int GNu::cJOYSTICK::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	uint8 *rpos = (uint8*)data;
	uint8 version = *rpos; ++rpos; if(version > 1)return 0; /*版本*/
	aType = *((unsigned int*)rpos);	rpos += 4;
	zRngL = *((short	   *)rpos); rpos += 2;
	zRngT = *((short	   *)rpos);	rpos += 2;
	zRngR = *((short	   *)rpos);	rpos += 2;
	zRngB = *((short	   *)rpos);	rpos += 2;
	memcpy(&zVpUV,		rpos, 8);	rpos += 8;
	memcpy(&zVectorScl, rpos, 4);	rpos += 4;
	unsigned int pointer_id;
	memcpy(&pointer_id,    rpos, 4);	rpos += 4;
	if(pointer_id != 0xffffffff)aBackDiskImg  = (*(share_objs->pimage_refs))[pointer_id];
	memcpy(&pointer_id,    rpos, 4);	rpos += 4;
	if(pointer_id != 0xffffffff)aArrowImg	  = (*(share_objs->pimage_refs))[pointer_id];
	memcpy(&aBackDiskColor,	   rpos, 4); rpos += 4;
	memcpy(&aArrowColor,	   rpos, 4); rpos += 4;
	memcpy(&aVectorMaxLen,     rpos, 4); rpos += 4;
	memcpy(&aVectorMappingRef, rpos, 4); rpos += 4;
	memcpy(&aPressMappingRef,  rpos, 4); rpos += 4;
	memcpy(&aVecMappingKeyType,rpos, 1); rpos += 1;
	memcpy(&aPressMappingKey,  rpos, 1); rpos += 1;
	if(zVpUV.x > 0.0f)zReCptNearRange();
	zInit(-1.0f);
	return rpos - data;
}
#endif //_GN_UIBASE_H_