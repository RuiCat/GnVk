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
#ifdef _GN_INPUT_H_
#ifndef _GN_MAIN_H_
namespace GN {
	inline uint8 GetTouchMaxNum(){ return 1; }
};
#endif
GN_INIT_API bool  GN::Key  [GN_MAX_KEY_NUM];
GN_INIT_API bool  GN::Key0 [GN_MAX_KEY_NUM];
GN_INIT_API bool  GN::Key1 [GN_MAX_KEY_NUM];
GN_INIT_API bool  GN::Key11[GN_MAX_KEY_NUM];
GN_INIT_API bool  GN::KeyDown;

GN_INIT_API bool         GN::Mouse [GN_MAX_MK_NUM];
GN_INIT_API bool         GN::Mouse0[GN_MAX_MK_NUM];
GN_INIT_API bool         GN::Mouse1[GN_MAX_MK_NUM];
GN_INIT_API bool         GN::Mouse2[GN_MAX_MK_NUM];
GN_INIT_API bool         GN::MouseQ[GN_MAX_MK_NUM];
GN_INIT_API bool         GN::MouseI[GN_MAX_MK_NUM];
GN_INIT_API unsigned int GN::MouseT[GN_MAX_MK_NUM];

GN_INIT_API bool         GN::MouseM;
GN_INIT_API int          GN::MouseMX;
GN_INIT_API int          GN::MouseMY;

GN_INIT_API unsigned short GN::MouseMKS;

GN_INIT_API bool    GN::TouchM;
GN_INIT_API short  *GN::TouchX = 0;
GN_INIT_API short  *GN::TouchY = 0;
GN_INIT_API uint8  *GN::TouchZ = 0;

GN_INIT_API bool  GN::ViewReSized = true;

/********************************************************************************************************/
enum GT_eINPUT_TYPE{  //输入操作消息类型
	GT_eINPUT_GAME_EXIT  , //退出程序
	GT_eINPUT_IME        , //IME文字输入
	GT_eINPUT_KEYBOARD   , //按键盘
    GT_eINPUT_MOUSE_MOVE , //鼠标移动
    GT_eINPUT_MOUSE_KEY  , //鼠标点击
    GT_eINPUT_TOUCH_KEY  , //触摸点击
    GT_eINPUT_TOUCH_MOVE , //触摸拖动
    GT_eINPUT_INFV_UPDATE, //浮点数值输入更新
    GT_eINPUT_VIEW_SIZE    //窗口大小改变
};
struct GT_sINPUT_PARAME
{
   unsigned int  time;       //消息离开始记录的时间
   short         parame[10]; //参数 !!当是文字输入消息时 该数组头两字节是unsigned short 类型存字符长度 如果长度<=18 那么之后的就存字符内容 否则unsigned short之后存的是内存堆里的字符串指针
							 //		!!当时浮点数值输入消息时 该数组头两字节是起始索引 之后两字节是浮点值数 接下来的空间就是存浮点数据了
   uint8		 type;       //输入操作消息类型 取值自 GT_eINPUT_TYPE

  ~GT_sINPUT_PARAME();
   void ImeParameMake(void *str, unsigned short len);  //构建文字输入类型的参数数据(输入内容, 内容长度)
};
GT_sINPUT_PARAME::~GT_sINPUT_PARAME(){ 
	if(type == GT_eINPUT_IME){
		if(*((unsigned short*)parame) > 18)delete [] (*((char**)(parame+1)));
	}
}
void GT_sINPUT_PARAME::ImeParameMake(void *str, unsigned short len){
	if(len > 18){
		char *add_str = new char[len];
		*((char**)(parame+1)) = add_str;
		  memcpy(add_str,  str, len);
	}else memcpy(parame+1, str, len);
}
unsigned int GT_InputParameSizes[] = { 0, 20, 4, 6, 8, 4, 8, 20, 4 }; //各输入操作消息的参数数据总大小字节 !顺序一一对应 GT_eINPUT_TYPE

GN::sLIST<GT_sINPUT_PARAME> GT_InputRecList[2];   //0:录制输入队列 1:回放演奏输入队列
GN::sLIST<GT_sINPUT_PARAME>::iterator GT_InputIt; //定位播放的当前位置

GN_INIT_API unsigned int GT_RunTotalTime    = 0;
GN_INIT_API unsigned int GT_RunTotalTimeApp = 0;
GN_INIT_API unsigned int GT_LastRunTotalTimeApp = 0;
GN_INIT_API unsigned int GT_RunTotalFpsApp = 0;

bool         GT_InputNotIsMapping  = true;              //标记当前输入消息是否非从其它映射得来的
bool         GT_InputRecOn         = false;             //是否开启动输入操作记录
bool         GT_InputPlaying       = false;             //输入操作回放是否进行中
uint8		 GT_InputRecID         = 0;                 //输入操作记录引用的队列编号
uint8		 GT_InputPlayID        = 1;                 //输入操作演示引用的队列编号
unsigned int GT_InputRecStartTime;                      //输入操作记录开始时间
unsigned int GT_InputPlayStartTime;                     //输入操作演示开始时间

GN_INIT_API void GN::InputRecReStart(){
	InputRecClear(); 
	GT_InputRecStartTime = GT_RunTotalTimeApp; 
	GT_InputRecOn = true;
}
GN_INIT_API void GN::InputRecExitMsg(){
	GT_sINPUT_PARAME prm;
	prm.type = GT_eINPUT_GAME_EXIT;
    prm.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
	GT_InputRecList[GT_InputRecID].push_back(prm);
}
GN_INIT_API void GN::InputRecEnd(){ GT_InputRecOn = false; }

