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
#include "GN_Def.h"
#ifdef _GN_2DSCENE_H_
/*************************************************** 2D场景对象 ****************************************************************/
GN_API const char *GNg::ObjTypeNames[] = { "群组", "静态图像", "镜头光环", "正规动画图筐", "动画图筐", "图筐发射器", "复合发射器", "字筐发射器" };

P<GNg::iOBJ2D_EXT> GT_DefObj2dExtCreate(unsigned int _type){
	return P<GNg::iOBJ2D_EXT>(1);
}
P<GNg::iOBJ2D_EXT> (*GNg::iOBJ2D_EXT::aOnCreate)(unsigned int _type) = GT_DefObj2dExtCreate;

GN::sLIST<int*> GT_2DObjectPlugins;

GN_API void GN_Plugin2DObjectAdd(int *module, GT_PFUNC_VOID create_func){
	GT_2DObjectPlugins.push_back(module);
}
GN_API int	GN_Plugins2DObjectRemove(){
	GNg::iOBJ2D_EXT::aOnCreate = GT_DefObj2dExtCreate;
	return GT_RemoveListPlugins(GT_2DObjectPlugins);
}
/*-----------------------------------------------------------------------------------------------------------------------------*/
void GNg::cOBJPTR::UnComment(cOBJ2D *obj){
	if(aPtr && zParent){
		if(obj)*obj = *aPtr;
		((GN::sLIST<GNg::cOBJ2D>*)(zParent->aObj))->del(aPtr);
		aPtr = 0;
	}
}
void GNg::cOBJPTR::InsertFront(cOBJ2D &obj, cOBJPTR *ptr_out){
	if(aPtr && zParent){
		GN::sLIST<GNg::cOBJ2D> *_subs = (GN::sLIST<GNg::cOBJ2D>*)(zParent->aObj);
		GN::sLIST<GNg::cOBJ2D>::iterator ito = _subs->insert(++(_subs->to_iterator(aPtr)), obj);
		if(ptr_out){
			ptr_out->zParent = zParent;
			ptr_out->aPtr = &(*ito);
		}
	}
}
void GNg::cOBJPTR::InsertBack (cOBJ2D &obj, cOBJPTR *ptr_out){
	if(aPtr && zParent){
		GN::sLIST<GNg::cOBJ2D> *_subs = (GN::sLIST<GNg::cOBJ2D>*)(zParent->aObj);
		GN::sLIST<GNg::cOBJ2D>::iterator ito = _subs->insert(_subs->to_iterator(aPtr), obj);
		if(ptr_out){
			ptr_out->zParent = zParent;
			ptr_out->aPtr = &(*ito);
		}
	}
}
int GNg::cOBJPTR::OrderMove(int fn){
	if(fn && aPtr && zParent)
		 return ((GN::sLIST<GNg::cOBJ2D>*)(zParent->aObj))->order(&aPtr, fn);
	else return 0;
}
GNg::cOBJPTR GNg::cOBJPTR::Clone(){
	GNg::cOBJPTR new_obj;
	if(aPtr && zParent){
		new_obj.zParent = zParent;
		new_obj.aPtr = ((GN::sLIST<GNg::cOBJ2D>*)(zParent->aObj))->add(cOBJ2D());
		new_obj.aPtr->CopyData(*aPtr, true);
	}
	return new_obj;
}
/*-----------------------------------------------------------------------------------------------------------------------------*/
GNg::cOBJ2D& GNg::cOBJ2D::operator =(const cOBJ2D &src){
	this->Delete();
	aUsExt   = src.aUsExt;
	aObj     = src.aObj;
	aOBJtype = src.aOBJtype;
	return *this;
}
//从组里查找2D对象(组, 查找对象名, 当前在对象名表里历遍到的名, 对象地址输出)找到了对象返回0 否则返回当前历遍到的名
static char** GT_SearchObj2dFromGroup(GNg::cOBJ2D *node, const char *name, char **ppname, GNg::cOBJPTR *_optr){
	GN::sLIST<GNg::cOBJ2D> *objs = (GN::sLIST<GNg::cOBJ2D>*)(node->aObj);
	GN::sLIST<GNg::cOBJ2D>::iterator ito = objs->begin(), eto = objs->end();
	for(; ito != eto; ++ito){
		if(!strcmp(*ppname, name)){
			 _optr->zParent = node;
			 _optr->aPtr = &(*ito);
			 return 0;
		}
		++ppname;
		if(ito->aOBJtype == GNg::eOBJ_GROUP){
			ppname = GT_SearchObj2dFromGroup(&(*ito), name, ppname, _optr);
			if(!ppname)return 0;
		}
	}
	return ppname;
}
//从组里查找具体内容匹配2D对象(组, 具体内容, 对象地址输出)返回是否找到了对象
static bool GT_SearchObj2dFromGroup(GNg::cOBJ2D *node, const void *_obj, GNg::cOBJPTR *_optr){
	GN::sLIST<GNg::cOBJ2D> *objs = (GN::sLIST<GNg::cOBJ2D>*)(node->aObj);
	GN::sLIST<GNg::cOBJ2D>::iterator ito = objs->begin(), eto = objs->end();
	for(; ito != eto; ++ito){
		if(ito->aObj == _obj){
			 _optr->zParent = node;
			 _optr->aPtr = &(*ito);
			 return true;
		}
		if(ito->aOBJtype == GNg::eOBJ_GROUP && GT_SearchObj2dFromGroup(&(*ito), _obj, _optr))return true;
	}
	return false;
}
GNg::cOBJPTR GNg::cOBJ2D::Search(const void *_obj){
	GNg::cOBJPTR optr;
	if(aOBJtype == eOBJ_GROUP)GT_SearchObj2dFromGroup(this, _obj, &optr);
	return optr;
}
void GNg::cOBJ2D::Comment(cOBJ2D &obj2d, cOBJPTR *ptr_out){
	if(aOBJtype == eOBJ_GROUP){
		if(ptr_out){
			ptr_out->zParent = this;
			ptr_out->aPtr = ((GN::sLIST<GNg::cOBJ2D>*)aObj)->add(obj2d);
		}else ((GN::sLIST<GNg::cOBJ2D>*)aObj)->push_back(obj2d);
	}
}
void GNg::cOBJ2D::Delete(){
	if(aObj && aUsExt.Us() < 2){
		switch(aOBJtype){
		case eOBJ_IMAGE:			   delete ((sIMAGE_OBJ*)aObj);			 break;			
		case eOBJ_FRAME_RC:			   delete []((cFRAME_RC*)aObj);			 break;				
		case eOBJ_FRAME:			   delete []((cFRAME*)aObj);			 break;			
		case eOBJ_FRAME_LAUNCHER:	   delete ((cFRAME_LAUNCHER*)aObj);		 break;		
		case eOBJ_FRAME_LAUNCHER2D:	   delete ((cFRAME_LAUNCHER2D*)aObj);	 break;	
		case eOBJ_FRAME_LAUNCHER_FONT: delete ((cFRAME_LAUNCHER_FONT*)aObj); break;	
		case eOBJ_LENS_FLARE:		   delete ((cLENS_FLARE_OBJ*)aObj);		 break;
		case eOBJ_GROUP:			   delete ((GN::sLIST<cOBJ2D>*)aObj);	 break;	
		}
		aObj = 0;
		aUsExt.Delete();
	}
}
void GNg::cOBJ2D::CopyData(const cOBJ2D &src, bool cpy_state){
	this->Delete();
	aOBJtype = src.aOBJtype;
	switch(aOBJtype){
	case eOBJ_IMAGE:
		aObj = new sIMAGE_OBJ(*((sIMAGE_OBJ*)src.aObj));
		break;			
	case eOBJ_FRAME_RC:{
		cFRAME_RC *_obj = new cFRAME_RC[1];
		*_obj = *((cFRAME_RC*)src.aObj);
		if(!cpy_state)_obj->ReSetPlaytime();
		aObj = _obj;
		}break;			
	case eOBJ_FRAME:{
		cFRAME    *_obj = new cFRAME[1];
		*_obj = *((cFRAME*)src.aObj);
		if(!cpy_state)_obj->ReSetPlaytime();
		aObj = _obj;
		}break;			
	case eOBJ_FRAME_LAUNCHER:{
		cFRAME_LAUNCHER    *_obj;
		if(cpy_state){
			  _obj = new cFRAME_LAUNCHER;
			  _obj->PerfectCopy(*((cFRAME_LAUNCHER*)src.aObj));
		}else _obj = new cFRAME_LAUNCHER(*((cFRAME_LAUNCHER*)src.aObj));
		aObj = _obj;
		}break;		
	case eOBJ_FRAME_LAUNCHER2D:{
		cFRAME_LAUNCHER2D  *_obj;
		if(cpy_state){
			  _obj = new cFRAME_LAUNCHER2D;
			  _obj->PerfectCopy(*((cFRAME_LAUNCHER2D*)src.aObj));
		}else _obj = new cFRAME_LAUNCHER2D(*((cFRAME_LAUNCHER2D*)src.aObj));
		aObj = _obj;
		}break;	
	case eOBJ_FRAME_LAUNCHER_FONT:{
		cFRAME_LAUNCHER_FONT *_obj;
		if(cpy_state){
			  _obj = new cFRAME_LAUNCHER_FONT;
			  _obj->PerfectCopy(*((cFRAME_LAUNCHER_FONT*)src.aObj));
		}else _obj = new cFRAME_LAUNCHER_FONT(*((cFRAME_LAUNCHER_FONT*)src.aObj));
		aObj = _obj;
		}break;
	case eOBJ_LENS_FLARE:
		aObj = new cLENS_FLARE_OBJ(*((cLENS_FLARE_OBJ*)src.aObj));
		break;
	case eOBJ_GROUP:{
		GN::sLIST<cOBJ2D> *_objs2 = (GN::sLIST<cOBJ2D>*)src.aObj, *_objs = new GN::sLIST<cOBJ2D>;
		GN::sLIST<cOBJ2D>::iterator ito = _objs2->begin(), eto = _objs2->end();
		for(; ito != eto; ++ito)_objs->add(cOBJ2D())->CopyData(*ito, cpy_state);
		aObj = _objs;
		}break;
	default: aObj = 0;	break;
	}
	if(src.aUsExt){
	    aUsExt = src.aUsExt->OnCloneOut();
        aUsExt->obj  = this; /*将扩展绑定对象指向该对象*/
	    aUsExt->type = src.aUsExt->type;
	}
}
struct GT_sFRAME_AMSTOP_TASK :public GN_sTIME_EXEC_TASK //图框动画停止任务
{
	GNg::cOBJ2D			check_node;			//检测节点

