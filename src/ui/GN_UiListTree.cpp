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
#ifdef _GN_UILISTTREE_H_
GN_API void GNu::MakeSimpleArrowIcon(GNg::sIMAGE *_img, int _w, int _h, bool style, int frame_w){
	uint8 locbuf[4096];
	GN::sTBUF_INT tbuf(locbuf, 4096);
	_img->CreateEmptyToData(_w, _h, 1, GNg::CR_ALPHA);
	uint8 *_arrow_data = (uint8*)tbuf.GetSpace(_w * _img->height);
	GNg::GenArrowDataToMem(_arrow_data, _w, _img->height, style, frame_w);
	_img->DataFill(_arrow_data, GNg::CR_ALPHA, _w, _img->height);
}
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern bool    GT_ImeOnRecv;
extern uint8   GT_ImeCount;
extern char    GT_ImeChar[];

static const char *GT_ERR_TreeViewInitFail = "调用 cTREE_VIEW::InitStyleToData 失败! %s!\n";
P<GNg::sIMAGE> GNu::cTREE_VIEW::aDefaultUnfoldImg;

bool GNu::cTREE_VIEW::zSelectProc(P_ &node, unsigned int zid, bool has_focus){
	GNu::cTREE_NODE_OPTR *_np = zNodeOptr;
	bool draw;
	if(has_focus){ /*模版代码:选择列表选择处理*/
		if(zFocusLastNode != node){ zFocusLastNode = node; zIsFocusFrameShow = true; }
		draw = zIsFocusFrameShow;
		if(zDraging && !_np->HasSelect(zFocusLastNode))draw = false;
		if(GN::MouseI[GM_LEFT]){  //选择处理
			if(zMultSelect){ //多选
				    if(zMsLastState && (GN::Key[GK_LSHIFT] || GN::Key[GK_RSHIFT])){ //快捷键段选
	                    if     (zMsLastState == 1)_np->SelectTag(node, 1, -1); //加段
	                    else if(zMsLastState == 2)_np->SelectTag(node, 0, -1); //减段
						zMsLastState = 0;
					}else{
						if(_np->HasSelect(node)){
						    _np->SelectTag(node, 0, 1);
                            zIsFocusFrameShow = false;
							zMsLastState = 2;
						}else{
						    _np->SelectTag(node, 1, 1);
                            zIsFocusFrameShow = true;
							zMsLastState = 1;
						}
					}
			}else{
				_np->SelectTag(node, 1, 0);
				aSelected = zid + 1;		//单选
			}
		}
		return draw;
	}else return _np->HasSelect(node); //需高亮显示其它已被选中的选项
}
bool GNu::cTREE_VIEW::InitStyleToData(P<cTREE_NODE_OPTR> node_optr, const GNg::cTEXT_ART *text_style,unsigned int text_color, bool mult_select, short _x ,short _y, short _w, short _h, bool default_am, unsigned int focus_color, unsigned short focus_blend){
	if(text_style)zTextOut = (GNg::cTEXT_ART*)text_style;
	else if(GN::RunRec.zGWPrintReady){
		GN::RunRec.aFont.Release(); //如调试信息已开启 也可用调试信息的文本式样来显示
		zTextOut = &GN::RunRec.aFont;
	}else{
		zZoningNum = 0;
		GN_Log(GT_ERR_TreeViewInitFail, "还没指定文本式样");
	    return false;
	}
	if(node_optr){
		zNodeOptr = node_optr;  zZoningNum = 128;
	}else if(!zNodeOptr){
		zZoningNum = 0;
       #ifdef GN_DEBUG
        GN_Log(GT_ERR_TreeViewInitFail, "还没指定树节点查找器");
       #endif
	    return false;
	}
	aTextColor = text_color ? text_color : GT_AppDefaultColorFont;
	zMultSelect = mult_select;

	// 使能自动适应宽高 ------------------------------------------------------------
	GNg::sARRANGE_FMT &tfmt = zTextOut->zParame;
	short dw, dh;
	if(tfmt.erect_print)dw = tfmt.i_compartment + 1;
	else                dw = _w;
	if(tfmt.erect_print)dh = _h;
	else                dh = tfmt.j_compartment + 1;
	
	// 初始化继承类相关数据 -----------------------------------------------------------------------------------
	bool ret = cSCROLL_LIST::InitStyleToData(0, _x,_y,_w,_h, default_am,dw,dh,zZoningNum, focus_color,focus_blend);
	cSCROLL_LIST::SetZoningDrawCall((void (*)(int,int,int,int,unsigned int,unsigned int,unsigned int,bool,void*))(zNodeOptr->GetDrawNodeFunc()), this);
	zFFrames->aColor_range = 127;
	zArrowAm.aFrame_ts.sx = (float)tfmt.height / (float)zOffsetArrow->width;
	zReSelectState();
	zFocusLastNode.Delete();
	zDraging = 0;
	return ret;
}
void GNu::cTREE_VIEW::SetNodeOptr(P<cTREE_NODE_OPTR> node_optr){
	zNodeOptr = node_optr;  zZoningNum = 128;
	cSCROLL_LIST::SetZoningDrawCall((void (*)(int,int,int,int,unsigned int,unsigned int,unsigned int,bool,void*))(zNodeOptr->GetDrawNodeFunc()), this);
}
void GNu::cTREE_VIEW::RePos(){
	GNu::cSCROLL_LIST::RePos();
	zNodeOptr->OnReArranged(0,0, zFFrames?zFFrames->zMax_num:0);
}
void GNu::cTREE_VIEW::SetPos(short _x,short _y,short _w,short _h){
	GNg::sRECT2 rc2d((float)_x, (float)_y, (float)(_x+_w), (float)(_y+_h));
	SetPos(rc2d);
}
void GNu::cTREE_VIEW::SetPos(GNg::sRECT2 &range){
	cSELECT_LIST_TEXT::SetPos(range);
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sARRANGE_FMT &pfmt = _fframe->zParame;
	if(zTextOut->zParame.erect_print)pfmt.height = _fframe->aGenRngH;
	else							 pfmt.width  = _fframe->aGenRngW;
}
void GNu::cTREE_VIEW::SetMultSelect(bool yes){
	AllSelect(false);
	zMultSelect = yes;
	zReSelectState();
	zFocusLastNode.Delete();
}
int GNu::cTREE_VIEW::CheckConvertActionSelectID(int action){
	if(action < 0){
		return -SearchNodeID( SearchNode(-action) , false);
	}else if(action && action <= eUI_ACT_SELECT_IDMAX){
		return  SearchNodeID( SearchNode( action) , false);
	}
	return action;
}
static bool GT_TreeSelectNodesCollect(P_ &node, void *us_arg){
	GNu::cTREE_VIEW *tvw = (GNu::cTREE_VIEW*)us_arg;
	if(tvw->GetNodeOptr()->HasSelect(node)){
		((GN::sLIST< P_ >*)(tvw->zProcArg))->push_back(node);
		if(tvw->zMsLastID < 32)tvw->aSelected |= 1 << tvw->zMsLastID;
	}
	tvw->zMsLastID++;
	return true;
}
static bool GT_TreeSelectNodeCollect(P_ &node, void *us_arg){
	GNu::cTREE_VIEW *tvw = (GNu::cTREE_VIEW*)us_arg;
	tvw->aSelected++;
	if(tvw->GetNodeOptr()->HasSelect(node)){
		((GN::sLIST< P_ >*)(tvw->zProcArg))->push_back(node);
		return false;
	}
	return true;
}
unsigned int GNu::cTREE_VIEW::GetSelectNodes(GN::sLIST< P_ > *_node_list_out){
	zProcArg = _node_list_out;
	aSelected = 0;
	if(zMultSelect){
		zMsLastID = 0;
		zNodeOptr->Search_exec(P_(), true, GT_TreeSelectNodesCollect, this);
	}else{
		if(!zNodeOptr->Search_exec(P_(), true, GT_TreeSelectNodeCollect , this))aSelected = 0;
	}
	return aSelected;
}
static bool GT_TreeSelectAllNode(P_ &node, void *us_arg){
	GNu::cTREE_VIEW *tvw = (GNu::cTREE_VIEW*)us_arg;
	tvw->GetNodeOptr()->SelectTag(node, tvw->zMsLastID, 1);
	return true;
}
void GNu::cTREE_VIEW::AllSelect(bool yes, P_ g){
	zMsLastID = yes;
	zNodeOptr->Search_exec(g, false, GT_TreeSelectAllNode , this);	
}
static bool GT_TreeUnfoldAllNode(P_ &node, void *us_arg){
	GNu::cTREE_VIEW *tvw = (GNu::cTREE_VIEW*)us_arg;
	tvw->GetNodeOptr()->UnfoldTag(node, tvw->zMsLastID);
	return true;
}
void GNu::cTREE_VIEW::AllUnfold(bool yes, P_ g){
	zMsLastID = yes;
	zNodeOptr->Search_exec(g, false, GT_TreeUnfoldAllNode , this);
}
static bool GT_TreeCancelAndCloseAllNode(P_ &node, void *us_arg){
	GNu::cTREE_NODE_OPTR *optr = ((GNu::cTREE_VIEW*)us_arg)->GetNodeOptr();
	optr->SelectTag(node, 0, 1);
	optr->UnfoldTag(node, 0);
	return true;
}
void GNu::cTREE_VIEW::AllCancelAndClose(P_ g){
	zNodeOptr->Search_exec(g, false, GT_TreeCancelAndCloseAllNode , this);
	RePos();
	zReSelectState();
	zFocusLastNode.Delete();
}
static bool GT_TreeNodeNotEqualNumCollect(P_ &node, void *us_arg){
	if(node == *((P_*)(((void**)us_arg)[1])))return false;
	*((unsigned int*) (((void**)us_arg)[0])) += 1;
	return true;
}
unsigned int GNu::cTREE_VIEW::SearchNodeID(P_ node, bool unfold){
	unsigned int id = 1;
	void *args[] = { &id, &node };
	if(zNodeOptr->Search_exec(P_(), unfold, GT_TreeNodeNotEqualNumCollect, args))return id;
	return 0;
}
static bool GT_TreeNodeIdChackToCollect(P_ &node, void *us_arg){
	unsigned int *ids = (unsigned int*)us_arg;
	if(ids[0] == ids[1]){
		*((P_**)us_arg) = &node;
		return false;
	}
	++(ids[0]);
	return true;
}
P_ GNu::cTREE_VIEW::SearchNode(unsigned int  id, bool unfold){
	unsigned int ids[] = { 1, id };
	if(zNodeOptr->Search_exec(P_(), unfold, GT_TreeNodeIdChackToCollect, ids))return *(*((P_**)ids));
	return P_();
}
static bool GT_TreeNodeNameChackToCollect(P_ &node, void *us_arg){
	GNu::cTREE_VIEW *tvw = (GNu::cTREE_VIEW*)us_arg;
	if(!((tvw->zFocusLastID & 128) && tvw->GetNodeOptr()->HasSelect(node))){
		char *node_name = tvw->GetNodeOptr()->GetNodeName(node);
		unsigned int len = tvw->zFocusLastID & 127;
		if(!(len?strncmp(node_name, (char*)(tvw->zProcArg), len) : strcmp(node_name, (char*)(tvw->zProcArg)))){
			tvw->zProcArg = &node; 
			return false;
		}
	}
	tvw->zMsLastID++;
	return true;
}
P_ GNu::cTREE_VIEW::SearchNode(unsigned int &id, const char *name, uint8 cmpn, bool ign_sl, bool unfold){
	zProcArg = (void*)name;
	zFocusLastID = cmpn;
	if(ign_sl)zFocusLastID |= 128;
	zMsLastID = 1;
	if(zNodeOptr->Search_exec(P_(), unfold, GT_TreeNodeNameChackToCollect, this)){
		id = zMsLastID;
		return *((P_*)zProcArg);
	}else{
		id = 0;
		return P_();
	}
}
void GNu::cTREE_VIEW::ScrollToNodeID(unsigned int find_id1){
	cTREE_NODE_OPTR *_node_optr = zNodeOptr;
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sARRANGE_FMT *pfmt = &(_fframe->zParame);
	int line_h = (pfmt->erect_print ? pfmt->i_compartment : pfmt->j_compartment);

   #define GT_TMP_SCROLL_OFS_CPT \
	unsigned int find_id0 = SearchNodeID(_node_optr->GetCurFristNode(), true); /*计算滚动差距*/\
	if	   (find_id0 + 1 > find_id1)zTextOffsettg += (float)(((int)(find_id0 + 1) - (int)find_id1) * line_h);\
	else if(find_id0 + _fframe->zMax_num-3 < find_id1)zTextOffsettg += (float)(((int)(find_id0 + _fframe->zMax_num-3) - (int)find_id1) * line_h);

	GT_TMP_SCROLL_OFS_CPT
}
void GNu::cTREE_VIEW::ScrollToNode(P_ node){
	ScrollToNodeID(SearchNodeID(node, true));   
}
void GNu::cTREE_VIEW::ScrollToNode(const char *name, uint8 cmpn){
	unsigned int find_id1;
	P_ find_node = SearchNode(find_id1, name, cmpn, false, true);
	if(find_node.Has())ScrollToNodeID(find_id1);
}
void GNu::cTREE_VIEW::RefreshVisualNode(unsigned int vid, unsigned int count){
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	if(_fframe->zFontFrame->zFEvent){
		unsigned int fn = vid + count;
		if(_fframe->zMax_num < fn)fn = _fframe->zMax_num;
		if(vid < fn){
			unsigned int *nowtimes = _fframe->zNowtime_s, step_t = _fframe->zGen_once_numB * _fframe->zEnd_time / (fn - vid), vid2 = fn-1, nt = 0;
			unsigned int vid1 = vid2 - _fframe->zGen_once_numB;
			for(; ; --vid2){
				if(vid2 == vid1){
					vid1 -= _fframe->zGen_once_numB;
					nt += step_t;
				}
				nowtimes[vid2] = nt;
				if(vid == vid2)break;
			}
		}
	}
}
void GNu::cTREE_VIEW::zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am){
	GNg::sARRANGE_FMT *pfmt = &(zFFrames->zParame);
	zNodeOptr->OnReArranged(sp, (pfmt->erect_print? (pfmt->overturn_print?zTextOffsetBk:-zTextOffsetBk)/(float)pfmt->i_compartment : -zTextOffsetBk/(float)pfmt->j_compartment), zFFrames->zMax_num);
	cSELECT_LIST_TEXT::zOnReArranged(sp, sp_last, ret_am);
}
bool GNu::cTREE_VIEW::zOnAddScroolOffset(int move_dist){
	zTextOffset = zTextOffsetBk;
	if(move_dist){
		zTextOffsettg += (float)move_dist;\
		GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
		GNg::sARRANGE_FMT *pfmt = &(_fframe->zParame);
		if(pfmt->erect_print && pfmt->overturn_print){ /*竖排列与翻转输出处理有点特殊的增加偏移量的处理*/\
			short line_h = pfmt->i_compartment;\
			if      (move_dist < 0){\
				float ofstg = (float)(((short)GN_Round(zTextOffset/(float)line_h) - zNodeOptr->CanDrawLastNodeNum((short)(zTextOffset - zTextOffsettg) / line_h + 1)) * line_h);
				zTextOffsettg = ofstg > zTextOffsettg ? ofstg : zTextOffsettg;
			}else/* if(move_dist > 0)*/{\
				float ofstg = (float)(((short)zTextOffset/line_h+1				  + zNodeOptr->CanDrawNextNodeNum((short)(zTextOffsettg - zTextOffset) / line_h + 1)) * line_h);
				zTextOffsettg = ofstg < zTextOffsettg ? ofstg : zTextOffsettg;
			}\
		}else{ /*一般的增量偏移处理*/\
			short line_h = (pfmt->erect_print ? pfmt->i_compartment : pfmt->j_compartment);
			if      (move_dist > 0){\
				float ofstg = (float)(((short)zTextOffset/line_h   + zNodeOptr->CanDrawLastNodeNum((short)(zTextOffsettg - zTextOffset) / line_h + 1)) * line_h);
				zTextOffsettg = ofstg < zTextOffsettg ? ofstg : zTextOffsettg;

			}else/* if(move_dist < 0)*/{\
				float ofstg = (float)(((short)zTextOffset/line_h-1 - zNodeOptr->CanDrawNextNodeNum((short)(zTextOffset - zTextOffsettg) / line_h + 1)) * line_h);
				zTextOffsettg = ofstg > zTextOffsettg ? ofstg : zTextOffsettg;
			}\
		}\
	}
	return true;
}
unsigned int GNu::cTREE_VIEW::UpdateRun(unsigned int updatatime, unsigned int dc){
	cTREE_NODE_OPTR *_node_optr = zNodeOptr;
	if(!_node_optr)return 0;
	if(!aUnfoldIcon){
		if(!aDefaultUnfoldImg){
			aDefaultUnfoldImg.New(1);
			int fh = GN::RunRec.aFont.GetFontHeight();
			MakeSimpleArrowIcon(aDefaultUnfoldImg, (fh<<1)/3-1, fh, 1, 1);
		}
		aUnfoldIcon = aDefaultUnfoldImg;
	}
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sARRANGE_FMT *pfmt = &(_fframe->zParame);
	int line_h = (pfmt->erect_print ? pfmt->i_compartment : pfmt->j_compartment);
	
	_node_optr->OnUpdateBegin(updatatime);
//GN_RunValueCheck2(zZoningNum, 30); GN_RunValueCheck2(zTextOffsettg, 29); GN_RunValueCheck2(zTextOffset, 28);
	zTextOffsetBk = zTextOffset;
	if(zTextOffset > 0.0f)zTextOffset = (float)(((int)zTextOffset % line_h) - line_h);
	else{
		int zn = (int)((pfmt->erect_print ? _fframe->aGenRngW : _fframe->aGenRngH) - zTextOffset) / line_h + 1;
		if(zn > zZoningNum){
			zZoningNum = zn + (zn>>1);
			zReArrange = true;
		}
	}
	unsigned int ret = cSCROLL_LIST::UpdateRun(updatatime, true, dc);
	_node_optr->OnUpdateEnd();
	if(zIsMouseFocus){
		if(GN::Mouse[GM_LEFT]){
			if(GN::MouseT[GM_LEFT] > 760){ zDraging = 1; zKeyOnDown = false; }
			if(zDraging){ //绘制插入线
				unsigned int line_cr = zArrowAm.aLevel_color;
				if(!_node_optr->HasSelect(zFocusLastNode))line_cr = (zArrowAm.aLevel_color&0xff000000)|(aBackColor&0x00ffffff);
				GNg::sDRAW_AREA *rng_datas = zDrawRangeData;
				int zy, zh;
				if(pfmt->erect_print){
				  if(pfmt->overturn_print){
				    zh = rng_datas->x - rng_datas[1].x;
					zy = rng_datas->x+zh - GNg::ViewCur->mouseMX;
				  }else{
					zh = rng_datas[1].x - rng_datas->x;
					zy = GNg::ViewCur->mouseMX - rng_datas->x;
				  }
				}else{
					zh = rng_datas[1].y - rng_datas->y;
					zy = GNg::ViewCur->mouseMY - rng_datas->y;
				}
				zFocusLastVid = zy / zh;
				int zy1= zh * zFocusLastVid;
				if(zy - zy1 > (zh>>1)){
					zy1 += zh;
					  zDraging = 2;
				}else zDraging = 1;
				if(pfmt->erect_print){
					GNg::ImageDraw(0, (pfmt->overturn_print? rng_datas->x+zh-zy1 : rng_datas->x+zy1) -(aLineWidth>>1), rng_datas->y, aLineWidth, rng_datas->h, line_cr, 0, aFocusBlend);
				}else{
					GNg::ImageDraw(0, rng_datas->x, rng_datas->y+zy1-(aLineWidth>>1), rng_datas->w, aLineWidth, line_cr, 0, aFocusBlend);
				}
			}
		}else if(zDraging){
			if(!GN::MouseQ[GM_LEFT] && !_node_optr->HasSelect(zFocusLastNode)){ //按紧移动过再松开
				GN::sLIST< P_ > sl_nodes;
				GetSelectNodes(&sl_nodes);
				if(sl_nodes.size()){
					_node_optr->MoveNodes(zFocusLastNode, zDraging==2, &sl_nodes);
					RefreshVisualNode(zFocusLastVid, sl_nodes.size());
				}
			}
			zDraging = 0;
		}
		if(GN::Mouse1[GM_RIGHT]){
			AllSelect(false);
			zDraging = 0;
		}
		if(aEnableImeSelect){
			GT_ImeOnRecv = 1;
			if(GT_ImeCount){ //键盘有字符输入
				unsigned int find_id1;
				P_ find_node = SearchNode(find_id1, GT_ImeChar, GT_ImeCount, true, true);
				if(find_node.Has()){
					_node_optr->SelectTag(find_node, 1, zMultSelect);
					GT_TMP_SCROLL_OFS_CPT
				}
		}	}
	}else zDraging = 0;
	return ret;
}
void GT_SendSelectChangeAction(GNu::cSELECT_LIST_ICON *, unsigned int , unsigned int );