GN_INIT_API void GN::InputRecSave(const char *file_name,unsigned int dateID){
	if(GT_InputRecList[GT_InputRecID].empty())return;
	GNf::sFILE *fp = GNf::Open((char*)file_name,"wb",GNf::eFILE_STD);
	if(!fp){
		GN_Log("保存输入操作记录失败!\n"); 
		return;
	}
	uint8 locbuf[131072]/*128K*/, fh[4] = { 0x21,0x08,0x18,0x08/*GNd::eDATA_GIN*/};
	unsigned int size = GT_InputRecList[GT_InputRecID].size();
	GN::sTBUF_INT tbuf(locbuf, 131072);
    uint8 *buf = (uint8*)tbuf.GetSpace(16 + size * 64/*(4+2+58)*/);
    uint8 *wpos = buf;
	memcpy(wpos,  fh,					4); wpos += 4;  //写入标头
    memcpy(wpos, &dateID,				4); wpos += 4;  //写入文件更新序号
	memcpy(wpos, &GT_InputRecStartTime, 4); wpos += 4;  //写入本回记录的开始时间
	memcpy(wpos, &size,					4); wpos += 4;  //写入记录条目数

	GT_sINPUT_PARAME *_inp;
	GN::sLIST<GT_sINPUT_PARAME>::iterator it = GT_InputRecList[GT_InputRecID].begin(), et = GT_InputRecList[GT_InputRecID].end();
	for(; it != et; ++it){ //写入每条记录数据段
		_inp = &(*it);
		memcpy(wpos, &(_inp->time),4);      wpos += 4;  //写入消息时间
		memcpy(wpos, &(_inp->type),1);      wpos += 1;  //写入消息类型

		//按类型写入相应长度的参数数据
		if(_inp->type == GT_eINPUT_IME){			   //字符输入类型写入
			unsigned short len = *((unsigned short*)_inp->parame);
			memcpy(wpos,  &len, 2);			wpos += 2;
			memcpy(wpos, len>18? *((char**)(_inp->parame+1)): (char*)(_inp->parame+1),  len);  wpos += len;     
		
		}else if(_inp->type == GT_eINPUT_INFV_UPDATE){ //浮点输入类型写入
			unsigned int wsize = (_inp->parame[1]+1)<<2;
			memcpy(wpos, _inp->parame, wsize); wpos += wsize; 
		}else{										   //其它输入类型写入
			memcpy(wpos, _inp->parame, GT_InputParameSizes[_inp->type]); wpos += GT_InputParameSizes[_inp->type]; 
		}
	}
	GNf::Write((char*)buf, 1, wpos-buf, fp); //把缓冲数据一次性写入文件
	GNf::Close(fp);
}
GN_INIT_API void GN::InputRecClear(){
	GT_InputRecList[GT_InputRecID].clear();
	GT_InputRecOn = false;
}
GN_INIT_API bool GN::InputRecOpenPlay(const char *file_name,unsigned int *dateID,unsigned int *time){
    InputRecClearPlay();
	int f_size;
	uint8 *buf = (uint8*)GNf::FileFullLoadToMemory((char*)file_name,&f_size);
	if(!buf){
		GN_Log("打开输入操作记录文件: %s 失败!\n", file_name);
		return false;
	}
	if(f_size < 16 || buf[0] != 0x21 || buf[1] != 0x08 || buf[2] != 0x18 || buf[3] != 0x08/*GNd::eDATA_GIN*/){
		GN_Log("文件: %s 不是有效的输入操作记录文件!\n");
		delete [] buf;
		return false;
	}
	uint8 *rpos = buf + 4;
    if(dateID)*dateID = *((unsigned int*)rpos);
    rpos += 4;
    if(time)*time = *((unsigned int*)rpos);
    rpos += 4;
    unsigned int count = *((unsigned int*)rpos);  rpos += 4;
	unsigned int i = 0;
	GT_sINPUT_PARAME inp;
	for(; i < count; ++i){ //连续读取记录数据
        inp.time    = *((unsigned int*)rpos);   rpos += 4;
		inp.type    = *((uint8*)rpos);		    rpos += 1;

		if      (inp.type == GT_eINPUT_IME){
			memcpy(inp.parame, rpos, 2);		rpos += 2;
			unsigned short len = *((unsigned short*)inp.parame);
			inp.ImeParameMake(rpos, len);		rpos += len;

        }else if(inp.type == GT_eINPUT_INFV_UPDATE){
			unsigned int rsize = (((short*)rpos)[1]+1)<<2;
			memcpy(inp.parame, rpos, rsize);    rpos += rsize;
		}else{
			memcpy(inp.parame, rpos, GT_InputParameSizes[inp.type]); rpos += GT_InputParameSizes[inp.type];
		}
		GT_InputRecList[GT_InputPlayID].push_back(inp);
	}
    delete [] buf;
	return true;
}
GN_INIT_API void GN::InputRecRePlay(){
	GT_InputPlaying = true;
	GT_InputPlayStartTime = GT_RunTotalTimeApp; 
	GT_InputIt = GT_InputRecList[GT_InputPlayID].begin(); 
}
GN_INIT_API void GN::InputRecStopPlay(){
    GT_InputPlaying = false;
}
GN_INIT_API void GN::InputRecClearPlay(){
	GT_InputRecList[GT_InputPlayID].clear();
    GT_InputPlaying = false;
}
GN_INIT_API void GN::InputRecSwapToPlay(){
	GN_Swap( uint8 , GT_InputPlayID, GT_InputRecID );
	GT_InputPlaying   = false;
	GT_InputRecOn     = false;
}
GN_INIT_API uint8 GN::InputRecLastKeyUpGet(){
	GN::sLIST<GT_sINPUT_PARAME>::reverse_iterator itr = GT_InputRecList[GT_InputRecID].rbegin(), etr = GT_InputRecList[GT_InputRecID].rend();
	for(; itr != etr; ++itr){
		if(itr->type == GT_eINPUT_KEYBOARD){
			if(!itr->parame[1])return itr->parame[0];
		}
	}
	return 0xff;
}
/*-----------------------------------------------------  文字输入 -----------------------------------------------------------------------*/

