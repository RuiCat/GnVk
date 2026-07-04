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
#ifdef _GN_AMCTRL_H_
GN_API const char *GNg::AmModeNames[] = { "无","循环播放","来回播放","播放一次消失","播放一次停住" };

GN_API GNg::sAM_NODE GNg::AmNode(1000);

void GNg::sAM_CTRL::InitSegment(const P<sAM_NODE> &node_array){
	if(node_array){
		zNodes      = node_array;
		ztEndtime   = zEndtime;
        ztAm_mode   = zAm_mode;
		zSegmentID  = 0; 
		zSegmentSet();
		zNowtime    = zStarttime;
		zStartRunTime = zRuntime;
	}else{
		zNodes.Delete();
		zEndtime = ztEndtime;
		zAm_mode = ztAm_mode;
		zStarttime = 0; zNowtime = 0;
	}
}
void GNg::sAM_CTRL::zSegmentSet(){
	zStarttime  = zSegmentID ? zNodes[zSegmentID - 1].node_time : 0;
	if(zSegmentID < (unsigned short)zNodes.Num()){
	    zEndtime = zNodes[zSegmentID].node_time;
	    zAm_mode = zNodes[zSegmentID].am_mode;
	}else{
		zEndtime = ztEndtime;
		zAm_mode = ztAm_mode;
	}
}
void GNg::sAM_CTRL::SeekSegment(unsigned short segmentID){
    zSegmentID = segmentID % (zNodes.Num() + 1);
	zSegmentSet();
    zNowtime = zStarttime;
	zStartRunTime = zRuntime;
}
void GNg::sAM_CTRL::SetTimeControl(unsigned int now_time,unsigned int start_time,unsigned int end_time,unsigned short am_mode){
    zAm_mode   = am_mode;
	if(!end_time)end_time = 1;
	if(now_time > end_time)now_time = end_time;
	zNowtime   = now_time;
	zStarttime = start_time;
	zEndtime   = end_time;
}
void GNg::sAM_CTRL::TimeRunSegmentSet(unsigned int st, unsigned int tt, eAM_TRS_OP op){
	zHasTRS = true;
	if(op == eAM_TRS_FLUSH)zPlaying = true;
	unsigned int ft = (unsigned int)(GN::TimeRatio * 16.0f), nt = zNowtime;
    switch(st){
	case eAM_TCR_START: zNowtime = zStarttime; break;
	case eAM_TCR_END:   zNowtime = zEndtime;   break;
	case eAM_TCR_LAST:  zNowtime = nt - ft;    break;
	case eAM_TCR_NEXT:  zNowtime = nt + ft;    break;
    case eAM_TCR_CUR:						   break;
	default: zNowtime = st;					   break;
	}
    switch(tt){
	case eAM_TCR_START: zTargetTime = zStarttime; break;
	case eAM_TCR_END:   zTargetTime = zEndtime;   break;
	case eAM_TCR_LAST:  zTargetTime = nt - ft;    break;
	case eAM_TCR_NEXT:  zTargetTime = nt + ft;    break;
    case eAM_TCR_CUR:   zTargetTime = nt;		  break;
	default: zTargetTime = tt;					  break;
	}
}
int8 GT_sAM_CTRL_AtoL(char *str, unsigned int *t){
	char *pstr = GN_StrCutSpaceFL(str);
	if(*pstr == '.' || *pstr == '-' || *pstr == '|' || *pstr == '<' || *pstr == '>'){
		if(pstr[1])return 1;
		else{
			switch(*pstr){
			case '.': *t = GNg::eAM_TCR_START; break;
			case '-': *t = GNg::eAM_TCR_CUR;   break;
			case '|': *t = GNg::eAM_TCR_END;   break;
            case '<': *t = GNg::eAM_TCR_LAST;  break;
			case '>': *t = GNg::eAM_TCR_NEXT;  break;
			}
            return 0;
		} 
	}else if(*pstr >= '0' && *pstr <= '9'){
	    char *pstr3 = pstr + 1;
		while(*pstr3 >= '0' && *pstr3 <= '9')++pstr3; //移动到数字之后的位置
		if(*pstr3)return 1;
		else{
		    *t = (unsigned int)atol(pstr);
			return 0;
		}
	}
    return 2;
}
int8 GNg::sAM_CTRL::TimeRunSegmentAtoL(char *ts_str, unsigned int *st, unsigned int *tt){
    if(!ts_str || !(*ts_str) || *ts_str == ',')return 1;
	char *pstr  = (char*)ts_str, *pstr2 = 0;
	do{ 
		if(*pstr == ','){ //检测号分隔是否正确
			if(pstr2)return 1;
			else{
			   *pstr = 0;
				pstr2 = pstr + 1;
				if(*pstr2 == 0)return 1;
			}
		}
		++pstr;
	}while(*pstr);
    if(!pstr2)return 1;

    int8 rv = GT_sAM_CTRL_AtoL(ts_str, st);
    if(rv)return rv;
    return GT_sAM_CTRL_AtoL(pstr2, tt);
}
unsigned short GNg::sAM_CTRL::zAmTimeProce(unsigned int updatetime){
	unsigned short ret = 0;
	if(updatetime){ //时间由自己管理处理 如是独立无被其他两个以上的父对象引用推荐用这种处理 这个对于懒得设置相接的动画事件 可没那么多意想不到的变化,
		 if(zNodes.Has() && zNowtime > zEndtime){ //有动画段落节点
		   #define GT_TMP_SEEK_NEXT_TIME_NODE /*播完跳转到下个时间节点*/\
			unsigned short node_n = (unsigned short)zNodes.Num();\
			sAM_NODE *_nodes = zNodes;\
			if(zSegmentID < node_n && _nodes[zSegmentID].seek_time <= ztEndtime){\
			    zNowtime = _nodes[zSegmentID].seek_time;\
                zSegmentID = 0; /*计算新跳转到的时间所对应的段落节位置*/\
			    while(zSegmentID < node_n && zNowtime > _nodes[zSegmentID].node_time)++zSegmentID;\
			    zSegmentSet();  /*重新设置开始时间和结束时间*/
			
			GT_TMP_SEEK_NEXT_TIME_NODE
		    }
	     }
		 if(zNowtime > zEndtime){ zAm_s_e = true; ret = 4; }
		 else if(zNowtime < zStarttime + updatetime){ zAm_s_e = false; ret = 0; }

		 if(zHasTRS){ //时间运行段控制
			 if(zPlaying){
				 if		 (zTargetTime > zNowtime){
				     zNowtime += updatetime;
					 if(zNowtime >= zTargetTime)zPlaying = false;
				 }else if(zTargetTime < zNowtime){
					 zNowtime -= updatetime > zNowtime ? zNowtime : updatetime;
					 if(zNowtime <= zTargetTime)zPlaying = false;
				 }else zPlaying = false;
			 }else ret = 4;
			 if(zAm_mode == 3 && zAm_s_e)return 3;
		 }else{
			 if(zAm_mode == 3 && zAm_s_e)return 3;	//播放完一次消失
	         else if(zAm_mode == 1 && zAm_s_e){     //循环播放
				 zAm_s_e = false;
			     zNowtime = zStarttime;
				 ret = 1;
	         }else if(zAm_mode == 2 && zAm_s_e){ zNowtime -= updatetime; ret = 2; } //来回播放
	         else zNowtime += updatetime;
		 }
		 zRuntime += updatetime;

    }else{ //时间由上一级传递落来统一连锁的处理 如果它是子对象而且给多个父对象引用 或在粒子系统的情况推荐这个处理
		 if(zNodes.Has()){ //有动画段落节点
		    unsigned int _runtime = zRuntime - zStartRunTime;
			if(_runtime > zEndtime - zStarttime){
			    GT_TMP_SEEK_NEXT_TIME_NODE
					_runtime = zNowtime - zStarttime;
					zStartRunTime = zRuntime - _runtime; //记录预估开始运行时间
					_runtime = _runtime ? _runtime : 1;
			    }
            }
			if(zAm_mode == 1){  //循环播放
				#define GT_TMP_AM_LOOP_CTRL( RTIME ) \
			     zNowtime = RTIME % (zEndtime - zStarttime + 1);\
			     if(zNowtime <= 28)ret = 1;\
                 zNowtime += zStarttime;

				 GT_TMP_AM_LOOP_CTRL( _runtime )

			}else if(zAm_mode == 2){         //来回播放
				#define GT_TMP_AM_COME_BACK_CTRL( RTIME ) \
		         unsigned int temp_time = zEndtime - zStarttime;\
			     unsigned int tsn  = RTIME / temp_time;\
		         zNowtime = RTIME - tsn * temp_time;\
			     if(tsn & 1) { zNowtime = temp_time - zNowtime; ret = 2; } /*倒播*/\
                 zNowtime += zStarttime;

				 GT_TMP_AM_COME_BACK_CTRL( _runtime )

		     }else if(zAm_mode == 3){
			     if(_runtime > (zEndtime - zStarttime))return 3; //播放完一次消失
                 zNowtime = _runtime + zStarttime;
		     }else {
			     zNowtime = _runtime + zStarttime;
			     if(zNowtime > zEndtime)ret = 4;
		     }
		 }else if(zRuntime >= zStarttime){ //没分段时间的 当运行时间大于开始时间再来处理
			 if(zAm_mode==1){  //循环播放
				 GT_TMP_AM_LOOP_CTRL( (zRuntime-zStarttime) )
			    
			 }else if(zAm_mode == 2){			   //来回播放
		         unsigned int runtime1 = zRuntime - zStarttime;
				 GT_TMP_AM_COME_BACK_CTRL( runtime1 )

		     }else{
				 if(zAm_mode == 3){
				     if(zRuntime > zEndtime)return 3; //播放完一次消失
		         }else if(zRuntime > zEndtime)ret = 4;
				 zNowtime = zRuntime;
			 }
		 }else zNowtime = zRuntime;
    }
	return ret;
}
uint8* GNg::sAM_CTRL::zDataSave(uint8 *wpos){
	memcpy(wpos,&zRuntime,4);    wpos+=4;
	memcpy(wpos,&zEndtime,4);    wpos+=4;
	memcpy(wpos,&zStarttime,4);  wpos+=4;
    memcpy(wpos,&zNowtime,4);    wpos+=4;
    memcpy(wpos,&zAm_mode,2);    wpos+=2;
	memcpy(wpos,&zAm_s_e,1);     wpos+=1;
	memcpy(wpos,&zSegmentID,2);  wpos+=2;
	memcpy(wpos,&ztAm_mode,2);   wpos+=2;
    memcpy(wpos,&ztEndtime,4);   wpos+=4;

	unsigned int num = zNodes.Num(), size;
	memcpy(wpos,&num,4);         wpos+=4;
	if(num){
        size = sizeof(sAM_NODE) * num;
		memcpy(wpos,zNodes.Begin(),size); wpos+=size;
	}
	return wpos;
}
uint8 *GNg::sAM_CTRL::zDataLoad(uint8 *rpos){
   #ifdef GN_USING_ADDITIONAL_LIB
    memcpy(&zRuntime,    rpos, 4);  rpos+=4;
	memcpy(&zEndtime,    rpos, 4);  rpos+=4;
	memcpy(&zStarttime,  rpos, 4);  rpos+=4;
    memcpy(&zNowtime,    rpos, 4);  rpos+=4;
    memcpy(&zAm_mode,    rpos, 2);  rpos+=2;
	memcpy(&zAm_s_e,     rpos, 1);  rpos+=1;
	memcpy(&zSegmentID,  rpos, 2);  rpos+=2;
	memcpy(&ztAm_mode,   rpos, 2);  rpos+=2;
    memcpy(&ztEndtime,   rpos, 4);  rpos+=4;

	unsigned int num, size;
	memcpy(&num, rpos, 4);  rpos+=4;
	if(num){
		zNodes.New(num);
        size = sizeof(sAM_NODE) * num;
		memcpy(zNodes.Begin(), rpos, size); rpos+=size;
	}
   #endif
	return rpos;
}
#endif //_GN_AMCTRL_H_