void GNu::cTREE_VIEW::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	unsigned int last_sel = aSelected; 
	GT_TMP_SCROLL_LIST_UPDATE( cTREE_VIEW::UpdateRun(updatetime, parent->c) , if(last_sel != aSelected)GT_SendSelectChangeAction(this,last_sel,aSelected); ) 
}
P<GNu::iUI_OBJECT> GNu::cTREE_VIEW::Clone(){
	P<cTREE_VIEW> ui_treeview(1);
	*ui_treeview = *this;
	ui_treeview->zSpecClone();
	ui_treeview->zNodeOptr = this->zNodeOptr->OnCloneOut();
	return ui_treeview;
}
/******************************************************************************************************************************************************************/
void GNu::cTREE_NODE__OPTR::zDrawNodesCollectAndStatist(P_ g, short lv){
	GN::sLIST< P_ >::iterator it = GetNodeSubs(g)->begin(), et = GetNodeSubs(g)->end();
	for(; it != et; ++it){
		P_ node = (*it);
		if(zVisNodeTotalNum >= zStid && zVisNodeTotalNum < zEndid){
			zNodeLevels[(unsigned int)(zDrawNodeCur-(P_*)zDrawNodes)] = lv; 
			*zDrawNodeCur = node; ++zDrawNodeCur;
		}
		++zVisNodeTotalNum;
		if(GetNodeSubs(node)->size() && HasUnfold(node))zDrawNodesCollectAndStatist(node, lv+1);
	}
}
P_  GNu::cTREE_NODE__OPTR::GetNodeAndSeekNext(short &level_n){
	unsigned int dkn = (unsigned int)(zDrawNodeCur - (P_*)zDrawNodes);
	level_n = zNodeLevels[dkn];
	P_ *retp = zDrawNodeCur++;
	return *retp;
}
void GNu::cTREE_NODE__OPTR::OnUpdateBegin(unsigned int updatatime){
	if(zNodeNeedRefresh)OnReArranged(0, zStid, zDrawNodes.Num());
	else zDrawNodeCur = zDrawNodes;
}
void GNu::cTREE_NODE__OPTR::OnReArranged(int id, float st_nid, unsigned int max_vnn){ //当框滚动或被重排列后的处理(当前排列起始区域号, 列表框可见起始节点ID, 列表框当前最大可见节点数)
	 zStid = (int)st_nid;
	 zEndid = zStid + max_vnn;
	 if(zDrawNodes.Num() != max_vnn){
		 zDrawNodes.New(max_vnn);
		 zNodeLevels.New(max_vnn);
	 }
	 zDrawNodeCur = zDrawNodes;
	 if(zStid == 0){ zNodeLevels[0] = 0; *zDrawNodeCur = aRoot; ++zDrawNodeCur; }
	 zVisNodeTotalNum = 1;
	 if(GetNodeSubs(aRoot)->size() && HasUnfold(aRoot))zDrawNodesCollectAndStatist(aRoot, 1);
	 P_ *node_end = (P_*)zDrawNodes + max_vnn;
	 for(; zDrawNodeCur < node_end; ++zDrawNodeCur)zDrawNodeCur[0].zData = 0;
	 zDrawNodeCur = zDrawNodes;
	 zNodeNeedRefresh = false;
}
void GNu::cTREE_NODE__OPTR::Init(P_ root, unsigned int max_vnn){
	aRoot = root;
	OnReArranged(0,0,max_vnn);
}
void GNu::cTREE_NODE__OPTR::Clear(){
	zMsLastNode.Delete();
	zNodeLevels.Delete();
	zDrawNodes.Delete();
	aRoot.Delete();
}
//查找节点段落设置选中标记(当前检索节点, 段落首或尾节点, [0]当前节点ID [1]段落首或尾节点ID [2]是否检索过段落节点 [3]是否选中标记,  树节点操作器)返回查找中途是否有中断
static bool GT_NodeSegFindSetSelectTag(P_ &g, P_ &node, int *ids/*[4]*/, GNu::cTREE_NODE__OPTR *optr){
	if(ids[2]){
		if(ids[0] >= ids[1])return true;
		optr->SelectTag1(g, ids[3]);
	}else if(ids[0] > ids[1]){
		if(g == node)return true;
		optr->SelectTag1(g, ids[3]);
	}else if(g == node)ids[2] = 1;

   #define GT_TMP_LOOP_UNFOLD_SUB_NODES( PROC ) \
	++(ids[0]);\
	if(optr->HasUnfold(g)){\
		GN::sLIST< P_ >::iterator in = optr->GetNodeSubs(g)->begin(), en = optr->GetNodeSubs(g)->end();\
		for(; in != en; ++in){\
			if( PROC (*in, node, ids, optr))return true;\
	}	}\
	return false;
	GT_TMP_LOOP_UNFOLD_SUB_NODES( GT_NodeSegFindSetSelectTag )
}
//查找节点ID(当前检索节点, 要查找的节点, [0]当前节点ID,  树节点操作器)返回查找中途是否有中断
static bool GT_NodeFindID(P_ &g, P_ &node, int *ids, GNu::cTREE_NODE__OPTR *optr){
	if(g == node)return true;
	GT_TMP_LOOP_UNFOLD_SUB_NODES( GT_NodeFindID )
}
void GNu::cTREE_NODE__OPTR::SelectTag(P_ node, uint8 on, int8 mode){
	if(on){
		SelectTag1(node, on);
		if(aSelectAddProc)aSelectAddProc(node, 1);
	}else{
		if(aSelectAddProc && HasSelect(node))aSelectAddProc(node, 0);
		SelectTag1(node, 0);
	}
   if(zMsLastNode == node)return;
   if(mode == -1){
	   int id_s[4] = { 0, 0, 0, on };
	   if(GT_NodeFindID(aRoot, zMsLastNode, id_s+1, this)){ //段选
			GT_NodeSegFindSetSelectTag(aRoot, node, id_s, this);
	   }
   }else if(!mode && zMsLastNode.zData){
		if(aSelectAddProc && HasSelect(zMsLastNode))aSelectAddProc(zMsLastNode, 0);
		SelectTag1(zMsLastNode, 0);
   }
   zMsLastNode = node;
}
int GNu::cTREE_NODE__OPTR::CanDrawLastNodeNum(int num){
	return zStid < num? zStid: num;
}
int GNu::cTREE_NODE__OPTR::CanDrawNextNodeNum(int num){
	int num1 = zVisNodeTotalNum - zEndid; 
	if(num1 < 0)num1 = 0;
	return num1 < num ? num1 : num;
}
P<GNu::cTREE_NODE_OPTR> GNu::cTREE_NODE__OPTR::OnCloneOut(){	//复制出该对象的一个新副本
	P<cTREE_NODE__OPTR> clone(1);
	*clone = *this;
	clone->zDrawNodes.Copy(zDrawNodes);
	clone->zNodeLevels.Copy(zNodeLevels);
	return clone;
}
static bool GT_Node__Search_exec(P_ &node, bool (*proc)(P_ &node, void *us_arg), void *_us_arg, GNu::cTREE_NODE__OPTR *optr){
	if(proc(node, _us_arg)){
		GN::sLIST< P_ >::iterator in = optr->GetNodeSubs(node)->begin(), en = optr->GetNodeSubs(node)->end();
		for(; in != en; ++in){ 
			if(GT_Node__Search_exec(*in, proc, _us_arg, optr))return true;
		} 
		  return false;
	}else return true;
}
static bool GT_Node__UnfoldSearch_exec(P_ &node, bool (*proc)(P_ &node, void *us_arg), void *_us_arg, GNu::cTREE_NODE__OPTR *optr){
	if(proc(node, _us_arg)){
	  if(optr->HasUnfold(node)){
		GN::sLIST< P_ >::iterator in = optr->GetNodeSubs(node)->begin(), en = optr->GetNodeSubs(node)->end();
		for(; in != en; ++in){ 
			if(GT_Node__UnfoldSearch_exec(*in, proc, _us_arg, optr))return true;
		} 
	  }
		  return false;
	}else return true;
}
bool GNu::cTREE_NODE__OPTR::Search_exec(P_ g, bool unfold, bool (*proc)(P_ &node, void *us_arg), void *_us_arg){
	if(unfold)return GT_Node__UnfoldSearch_exec(g.Has()?g : aRoot, proc, _us_arg, this);
	else	  return GT_Node__Search_exec	   (g.Has()?g : aRoot, proc, _us_arg, this);
}
P_ * GNu::cTREE_NODE__OPTR::SearchParentNodes(P_ &g, P_ &node, P_ *parent_out){
	*parent_out = g;
	GN::sLIST< P_ >::iterator in = GetNodeSubs(g)->begin(), en = GetNodeSubs(g)->end();
	for(; in != en; ++in){ 
		if(*in == node)return parent_out;
		P_ *parent_ret = SearchParentNodes(*in , node, parent_out+1);
		if(parent_ret)return parent_ret;
	}
	return 0;
}
void GNu::cTREE_NODE__OPTR::MoveNodes(P_ pos, bool pos_back, GN::sLIST<P_> *nodes){
	GN::sLIST< P_ > nodes1;
	{
		uint8 locbuf[2048];
		GN::sTBUF_INT tbuf(locbuf, 2048);
		P_ * p_nodes = (P_*)tbuf.GetSpace(sizeof(P_) * nodes->size());
		int pni = 0;
		P_ ppps_[64], _ppps_[64], tmp;
		P_ *pst, *pend = SearchParentNodes(aRoot, pos, ppps_), *_pend;
		GN::sLIST<P_>::iterator in = nodes->begin(), en = nodes->end();
		for(; in != en; ++in, ++pni){
			tmp = *in;
			for(pst = ppps_; pst <= pend; ++pst){
				if(tmp == *pst)break;
			}
			if(pst > pend){
				nodes1.push_back(tmp);
				  p_nodes[pni] = *(SearchParentNodes(aRoot, tmp, _ppps_));
			}else p_nodes[pni].zData = 0;
		}
		for(pni = 0, in = nodes->begin(); in != en; ++in, ++pni){
			if(p_nodes[pni].zData)GetNodeSubs(p_nodes[pni])->remove(*in);
		}
	}
	GN::sLIST< P_ > nodes_bk;
	if(HasUnfold(pos) && GetNodeSubs(pos)->empty());
	else if(aRoot == pos){
		nodes_bk.splice(nodes_bk.end(), *(GetNodeSubs(pos)));
	}else{
		if(pos_back && HasUnfold(pos)){
			nodes_bk.splice(nodes_bk.end(), *(GetNodeSubs(pos)));
		}else{
			P_ pos2 = pos;
			P_ ppps[64];
			pos = *(SearchParentNodes(aRoot, pos2, ppps));
			GN::sLIST< P_ > *p_subs = GetNodeSubs(pos);
			GN::sLIST< P_ >::iterator inn = p_subs->begin(), enn = p_subs->end();
			for(; inn != enn; ++inn){
				if(*inn == pos2){
					if(pos_back)++inn;
					if(inn != enn)nodes_bk.splice(nodes_bk.end(), *p_subs, inn, enn);
					break;
			}	}
		}
	}
	GN::sLIST< P_ > *_subs = GetNodeSubs(pos);
	_subs->splice(_subs->end(), nodes1);
	_subs->splice(_subs->end(), nodes_bk);
	RefreshNodes();
}
/******************************************************************************************************************************************************************/
//判断指定节点是否含展开标记(节点名)返回是否
inline uint8 GT_HasUnfold(P<char> &objname){
	return objname[objname.Num()-1] & 1;
}
GN_API int GNu::TreeVisualNodeNumStatist(P<GN::sNODE_> &node){
	int objn = 1;
	if(GT_HasUnfold(node->aName)){
		GN::sLIST< P<GN::sNODE_> >::iterator ito = node->zSub.begin(), eto = node->zSub.end();
		for(; ito != eto; ++ito)objn += TreeVisualNodeNumStatist(*ito);
	}
	return objn;
}
P<GN::sNODE_> GNu::cTREE_NODE_P_OPTR::GetNodeAndSeekNext(short &level_n){
	if(zCurLevelBk){
		if(!zCurLevel->parent)return P<GN::sNODE_>();
	}else zCurLevelBk = zBackupLevels(zNextBk); //首个绘制节点

	level_n = zCurLevel->lid;
	P<GN::sNODE_> ret = zCurLevel->sobj;
	zSeekNextNode();
	return ret;
}
short GNu::cTREE_NODE_P_OPTR::GetNodeOnLevel(){
	return zCurLevel->lid;
}
void GNu::cTREE_NODE_P_OPTR::Init(P<GN::sNODE_> root, void (*comment_func)(GN::sNODE_*, P<GN::sNODE_>)){
	parent = 0;
	lid	   = 0;
	sobj   = root;
	zCurLevel = this;
	zCurShowStartNid = 0;
	aCommentFunc = comment_func;
}
void GNu::cTREE_NODE_P_OPTR::Clear(){
	zNextBk.Delete();	
	zNextBk2.Delete();	
	zMsLastNode.Delete();
	next.Delete();
	sobj.Delete();
}
bool GNu::cTREE_NODE_P_OPTR::HasUnfold(P_ g){
	if((CVP(GN::sNODE_)g)->zSub.empty())return 0;
	P<char> &objname = (CVP(GN::sNODE_)g)->aName;
	return (objname[objname.Num()-1] & 1);
}
void GNu::cTREE_NODE_P_OPTR::UnfoldTag(P_ g, uint8 on){
	if((CVP(GN::sNODE_)g)->zSub.empty())return;
	P<char> &objname = (CVP(GN::sNODE_)g)->aName;
   #define GT_TMP_CHECK_AND_CREATE_OBJ_TAG_BUF \
	unsigned int name_num = objname.Num();\
	if((unsigned int)strlen(objname)+1 == name_num){ /*创建标记空间*/\
		P<char> name1(name_num+1);\
		/* TODO: verify buffer size */\
		strcpy(name1, objname); name1[name_num] = 0;\
		objname = name1;\
	}
	GT_TMP_CHECK_AND_CREATE_OBJ_TAG_BUF
	if(on)objname[objname.Num()-1] |= 1;
	else  objname[objname.Num()-1] &= 254; //~1
}
bool GNu::cTREE_NODE_P_OPTR::HasSelect(P_ node){
	P<char> &objname = (CVP(GN::sNODE_)node)->aName;
	return (objname[objname.Num()-1] & 2)?true:false;
}
void GNu::cTREE_NODE_P_OPTR::SelectTag(P_ node, uint8 on, int8 mode){
	P<char> &objname = (CVP(GN::sNODE_)node)->aName;
	GT_TMP_CHECK_AND_CREATE_OBJ_TAG_BUF

	if(on){
		objname[objname.Num()-1] |= 2;
		if(aSelectAddProc)aSelectAddProc(node, 1);
	}else{
		if(aSelectAddProc && (objname[objname.Num()-1] & 2))aSelectAddProc(node, 0);
		objname[objname.Num()-1] &= 253; //~2
	}
   if(zMsLastNode == node)return;
   if(mode == -1){
	   if(zMsLastNode->zParent && GT_HasUnfold(zMsLastNode->zParent->aName)){ //段选
			sTREE_LEVEL_ *cur_level_bk = zBackupLevels(zNextBk2);
			if(zCurLevel->parent){ //不是末尾
				do{
					if(zCurLevel->sobj == zMsLastNode){ //向下找到上次点选节点
						zSeekLastNode();
						while(zCurLevel->sobj != node){ //按原路返回不断设置标记到当前节点为止
							SelectTag(zCurLevel->sobj, on, 1);
							zSeekLastNode();
						}
						zSeekNextNode();
						zMsLastNode = node;
						return;
					}
				}while(zSeekNextNode());
				GN_Swap( P<sTREE_LEVEL_> , next , zNextBk2 );
				zCurLevel = cur_level_bk;
				cur_level_bk = zBackupLevels(zNextBk2);
			}
			P_ node2 = zMsLastNode;
			zSeekLastNode();
			zSeekLastNode();
			while(zCurLevel->sobj != node2){ //向上设置标记到上次点选节点为止
				SelectTag(zCurLevel->sobj, on, 1);
				zSeekLastNode();
			}
			GN_Swap( P<sTREE_LEVEL_> , next , zNextBk2 );
			zCurLevel = cur_level_bk;
	   }
   }else if(!mode && zMsLastNode){
		P<char> &objname0 = zMsLastNode->aName;
		if(aSelectAddProc && (objname0[objname0.Num()-1] & 2))aSelectAddProc(zMsLastNode, 0);
		objname0[objname0.Num()-1] &= 253; //~2
   }
   zMsLastNode = node;
}
GNu::sTREE_LEVEL_ * GNu::cTREE_NODE_P_OPTR::zBackupLevels(P<sTREE_LEVEL_> &next2){
	if(zCurLevel == this)return this;
	else{
		if(!next2){
			next2.New(1);
			next2->parent = this;
			next2->lid = 1;
		}
		sTREE_LEVEL_ *_lv = next, *_lv2 = next2;
		for(;;){
			_lv2->sobj = _lv->sobj;
			_lv2->iter = _lv->iter;
			if(_lv == zCurLevel)break;
			if(!(_lv2->next)){
				_lv2->next.New(1);
				_lv2->next->parent = _lv2;
				_lv2->next->lid = _lv2->lid + 1;
			}
			_lv2 = _lv2->next;
			_lv  = _lv ->next;
		}
		return _lv2;
	}
}
bool GNu::cTREE_NODE_P_OPTR::zSeekLastNode(){
	if(zCurLevel->parent){
		if(zCurLevel->iter == zCurLevel->parent->sobj->zSub.begin()){
			zCurLevel = zCurLevel->parent;
			return zCurLevel->parent ? true : false;
		}else{
			--(zCurLevel->iter);
			zCurLevel->sobj = *(zCurLevel->iter);
		}
	}
	while(HasUnfold(zCurLevel->sobj)){
	   #define GT_TMP_TREE_NODE3D_TO_NEXT_LEVEL( ITER ) \
	    if(!(zCurLevel->next)){\
			zCurLevel->next.New(1);\
			zCurLevel->next->parent = zCurLevel;\
			zCurLevel->next->lid = zCurLevel->lid + 1;\
		}\
		zCurLevel = zCurLevel->next;\
		zCurLevel->iter = ITER ;\
		zCurLevel->sobj = *(zCurLevel->iter)

		GT_TMP_TREE_NODE3D_TO_NEXT_LEVEL( --(zCurLevel->parent->sobj->zSub.end()) );
	}
	return zCurLevel->parent ? true : false;
}
bool GNu::cTREE_NODE_P_OPTR::zSeekNextNode(){
	if(HasUnfold(zCurLevel->sobj)){
		GT_TMP_TREE_NODE3D_TO_NEXT_LEVEL( zCurLevel->parent->sobj->zSub.begin() );
		return true;
	}else{
		while(zCurLevel->parent){
			++(zCurLevel->iter);
			if(zCurLevel->iter == zCurLevel->parent->sobj->zSub.end())zCurLevel = zCurLevel->parent;
			else{
				zCurLevel->sobj = *(zCurLevel->iter);
				return true;
			}
		}
		return false;
	}
}
void GNu::cTREE_NODE_P_OPTR::zSeekLastNode(int count){
	while(count){
		if(zSeekLastNode())--count;
		else break;
	}
}
void GNu::cTREE_NODE_P_OPTR::zSeekNextNode(int count){
	while(count){
		if(zSeekNextNode())--count;
		else break;
	}
}
int GNu::cTREE_NODE_P_OPTR::CanDrawLastNodeNum(int num){ //GN_RunValueCheck2(num, 31);
	if(zCurLevelBk){
		GN_Swap( P<sTREE_LEVEL_> , next , zNextBk ); //先恢复检索层级
		zCurLevel = zCurLevelBk;
	}
	zCurLevelBk = zBackupLevels(zNextBk);  //再备份检索层级
	
	if(zCurLevel->parent){
		int i = 1;
		for(; i < num && zSeekLastNode(); ++i);  
		return i;
	}
	return 0;
}
int GNu::cTREE_NODE_P_OPTR::CanDrawNextNodeNum(int num){
	if(zCurLevel->parent){
		int i = 1;
		for(; i < num && zSeekNextNode(); ++i); //GN_RunValueCheck2(i, 32);
		return i;
	}
	return 0;
}
void GNu::cTREE_NODE_P_OPTR::OnUpdateBegin(unsigned int updatatime){
	GNu::sTREE_LEVEL_ *p_level = zCurLevel->parent;
	if(p_level){ //检测并缝好树在外面被剪断删除的节点
		GN::sNODE_ *gp = p_level->sobj;
		if(gp == (GN::sNODE_*)(this->sobj) || gp->HasParent(this->sobj)){
			if(!zCurLevel->sobj->zParent){ //仅断开了与上一级的链接
				if(p_level->sobj->zSub.size()){
					zCurLevel->iter = p_level->sobj->zSub.begin();
					zCurLevel->sobj = *(zCurLevel->iter);
				}else zCurLevel = zCurLevel->parent;
			}
		}else{ //已经断开与主节点链接了
			zCurLevel = this;
			zSeekNextNode(zCurShowStartNid);
		}
	}
	zCurLevelBk = 0;
}
void GNu::cTREE_NODE_P_OPTR::OnUpdateEnd(){ //GN_RunValueCheck2(zCurShowStartNid, 37); GN_RunValueCheck2(zCurLevel->sobj.Begin(), 36); GN_RunValueCheck2(zCurLevel->parent, 35);
	if(zCurLevelBk){
		GN_Swap( P<sTREE_LEVEL_> , next , zNextBk );
		zCurLevel = zCurLevelBk;
	}
}
P<GNu::cTREE_NODE_OPTR> GNu::cTREE_NODE_P_OPTR::OnCloneOut(){
	P<GNu::cTREE_NODE_P_OPTR> optr(1);
	GNu::cTREE_NODE_P_OPTR *_optr = optr;
	_optr->parent = 0;
	_optr->lid = 0;
	_optr->sobj = this->sobj;
	_optr->iter = this->iter;
	_optr->zCurShowStartNid = this->zCurShowStartNid;
	_optr->zCurLevelBk = 0;
	_optr->zCurLevel = this->zBackupLevels(_optr->next);
	if(_optr->zCurLevel == this)_optr->zCurLevel = _optr;
	if(_optr->next)_optr->next->parent = _optr;
	return optr;
}
void GNu::cTREE_NODE_P_OPTR::OnReArranged(int id, float st_nid, unsigned int max_vnn){
	int seek_n = (int)st_nid - zCurShowStartNid;
	if(seek_n){
		if(seek_n < 0)zSeekLastNode(-seek_n);
		else		  zSeekNextNode( seek_n);
	}
	zCurShowStartNid = (int)st_nid;
}
/*static*/ void GT_TreeNodedDrawCall(int x, int y, int w, int h, unsigned int color, unsigned int  zid, unsigned int  vid, bool has_focus, void *arg){
    GNu::cTREE_VIEW *sl = (GNu::cTREE_VIEW*)arg;  /*模板代码:树节点绘制处理 ----------------------------------------------*/
	GNu::cTREE_NODE_P_OPTR *optr = (GNu::cTREE_NODE_P_OPTR*)((GNu::cTREE_NODE_OPTR*)(sl->GetNodeOptr()));
	short  lv;
	P<GN::sNODE_> node = optr->GetNodeAndSeekNext(lv);
	GN::sNODE_  *_node = node;
	if(_node){
		GNg::cTEXT_ART *_text_out = sl->GetTextStyle();
		short fw = _text_out->GetFontWidth();
		int ofs = fw + sl->aSpacing * lv;
		fw <<= 1;
		GNg::sARRANGE_FMT &tfmt = _text_out->zParame;
		GNg::sIMAGE *_icon = sl->GetIcons(), *_ufimg = sl->aUnfoldIcon;
		if(_node->zSub.size()){
			unsigned short tid = GT_HasUnfold(_node->aName);
			if(has_focus && GN::MouseI[GM_LEFT]){ //点选展开或缩回节点
				int lx = tfmt.erect_print ? GNg::ViewCur->mouseMY - (ofs+y) : GNg::ViewCur->mouseMX - (ofs+x);
				if(lx > 0 && lx < _ufimg->height){
					optr->UnfoldTag(node, !tid);
					if(tid)sl->AllSelect(0, node); //缩回事取消该节点里的所有选中标记
					else   sl->RefreshVisualNode(vid+1, _node->zSub.size()); //通知刷新展开的节点弹出动画
					has_focus = false;
				}
			} //绘制展开图标
			unsigned int arrw_color = sl->aAffectColor ? GNg_MultiplyColorRatio(sl->c, color) : sl->c;
			if(tfmt.erect_print){
				if(tid && _ufimg->texnum == 1){
					  int uf_ofs = ((_ufimg->height-_ufimg->width)>>1), ufw = _ufimg->width;
					  if(sl->IsOverturnArrange()){ uf_ofs += _ufimg->width; ufw = -ufw; }
					  GNg::ImageDraw   (_ufimg, (int)((float)(w-_ufimg->height)*tfmt.near_pointU)+x+uf_ofs, ofs+y, ufw,0, arrw_color, _ufimg->texnum==1?0:tid);
				}else GNg::ImageDrawRot(_ufimg, (int)((float)(w-_ufimg->height)*tfmt.near_pointU)+x+(_ufimg->height>>1), ofs+y +(_ufimg->height>>1), 0,0, 90.0f, 0.5f, 0.5f, arrw_color);
			}else{
				if(tid && _ufimg->texnum == 1){
					  GNg::ImageDrawRot(_ufimg, (ofs+x)+(_ufimg->width>>1), (int)((float)(h-_ufimg->height)*tfmt.near_pointV)+y +(_ufimg->height>>1), 0,0, 90.0f, 0.5f, 0.5f, arrw_color);
				}else GNg::ImageDraw   (_ufimg, ofs+x, (int)((float)(h-_ufimg->height)*tfmt.near_pointV)+y, 0,0, arrw_color, tid);
			}
			if(sl->aMergeIcon){ 
				if(_icon){
				  ofs +=  _icon->width + fw; _icon = 0;
				}
			}else ofs += _ufimg->width + fw;
		}else if(!sl->aMergeIcon)
				  ofs += _ufimg->width + fw;
		if(tfmt.erect_print){
			if(_icon){ //绘制节点图标
				uint8 icid = *((uint8*)_node + optr->aNodeIconIdAddrOffset);
				GNg::ImageDraw(_icon, (int)((float)(w-_icon->width)*tfmt.near_pointU)+x, ofs+y,  0,0, color, icid >= (uint8)_icon->texnum? 0 : icid);
				ofs += _icon->width + fw;
			}
			if(optr->aDrawNodeAddProc)ofs += optr->aDrawNodeAddProc(_node, x, ofs+y, w, h, color, has_focus, arg) + fw;
			y += ofs; h -= ofs;
		}else{
			if(_icon){ //绘制节点图标
				uint8 icid = *((uint8*)_node + optr->aNodeIconIdAddrOffset);
				GNg::ImageDraw(_icon, ofs+x, (int)((float)(h-_icon->height)*tfmt.near_pointV)+y, 0,0, color, icid >= (uint8)_icon->texnum? 0 : icid);
				ofs += _icon->width + fw;
			}
			if(optr->aDrawNodeAddProc)ofs += optr->aDrawNodeAddProc(_node, ofs+x, y, w, h, color, has_focus, arg) + fw;
			x += ofs; w -= ofs;
		}
		GT_SELECT_LIST_DrawText( _text_out , _node->aName );
		GT_SELECT_LIST_ProcAndDrawFocus( zSelectProc(node,zid,has_focus) , GetFocusChangeColor() , FocusImg() )
	}
}
static void GT_TreeNode_DrawCall(int x, int y, int w, int h, unsigned int color, unsigned int  zid, unsigned int  vid, bool has_focus, void *arg){
    GNu::cTREE_VIEW *sl = (GNu::cTREE_VIEW*)arg; /*模板代码:树节点绘制处理 ----------------------------------------------*/
	GNu::cTREE_NODE__OPTR *optr = (GNu::cTREE_NODE__OPTR*)((GNu::cTREE_NODE_OPTR*)(sl->GetNodeOptr()));
	short  lv;
	P_ node = optr->GetNodeAndSeekNext(lv);
//	GN::sNODE_  *_node = node;
	if( node.zData){
		GNg::cTEXT_ART *_text_out = sl->GetTextStyle();
		short fw = _text_out->GetFontWidth();
		int ofs = fw + sl->aSpacing * lv;
		fw <<= 1;
		GNg::sARRANGE_FMT &tfmt = _text_out->zParame;
		GNg::sIMAGE *_icon = sl->GetIcons(), *_ufimg = sl->aUnfoldIcon;
		if(optr->GetNodeSubs(node)->size()){
			unsigned short tid = optr->HasUnfold(node);
			if(has_focus && GN::MouseI[GM_LEFT]){ //点选展开或缩回节点
				int lx = tfmt.erect_print ? GNg::ViewCur->mouseMY - (ofs+y) : GNg::ViewCur->mouseMX - (ofs+x);
				if(lx > 0 && lx < _ufimg->height){
					optr->UnfoldTag(node, !tid);
					if(tid)sl->AllSelect(0, node); //缩回事取消该节点里的所有选中标记
					else   sl->RefreshVisualNode(vid+1, optr->GetNodeSubs(node)->size()); //通知刷新展开的节点弹出动画
					has_focus = false;
				}
			} //绘制展开图标
			unsigned int arrw_color = sl->aAffectColor ? GNg_MultiplyColorRatio(sl->c, color) : sl->c;
			if(tfmt.erect_print){
				if(tid && _ufimg->texnum == 1){
					  int uf_ofs = ((_ufimg->height-_ufimg->width)>>1), ufw = _ufimg->width;
					  if(sl->IsOverturnArrange()){ uf_ofs += _ufimg->width; ufw = -ufw; }
					  GNg::ImageDraw   (_ufimg, (int)((float)(w-_ufimg->height)*tfmt.near_pointU)+x+uf_ofs, ofs+y, ufw,0, arrw_color, _ufimg->texnum==1?0:tid);
				}else GNg::ImageDrawRot(_ufimg, (int)((float)(w-_ufimg->height)*tfmt.near_pointU)+x+(_ufimg->height>>1), ofs+y +(_ufimg->height>>1), 0,0, 90.0f, 0.5f, 0.5f, arrw_color);
			}else{
				if(tid && _ufimg->texnum == 1){
					  GNg::ImageDrawRot(_ufimg, (ofs+x)+(_ufimg->width>>1), (int)((float)(h-_ufimg->height)*tfmt.near_pointV)+y +(_ufimg->height>>1), 0,0, 90.0f, 0.5f, 0.5f, arrw_color);
				}else GNg::ImageDraw   (_ufimg, ofs+x, (int)((float)(h-_ufimg->height)*tfmt.near_pointV)+y, 0,0, arrw_color, tid);
			}
			if(sl->aMergeIcon){ 
				if(_icon){
				  ofs +=  _icon->width + fw; _icon = 0;
				}
			}else ofs += _ufimg->width + fw;
		}else if(!sl->aMergeIcon)
				  ofs += _ufimg->width + fw;
		if(tfmt.erect_print){
			if(_icon){ //绘制节点图标
				uint8 icid = *((uint8*)node.zData->pmem + optr->aNodeIconIdAddrOffset);
				GNg::ImageDraw(_icon, (int)((float)(w-_icon->width)*tfmt.near_pointU)+x, ofs+y,  0,0, color, icid >= (uint8)_icon->texnum? 0 : icid);
				ofs += _icon->width + fw;
			}
			if(optr->aDrawNodeAddProc)ofs += optr->aDrawNodeAddProc(node, x, ofs+y, w, h, color, has_focus, arg) + fw;
			y += ofs; h -= ofs;
		}else{
			if(_icon){ //绘制节点图标
				uint8 icid = *((uint8*)node.zData->pmem + optr->aNodeIconIdAddrOffset);
				GNg::ImageDraw(_icon, ofs+x, (int)((float)(h-_icon->height)*tfmt.near_pointV)+y, 0,0, color, icid >= (uint8)_icon->texnum? 0 : icid);
				ofs += _icon->width + fw;
			}
			if(optr->aDrawNodeAddProc)ofs += optr->aDrawNodeAddProc(node, ofs+x, y, w, h, color, has_focus, arg) + fw;
			x += ofs; w -= ofs;
		}
		GT_SELECT_LIST_DrawText( _text_out , optr->GetNodeName(node) );
		GT_SELECT_LIST_ProcAndDrawFocus( zSelectProc(node,zid,has_focus) , GetFocusChangeColor() , FocusImg() )
	}
}
void *GNu::cTREE_NODE__OPTR::GetDrawNodeFunc(){ return (void*)GT_TreeNode_DrawCall; }