bool           GT_ImeOn     = 0;		//记录当前是否要接受输入
bool           GT_ImeOnRecv = 0;		//标记当前是否有需要接受输入
unsigned char  GT_ImeCount  = 0;		//最近输进来字符数
char           GT_ImeChar[256] = {0};	//最近输入进来的字符数据

GN_INIT_API bool GN_ImeIsOpen()  { return (GT_ImeOn || GT_ImeOnRecv); }
GN_INIT_API bool GN_SkbNeedOpen(){ return  GT_ImeOn; }

/*****************************************************************************************************************************************/
#ifdef _GN_MAIN_H_
extern void (*GT_BreakLoopHookProc)();
#endif
unsigned int  GT_LastOnMouseTime[5]={10000000, 10000000, 10000000, 10000000, 10000000}; //储存上次按过鼠标键到现在的时间
unsigned int  GT_MouseInterval     = 600;   //鼠标双击间隔时间
         int  GT_MousePosDeviation = 32;    //鼠标双击允许位置误差
bool		  GT_MouseRePosX	   = false; //鼠标X出界自动重置为中间位置
bool		  GT_MouseRePosY	   = false; //鼠标Y出界自动重置为中间位置
GNg::cVIEW2D *GT_MouseRePosView	   = 0;		//鼠标出界自动重置为中间位置对应的视口

GN::sLIST<GNg::sRECT> GT_LastDelayedMouseStates;  //记录上次需延误的鼠标状态更变消息
GN_INIT_API unsigned int GT_IdleTime = 0;