	virtual bool Run();						//运行任务()返回是否可以丢弃该任务
};
//检测节点里的动画筐播放结束就停止(节点)
void GT_CheckFrameAmEndStop(GNg::cOBJ2D	&ck_node){
	switch(ck_node.aOBJtype){
	case GNg::eOBJ_FRAME_RC:		
	case GNg::eOBJ_FRAME:{
		GNg::cFRAME_RC *frc = (GNg::cFRAME_RC*)(ck_node.aObj);
		if(!frc->zAm_s_e && frc->zAm_mode == GNg::eAM_REPEAT){
			frc->zNowtime = frc->zEndtime;
			frc->SetAmMode(GNg::eAM_ONCE_HIDE);
		}
		}break;
	case GNg::eOBJ_GROUP:{
		GN::sLIST<GNg::cOBJ2D>::iterator ito = ((GN::sLIST<GNg::cOBJ2D>*)(ck_node.aObj))->begin(), eto = ((GN::sLIST<GNg::cOBJ2D>*)(ck_node.aObj))->end();
		for(; ito != eto; ++ito)GT_CheckFrameAmEndStop(*ito);
		}break;
	}
}
bool GT_sFRAME_AMSTOP_TASK::Run(){
	if(check_node.aUsExt.Us() < 2)return true;
	GT_CheckFrameAmEndStop(check_node);
	if(GN::TimeGetRunCount() >= exec_time){
		if(proc)proc(usarg);
		return true;
	}
	return false;
}
unsigned int GT_Obj2dAnimationIn(GNg::cOBJ2D *_obj, bool yes){
	unsigned int end_time = 0;
	switch(_obj->aOBJtype){
	case GNg::eOBJ_FRAME_RC:		
	case GNg::eOBJ_FRAME:{
		GNg::cFRAME_RC *frc = (GNg::cFRAME_RC*)(_obj->aObj);
		if(yes){
			if(frc->zAm_mode == GNg::eAM_ONCE_HIDE){ frc->SetAmMode(GNg::eAM_ONCE_KEEP); frc->ReSetPlaytime(); }
		}else if(frc->zAm_mode == GNg::eAM_ONCE_KEEP)frc->SetAmMode(GNg::eAM_REPEAT);
		end_time = frc->zEndtime;
		}break;
	case GNg::eOBJ_FRAME_LAUNCHER_FONT:{
		GNg::cFRAME_LAUNCHER_FONT *ffm = (GNg::cFRAME_LAUNCHER_FONT*)(_obj->aObj);
		if(yes){
			uint8 locbuf[4096];
			ffm->zTextSave(locbuf);
			ffm->SetText((char*)(locbuf+4), *((unsigned int*)locbuf));
		}else ffm->ToHide();
		end_time = ffm->zFontFrame->zEndtime; 
		}break;	
	case GNg::eOBJ_FRAME_LAUNCHER:	
	case GNg::eOBJ_FRAME_LAUNCHER2D:
		if(yes)((GNg::cFRAME_LAUNCHER*)(_obj->aObj))->ContinueGenerate();
		else   ((GNg::cFRAME_LAUNCHER*)(_obj->aObj))->StopGenerate();
		break;
	case GNg::eOBJ_GROUP:{
		unsigned int end_time1;
		GN::sLIST<GNg::cOBJ2D>::iterator ito = ((GN::sLIST<GNg::cOBJ2D>*)(_obj->aObj))->begin(), eto = ((GN::sLIST<GNg::cOBJ2D>*)(_obj->aObj))->end();
		for(; ito != eto; ++ito){
			end_time1 = GT_Obj2dAnimationIn(&(*ito), yes);
			if(end_time1 > end_time)end_time = end_time1;
		}
		}break;
	}
	return end_time;
}
unsigned int GNg::cOBJ2D::AnimationIn(bool yes, void (*end_proc)(void*), void *us_arg){
	unsigned int end_time = GT_Obj2dAnimationIn(this, yes);
	switch(aOBJtype){
	case GNg::eOBJ_FRAME_RC:		
	case GNg::eOBJ_FRAME:
	case GNg::eOBJ_GROUP:
		if(yes){
		  if(end_proc){
			P<GN_sTIME_EXEC_TASK> time_exec_task(1);
			GN_sTIME_EXEC_TASK *_exec_task = time_exec_task;
			_exec_task->exec_time = end_time + GN::TimeGetRunCount();
			_exec_task->proc = end_proc;
			_exec_task->usarg = us_arg;
			GN_TaskPushToIdleExec(time_exec_task, false);
		  }
		}else{
			P<GT_sFRAME_AMSTOP_TASK> frame_stop_task(1);
			GT_sFRAME_AMSTOP_TASK *_stop_task = frame_stop_task;
			_stop_task->exec_time = end_time + GN::TimeGetRunCount();
			_stop_task->proc = end_proc;
			_stop_task->usarg = us_arg;
			_stop_task->check_node = *this;
			GN_TaskPushToIdleExec(frame_stop_task, false);
		}
		break;
	}
	return end_time;
}
void GNg::cOBJ2D::SetBlendMode(unsigned char blend){
	switch(aOBJtype){
	case eOBJ_IMAGE: ((sIMAGE_OBJ*)aObj)->bmode = blend; break;			
	case eOBJ_FRAME_RC:		
	case eOBJ_FRAME: ((cFRAME_RC*)aObj)->aBlendmode = blend; break;			
	case eOBJ_FRAME_LAUNCHER:	
	case eOBJ_FRAME_LAUNCHER2D: ((cFRAME_LAUNCHER*)aObj)->zFrame->aBlendmode = blend; break;	
	case eOBJ_FRAME_LAUNCHER_FONT: ((cFRAME_LAUNCHER_FONT*)aObj)->zFontFrame->aBlendmode = blend; break;	
	case eOBJ_GROUP:{
		GN::sLIST<cOBJ2D>::iterator ito = ((GN::sLIST<cOBJ2D>*)aObj)->begin(), eto = ((GN::sLIST<cOBJ2D>*)aObj)->end();
		for(; ito != eto; ++ito)ito->SetBlendMode(blend);
		}break;
	}
}
void GNg::cOBJ2D::SetColor(unsigned int color){
	switch(aOBJtype){
	case eOBJ_IMAGE: ((sIMAGE_OBJ*)aObj)->c = color; break;			
	case eOBJ_FRAME_RC:		
	case eOBJ_FRAME: ((cFRAME_RC*)aObj)->aColor = color; break;			
	case eOBJ_FRAME_LAUNCHER:	
	case eOBJ_FRAME_LAUNCHER2D: ((cFRAME_LAUNCHER*)aObj)->aGenColor = color; break;	
	case eOBJ_FRAME_LAUNCHER_FONT: ((cFRAME_LAUNCHER_FONT*)aObj)->aGenColor = color; ((cFRAME_LAUNCHER_FONT*)aObj)->ReSetPlaytime(); break;	
	case eOBJ_LENS_FLARE: ((cLENS_FLARE_OBJ*)aObj)->aColor = color; break;
	case eOBJ_GROUP:{
		GN::sLIST<cOBJ2D>::iterator ito = ((GN::sLIST<cOBJ2D>*)aObj)->begin(), eto = ((GN::sLIST<cOBJ2D>*)aObj)->end();
		for(; ito != eto; ++ito)ito->SetColor(color);
		}break;
	}
}
void GNg::cOBJ2D::ColorBlend(unsigned int color, unsigned int (*func)(unsigned int _color, unsigned int obj_color)){
	switch(aOBJtype){
	case eOBJ_IMAGE: ((sIMAGE_OBJ*)aObj)->c = func(color, ((sIMAGE_OBJ*)aObj)->c); break;			
	case eOBJ_FRAME_RC:		
	case eOBJ_FRAME: ((cFRAME_RC*)aObj)->aColor = func(color, ((cFRAME_RC*)aObj)->aColor); break;			
	case eOBJ_FRAME_LAUNCHER:	
	case eOBJ_FRAME_LAUNCHER2D: ((cFRAME_LAUNCHER*)aObj)->aGenColor = func(color, ((cFRAME_LAUNCHER*)aObj)->aGenColor); break;	
	case eOBJ_FRAME_LAUNCHER_FONT: ((cFRAME_LAUNCHER_FONT*)aObj)->aGenColor = func(color, ((cFRAME_LAUNCHER_FONT*)aObj)->aGenColor); ((cFRAME_LAUNCHER_FONT*)aObj)->ReSetPlaytime(); break;	
	case eOBJ_LENS_FLARE: ((cLENS_FLARE_OBJ*)aObj)->aColor = func(color, ((cLENS_FLARE_OBJ*)aObj)->aColor); break;
	case eOBJ_GROUP:{
		GN::sLIST<cOBJ2D>::iterator ito = ((GN::sLIST<cOBJ2D>*)aObj)->begin(), eto = ((GN::sLIST<cOBJ2D>*)aObj)->end();
		for(; ito != eto; ++ito)ito->ColorBlend(color, func);
		}break;
	}
}
void GNg::cOBJ2D::ToMove(float x, float y){
	switch(aOBJtype){
	case eOBJ_IMAGE: ((sIMAGE_OBJ*)aObj)->x += x; ((sIMAGE_OBJ*)aObj)->y += y; break;			
	case eOBJ_FRAME_RC:		
	case eOBJ_FRAME: ((cFRAME_RC*)aObj)->aFrame_ts.x += x; ((cFRAME_RC*)aObj)->aFrame_ts.y += y; break;			
	case eOBJ_FRAME_LAUNCHER:	
	case eOBJ_FRAME_LAUNCHER2D: ((cFRAME_LAUNCHER*)aObj)->aGenRngX += x; ((cFRAME_LAUNCHER*)aObj)->aGenRngY += y; break;	
	case eOBJ_FRAME_LAUNCHER_FONT: ((cFRAME_LAUNCHER_FONT*)aObj)->Move(x,y); break;	
	case eOBJ_LENS_FLARE: ((cLENS_FLARE_OBJ*)aObj)->aSrcX += x;	((cLENS_FLARE_OBJ*)aObj)->aSrcY += y; break;
	case eOBJ_GROUP:{
		GN::sLIST<cOBJ2D>::iterator ito = ((GN::sLIST<cOBJ2D>*)aObj)->begin(), eto = ((GN::sLIST<cOBJ2D>*)aObj)->end();
		for(; ito != eto; ++ito)ito->ToMove(x,y);
		}break;
	}
}
unsigned int GNg::cOBJ2D::GetIncludeTypeObjNum(int8 obj_type){
	unsigned int num = (aOBJtype == obj_type ? 1 : 0);
	if(aOBJtype == eOBJ_GROUP){
		GN::sLIST<cOBJ2D>::iterator ito = ((GN::sLIST<cOBJ2D>*)aObj)->begin(), eto = ((GN::sLIST<cOBJ2D>*)aObj)->end();
		for(; ito != eto; ++ito)num += ito->GetIncludeTypeObjNum(obj_type);
	}
	return num;
}
unsigned int GNg::cOBJ2D::GetIncludeTypeObjs(cOBJ2D *mds_out, int8 obj_type){
	unsigned int num = 0;
	if(aOBJtype == obj_type){ *mds_out = *this; num = 1; }
	if(aOBJtype == eOBJ_GROUP){
		GN::sLIST<cOBJ2D>::iterator ito = ((GN::sLIST<cOBJ2D>*)aObj)->begin(), eto = ((GN::sLIST<cOBJ2D>*)aObj)->end();
		for(; ito != eto; ++ito)num += ito->GetIncludeTypeObjs(mds_out+num, obj_type);
	}
	return num;
}
P< GNg::cOBJ2D > GNg::cOBJ2D::GetIncludeTypeObjs(int8 obj_type){        
	unsigned int obn = GetIncludeTypeObjNum(obj_type);
	if(obn){
		P< cOBJ2D > objs(obn);
		GetIncludeTypeObjs(objs, obj_type);
		return objs;
	}else return P_();
}
void GNg::cOBJ2D::ExpandWorldBox(sRECT2 *box_out){
	if(eOBJ_GROUP != eOBJ_GROUP){
		sRECT2 rc;
		switch(aOBJtype){
		case eOBJ_IMAGE:{
			sIMAGE_OBJ *_img = (sIMAGE_OBJ*)aObj;
			rc.left = (float)_img->x; rc.top = (float)_img->y; rc.right = (float)(_img->x + _img->w);  rc.bottom = (float)(_img->y + _img->h);
			}break;			
		case eOBJ_FRAME_RC:
			rc = ((cFRAME_RC*)aObj)->aLevel_range; 
			break;
		case eOBJ_FRAME:{
			cFRAME *_fm = (cFRAME*)aObj;
			sVEC2 vexs[4];
			*((sRECT2*)vexs) = _fm->aLevel_range;
			vexs[2].x = _fm->aLevel_ts.x;  vexs[2].y = _fm->aLevel_ts.y;
			vexs[3] = vexs[0] + (vexs[1] - vexs[2]);
			GetRangeBoxFromVertexs(&rc, vexs, 4);
			}break;			
		case eOBJ_FRAME_LAUNCHER:	
		case eOBJ_FRAME_LAUNCHER_FONT:{
			cFRAME_LAUNCHER *laun = ((cFRAME_LAUNCHER*)aObj);
			rc.left = laun->aGenRngX; rc.top = laun->aGenRngY; rc.right = laun->aGenRngX + laun->aGenRngW;  rc.bottom = laun->aGenRngY + laun->aGenRngH;
			}break;	
		case eOBJ_FRAME_LAUNCHER2D:{	
			cFRAME_LAUNCHER2D *laun2d = ((cFRAME_LAUNCHER2D*)aObj);
			float w2 = laun2d->aGenRngW * 0.5f, h2 = laun2d->aGenRngH * 0.5f;  /*模版代码:粗略计算复合图筐发射器范围*/
			GNg::sVEC2 cp = laun2d->zGenRngAx * w2 + laun2d->zGenRngAy * h2 + GNg::sVEC2(laun2d->aGenRngX,laun2d->aGenRngY);
			rc.left = cp.x - w2;  rc.top = cp.y - h2;  rc.right = cp.x + w2;  rc.bottom = cp.y + h2;
			}break;		
		}
		RectAdd(box_out, &rc);
	}else{
		GN::sLIST<cOBJ2D>::iterator ito = ((GN::sLIST<cOBJ2D>*)aObj)->begin(), eto = ((GN::sLIST<cOBJ2D>*)aObj)->end();
		for(; ito != eto; ++ito)ito->ExpandWorldBox(box_out);
	}
}
void GNg::cOBJ2D::GetWorldBox(GNg::sRECT2 *box_out){
	box_out->Init(GN_INT_MAX, GN_INT_MAX, GN_INT_MIN, GN_INT_MIN);
	ExpandWorldBox(box_out);
}
unsigned int GNg::cOBJ2D::UpdatePlay(unsigned int updatetime){
	switch(aOBJtype){
	case eOBJ_IMAGE:
		if(((sIMAGE_OBJ*)aObj)->HitView(ViewCur))((sIMAGE_OBJ*)aObj)->Show();
		return 1;
	case eOBJ_FRAME_RC:			   return ((cFRAME_RC*)aObj)->PlayFrame(updatetime);
	case eOBJ_FRAME:			   return ((cFRAME*)aObj)->PlayFrame(updatetime);
	case eOBJ_FRAME_LAUNCHER:	   return ((cFRAME_LAUNCHER*)aObj)->PlayFrame(updatetime);
	case eOBJ_FRAME_LAUNCHER2D:	   return ((cFRAME_LAUNCHER2D*)aObj)->PlayFrame(updatetime);
	case eOBJ_FRAME_LAUNCHER_FONT: return ((cFRAME_LAUNCHER_FONT*)aObj)->PlayFrame(updatetime);
	case eOBJ_LENS_FLARE:{
		cLENS_FLARE_OBJ *_flare = (cLENS_FLARE_OBJ*)aObj;
		cVIEW2D *view = ViewCur;
		short px = (short)_flare->aSrcX, py = (short)_flare->aSrcY;
		if(px > view->ortho_left && px < view->ortho_right && py > view->ortho_top && py < view->ortho_bottom){
			_flare->PosDraw();
			if(GNg::cSCENE2D::zCurProcScene)GNg::cSCENE2D::zCurProcScene->zProcLensFlares.push_back(_flare);
		}
		}return 1;
	case eOBJ_GROUP:{
		GN::sLIST<cOBJ2D> *objs = (GN::sLIST<cOBJ2D>*)aObj;
		GN::sLIST<cOBJ2D>::iterator ito = objs->begin(), eto = objs->end();
		unsigned int max_time = 0, now_time;
		for(; ito != eto; ++ito){
			now_time = ito->UpdatePlay(updatetime);
			if(now_time > max_time)max_time = now_time; //统计最大时间作为返回时间
		}
		return max_time;
		}
	}
	return 0;
}
unsigned int GNg::cSCENE2D::UpdatePlay(unsigned int updatetime){
	zCurProcScene = this;
	unsigned int ret = cOBJ2D::UpdatePlay(updatetime);
	zCurProcScene = 0;
	unsigned int li = 0;
	for(; li < zProcLensFlares.num; ++li)zProcLensFlares.data[li]->Display(updatetime);
	zProcLensFlares.reset();
	return ret;
}
GNg::cSCENE2D * GNg::cSCENE2D::zCurProcScene = 0;