static bool GT_NodeP_Search_exec(P<GN::sNODE_> &node, bool (*proc)(P_ &node, void *us_arg), void *_us_arg){
	if(proc(node, _us_arg)){
		GN::sLIST< P<GN::sNODE_> >::iterator in = node->zSub.begin(), en = node->zSub.end();
		for(; in != en; ++in){ 
			if(GT_NodeP_Search_exec(*in, proc, _us_arg))return true;
		} 
		  return false;
	}else return true;
}
static bool GT_NodeP_UnfoldSearch_exec(P<GN::sNODE_> &node, bool (*proc)(P_ &node, void *us_arg), void *_us_arg){
	if(proc(node, _us_arg)){
	  if(GT_HasUnfold(node->aName)){
		GN::sLIST< P<GN::sNODE_> >::iterator in = node->zSub.begin(), en = node->zSub.end();
		for(; in != en; ++in){ 
			if(GT_NodeP_UnfoldSearch_exec(*in, proc, _us_arg))return true;
		} 
	  }
		  return false;
	}else return true;
}
bool GNu::cTREE_NODE_P_OPTR::Search_exec(P_ g, bool unfold, bool (*proc)(P_ &node, void *us_arg), void *_us_arg){
	if(unfold)return GT_NodeP_UnfoldSearch_exec(g.Has()?(CVP(GN::sNODE_)g) : sobj, proc, _us_arg);
	else	  return GT_NodeP_Search_exec	   (g.Has()?(CVP(GN::sNODE_)g) : sobj, proc, _us_arg);
}
void GNu::cTREE_NODE_P_OPTR::MoveNodes(P_ pos, bool pos_back, GN::sLIST<P_> *nodes){
	P<GN::sNODE_> pos0 = CVP(GN::sNODE_)pos;
	GN::sNODE_ *tg_node = pos0;
	GN::sLIST< P<GN::sNODE_> > nodes1;
	{
		P<GN::sNODE_> tmp;
		GN::sLIST<P_>::iterator in = nodes->begin(), en = nodes->end();
		for(; in != en; ++in){
			tmp = CVP(GN::sNODE_)(*in);
			if(!tg_node->HasParent(tmp)){
				aCommentFunc(0,tmp);
				nodes1.push_back(tmp);
		}	}
	}
	bool need_reseek = zCurLevel->parent && !(zCurLevel->sobj->HasParent(this->sobj));
	GN::sLIST< P<GN::sNODE_> > nodes_bk;
	if(GT_HasUnfold(tg_node->aName) && tg_node->zSub.empty());
	else if(sobj == pos){
		tg_node = sobj;
		nodes_bk.splice(nodes_bk.end(), tg_node->zSub);
	}else{
		if(pos_back && HasUnfold(pos)){
			nodes_bk.splice(nodes_bk.end(), tg_node->zSub);
		}else{
			GN::sNODE_ *pos1 = tg_node;
			tg_node = tg_node->zParent;
			GN::sLIST< P<GN::sNODE_> >::iterator inn = tg_node->zSub.begin(), enn = tg_node->zSub.end();
			for(; inn != enn; ++inn){
				if((GN::sNODE_*)(*inn) == pos1){
					if(pos_back)++inn;
					if(inn != enn)nodes_bk.splice(nodes_bk.end(), tg_node->zSub, inn, enn);
					break;
			}	}
		}
	}
	GN::sLIST< P<GN::sNODE_> >::iterator itn = nodes1.begin(), etn = nodes1.end();
	for(; itn != etn; ++itn)aCommentFunc(tg_node, (*itn));

	tg_node->zSub.splice(tg_node->zSub.end(), nodes_bk);
	if(need_reseek){
		zCurLevel = this;
		zSeekNextNode(zCurShowStartNid);
	}
}
char* GNu::cTREE_NODE_P_OPTR::GetNodeName(P_ &node){
	return (CVP(GN::sNODE_)node)->aName;
}
P_ GNu::cTREE_NODE_P_OPTR::GetCurFristNode(){
	return zCurLevel->sobj;
}
void *GNu::cTREE_NODE_P_OPTR::GetDrawNodeFunc(){ return (void*)GT_TreeNodedDrawCall; }
int	  GNu::cTREE_NODE_P_OPTR::GetVisualNodeNum(){ return TreeVisualNodeNumStatist(sobj); }