GN_INIT_API void GF_ReSetInputState( void(*get_ime_func)(char *ime_str,unsigned char *ime_len) ){
    //重设这些状态-------------------------------------------------------------------------------
	memset(GN::Mouse0,0, GN_MAX_MK_NUM);
    memset(GN::Mouse1,0, GN_MAX_MK_NUM);
    memset(GN::Mouse2,0, GN_MAX_MK_NUM);
	memset(GN::MouseQ,0, GN_MAX_MK_NUM);
	memset(GN::MouseI,0, GN_MAX_MK_NUM);

	memset(GN::Key0,  0, GN_MAX_KEY_NUM);
	memset(GN::Key1,  0, GN_MAX_KEY_NUM);
	memset(GN::Key11, 0, GN_MAX_KEY_NUM);

	uint8 ti = 0, tuch_n = GN::GetTouchMaxNum(), *touch_zs = GN::TouchZ;
	for(; ti < tuch_n; ++ti){
        if     (touch_zs[ti] == 255)  touch_zs[ti] = 254;
        else if(touch_zs[ti] == 1  ){ touch_zs[ti] = 0;

    }   }
	GN::TouchM = false;
	GN::MouseM = false;
    GN::MouseMKS = 0;
	GN::KeyDown = false;
	if(GT_RunTotalTime > 20 && GT_RunTotalTimeApp > 20)GN::ViewReSized = false;
	GT_ImeCount = 0;
	
	if(GN::Mouse [GM_TOP   ]){ GN::Mouse [GM_TOP   ] = 0; GN::Mouse0[GM_TOP   ] = 1; } //调配一些鼠标状态更变
	if(GN::Mouse [GM_BOTTOM]){ GN::Mouse [GM_BOTTOM] = 0; GN::Mouse0[GM_BOTTOM] = 1; }
    if(GN::MouseT[GM_TOP   ]){ GN::MouseT[GM_TOP   ] = 0; GN::MouseQ[GM_TOP   ] = 1; GN::MouseI[GM_TOP   ] = 1; }
	if(GN::MouseT[GM_BOTTOM]){ GN::MouseT[GM_BOTTOM] = 0; GN::MouseQ[GM_BOTTOM] = 1; GN::MouseI[GM_BOTTOM] = 1; }

	if(GT_LastDelayedMouseStates.size()){ //消化延误的消息
		GN::sLIST<GNg::sRECT>::iterator itdms = GT_LastDelayedMouseStates.begin(), etdms = GT_LastDelayedMouseStates.end();
		GNg::sRECT *_dms;
		for(; itdms != etdms; ++itdms){
		    _dms = &(*itdms);
			GF_SetMouseState((uint8)_dms->right, _dms->bottom?1:0, _dms->left, _dms->top);
		}
		GT_LastDelayedMouseStates.clear();
	}
    //输入控制演示处理,将记录到的消息转为相应控制调用---------------------------------------------------------------------------------------
    if(GT_InputPlaying){
		GT_sINPUT_PARAME *_inp;
        GN::sLIST<GT_sINPUT_PARAME>::iterator itend = GT_InputRecList[GT_InputPlayID].end();
		for(; GT_InputIt != itend && GT_InputIt->time + GT_InputPlayStartTime <= GT_RunTotalTimeApp; ++GT_InputIt){
		    _inp = &(*GT_InputIt);
			switch(_inp->type){
			case GT_eINPUT_MOUSE_MOVE:{
				GNg::cVIEW2D *last_rpv = GT_MouseRePosView;
				GT_MouseRePosView = 0;
                GF_SetMouseMoveState((unsigned short)_inp->parame[0],_inp->parame[1],_inp->parame[2]);
				GT_MouseRePosView = last_rpv;
			   }break;
		    case GT_eINPUT_MOUSE_KEY:
			    GF_SetMouseState((uint8)_inp->parame[0], _inp->parame[1]?1:0, _inp->parame[2], _inp->parame[3]);
			    break;
			case GT_eINPUT_TOUCH_MOVE:
			    GF_SetTouchMoveState((uint8)_inp->parame[0],(uint8)_inp->parame[1],_inp->parame[2],_inp->parame[3]);
			    break;
			case GT_eINPUT_TOUCH_KEY:
			    GF_SetTouchState((uint8)_inp->parame[0],(uint8)_inp->parame[1]);
			    break;
			case GT_eINPUT_INFV_UPDATE:
			    GF_SetInFvState(_inp->parame[0], (float*)(_inp->parame+2), _inp->parame[1]);
			    break;
		    case GT_eINPUT_KEYBOARD:
                GF_SetKeyState(_inp->parame[0], _inp->parame[1]?1:0);
			    break;
		    case GT_eINPUT_IME:
			    GT_ImeCount = *((unsigned short*)_inp->parame); 
				memcpy(GT_ImeChar, GT_ImeCount>18? *((char**)(_inp->parame+1)):(char*)(_inp->parame+1), GT_ImeCount);
                break;
          #ifdef _GN_MAIN_H_
		    case GT_eINPUT_GAME_EXIT:
                if(GN_GameBreakLoopProc)GN_GameBreakLoopProc();
		        else GN_GameBreakLoop();
			    break;
            case GT_eINPUT_VIEW_SIZE:
			    GN_SetWindowPos(0,0,_inp->parame[0],_inp->parame[1]);
			    break;
          #endif
		    }
	    }
    }
    if(GT_ImeOn || GT_ImeOnRecv)get_ime_func(GT_ImeChar, &GT_ImeCount);  //接收文本输入来的内容
   #ifdef _GN_MAIN_H_
	else GN_SetImeFouscePos( -10000, -10000);
   #endif
    if(GT_ImeCount){
        GT_IdleTime = 0;
        if(GT_InputRecOn){  //当有文字输入进来时就记录输入
            GT_sINPUT_PARAME inp;
            inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
	        inp.type = GT_eINPUT_IME;
			inp.ImeParameMake(GT_ImeChar, GT_ImeCount);
			GT_InputRecList[GT_InputRecID].push_back(inp);
    }   }
}
GN_INIT_API void GF_DownKeyAllLoosen(){
	int mx = GN::MouseMX, my = GN::MouseMY;
	bool *keys = GN::Key, *mouses = GN::Mouse;
	uint8 i, n;
	for(i = 0; i < GN_MAX_KEY_NUM; ++i)if(keys  [i])GF_SetKeyState((int)i, 0);
	for(i = 0; i < GN_MAX_MK_NUM;  ++i)if(mouses[i])GF_SetMouseState(i,0, mx,my);

	if(!GN::TouchZ)return;
    uint8 *touch_zs = GN::TouchZ;

	n = GN::GetTouchMaxNum();
	for(i = 0; i < n; ++i){
        if(touch_zs[i] > 1)GF_SetTouchState(i, 1);
    }
}
GN_INIT_API void GF_SetMouseState(uint8 mkey,bool is_down, int px,int py){ //GN_Log("state: mkey=%d, is_down=%d, px=%d, py=%d\n", (int)mkey, is_down?1:0, px,py);
	if(!is_down && GN::MouseT[mkey] == 1){
		GT_LastDelayedMouseStates.push_back(GNg::sRECT(px,py,(int)mkey,0));
		return;
	}
	if(GT_InputRecOn && GT_InputNotIsMapping){ //记录鼠标点击输入
		GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
		inp.type = GT_eINPUT_MOUSE_KEY;
		inp.parame[0] = (short)mkey;
		inp.parame[1] = is_down ? 1 : 0;
		inp.parame[2] = px;
		inp.parame[3] = py;
		GT_InputRecList[GT_InputRecID].push_back(inp);
	}
	GN::Mouse[mkey] = is_down;
	GN::MouseMX		= px;
    GN::MouseMY		= py;
	if(is_down){
		GN::Mouse1[mkey] = 1;
        GN::MouseT[mkey] = 1;
		if(  GT_LastOnMouseTime[mkey] <= GT_MouseInterval){ 
			  GN::Mouse2[mkey] = 1;
			  GT_LastOnMouseTime[mkey] = 10000000;
		}else GT_LastOnMouseTime[mkey] = 0;
	}else{
		GN::Mouse0[mkey] = 1;
		if(GN::MouseT[mkey]){
			GN::MouseQ[mkey] = 1;
			if(GN::MouseT[mkey] <= GT_MouseInterval)GN::MouseI[mkey] = 1;
			GN::MouseT[mkey] = 0;
	}	}
   #ifdef _GN_2D_H_
	GN::sLIST<GNg::cVIEW2D*>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
	GNg::cVIEW2D *pv;
	for(; itv != etv; ++itv){
	    pv = (*itv);
	    pv->mouseMX = pv->CoordWindowToViewX( px ); //窗口坐标转为后台坐标
        pv->mouseMY = pv->CoordWindowToViewY( py );
	}
   #endif
	GT_IdleTime = 0;
}
GN_INIT_API void GF_SetMouseMoveState(unsigned short keys_state, int new_x,int new_y){
    if(new_x == GN::MouseMX && new_y == GN::MouseMY)return; //忽略位置相同的消息 以免浪费性能

	if(GT_InputRecOn && GT_InputNotIsMapping){ //记录鼠标移动输入
		GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
		inp.type = GT_eINPUT_MOUSE_MOVE;
		inp.parame[0] = keys_state;
		inp.parame[1] = new_x;
		inp.parame[2] = new_y;
		GT_InputRecList[GT_InputRecID].push_back(inp);
	}
	static unsigned int lastMouseTime = 0;
	int vx, vy; /*模版代码:计算在额定时间内的位置差*/
	if(GT_RunTotalTime - lastMouseTime > 250){
		vx = 0;
		vy = 0;
	}else{ 
		vx = new_x - GN::MouseMX; 
		vy = new_y - GN::MouseMY; 
	}
	GN::MouseMX = new_x;
    GN::MouseMY = new_y;
	GN::MouseM  = 1;
	GN::MouseMKS = keys_state;
	memset(GN::MouseT, 0, 20 /* sizeof(unsigned int)*5 */);

   #ifdef _GN_MAIN_H_
	if((vx > GT_MousePosDeviation || vy > GT_MousePosDeviation
	 || vx < GT_MousePosDeviation || vy < GT_MousePosDeviation))
	{
		GT_LastOnMouseTime[0] = 10000000;
		GT_LastOnMouseTime[1] = 10000000;
		GT_LastOnMouseTime[2] = 10000000;
		GT_LastOnMouseTime[3] = 10000000;
		GT_LastOnMouseTime[4] = 10000000;
	}
   #endif
   #ifdef _GN_2D_H_
    GN::sLIST<GNg::cVIEW2D*>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
    GNg::cVIEW2D *pv;
    for(; itv != etv; ++itv){
	    pv = (*itv);
	    pv->mouseMX  = pv->CoordWindowToViewX( new_x ); //窗口坐标转为后台坐标
        pv->mouseMY  = pv->CoordWindowToViewY( new_y );
        pv->mouseMSX = vx * pv->zScreenRatioW;
        pv->mouseMSY = vy * pv->zScreenRatioH;

		//使适合第一人称射击游戏的鼠标会轮回中心的处理
		if(pv == GT_MouseRePosView){
			short bw = pv->viewW >> 4, bh = pv->viewH >> 4, xpos, ypos;
			if(GT_MouseRePosX && ((short)new_x > pv->viewX+pv->viewW-bw || (short)new_x < pv->viewX+bw)) xpos = pv->viewX+(pv->viewW >> 1);
			 else xpos = new_x;
			if(GT_MouseRePosY && ((short)new_y > pv->viewY+pv->viewH-bh || (short)new_y < pv->viewY+bh)){ypos = pv->viewY+(pv->viewH >> 1);
				GN_SetCursorPos(xpos, ypos, false);
			}else{
				ypos = new_y;
				if(xpos != (short)new_x)GN_SetCursorPos(xpos, ypos, false);
			}
			if(GT_MouseRePosX){ //速度轮回时会太长抵扣一下
				int rng = (pv->orthoW>>2) + (pv->orthoW>>3);
				if	   (pv->mouseMSX >  rng)pv->mouseMSX -= (pv->orthoW>>1) - (pv->orthoW>>4);
				else if(pv->mouseMSX < -rng)pv->mouseMSX += (pv->orthoW>>1) - (pv->orthoW>>4);
			}
			if(GT_MouseRePosY){
				int rng = (pv->orthoH>>2) + (pv->orthoH>>3);
				if	   (pv->mouseMSY >  rng)pv->mouseMSY -= (pv->orthoH>>1) - (pv->orthoH>>4);
				else if(pv->mouseMSY < -rng)pv->mouseMSY += (pv->orthoH>>1) - (pv->orthoH>>4);
			}
		}
    }
   #endif
	lastMouseTime = GT_RunTotalTime;
	GT_IdleTime = 0;
}
/******************************************************** 触摸消息处理 ***************************************************************/
unsigned short GT_DitherRange       = 16;      //触摸映射为鼠标按下的防斗容忍范围
uint8		   GT_TouchMappingMKey  = GM_LEFT; //当前触摸点映射为鼠标的键位
unsigned int   GT_Touch0DownTime    = 0;       //首个触摸点原位按紧不动还未映射为其它鼠键时的累计时间
float          GT_LastZoomLen;                 //上次滚轮映射缩放剩余的累计长度