GNg::cSCENE2D::cSCENE2D():zObjNameBuf(0){
	aOBJtype = eOBJ_GROUP;
}
GNg::cSCENE2D::~cSCENE2D(){
	GN_DeleteArray(zObjNameBuf);
}
GNg::cOBJPTR GNg::cSCENE2D::Search(const char *name){
	GT_TMP_SEARCH_SCENE_PTR( GNg::cOBJPTR , Obj2d , this )
}
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern GN_API const char* GT_SYS_GV_Point32ImgName;
extern GN_API const char* GT_SYS_FontDefName;

//写入图像名到缓冲(缓冲写位置, 图像, 临时名处理缓冲, 是否输出图像纹理数和颜色格式)返回最后的写完位置
GN_API uint8 *GT_WriteImageNameToBuffer(uint8 *wpos, GNg::sIMAGE *_img, char *img_name/*[64]*/, bool texn_out){
    unsigned int num;
	char *pimg_name = _img->fileName;
	if(pimg_name){
		char *pimg_name1 = &(pimg_name[ strlen(pimg_name) ]);
        num = 0;
		while(pimg_name1 != pimg_name && *pimg_name1 != '/' && *pimg_name1 != '\\'){ pimg_name1--; num++; }
        if(pimg_name1 != pimg_name){ pimg_name1++; num--; }  /*排除目录名 找到斋文件名*/
        num+=1;
		strcpy(img_name,pimg_name1);
		if(_img->add_0000){
            strcat(img_name,"0000");
			num += 4;
		}
        memcpy(wpos, &num, 4); wpos += 4; /*写入图象名长度到缓冲*/
        memcpy(wpos, img_name, num); wpos += num; /*写入图象名*/
		if(texn_out){
			unsigned short crfmt = _img->cFormat;
			memcpy(wpos, &(_img->texnum), 2);  wpos += 2;
			memcpy(wpos, &crfmt,		  2);  wpos += 2;
		}
	}else{ /*动态的纹理*/
		if(_img->texture == GN::RunRec.aGV_Point32->texture){
			num = strlen(GT_SYS_GV_Point32ImgName)+1;
			memcpy(wpos, &num, 4); wpos += 4;   /*写入图象名长度到缓冲*/
			memcpy(wpos, GT_SYS_GV_Point32ImgName, num); wpos += num; /*写入图象名*/
		}else/* if(_img->cFormat == GNg::CR_LUMINANCE && _img->width == 16 && _img->height == 16)*/{
			num = 0;
			memcpy(wpos, &num, 4); wpos += 4;   /*写入图象名长度到缓冲*/
		}
	}
	return wpos;
}
//写入各个动画筐数据到文件(返回数据起始在文件的位置, 临时处理缓冲, 动画筐是否嵌入到图框发射里的 2为3D图框, 动画筐列表, 收集在动画图筐里的所有共享对象, 文件)
GN_API void GT_WriteFramesToFile(unsigned int *rc_pos, uint8 *buf, uint8 in_launc, GN::sLIST< GNg::cFRAME* > &list_frame, GNg::cFRAME_RC::sREF_OBJSW *_in_frame_objs, GNf::sFILE *fp){
	uint8 *wpos;
	rc_pos[ 0 ] = (unsigned int)GNf::Tell(fp);
    unsigned int num = (unsigned int)list_frame.size();
    GNf::Write((char*)&num , 4, 1, fp);
	if(num){ //写入各个框数据
	    GN::sLIST< GNg::cFRAME* >::iterator im = list_frame.begin(), em = list_frame.end();
		for(; im != em; ++im){
			wpos = (*im)->zDataSave(buf,_in_frame_objs, in_launc);
			GNf::Write((char*)buf, 1, wpos - buf, fp);
		}
	}
}
//写入动画筐里所引用到的所有数据到文件(返回5种数据起始在文件的位置, 临时处理缓冲, 在发射器里的保留鉴别对象列表, 所有在动画图筐里的共享对象, 文件)
GN_API void GT_WriteInFrameSubObjDataToFile(unsigned int *rc_pos/*[5]*/, uint8 *buf, GN::sLIST< P<char> > &list_keep_obj, GNg::cFRAME_RC::sREF_OBJSW *_in_frame_objs, GNf::sFILE *fp){
	uint8 *wpos;
	rc_pos[ 0 ] = (unsigned int)GNf::Tell(fp);
	unsigned int num = _in_frame_objs ? _in_frame_objs->pevs.size() : 0;
    GNf::Write((char*)&num , 4, 1, fp);
	if(num){ //写入各个动画事件组
		GNg::sAM_EVENT *_evs;
		unsigned int  evi, evn;
		GN::sLIST<unsigned int >::iterator itn = _in_frame_objs->evns.begin();
		GN::sLIST< GNg::sAM_EVENT* >::iterator im = _in_frame_objs->pevs.begin(), em = _in_frame_objs->pevs.end();
		for(; im != em; ++im, ++itn){
			 evn = (*itn);
			_evs = (*im);
			wpos = buf;
			memcpy(wpos, &evn, 4); wpos += 4;
			for(evi = 0; evi < evn; ++evi)wpos = _evs[evi].zDataSave(wpos);
			GNf::Write((char*)buf, 1, wpos - buf, fp);
		}
	}
	rc_pos[ 1 ] = (unsigned int)GNf::Tell(fp);
	num = _in_frame_objs ? _in_frame_objs->ptidxs.size() : 0;
    GNf::Write((char*)&num , 4, 1, fp);
	if(num){ //写入各个动画框图序ID组
		unsigned short tdn;
		GN::sLIST<unsigned short>::iterator itn = _in_frame_objs->tidxns.begin();
		GN::sLIST<unsigned char*>::iterator im = _in_frame_objs->ptidxs.begin(), em = _in_frame_objs->ptidxs.end();
		for(; im != em; ++im, ++itn){
			 tdn = (*itn);
			wpos = buf;
			memcpy(wpos, &tdn, 4);	  wpos += 4;
			memcpy(wpos, (*im), tdn); wpos += tdn;
			GNf::Write((char*)buf, 1, wpos - buf, fp);
		}
	}
	rc_pos[ 2 ] = (unsigned int)GNf::Tell(fp);
	num = /*_in_frame_objs ? _in_frame_objs->psounds.size() :*/ 0; /*模版代码:暂时不记录筐用到的声音对象*/
    GNf::Write((char*)&num , 4, 1, fp);
	if(num){
		;
	}
	rc_pos[ 3 ] = (unsigned int)GNf::Tell(fp); //图框的保留鉴别对象
	num = _in_frame_objs ? _in_frame_objs->pkeeps.size() : 0;
    GNf::Write((char*)&num , 4, 1, fp);

	rc_pos[ 4 ] = (unsigned int)GNf::Tell(fp); //图框发射器的保留鉴别对象
	num = list_keep_obj.size();
    GNf::Write((char*)&num , 4, 1, fp);
}
//转换图像名为适合读取的名(读取名输出, 图像名, 图像长度)
GN_API void GT_ConvertImageNameForLoad(char *img_name_out, const char *img_name, unsigned int name_len){
	strcpy(img_name_out, img_name);
	if(name_len > 3 && !strcmp(img_name_out+(name_len-4),"0000"))img_name_out[name_len-4] = 0;
	else strcpy(img_name_out+name_len, ".png");
}
//读取各个动画筐数据(缓冲读取位置, 动画筐列表, 所有在动画图筐里的共享引用对象)返回最后的读完位置
GN_API uint8 *GT_ReadFrames(uint8 *rpos, P< GNg::cFRAME* > &p_frames, GNg::cFRAME_RC::sREF_OBJSR *_frame_ref_objs){
    unsigned int ni, num;
	memcpy(&num, rpos, 4); rpos += 4; //GN_Log("fmn=%d\n", num);
	if(num){\
		p_frames.New(num);
		GNg::cFRAME **_frames = p_frames;
		for(ni = 0; ni < num; ++ni){
			_frames[ni] = new GNg::cFRAME;
			rpos = _frames[ni]->zDataLoad(rpos, _frame_ref_objs); //读取动画图框数据
			if(_frames[ni]->zPlay_obj == &GNg::Image){
				_frames[ni]->zPlay_obj = new GNg::sIMAGE;
				*(_frames[ni]->zPlay_obj) = *(GN::RunRec.aGV_Point32);
			}
		}
	}
	return rpos;
}
//读取动画筐里所引用到的所有数据(返回5种数据起始在文件的位置, 场景文件数据, 在发射器里的保留鉴别引用对象列表, 所有在动画图筐里的共享引用对象)
GN_API void GT_ReadInFrameSubObjData(unsigned int *rc_pos/*[5]*/, uint8 *fbuf, P< P<char> > &list_keep_obj, GNg::cFRAME_RC::sREF_OBJSR *_frame_ref_objs){
	unsigned int num, ni, ki, kn = *((unsigned int*)(fbuf + rc_pos[4]));
	list_keep_obj.New(kn); //GN_Log("lk=%d\n", kn);
	P<char> *_keepobs = list_keep_obj;
	for(ki = 0; ki < kn; ++ki)_keepobs[ki].New(1); //读入发射器保留鉴别对象数据

	kn = *((unsigned int*)(fbuf + rc_pos[3])); //GN_Log("fk=%d\n", kn);
	_frame_ref_objs->pkeeps.New(kn);
	_keepobs = _frame_ref_objs->pkeeps;
	for(ki = 0; ki < kn; ++ki)_keepobs[ki].New(1); //读入动画框保留鉴别对象数据

	uint8 *rpos = fbuf + rc_pos[2];  /*跳转到读取音频名数据位置*/
    memcpy(&num, rpos, 4); rpos += 4;
	if(num){
		////////////////////////;
	}
	rpos = fbuf + rc_pos[1];  /*跳转到读取动画框图序ID数据位置*/\
    memcpy(&num, rpos, 4); rpos += 4; //GN_Log("tin=%d\n", num);
	if(num){\
		_frame_ref_objs->ptidxs.New(num);\
		_frame_ref_objs->tidxns.New(num);\
		unsigned char **_tidxs = _frame_ref_objs->ptidxs;
		unsigned short *tdns = _frame_ref_objs->tidxns, tdn;
		for(ni = 0; ni < num; ++ni){\
			memcpy(&tdn, rpos, 4); rpos += 4;\
			 tdns[ni] = tdn;
			_tidxs[ni] = new unsigned char[tdn];
			memcpy(_tidxs[ni], rpos, tdn); rpos += tdn;\
		}\
	}\
	rpos = fbuf + rc_pos[0];  /*跳转到读取动画框事件数据位置*/\
    memcpy(&num, rpos, 4); rpos += 4; //GN_Log("evn=%d\n", num);
	if(num){\
		_frame_ref_objs->pevs.New(num);\
		_frame_ref_objs->evns.New(num);\
		GNg::sAM_EVENT **_evs = _frame_ref_objs->pevs;
		unsigned int  *_evns = _frame_ref_objs->evns, evn, evi;
		for(ni = 0; ni < num; ++ni){\
			memcpy(&evn, rpos, 4); rpos += 4;\
			 _evns[ni] = evn;
			_evs[ni] = new GNg::sAM_EVENT[evn];
			for(evi = 0; evi < evn; ++evi)rpos = _evs[ni][evi].zDataLoad(rpos);
		}\
	}\
}
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//从文件缓冲里读取2D场景共享引用对象集数据(2D场景共享引用对象集, 各种对象数据在文件的位置, 2D场景路径文件名, 文件缓冲, 返回读取进度)
void GT_Scene2DLoadShareObjData(GT_sSCENE2D_SHARE_OBJS_R &ref_objs, unsigned int *data_pos, const char *file_name, char *fbuf, float &progress){
    char file_path[384];
	strcpy(file_path, file_name);
	char *_filename = file_path + (GN_StrGetFileName(file_name) - file_name);

	unsigned int si, num = (*((unsigned int*)((uint8*)fbuf+data_pos[9]))) + (*((unsigned int*)((uint8*)fbuf+data_pos[8])));
	float step_progr = 0.6f / (float)num;
	uint8 *rpos;

	//为2D场景读取图片( 数据位置ID , 图像数组分配方法 , 图像分配方法 , 指定空图像的方法 , 指定图像读取失败的方法)
   #define GT_TMP_SCENE2D_LOAD_IMAGE( DID , NEW_ARRAY_FUNC , NEW_FUNC , NULL_IMG , FAIL_IMG ) \
	rpos = (uint8*)fbuf + data_pos[ DID ];  /*跳转到读取图片*/\
    memcpy(&num, rpos, 4); rpos += 4;\
	if(num){\
		NEW_ARRAY_FUNC ;\
		unsigned int str_size;\
		for(si = 0; si < num; ++si){\
		    memcpy(&str_size, rpos, 4);  rpos += 4; /*读图象名长度*/\
			if(str_size){\
				_load_imgs[si] NEW_FUNC ;\
				if(!strcmp((char*)rpos, GT_SYS_GV_Point32ImgName)){\
					*(_load_imgs[si]) = *(GN::RunRec.aGV_Point32);\
					rpos += str_size;\
				}else{\
					GT_ConvertImageNameForLoad(_filename, (char*)rpos, str_size-1);\
					if(!GNg::ImageLoad(file_path, *((unsigned short*)(rpos+str_size)), _load_imgs[si], (GNg::COLOR_FORMAT)(*((unsigned short*)(rpos+(str_size+2)))))){\
						if(strcmp((char*)rpos, GT_SYS_FontDefName)){\
							FAIL_IMG ;\
						}else *(_load_imgs[si]) = *(GN::RunRec.aFont.GetFontMap());\
					}\
					rpos += str_size + 4;\
				}\
			} NULL_IMG \
			progress += step_progr;\
		}\
	}
	GT_TMP_SCENE2D_LOAD_IMAGE( 9 , ref_objs.frame_ref_objs.pimgs.New(num); GNg::sIMAGE **_load_imgs = ref_objs.frame_ref_objs.pimgs , =new GNg::sIMAGE , else _load_imgs[si] = &GNg::Image; , delete _load_imgs[si];_load_imgs[si] = &GNg::Image )
	GT_TMP_SCENE2D_LOAD_IMAGE( 8 , ref_objs.pimages.New(num); P<GNg::sIMAGE> *_load_imgs = ref_objs.pimages , .New(1) ,  , _load_imgs[si].Delete() )

	//progress = 0.71f;
	rpos = (uint8*)fbuf + data_pos[7]; /*跳转到读取镜头光环效果*/
    memcpy(&num, rpos, 4); rpos += 4;
	if(num){
		ref_objs.pflares.New(num);
		for(si = 0; si < num; ++si){
			*_filename = 0;
			ref_objs.pflares[si].New(1);
			rpos = ref_objs.pflares[si]->zDataLoad(rpos, file_path);
	}	}
	GNg::cFRAME_RC::sREF_OBJSR *_fm_ref_objs = &(ref_objs.frame_ref_objs);
	GT_ReadInFrameSubObjData(data_pos+2, (uint8*)fbuf, ref_objs.pkeep_objs, _fm_ref_objs); //读取动画图框里引用到的对象数据
	
	progress = 0.76f;
	rpos = (uint8*)fbuf + data_pos[1]; /*跳转到读取子筐组数据*/
	memcpy(&num, rpos, 4); rpos += 4;
	if(num){
		unsigned int frame_wsize;
		{
			uint8 try_buf[sizeof(GNg::cFRAME)];
			GNg::cFRAME_RC size_try_frame;
			frame_wsize = size_try_frame.zDataSave(try_buf, 0, 0) - try_buf;
		}
		ref_objs.frame_ref_objs.pframes.New(num);
		ref_objs.frame_ref_objs.fmns.New(num);
		GNg::cFRAME_RC **_sfms = ref_objs.frame_ref_objs.pframes;
		unsigned short *_sfns = ref_objs.frame_ref_objs.fmns;
		unsigned short i, sn;
		for(si = 0; si < num; ++si){
			_sfns[si] = sn = *((unsigned short*)(rpos+1));
			_sfms[si] = (*rpos)? new GNg::cFRAME[sn] : new GNg::cFRAME_RC[sn];  //先分配好空间
			rpos += 3 + sn * frame_wsize;	//跳过暂不读数据
		}
		GNg::cFRAME_RC *_sfc;
		GNg::cFRAME	  *_sfm;
		rpos = (uint8*)fbuf + (data_pos[1] + 4); 
		for(si = 0; si < num; ++si){		//第二轮才开始读数据
			sn   = _sfns[si];
			if(*rpos){
				rpos += 3;
				_sfm = (GNg::cFRAME*)(_sfms[si]);
				for(i = 0; i < sn; ++i)rpos = _sfm[i].zDataLoad(rpos, _fm_ref_objs);  //读取子筐数据
			}else{
				rpos += 3;
				_sfc = _sfms[si];
				for(i = 0; i < sn; ++i)rpos = _sfc[i].zDataLoad(rpos, _fm_ref_objs);  //读取子筐数据
			}
		}
	}
	progress = 0.78f;
	GT_ReadFrames((uint8*)fbuf + data_pos[0], ref_objs.pframes, _fm_ref_objs); //读取发射器里引用到的筐
}
//保存2D场景共享对象集数据到文件流(2D场景共享对象集, 字体图列表, 返回各种对象数据在文件的位置, 图像导出路径里的任意文件名, 文件流)
void GT_Scene2DSaveShareObjData(GT_sSCENE2D_SHARE_OBJS &share_objs, GN::sLIST<GNg::sIMAGE*> *fontmaps, unsigned int *rc_pos/*[10]*/, const char *exp_pathfile, GNf::sFILE *fp){
	const unsigned int buf_size1 = sizeof(GNg::cFRAME) << 3;
	uint8 buf[buf_size1 + (sizeof(GNg::sAM_NODE)<<5)], *wpos;
	GT_WriteFramesToFile(rc_pos, buf, 1, share_objs.list_frame, &(share_objs.in_frame_objs), fp); //写入发射器引用到筐

	rc_pos[ 1 ] = (unsigned int)GNf::Tell(fp);  /*记录场景该项数据的起始位置*/
	GN::sLIST< GNg::cFRAME_RC* > &list_frames = share_objs.in_frame_objs.pframes;
    unsigned int num2 = list_frames.size(), num;
    GNf::Write((char*)&num2 , 4, 1, fp); /*写入子筐组数*/
	if(num2){							
		unsigned short si, sn;	//写入所有子筐组数据
		GN::sLIST< unsigned short > &list_fmns = share_objs.in_frame_objs.fmns;
		GN::sLIST< unsigned short >::iterator itfn = list_fmns.begin();
		GN::sLIST< GNg::cFRAME_RC* >::iterator	  itfm = list_frames.begin();
		GNg::cFRAME_RC *_sfms;
		for(;itfm != list_frames.end(); ++itfm, ++itfn){
			_sfms = *itfm;
			buf[0] = (_sfms->GetClassSize() == (int)sizeof(GNg::cFRAME) ? 1 : 0); //写入子筐类型
			*((unsigned short*)(buf+1)) = sn = *itfn; //写入该组子筐数
			wpos = buf + 3;
			for(si = 0; si < sn; ++si){
				if(buf[0])wpos = ((GNg::cFRAME*)_sfms)[si].zDataSave(wpos, &(share_objs.in_frame_objs), 0);
				else	  wpos =			    _sfms [si].zDataSave(wpos, &(share_objs.in_frame_objs), 0);
				GNf::Write((char*)buf, 1, wpos - buf, fp);
				wpos = buf;
			}
		}
	}
	GT_WriteInFrameSubObjDataToFile(rc_pos+2, buf, share_objs.list_keep_obj, &(share_objs.in_frame_objs), fp); //写入动画图框里引用到的对象数据
	
	if(fontmaps)share_objs.in_frame_objs.pimgs.splice(share_objs.in_frame_objs.pimgs.end(), *fontmaps);  //加入字图到筐图像里一起写入

	char exp_path[384], tmp_str[64];
	if(exp_pathfile){
		strcpy(exp_path, exp_pathfile);
		char *_filename = GN_StrGetFileName(exp_path);
		if(_filename > exp_path)*(_filename-1) = 0;
	}
	//向文件写入各种数据(      偏移位置号 , 相关数据连表 , 数据类型  )
   #define GT_TMP_SCENE_WRITE_DATA_WP( POS ,     _LIST ,      CLASS_T ) \
	rc_pos[ POS ] = (unsigned int)GNf::Tell(fp);  /*记录场景该项数据的起始位置*/\
    num = _LIST .size();\
    GNf::Write((char*)&num , 4, 1, fp); /*写入个数*/\
	if(num){\
	    GN::sLIST< CLASS_T >::iterator im = _LIST .begin(), em = _LIST .end();\
		for(; im != em; ++im){

	GT_TMP_SCENE_WRITE_DATA_WP( 7 , share_objs.list_flare ,  P<GNg::cLENS_FLARE> )
			wpos = (*im)->zDataSave(buf , exp_pathfile?exp_path:0 ); /*写入各个结构的数据*/
		    GNf::Write((char*)buf, 1, wpos - buf, fp);		    /*写入对象数据*/
	}	}
   #define GT_TMP_SCENE_WRITE_IMAGE_PATHS( POS_ , _LIST_ , CLASS_T_ ) \
	wpos = buf;\
	GT_TMP_SCENE_WRITE_DATA_WP( POS_ , _LIST_ , CLASS_T_ ) \
			wpos = GT_WriteImageNameToBuffer(wpos, (*im), tmp_str, true);\
			if(exp_pathfile)GNg::ExportImageFile(exp_path, (*im));\
			if((unsigned int)(wpos - buf) > buf_size1){ /*存一定量再一次过写*/\
				GNf::Write((char*)buf, 1, wpos - buf, fp);\
				wpos = buf;\
			}\
	}	}\
	if(wpos > buf)GNf::Write((char*)buf, 1, wpos - buf, fp);

	GT_TMP_SCENE_WRITE_IMAGE_PATHS( 8 , share_objs.list_img , P<GNg::sIMAGE> )
	GT_TMP_SCENE_WRITE_IMAGE_PATHS( 9 , share_objs.in_frame_objs.pimgs , GNg::sIMAGE* )
	num = list_frames.size();
	if(num > num2){
		GNf::Seek(fp, rc_pos[1], GNf::eSEEK_START);
		GNf::Write((char*)&num , 4, 1, fp); //如后来子筐边写边增多了就再写入子筐组数
	}
}
//读取2D对象数据(当前读取位置, 当前加入对象的组, 所有在动画图筐里的共享引用对象)返回最后的读完位置
uint8 *GT_Obj2dLoadData(uint8 *rpos, GN::sLIST<GNg::cOBJ2D> *cur_group, GT_sSCENE2D_SHARE_OBJS_R *_ref_objs){
	int8 obj_type = *rpos;	++rpos;
	if(obj_type == GNg::eOBJ_GROUP){
		GNg::cOBJ2D *node = cur_group->add(GNg::cOBJ2D());
		node->aOBJtype = obj_type;
		 GN::sLIST<GNg::cOBJ2D> *_group = new GN::sLIST<GNg::cOBJ2D>;
		node->aObj = _group;
		while(*rpos != 0xff)rpos = GT_Obj2dLoadData(rpos, _group, _ref_objs);
		++rpos;
	}else{
		GNg::cOBJ2D *node = cur_group->add(GNg::cOBJ2D());
		node->aOBJtype = obj_type;
		switch(obj_type){
		case GNg::eOBJ_IMAGE:				node->aObj = new GNg::sIMAGE_OBJ; rpos = ((GNg::sIMAGE_OBJ*)(node->aObj))->zDataLoad(rpos, _ref_objs->pimages); break;
		case GNg::eOBJ_FRAME_RC:			node->aObj = new GNg::cFRAME_RC[1]; rpos = ((GNg::cFRAME_RC*)(node->aObj))->zDataLoad(rpos, &(_ref_objs->frame_ref_objs)); break;
		case GNg::eOBJ_FRAME:				node->aObj = new GNg::cFRAME   [1]; rpos = ((GNg::cFRAME   *)(node->aObj))->zDataLoad(rpos, &(_ref_objs->frame_ref_objs)); break;
		case GNg::eOBJ_FRAME_LAUNCHER:	    node->aObj = new GNg::cFRAME_LAUNCHER;   rpos = ((GNg::cFRAME_LAUNCHER  *)(node->aObj))->zDataLoad(rpos, _ref_objs->pframes, _ref_objs->pkeep_objs); break;
		case GNg::eOBJ_FRAME_LAUNCHER2D:	node->aObj = new GNg::cFRAME_LAUNCHER2D; rpos = ((GNg::cFRAME_LAUNCHER2D*)(node->aObj))->zDataLoad(rpos, _ref_objs->pframes, _ref_objs->pkeep_objs); break;
		case GNg::eOBJ_FRAME_LAUNCHER_FONT: node->aObj = new GNg::cFRAME_LAUNCHER_FONT; rpos = ((GNg::cFRAME_LAUNCHER_FONT*)(node->aObj))->zDataLoad(rpos, _ref_objs->pframes, _ref_objs->pkeep_objs); break;
		case GNg::eOBJ_LENS_FLARE:			node->aObj = new GNg::cLENS_FLARE_OBJ; rpos = ((GNg::cLENS_FLARE_OBJ*)(node->aObj))->zDataLoad(rpos, _ref_objs->pflares); break;
		}
	}
	return rpos;
}
//写入2D对象数据到文件(2D对象, 文件, 所有在动画图筐里收集到的共享对象)
void GT_Obj2dSaveData(GNg::cOBJ2D *node, GNf::sFILE *_fp, GT_sSCENE2D_SHARE_OBJS *_share_objs){
	if(node->aOBJtype == GNg::eOBJ_GROUP){
		GNf::Write((char*)&(node->aOBJtype),1,1,_fp);
		GN::sLIST<GNg::cOBJ2D> *objs = (GN::sLIST<GNg::cOBJ2D>*)(node->aObj);
		GN::sLIST<GNg::cOBJ2D>::iterator ito = objs->begin(), eto = objs->end();
		for(; ito != eto; ++ito)GT_Obj2dSaveData(&(*ito), _fp, _share_objs);
		uint8 endg = 0xff;
		GNf::Write((char*)&endg,1,1,_fp);	//写入结束标记
	}else{
		uint8 buf[sizeof(GNg::cFRAME) + (sizeof(GNg::sAM_NODE)<<5)];
		buf[0] = node->aOBJtype;
		uint8 *wpos = buf + 1;
		switch(node->aOBJtype){
		case GNg::eOBJ_IMAGE:				wpos = ((GNg::sIMAGE_OBJ*)(node->aObj))->zDataSave(wpos, _share_objs->list_img); break;
		case GNg::eOBJ_FRAME_RC:
		case GNg::eOBJ_FRAME:				wpos = ((GNg::cFRAME_RC*)(node->aObj))->zDataSave(wpos, &(_share_objs->in_frame_objs), 0); break;
		case GNg::eOBJ_FRAME_LAUNCHER:	    wpos = ((GNg::cFRAME_LAUNCHER  *)(node->aObj))->zDataSave(wpos, _share_objs->list_frame, _share_objs->list_keep_obj); break;
		case GNg::eOBJ_FRAME_LAUNCHER2D:	wpos = ((GNg::cFRAME_LAUNCHER2D*)(node->aObj))->zDataSave(wpos, _share_objs->list_frame, _share_objs->list_keep_obj); break;
		case GNg::eOBJ_FRAME_LAUNCHER_FONT: wpos = ((GNg::cFRAME_LAUNCHER_FONT*)(node->aObj))->zDataSave(wpos, _share_objs->list_frame, _share_objs->list_keep_obj); break;
		case GNg::eOBJ_LENS_FLARE:			wpos = ((GNg::cLENS_FLARE_OBJ*)(node->aObj))->zDataSave(wpos, _share_objs->list_flare); break;
		}
		GNf::Write((char*)buf, 1, wpos - buf, _fp);	
	}
}
//从缓冲里读取对象名查找表(缓冲, 返回对象名集中存放的缓冲, 返回对象名查找表)返回最后的缓冲读完位置
uint8* GT_Scene2DLoadObjNameTable(uint8 *rpos, char **pname_buf, P<char*> &name_table){
	unsigned int si, num, name_buf_size = *((unsigned int*)rpos); rpos += 4;
	char *name_buf = new char[name_buf_size];
	memcpy(name_buf, rpos, name_buf_size); rpos += name_buf_size; //读取对象名表
	for(num = 0, si = 0; si < name_buf_size; ++si){
		if(!name_buf[si])++num;
	}
	name_table.New(num);
	char **_obj_names = name_table;
	_obj_names[0] = name_buf;
	--name_buf_size;
	for(num = 1, si = 1; si < name_buf_size; ++si){
		if(!name_buf[si])_obj_names[num++] = name_buf + (si+1); //标定各个对象名在缓冲里的起始位置
	} 
	//for(si = 0; si < num; ++si){ GN_Log("%s\n", _obj_names[si]); }  //输出所有对象名
	*pname_buf = name_buf;
	return rpos;
}
char *GT_Scene2DLoadFileHead(const char *func_name, const char *file_name, const char *ext_name1){
   #ifdef GN_DEBUG
	if(!file_name || !file_name[0]){ GN_Log("%s 没有指定文件名!\n", func_name); return 0; }
   #endif
	char *ext_name = GN_StrGetExtName(file_name);
	if(!ext_name){ GN_Log("%s 文件名:%s 有错!\n 缺少扩展名, 现时系统可支持的有%s 类型文件\n", func_name, file_name, ext_name1); return 0; }
	int fsize;
	char *fbuf = GNf::FileFullLoadToMemory((char*)file_name, &fsize);
	if(!fbuf){
		GN_Log("读取2D场景文件 %s 失败!\n", file_name);
		return 0;
	}
	if(strncmp(fbuf,ext_name1, 3)){
		delete [] fbuf;
		GN_Log("读取 %s 不是有效的2D场景文件! 或文件被破坏了!\n", file_name);
		return 0;
	}
	return fbuf;
}
GNf::sFILE *GT_Scene2DSaveFileHead(const char *class_name, const char *filename, uint8 *file_head/*[4]*/, int name_pos, char *name_buf, P<char*> &name_table){
	char *ext_name = GN_StrGetExtName(filename);
	if(!ext_name){
		file_head[3] = 0;
		GN_Log("%s::SaveToFile() 文件名:%s 有错!\n 必须指定所要保存的文件类型扩展名, 现时系统可支持保存为%s 类型的文件\n", class_name, filename, (char*)file_head);
		return 0;
	}
	GNf::sFILE *fp = GNf::Open((char*)filename,"wb"/*,GNf::eFILE_STD*/);
	if(!fp){
		GN_Log("保存2D场景文件 %s 失败! 该路径下禁止访问或存储器已满!\n", filename);
		return 0;
	}
	GNf::Write((char*)file_head,1,4,fp);
	if(!name_table){
		GNf::Close(fp);
		return 0;
	}
	GNf::Seek(fp, name_pos, GNf::eSEEK_CUR);	 //预留一些字节等下写入各种数据的起始地址

	char *last_name = name_table[name_table.Num()-1];
	unsigned int num = (last_name - name_buf) + strlen(last_name) + 1;
	GNf::Write((char*)&num , 4, 1, fp);  //写入对象名表大小*/
	GNf::Write(name_buf, 1, num, fp); //写入对象名表
	return fp;
}
bool GNg::cSCENE2D::InitLoadToData(const char *file_name, float &progress){
	progress = 0.0f;
	char *fbuf = GT_Scene2DLoadFileHead("cSCENE2D::InitLoadToData()", file_name, "g2d");
	if(!fbuf)return false;
	GN_DeleteArray(zObjNameBuf);
	progress = 0.1f;
	uint8 *rpos = (uint8*)fbuf + 3;
	uint8 version = *rpos;  ++rpos;
	unsigned int *data_pos = (unsigned int*)rpos;  rpos += (10<<2);
	rpos = GT_Scene2DLoadObjNameTable(rpos, &zObjNameBuf, zObjNameTable);
	progress = 0.11f;
	if((*rpos) != eOBJ_GROUP){
		delete [] fbuf;
		GN_Log("读取 %s 失败 文件可能损坏!\n", file_name);
		return false;
	}
	++rpos;
	uint8 *r_node_pos = rpos;
	GT_sSCENE2D_SHARE_OBJS_R ref_objs;
	GT_Scene2DLoadShareObjData(ref_objs, data_pos, file_name, fbuf, progress); //跳转去先读取各种被引用到的各个数据
	//-------------------------------------------------------------------------------------------------------------------------------------------
	progress = 0.8f;
	GN::sLIST<cOBJ2D> *_group = new GN::sLIST<cOBJ2D>;
	aObj = _group;
	rpos = r_node_pos;
	while(*rpos != 0xff)rpos = GT_Obj2dLoadData(rpos, _group, &ref_objs);	//递归进去读取场景树里的各节点内容
	//++rpos;
	delete [] fbuf;
	progress = 1.0f;
	return true;
}
bool GNg::cSCENE2D::SaveToFile(const char *filename, uint8 version, bool exp_img, cOBJ2D *node){
	uint8 file_head[4] = { 'g', '2', 'd', version };
	GNf::sFILE *fp = GT_Scene2DSaveFileHead("cSCENE2D", filename, file_head, (10<<2), zObjNameBuf, zObjNameTable);
	if(!fp)return false;

	GT_sSCENE2D_SHARE_OBJS share_objs;
	if(!node)node = this;
	GT_Obj2dSaveData(node, fp, &share_objs);
	unsigned int rc_pos[10];
	GT_Scene2DSaveShareObjData(share_objs, 0, rc_pos, exp_img?filename:0, fp);
	GNf::Seek(fp, 4, GNf::eSEEK_START);    //跳到文件开始的第4个字节
	GNf::Write((char*)rc_pos, 4, 10, fp);  //写入各种资源存放在文件的偏移位置信息
	GNf::Close(fp);
	return true;
}
#endif