/*************************************************************************************************************************************************************************************************/
uint8* GNu::cTREE_VIEW::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs){
	wpos = cSELECT_LIST_TEXT::zDataSave(wpos, share_objs);
	P<GNg::sIMAGE> uf_icon;
	if(aUnfoldIcon != aDefaultUnfoldImg)uf_icon = aUnfoldIcon;
	unsigned int pointer_id;
	GT_TMP_OBJECT_WRITE_INDEX( (*share_objs->pimage_list) , uf_icon );
	memcpy(wpos, &aSpacing,		 2);	  wpos += 2;
	memcpy(wpos, &aMergeIcon,	 1);	  wpos += 1;
	return wpos;
}
uint8* GNu::cTREE_VIEW::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs){
	rpos = cSELECT_LIST_TEXT::zDataLoad(rpos, share_objs);
	unsigned int pointer_id;
	memcpy(&pointer_id,    rpos, 4);	  rpos += 4;
	if(pointer_id != 0xffffffff)aUnfoldIcon = (*(share_objs->pimage_refs))[pointer_id];
	memcpy(&aSpacing,	rpos, 	 2);	  rpos += 2;
	memcpy(&aMergeIcon,	rpos,    1);	  rpos += 1;
	zArrowAm.aFrame_ts.sx = (float)zTextOut->zParame.height / (float)zOffsetArrow->width;
	return rpos;
}
unsigned int GNu::cTREE_VIEW::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	GT_TMP_PACK_DATA_SAVE( 1260 , cTREE_VIEW )
	return wpos - data_out;
}
unsigned int GNu::cTREE_VIEW::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	GT_TMP_PACK_DATA_LOAD( cTREE_VIEW ) 
	if(!zNodeOptr){ zZoningNum = 0; zFFrames->zMax_num = 0; }
	return rpos - data;
}
#endif //_GN_UILISTTREE_H_