GN_INIT_API void GF_SetTouchState(uint8 point_id, uint8 down){
	if(GT_TouchMappingMKey < GM_TOP){
		if(point_id == 0){ //如是首个触摸点就映射为鼠标事件
			if     (down == 255){
				GT_Touch0DownTime = 1;
				GT_TouchMappingMKey = GM_LEFT;

			}else if(down == 1){
				GT_InputNotIsMapping = 0;
				if(GT_Touch0DownTime){
				    GF_SetMouseState(GT_TouchMappingMKey, 1, GN::TouchX[0], GN::TouchY[0]);
					GT_Touch0DownTime = 0;
				}
				GF_SetMouseState(GT_TouchMappingMKey, 0, GN::TouchX[0], GN::TouchY[0]);
				GT_InputNotIsMapping = 1;
			}
		}else if(point_id == 1){
			if(down == 255){
				if(GT_Touch0DownTime){
					GT_Touch0DownTime = 0;
					GT_TouchMappingMKey = GM_TOP; //映射更变为滚轮键
					GT_LastZoomLen = 0.0f;
			}	}
		}
	}else{ //映射滚轮操作模式
		if(down == 1){
			if((!point_id && GN::TouchZ[1] <= 1) || (point_id == 1 && GN::TouchZ[0] <= 1)){
			    GT_TouchMappingMKey = GM_LEFT; //这时第一和二个触摸点都被释放就标记退出滚轮操作
			    if(GT_LastZoomLen > -0.01f && GT_LastZoomLen < 0.01f){
			        GT_InputNotIsMapping = 0;
                    GF_SetMouseState(GM_RIGHT, 1, GN::MouseMX, GN::MouseMY);
                    GF_SetMouseState(GM_RIGHT, 0, GN::MouseMX, GN::MouseMY);
                    GT_InputNotIsMapping = 1;
			    }
			}
		    if(point_id <= 1)GT_LastZoomLen = 0.0f;
		}
	}
    //--------------------------------------------------------------------------------------------
    if(GT_InputRecOn){ //记录触摸点击输入
		GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
		inp.type = GT_eINPUT_TOUCH_KEY;
		inp.parame[0] = (short)point_id;
		inp.parame[1] = (short)down;
		GT_InputRecList[GT_InputRecID].push_back(inp);
    }
    GN::TouchZ[point_id] = down;

	GT_IdleTime = 0;
}
GN_INIT_API void GF_SetTouchMoveState(uint8 point_id, uint8 down_z, short new_x,short new_y){
	if(GT_Touch0DownTime/* > 240*/ >= GT_MouseInterval){ //当有首个触摸点长按时就映射为鼠标左键双击消息
        GT_InputNotIsMapping = false;
        GF_SetMouseState(GM_LEFT,1,GN::MouseMX,GN::MouseMY);
        GN::Mouse2[GM_LEFT] = 1;
        GF_SetMouseState(GM_LEFT,0,GN::MouseMX,GN::MouseMY);
        GT_InputNotIsMapping = true;
        GT_Touch0DownTime = 0;
	}
	short *touch_x = GN::TouchX, *touch_y = GN::TouchY;
	if(	  touch_x[point_id] == new_x &&	   touch_y[point_id] == new_y
    &&(GN::TouchZ[point_id] == 255  ||  GN::TouchZ[point_id] == down_z))return; //忽略没更变的消息 以免浪费性能
    
    if(GT_TouchMappingMKey < GM_TOP){
		if(point_id == 0){ //如是首个触摸点就映射为鼠标事件
			short crs_x = new_x - touch_x[point_id], crs_y = new_y - touch_y[point_id];
			if(crs_x && crs_y){
				GT_InputNotIsMapping = 0;
				if(GT_Touch0DownTime){
					short dhr = (short)GT_DitherRange;
					if(crs_x > dhr || crs_x < -dhr || crs_y > dhr || crs_y < -dhr){
                        GF_SetMouseState(GT_TouchMappingMKey, 1, (int)new_x, (int)new_y);
				        GT_Touch0DownTime = 0;
					}
				}
			    GF_SetMouseMoveState(GT_TouchMappingMKey == GM_LEFT ? GMM_LEFT : GMM_RIGHT, (int)new_x,(int)new_y);
			    GT_InputNotIsMapping = 1;
			}
		}
	}else{ //映射滚轮操作模式
		if(point_id <= 1 && GN::TouchZ[point_id] > 1){
			GNg::sVEC2 tchp0_1, tchp0__1;
			if(point_id == 0){
				tchp0_1.x  = (float)(touch_x[0] - touch_x[1]),  tchp0_1.y  = (float)(touch_y[0] - touch_y[1]);
				tchp0__1.x = (float)(new_x      - touch_x[1]),  tchp0__1.y = (float)(new_y      - touch_y[1]);
			}else{
			    tchp0_1.x  = (float)(touch_x[1] - touch_x[0]),  tchp0_1.y  = (float)(touch_y[1] - touch_y[0]);
				tchp0__1.x = (float)(new_x      - touch_x[0]),  tchp0__1.y = (float)(new_y      - touch_y[0]);
			}
            GT_LastZoomLen += GNg::Vec2Length(&tchp0__1) - GNg::Vec2Length(&tchp0_1);
            
			GT_InputNotIsMapping = 0;
			if     (GT_LastZoomLen >  65.0f){ GT_LastZoomLen -= 65.0f; GF_SetMouseState(GM_TOP,    1, GN::MouseMX, GN::MouseMY); }
			else if(GT_LastZoomLen < -65.0f){ GT_LastZoomLen += 65.0f; GF_SetMouseState(GM_BOTTOM, 1, GN::MouseMX, GN::MouseMY); }
            GT_InputNotIsMapping = 1;
	}	}
    //--------------------------------------------------------------------------------------------
	if(GT_InputRecOn){ //记录触摸拖动输入
		GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
		inp.type = GT_eINPUT_TOUCH_MOVE;
		inp.parame[0] = (short)point_id;
		inp.parame[1] = (short)down_z;
		inp.parame[2] =  new_x;
		inp.parame[3] =  new_y;
		GT_InputRecList[GT_InputRecID].push_back(inp);
	}
	static unsigned int lastTouchTime = 0;
	short vx, vy; /*模版代码:计算在额定时间内的位置差*/
	if(GT_RunTotalTime - lastTouchTime > 250){ 
		vx = 0;
		vy = 0;
	}else{ 
		vx = new_x - touch_x[point_id]; 
		vy = new_y - touch_y[point_id]; 
	}
    if(GN::TouchZ[point_id] != 255)GN::TouchZ[point_id] = down_z;
    touch_x[point_id] = new_x;
    touch_y[point_id] = new_y;
	GN::TouchM = true;

   #ifdef _GN_2D_H_
    GN::sLIST<GNg::cVIEW2D*>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
    GNg::cVIEW2D *pv;
    for(; itv != etv; ++itv){
	    pv = (*itv);
	    pv->touchX [point_id] = pv->CoordWindowToViewX( new_x );
        pv->touchY [point_id] = pv->CoordWindowToViewY( new_y );
        pv->touchSX[point_id] = (short)(vx * pv->zScreenRatioW);
        pv->touchSY[point_id] = (short)(vy * pv->zScreenRatioH);
    }
   #endif
	lastTouchTime = GT_RunTotalTime;
	GT_IdleTime = 0;
}
/*************************************************************************************************************************************/
GN_INIT_API void GF_SetKeyState_(int key,bool is_down){
	if(GT_InputRecOn && GN::Key[key] != is_down){ //记录按键盘输入
		GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
		inp.type = GT_eINPUT_KEYBOARD;
		inp.parame[0] = key;
		inp.parame[1] = is_down? 1 : 0;
		GT_InputRecList[GT_InputRecID].push_back(inp);
	}
	if(!is_down)GN::Key0[key] = 1;
	GN::Key1[key]  = GN::KeyDown = !GN::Key[key];
	GN::Key11[key] = is_down;
	GN::Key[key]   = is_down;
	GT_IdleTime = 0;
}
GN_INIT_API void (*GF_SetKeyState)(int key, bool is_down) = GF_SetKeyState_;

GN_INIT_API void GF_SetInFvState(short start_infv_id,const float *infv, short in_count){
    if(GT_InputRecOn){ //记录浮点数值更新输入
	    GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
		inp.type = GT_eINPUT_INFV_UPDATE;
        inp.parame[0] = start_infv_id;
		inp.parame[1] = in_count;
		memcpy(inp.parame+2, infv, in_count<<2 /* *sizeof(float) */);
		GT_InputRecList[GT_InputRecID].push_back(inp);
	}
	memcpy(GNio::InFv+start_infv_id, infv, in_count<<2);
	//GT_IdleTime = 0;
}
int   GT_InPutLastPort = 0;		       //最近按下过的输入端口号
int   GT_InPutTestTime = 10000000;     //输入出端口测试时间
unsigned short *GT_IO_MappingKeys = 0; //IO输入出映射键数据

GN_INIT_API void GF_SetInPortState(unsigned short port_id, bool is_on){
    GNio::InPort[port_id] = is_on;
	if(GT_IO_MappingKeys && GT_IO_MappingKeys[port_id])
		GF_SetKeyState(GT_IO_MappingKeys[port_id], is_on);

	GT_InPutLastPort = port_id;
    GT_InPutTestTime = 0;
}
GN_INIT_API void GF_ReSetViewport(short new_w,short new_h){
	if(GT_InputRecOn){  //记录窗口大小改变
		GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GT_InputRecStartTime;
		inp.type = GT_eINPUT_VIEW_SIZE;
		inp.parame[0] = new_w;
		inp.parame[1] = new_h;
		GT_InputRecList[GT_InputRecID].push_back(inp);
	}
	GN::ViewReSized = true;
   #ifdef _GN_2D_H_
	GNg::ViewCur->ReSize(0,0, new_w,new_h);
   #ifdef GT_TMP_COMPATI_DX_DDRAW /*模版代码:重架设2D渲染状态*/
    if(!strcmp(GN_GetInterfaceInfoRender(),"DirectDraw")){
        GNg::ViewCur->BeginRender2D();
	}else
   #endif
        GNg::ViewCur->BeginRender2D();
   #endif
}
GN_INIT_API void GN_SetMouseDoubleClick(unsigned int Interval,unsigned int deviation){
    GT_MouseInterval = Interval;  GT_MousePosDeviation = deviation;
}
GN_INIT_API void GN_SetMouseOutViewRePos(GNg::cVIEW2D *view, bool xpos, bool ypos){
	GT_MouseRePosView = view;
	GT_MouseRePosX = xpos; GT_MouseRePosY = ypos;
}
/**************************************************************外部IO的东西*************************************************************************************************/

GN_INIT_API void GNio::SetIOMappingKey(short IO_port, unsigned short GK_key){
   #ifdef GN_DEBUG
	if(!GT_IO_MappingKeys){ GN_Log("调用 SetIOMappingKey 失败! 还未初始化外部IO 请先调用 GN_InitInput ...\n"); return; }
   #endif
	if(GK_key < GN_MAX_KEY_NUM && IO_port < (short)GetInPortNum())GT_IO_MappingKeys[IO_port] = GK_key;
}
GN_INIT_API void GNio::OutPutAllON(){
	uint8 i = 0, n = GetOutPutNum();
	for(; i < n; ++i)OutPut(i, 1);
}
GN_INIT_API void GNio::OutPutAllOFF(){
    uint8 i = 0, n = GetOutPutNum();
	for(; i < n; ++i)OutPut(i, 0);
}
GN_INIT_API bool GNio::OutPutTestDisplay(unsigned int nTime, unsigned int port_chg_time){
    static unsigned int outPutTestIime = 0, change = 10000;
	outPutTestIime += nTime;
	unsigned int port = outPutTestIime / port_chg_time;
	if(port != change){
	    if(port < 4){
	        switch(port){ //开头全部开两遍
	        case 0:
			case 2: OutPutAllON();  break;
	        case 1:
	        case 3: OutPutAllOFF(); break;
			}
	    }else{			  //然后一个接一个地开
		    int tp = (int)(port - 4);
			if(tp)OutPut(tp-1 , 0);
			if(tp < (int)GetOutPutNum())OutPut(tp, 1);
			else{
				outPutTestIime = 0;
				change = 10000;
                return 0;
			}
	    }
	    change = port;
	}
	if(port >= 4)GN_RunValueCheck(port - 4,"OUT PORT: ", port - 4);
	return 1;
}
GN_INIT_API int GNio::InPutTestDisplay(unsigned int nTime, unsigned int show_delay_time){
   #ifdef _GN_INIT_H
	unsigned int  fcolor = GT_AppDefaultColorFont;
	unsigned int  lcolor = GT_AppDefaultColorFocus;
   #else
	unsigned int  fcolor = 0xffffffff;
	unsigned int  lcolor = 0xffaf4f00;
   #endif
   #ifdef _GN_RUNINFO_H_ //文字输入出端口号提示
	if(GT_InPutTestTime < show_delay_time)
		GN_RunValueCheck(GT_InPutLastPort, "          IN  PORT: ", GT_InPutLastPort);
   #endif
   #ifdef _GN_G2D_H_
	short i = 0, n = (short)GetInPortNum();
    short w = GNg::ViewCur->orthoW / n, h = GNg::ViewCur->orthoH >> 3;
	for(; i < n; ++i){   //显示出当前输入状态
		if(InPort[i])GNg::ImageDraw(0, GNg::ViewCur->ortho_left + w*i, GNg::ViewCur->ortho_top, w,h, lcolor);
		GNg::LineFrame2dDraw(          GNg::ViewCur->ortho_left + w*i, GNg::ViewCur->ortho_top, w,h, 3, fcolor);
	}
   #endif
	GT_InPutTestTime += nTime;
    return GT_InPutLastPort;
}
extern uint8 GT_MouseCkN = 0;
bool GT_WS_MappingToKey = false;

GN_INIT_API void GN::SetWeightSensorMappingToKey(bool yes){ GT_WS_MappingToKey = yes; }
GN_INIT_API void GN::SetTouchMappingToMouseDitherRange(unsigned short dr){ GT_DitherRange = dr; }

/*******************************************************************************************************************************************/
GN_INIT_API void GN_InitInput(){
     if(GT_SystemInited & (1<<1))return;
     uint8 tn = GN::GetTouchMaxNum();
	 if(!GN::TouchX)GN::TouchX = new short[tn];
     if(!GN::TouchY)GN::TouchY = new short[tn];
     if(!GN::TouchZ)GN::TouchZ = new uint8[tn];
     memset(GN::TouchZ, 0, tn);

	 if(!GT_IO_MappingKeys)GT_IO_MappingKeys = new unsigned short[GNio::GetInPortNum()];
	 memset(GT_IO_MappingKeys, 0, GNio::GetInPortNum()<<1);

	 GT_SystemInited |= 1<<1;
}
GN_INIT_API void GN_UnInitInput(){
	 GN::InputRecClear();
	 GN::InputRecClearPlay();
	 GN_ValueChangeClearAll();
	#ifdef _GN_INPUTACT_H_
	 #if defined(GN_DEBUG) && defined(_GN_RUNINFO_H_)
	  if(GN::IA::sOBJ::zTotalList.size())GN_Log("还有输入动作没被移除!!\n");
     #endif
    #endif
	 GN_DeleteArray(GT_IO_MappingKeys);
	 GN_DeleteArray(GN::TouchX);
	 GN_DeleteArray(GN::TouchY);
	 GN_DeleteArray(GN::TouchZ);
	 GT_SystemInited -= 1<<1;
}
GN_INIT_API void GN_InputProc(unsigned int nTime){
	static unsigned int sensorCaps = GNio::GetSensorCaps(); //用静态变量以仅执行1次 减少性能开销
	if(sensorCaps){
        if(GT_WS_MappingToKey){ //感应器信号映射为键盘信号
			GN::Key[GK_A] = GNio::InFv[GI_WY] >  0.7f;
            GN::Key[GK_D] = GNio::InFv[GI_WY] < -0.7f;
            GN::Key[GK_W] = GNio::InFv[GI_WX] > -3.0f;
            GN::Key[GK_S] = GNio::InFv[GI_WX] < -5.0f;

            static float Rx = 0.0f, Rz = 0.0f;
            if(!Rx && GNio::InFv[GI_DX])Rx = GNio::InFv[GI_DX];
            if(!Rz && GNio::InFv[GI_DZ])Rz = GNio::InFv[GI_DZ];
            GN::Key[GK_LEFT]  = GNio::InFv[GI_DX] - Rx < -1.0f;
            GN::Key[GK_RIGHT] = GNio::InFv[GI_DX] - Rx >  1.0f;
            GN::Key[GK_UP]    = GNio::InFv[GI_DZ] - Rz >  1.0f;
            GN::Key[GK_DOWN]  = GNio::InFv[GI_DZ]  -Rz < -1.0f;
        }
	}
	if(GN::MouseT[0])GN::MouseT[0] += nTime;
	if(GN::MouseT[1])GN::MouseT[1] += nTime;
	if(GN::MouseT[2])GN::MouseT[2] += nTime;
	if(GN::MouseT[3])GN::MouseT[3] += nTime;
	if(GN::MouseT[4])GN::MouseT[4] += nTime;

	GT_LastOnMouseTime[0] += nTime;
	GT_LastOnMouseTime[1] += nTime;
	GT_LastOnMouseTime[2] += nTime;
    GT_LastOnMouseTime[3] += nTime;
    GT_LastOnMouseTime[4] += nTime;
	if(GT_Touch0DownTime)GT_Touch0DownTime += nTime;
	if(GT_MouseCkN < 5){
		 if(GT_MouseCkN & 1){
			 if(GN::MouseQ[0])++GT_MouseCkN;
			 else if(GN::Mouse0[0])GT_MouseCkN = 0;
		 }else{
             #define GT_TIME_OUTV( TOV ) #TOV
			 if(GN::MouseT[0] > 6000)++GT_MouseCkN;
			 else if(GN::Mouse0[0])GT_MouseCkN = 0;
	}	 }
    GT_ImeOn = false;
	GT_ImeOnRecv = false;
	GT_LastRunTotalTimeApp = GT_RunTotalTimeApp;
	GT_RunTotalTime    += nTime;  /*更新应用运行时间*/
	GT_RunTotalTimeApp += nTime;
	++GT_RunTotalFpsApp;
	GT_IdleTime        += nTime;
    GN::TimeRatioL = GN::TimeRatio = (float)nTime * 0.0625f /* /16.0f */;
	if(GT_MouseCkN >= 5){GN::RunRec.OutText(GT_TIME_OUTV(11282076),0, 160, 160);} if(GN::TimeRatioL > 3.0f)GN::TimeRatioL = 3.0f;

	GN_ValueChangeUpdateAll();

   #if defined _GN_2D_H_ && defined _GN_MAIN_H_
    GN::sLIST<GNg::cVIEW2D*>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
    GNg::cVIEW2D *pv;
	GNg::sRECT *_rng = GN_GetSoftKeyboardRange();
	if(_rng){ //如有软键盘 就传递它的范围到各个窗口
        for(; itv != etv; ++itv){
		    pv = (*itv);
		    pv->softKeyboardX = pv->CoordWindowToViewX(_rng->left);
			pv->softKeyboardW = pv->CoordWindowToViewX(_rng->right)  - pv->softKeyboardX;
            pv->softKeyboardY = pv->CoordWindowToViewY(_rng->top);
            pv->softKeyboardH = pv->CoordWindowToViewY(_rng->bottom) - pv->softKeyboardY;
        }
	}else{
	    for(; itv != etv; ++itv){
		    pv = (*itv);
            pv->softKeyboardX = -100000;
		    pv->softKeyboardW = 0;
			pv->softKeyboardY = -100000;
			pv->softKeyboardH = 0;
        }
	}
   #endif
   #ifdef _GN_INPUTACT_H_
    GN::IA::sOBJ::zUpdateAll(nTime);
   #endif
   #ifdef _GN_UI_H_
	GN_Swap( GNu::iUI_OBJECT* , GNu::RecvInputUiObject , GNu::RequestInputUiObject );
   #endif
}
#endif // _GN_INPUT_H_
