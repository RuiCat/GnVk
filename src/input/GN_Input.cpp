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
#include "GN_InputContext.h" // B7-4：input 子系统上下文（原 sINPUT_CONTEXT）
#ifdef _GN_INPUT_H_
#ifndef _GN_MAIN_H_
namespace GN {
inline uint8 GetTouchMaxNum() {
    return 1;
}
}; // namespace GN
#endif
GN_INIT_API bool GN::Key[GN_MAX_KEY_NUM];
GN_INIT_API bool GN::Key0[GN_MAX_KEY_NUM];
GN_INIT_API bool GN::Key1[GN_MAX_KEY_NUM];
GN_INIT_API bool GN::Key11[GN_MAX_KEY_NUM];
GN_INIT_API bool GN::KeyDown;

GN_INIT_API bool GN::Mouse[GN_MAX_MK_NUM];
GN_INIT_API bool GN::Mouse0[GN_MAX_MK_NUM];
GN_INIT_API bool GN::Mouse1[GN_MAX_MK_NUM];
GN_INIT_API bool GN::Mouse2[GN_MAX_MK_NUM];
GN_INIT_API bool GN::MouseQ[GN_MAX_MK_NUM];
GN_INIT_API bool GN::MouseI[GN_MAX_MK_NUM];
GN_INIT_API unsigned int GN::MouseT[GN_MAX_MK_NUM];

GN_INIT_API bool GN::MouseM;
GN_INIT_API int GN::MouseMX;
GN_INIT_API int GN::MouseMY;

GN_INIT_API unsigned short GN::MouseMKS;

GN_INIT_API bool GN::TouchM;
GN_INIT_API short *GN::TouchX = 0;
GN_INIT_API short *GN::TouchY = 0;
GN_INIT_API uint8 *GN::TouchZ = 0;

GN_INIT_API bool GN::ViewReSized = true;

/********************************************************************************************************/
enum GT_eINPUT_TYPE { //输入操作消息类型
    GT_eINPUT_GAME_EXIT, //退出程序
    GT_eINPUT_IME, //IME文字输入
    GT_eINPUT_KEYBOARD, //按键盘
    GT_eINPUT_MOUSE_MOVE, //鼠标移动
    GT_eINPUT_MOUSE_KEY, //鼠标点击
    GT_eINPUT_TOUCH_KEY, //触摸点击
    GT_eINPUT_TOUCH_MOVE, //触摸拖动
    GT_eINPUT_INFV_UPDATE, //浮点数值输入更新
    GT_eINPUT_VIEW_SIZE //窗口大小改变
};
// B7-4：GT_sINPUT_PARAME 的声明已随上下文迁 GN_InputContext.h；方法实现保留在下方。
GT_sINPUT_PARAME::~GT_sINPUT_PARAME() {
    if(type == GT_eINPUT_IME) {
        if(*((unsigned short *)parame) > 18) delete[](*((char **)(parame + 1)));
    }
}
void GT_sINPUT_PARAME::ImeParameMake(void *str, unsigned short len) {
    if(len > 18) {
        char *add_str = new char[len];
        *((char **)(parame + 1)) = add_str;
        memcpy(add_str, str, len);
    } else memcpy(parame + 1, str, len);
}

// B7-4：输入上下文（原 DA-M2 批 1/批 3 的 sINPUT_CONTEXT）已迁 GN_InputContext.h，状态经全局
// GN_InputState 访问；导出计时变量（GT_RunTotalTime 等）属公共 API 保留。
namespace GNg {
GN_InputContext GN_InputState; // 默认 input 上下文（仿 GN_Vk / GN_Core）
} // namespace GNg



// DA-M2 批 1b：IME 上下文（跨 TU 通过 GN::GetImeContext() 访问）。
GN_INIT_API GN::sIME_CONTEXT &GN::GetImeContext() {
    // B7-5：IME 上下文存储已迁 GNg::GN_InputState.aImeContext（访问器语义不变）。
    return GNg::GN_InputState.aImeContext;
}

GN_INIT_API unsigned int GT_RunTotalTime = 0;
GN_INIT_API unsigned int GT_RunTotalTimeApp = 0;
GN_INIT_API unsigned int GT_LastRunTotalTimeApp = 0;
GN_INIT_API unsigned int GT_RunTotalFpsApp = 0;







GN_INIT_API void GN::InputRecReStart() {
    InputRecClear();
    GNg::GN_InputState.aInputRecStartTime = GT_RunTotalTimeApp;
    GNg::GN_InputState.aInputRecOn = true;
}
GN_INIT_API void GN::InputRecExitMsg() {
    GT_sINPUT_PARAME prm;
    prm.type = GT_eINPUT_GAME_EXIT;
    prm.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
    GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(prm);
}
GN_INIT_API void GN::InputRecEnd() {
    GNg::GN_InputState.aInputRecOn = false;
}

GN_INIT_API void GN::InputRecSave(const char *file_name, unsigned int dateID) {
    if(GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].empty()) return;
    GNf::sFILE *fp = GNf::Open((char *)file_name, "wb", GNf::eFILE_STD);
    if(!fp) {
        GN_Log("保存输入操作记录失败!\n");
        return;
    }
    uint8 locbuf[131072] /*128K*/, fh[4] = {0x21, 0x08, 0x18, 0x08 /*GNd::eDATA_GIN*/};
    unsigned int size = GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].size();
    GN::sTBUF_INT tbuf(locbuf, 131072);
    uint8 *buf = (uint8 *)tbuf.GetSpace(16 + size * 64 /*(4+2+58)*/);
    uint8 *wpos = buf;
    memcpy(wpos, fh, 4);
    wpos += 4; //写入标头
    memcpy(wpos, &dateID, 4);
    wpos += 4; //写入文件更新序号
    memcpy(wpos, &GNg::GN_InputState.aInputRecStartTime, 4);
    wpos += 4; //写入本回记录的开始时间
    memcpy(wpos, &size, 4);
    wpos += 4; //写入记录条目数

    GT_sINPUT_PARAME *_inp;
    GN::sLIST<GT_sINPUT_PARAME>::iterator it = GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].begin(), et = GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].end();
    for(; it != et; ++it) { //写入每条记录数据段
        _inp = &(*it);
        memcpy(wpos, &(_inp->time), 4);
        wpos += 4; //写入消息时间
        memcpy(wpos, &(_inp->type), 1);
        wpos += 1; //写入消息类型

        //按类型写入相应长度的参数数据
        if(_inp->type == GT_eINPUT_IME) { //字符输入类型写入
            unsigned short len = *((unsigned short *)_inp->parame);
            memcpy(wpos, &len, 2);
            wpos += 2;
            memcpy(wpos, len > 18 ? *((char **)(_inp->parame + 1)) : (char *)(_inp->parame + 1), len);
            wpos += len;

        } else if(_inp->type == GT_eINPUT_INFV_UPDATE) { //浮点输入类型写入
            unsigned int wsize = (_inp->parame[1] + 1) << 2;
            memcpy(wpos, _inp->parame, wsize);
            wpos += wsize;
        } else { //其它输入类型写入
            memcpy(wpos, _inp->parame, GNg::GN_InputState.aInputParameSizes[_inp->type]);
            wpos += GNg::GN_InputState.aInputParameSizes[_inp->type];
        }
    }
    GNf::Write((char *)buf, 1, wpos - buf, fp); //把缓冲数据一次性写入文件
    GNf::Close(fp);
}
GN_INIT_API void GN::InputRecClear() {
    GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].clear();
    GNg::GN_InputState.aInputRecOn = false;
}
GN_INIT_API bool GN::InputRecOpenPlay(const char *file_name, unsigned int *dateID, unsigned int *time) {
    InputRecClearPlay();
    int f_size;
    uint8 *buf = (uint8 *)GNf::FileFullLoadToMemory((char *)file_name, &f_size);
    if(!buf) {
        GN_Log("打开输入操作记录文件: %s 失败!\n", file_name);
        return false;
    }
    if(f_size < 16 || buf[0] != 0x21 || buf[1] != 0x08 || buf[2] != 0x18 || buf[3] != 0x08 /*GNd::eDATA_GIN*/) {
        GN_Log("文件: %s 不是有效的输入操作记录文件!\n");
        delete[] buf;
        return false;
    }
    uint8 *rpos = buf + 4;
    if(dateID) *dateID = *((unsigned int *)rpos);
    rpos += 4;
    if(time) *time = *((unsigned int *)rpos);
    rpos += 4;
    unsigned int count = *((unsigned int *)rpos);
    rpos += 4;
    unsigned int i = 0;
    GT_sINPUT_PARAME inp;
    for(; i < count; ++i) { //连续读取记录数据
        inp.time = *((unsigned int *)rpos);
        rpos += 4;
        inp.type = *((uint8 *)rpos);
        rpos += 1;

        if(inp.type == GT_eINPUT_IME) {
            memcpy(inp.parame, rpos, 2);
            rpos += 2;
            unsigned short len = *((unsigned short *)inp.parame);
            inp.ImeParameMake(rpos, len);
            rpos += len;

        } else if(inp.type == GT_eINPUT_INFV_UPDATE) {
            unsigned int rsize = (((short *)rpos)[1] + 1) << 2;
            memcpy(inp.parame, rpos, rsize);
            rpos += rsize;
        } else {
            memcpy(inp.parame, rpos, GNg::GN_InputState.aInputParameSizes[inp.type]);
            rpos += GNg::GN_InputState.aInputParameSizes[inp.type];
        }
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputPlayId].push_back(inp);
    }
    delete[] buf;
    return true;
}
GN_INIT_API void GN::InputRecRePlay() {
    GNg::GN_InputState.aInputPlaying = true;
    GNg::GN_InputState.aInputPlayStartTime = GT_RunTotalTimeApp;
    GNg::GN_InputState.aInputIt = GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputPlayId].begin();
}
GN_INIT_API void GN::InputRecStopPlay() {
    GNg::GN_InputState.aInputPlaying = false;
}
GN_INIT_API void GN::InputRecClearPlay() {
    GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputPlayId].clear();
    GNg::GN_InputState.aInputPlaying = false;
}
GN_INIT_API void GN::InputRecSwapToPlay() {
    GN_Swap(uint8, GNg::GN_InputState.aInputPlayId, GNg::GN_InputState.aInputRecId);
    GNg::GN_InputState.aInputPlaying = false;
    GNg::GN_InputState.aInputRecOn = false;
}
GN_INIT_API uint8 GN::InputRecLastKeyUpGet() {
    GN::sLIST<GT_sINPUT_PARAME>::reverse_iterator itr = GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].rbegin(), etr = GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].rend();
    for(; itr != etr; ++itr) {
        if(itr->type == GT_eINPUT_KEYBOARD) {
            if(!itr->parame[1]) return itr->parame[0];
        }
    }
    return 0xff;
}
/*-----------------------------------------------------  文字输入 -----------------------------------------------------------------------*/






GN_INIT_API bool GN_ImeIsOpen() {
    return (GN::GetImeContext().aImeOn || GN::GetImeContext().aImeOnRecv);
}
GN_INIT_API bool GN_SkbNeedOpen() {
    return GN::GetImeContext().aImeOn;
}

/*****************************************************************************************************************************************/
#ifdef _GN_MAIN_H_
extern void (*GT_BreakLoopHookProc)();
#endif






GN_INIT_API unsigned int GT_IdleTime = 0;

GN_INIT_API void GF_ReSetInputState(void (*get_ime_func)(char *ime_str, unsigned char *ime_len)) {
    //重设这些状态-------------------------------------------------------------------------------
    memset(GN::Mouse0, 0, GN_MAX_MK_NUM);
    memset(GN::Mouse1, 0, GN_MAX_MK_NUM);
    memset(GN::Mouse2, 0, GN_MAX_MK_NUM);
    memset(GN::MouseQ, 0, GN_MAX_MK_NUM);
    memset(GN::MouseI, 0, GN_MAX_MK_NUM);

    memset(GN::Key0, 0, GN_MAX_KEY_NUM);
    memset(GN::Key1, 0, GN_MAX_KEY_NUM);
    memset(GN::Key11, 0, GN_MAX_KEY_NUM);

    uint8 ti = 0, tuch_n = GN::GetTouchMaxNum(), *touch_zs = GN::TouchZ;
    for(; ti < tuch_n; ++ti) {
        if(touch_zs[ti] == 255) touch_zs[ti] = 254;
        else if(touch_zs[ti] == 1) {
            touch_zs[ti] = 0;
        }
    }
    GN::TouchM = false;
    GN::MouseM = false;
    GN::MouseMKS = 0;
    GN::KeyDown = false;
    if(GT_RunTotalTime > 20 && GT_RunTotalTimeApp > 20) GN::ViewReSized = false;
    GN::GetImeContext().aImeCount = 0;

    if(GN::Mouse[GM_TOP]) {
        GN::Mouse[GM_TOP] = 0;
        GN::Mouse0[GM_TOP] = 1;
    } //调配一些鼠标状态更变
    if(GN::Mouse[GM_BOTTOM]) {
        GN::Mouse[GM_BOTTOM] = 0;
        GN::Mouse0[GM_BOTTOM] = 1;
    }
    if(GN::MouseT[GM_TOP]) {
        GN::MouseT[GM_TOP] = 0;
        GN::MouseQ[GM_TOP] = 1;
        GN::MouseI[GM_TOP] = 1;
    }
    if(GN::MouseT[GM_BOTTOM]) {
        GN::MouseT[GM_BOTTOM] = 0;
        GN::MouseQ[GM_BOTTOM] = 1;
        GN::MouseI[GM_BOTTOM] = 1;
    }

    if(GNg::GN_InputState.aLastDelayedMouseStates.size()) { //消化延误的消息
        GN::sLIST<GNg::sRECT>::iterator itdms = GNg::GN_InputState.aLastDelayedMouseStates.begin(), etdms = GNg::GN_InputState.aLastDelayedMouseStates.end();
        GNg::sRECT *_dms;
        for(; itdms != etdms; ++itdms) {
            _dms = &(*itdms);
            GF_SetMouseState((uint8)_dms->right, _dms->bottom ? 1 : 0, _dms->left, _dms->top);
        }
        GNg::GN_InputState.aLastDelayedMouseStates.clear();
    }
    //输入控制演示处理,将记录到的消息转为相应控制调用---------------------------------------------------------------------------------------
    if(GNg::GN_InputState.aInputPlaying) {
        GT_sINPUT_PARAME *_inp;
        GN::sLIST<GT_sINPUT_PARAME>::iterator itend = GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputPlayId].end();
        for(; GNg::GN_InputState.aInputIt != itend && GNg::GN_InputState.aInputIt->time + GNg::GN_InputState.aInputPlayStartTime <= GT_RunTotalTimeApp; ++GNg::GN_InputState.aInputIt) {
            _inp = &(*GNg::GN_InputState.aInputIt);
            switch(_inp->type) {
                case GT_eINPUT_MOUSE_MOVE: {
                    GNg::cVIEW2D *last_rpv = GNg::GN_InputState.aMouseRePosView;
                    GNg::GN_InputState.aMouseRePosView = 0;
                    GF_SetMouseMoveState((unsigned short)_inp->parame[0], _inp->parame[1], _inp->parame[2]);
                    GNg::GN_InputState.aMouseRePosView = last_rpv;
                } break;
                case GT_eINPUT_MOUSE_KEY:
                    GF_SetMouseState((uint8)_inp->parame[0], _inp->parame[1] ? 1 : 0, _inp->parame[2], _inp->parame[3]);
                    break;
                case GT_eINPUT_TOUCH_MOVE:
                    GF_SetTouchMoveState((uint8)_inp->parame[0], (uint8)_inp->parame[1], _inp->parame[2], _inp->parame[3]);
                    break;
                case GT_eINPUT_TOUCH_KEY:
                    GF_SetTouchState((uint8)_inp->parame[0], (uint8)_inp->parame[1]);
                    break;
                case GT_eINPUT_INFV_UPDATE:
                    GF_SetInFvState(_inp->parame[0], (float *)(_inp->parame + 2), _inp->parame[1]);
                    break;
                case GT_eINPUT_KEYBOARD:
                    GF_SetKeyState(_inp->parame[0], _inp->parame[1] ? 1 : 0);
                    break;
                case GT_eINPUT_IME:
                    GN::GetImeContext().aImeCount = *((unsigned short *)_inp->parame);
                    memcpy(GN::GetImeContext().aImeChar, GN::GetImeContext().aImeCount > 18 ? *((char **)(_inp->parame + 1)) : (char *)(_inp->parame + 1), GN::GetImeContext().aImeCount);
                    break;
#ifdef _GN_MAIN_H_
                case GT_eINPUT_GAME_EXIT:
                    if(GN_GameBreakLoopProc) GN_GameBreakLoopProc();
                    else GN_GameBreakLoop();
                    break;
                case GT_eINPUT_VIEW_SIZE:
                    GN_SetWindowPos(0, 0, _inp->parame[0], _inp->parame[1]);
                    break;
#endif
            }
        }
    }
    if(GN::GetImeContext().aImeOn || GN::GetImeContext().aImeOnRecv) get_ime_func(GN::GetImeContext().aImeChar, &GN::GetImeContext().aImeCount); //接收文本输入来的内容
#ifdef _GN_MAIN_H_
    else GN_SetImeFouscePos(-10000, -10000);
#endif
    if(GN::GetImeContext().aImeCount) {
        GT_IdleTime = 0;
        if(GNg::GN_InputState.aInputRecOn) { //当有文字输入进来时就记录输入
            GT_sINPUT_PARAME inp;
            inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
            inp.type = GT_eINPUT_IME;
            inp.ImeParameMake(GN::GetImeContext().aImeChar, GN::GetImeContext().aImeCount);
            GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
        }
    }
}
GN_INIT_API void GF_DownKeyAllLoosen() {
    int mx = GN::MouseMX, my = GN::MouseMY;
    bool *keys = GN::Key, *mouses = GN::Mouse;
    uint8 i, n;
    for(i = 0; i < GN_MAX_KEY_NUM; ++i)
        if(keys[i]) GF_SetKeyState((int)i, 0);
    for(i = 0; i < GN_MAX_MK_NUM; ++i)
        if(mouses[i]) GF_SetMouseState(i, 0, mx, my);

    if(!GN::TouchZ) return;
    uint8 *touch_zs = GN::TouchZ;

    n = GN::GetTouchMaxNum();
    for(i = 0; i < n; ++i) {
        if(touch_zs[i] > 1) GF_SetTouchState(i, 1);
    }
}
GN_INIT_API void GF_SetMouseState(uint8 mkey, bool is_down, int px, int py) { //GN_Log("state: mkey=%d, is_down=%d, px=%d, py=%d\n", (int)mkey, is_down?1:0, px,py);
    if(!is_down && GN::MouseT[mkey] == 1) {
        GNg::GN_InputState.aLastDelayedMouseStates.push_back(GNg::sRECT(px, py, (int)mkey, 0));
        return;
    }
    if(GNg::GN_InputState.aInputRecOn && GNg::GN_InputState.aInputNotIsMapping) { //记录鼠标点击输入
        GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
        inp.type = GT_eINPUT_MOUSE_KEY;
        inp.parame[0] = (short)mkey;
        inp.parame[1] = is_down ? 1 : 0;
        inp.parame[2] = px;
        inp.parame[3] = py;
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
    }
    GN::Mouse[mkey] = is_down;
    GN::MouseMX = px;
    GN::MouseMY = py;
    if(is_down) {
        GN::Mouse1[mkey] = 1;
        GN::MouseT[mkey] = 1;
        if(GNg::GN_InputState.aLastOnMouseTime[mkey] <= GNg::GN_InputState.aMouseInterval) {
            GN::Mouse2[mkey] = 1;
            GNg::GN_InputState.aLastOnMouseTime[mkey] = 10000000;
        } else GNg::GN_InputState.aLastOnMouseTime[mkey] = 0;
    } else {
        GN::Mouse0[mkey] = 1;
        if(GN::MouseT[mkey]) {
            GN::MouseQ[mkey] = 1;
            if(GN::MouseT[mkey] <= GNg::GN_InputState.aMouseInterval) GN::MouseI[mkey] = 1;
            GN::MouseT[mkey] = 0;
        }
    }
#ifdef _GN_2D_H_
    GN::sLIST<GNg::cVIEW2D *>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
    GNg::cVIEW2D *pv;
    for(; itv != etv; ++itv) {
        pv = (*itv);
        pv->mouseMX = pv->CoordWindowToViewX(px); //窗口坐标转为后台坐标
        pv->mouseMY = pv->CoordWindowToViewY(py);
    }
#endif
    GT_IdleTime = 0;
}
GN_INIT_API void GF_SetMouseMoveState(unsigned short keys_state, int new_x, int new_y) {
    if(new_x == GN::MouseMX && new_y == GN::MouseMY) return; //忽略位置相同的消息 以免浪费性能

    if(GNg::GN_InputState.aInputRecOn && GNg::GN_InputState.aInputNotIsMapping) { //记录鼠标移动输入
        GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
        inp.type = GT_eINPUT_MOUSE_MOVE;
        inp.parame[0] = keys_state;
        inp.parame[1] = new_x;
        inp.parame[2] = new_y;
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
    }
    // B7-5：GNg::GN_InputState.aLastMouseTime 已迁 GNg::GN_InputState.aLastMouseTime。
    int vx, vy; /*模版代码:计算在额定时间内的位置差*/
    if(GT_RunTotalTime - GNg::GN_InputState.aLastMouseTime > 250) {
        vx = 0;
        vy = 0;
    } else {
        vx = new_x - GN::MouseMX;
        vy = new_y - GN::MouseMY;
    }
    GN::MouseMX = new_x;
    GN::MouseMY = new_y;
    GN::MouseM = 1;
    GN::MouseMKS = keys_state;
    memset(GN::MouseT, 0, 20 /* sizeof(unsigned int)*5 */);

#ifdef _GN_MAIN_H_
    if((vx > GNg::GN_InputState.aMousePosDeviation || vy > GNg::GN_InputState.aMousePosDeviation || vx < GNg::GN_InputState.aMousePosDeviation || vy < GNg::GN_InputState.aMousePosDeviation)) {
        GNg::GN_InputState.aLastOnMouseTime[0] = 10000000;
        GNg::GN_InputState.aLastOnMouseTime[1] = 10000000;
        GNg::GN_InputState.aLastOnMouseTime[2] = 10000000;
        GNg::GN_InputState.aLastOnMouseTime[3] = 10000000;
        GNg::GN_InputState.aLastOnMouseTime[4] = 10000000;
    }
#endif
#ifdef _GN_2D_H_
    GN::sLIST<GNg::cVIEW2D *>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
    GNg::cVIEW2D *pv;
    for(; itv != etv; ++itv) {
        pv = (*itv);
        pv->mouseMX = pv->CoordWindowToViewX(new_x); //窗口坐标转为后台坐标
        pv->mouseMY = pv->CoordWindowToViewY(new_y);
        pv->mouseMSX = vx * pv->zScreenRatioW;
        pv->mouseMSY = vy * pv->zScreenRatioH;

        //使适合第一人称射击游戏的鼠标会轮回中心的处理
        if(pv == GNg::GN_InputState.aMouseRePosView) {
            short bw = pv->viewW >> 4, bh = pv->viewH >> 4, xpos, ypos;
            if(GNg::GN_InputState.aMouseRePosX && ((short)new_x > pv->viewX + pv->viewW - bw || (short)new_x < pv->viewX + bw)) xpos = pv->viewX + (pv->viewW >> 1);
            else xpos = new_x;
            if(GNg::GN_InputState.aMouseRePosY && ((short)new_y > pv->viewY + pv->viewH - bh || (short)new_y < pv->viewY + bh)) {
                ypos = pv->viewY + (pv->viewH >> 1);
                GN_SetCursorPos(xpos, ypos, false);
            } else {
                ypos = new_y;
                if(xpos != (short)new_x) GN_SetCursorPos(xpos, ypos, false);
            }
            if(GNg::GN_InputState.aMouseRePosX) { //速度轮回时会太长抵扣一下
                int rng = (pv->orthoW >> 2) + (pv->orthoW >> 3);
                if(pv->mouseMSX > rng) pv->mouseMSX -= (pv->orthoW >> 1) - (pv->orthoW >> 4);
                else if(pv->mouseMSX < -rng) pv->mouseMSX += (pv->orthoW >> 1) - (pv->orthoW >> 4);
            }
            if(GNg::GN_InputState.aMouseRePosY) {
                int rng = (pv->orthoH >> 2) + (pv->orthoH >> 3);
                if(pv->mouseMSY > rng) pv->mouseMSY -= (pv->orthoH >> 1) - (pv->orthoH >> 4);
                else if(pv->mouseMSY < -rng) pv->mouseMSY += (pv->orthoH >> 1) - (pv->orthoH >> 4);
            }
        }
    }
#endif
    GNg::GN_InputState.aLastMouseTime = GT_RunTotalTime;
    GT_IdleTime = 0;
}
/******************************************************** 触摸消息处理 ***************************************************************/



GN_INIT_API void GF_SetTouchState(uint8 point_id, uint8 down) {
    if(GNg::GN_InputState.aTouchMappingMKey < GM_TOP) {
        if(point_id == 0) { //如是首个触摸点就映射为鼠标事件
            if(down == 255) {
                GNg::GN_InputState.aTouch0DownTime = 1;
                GNg::GN_InputState.aTouchMappingMKey = GM_LEFT;

            } else if(down == 1) {
                GNg::GN_InputState.aInputNotIsMapping = 0;
                if(GNg::GN_InputState.aTouch0DownTime) {
                    GF_SetMouseState(GNg::GN_InputState.aTouchMappingMKey, 1, GN::TouchX[0], GN::TouchY[0]);
                    GNg::GN_InputState.aTouch0DownTime = 0;
                }
                GF_SetMouseState(GNg::GN_InputState.aTouchMappingMKey, 0, GN::TouchX[0], GN::TouchY[0]);
                GNg::GN_InputState.aInputNotIsMapping = 1;
            }
        } else if(point_id == 1) {
            if(down == 255) {
                if(GNg::GN_InputState.aTouch0DownTime) {
                    GNg::GN_InputState.aTouch0DownTime = 0;
                    GNg::GN_InputState.aTouchMappingMKey = GM_TOP; //映射更变为滚轮键
                    GNg::GN_InputState.aLastZoomLen = 0.0f;
                }
            }
        }
    } else { //映射滚轮操作模式
        if(down == 1) {
            if((!point_id && GN::TouchZ[1] <= 1) || (point_id == 1 && GN::TouchZ[0] <= 1)) {
                GNg::GN_InputState.aTouchMappingMKey = GM_LEFT; //这时第一和二个触摸点都被释放就标记退出滚轮操作
                if(GNg::GN_InputState.aLastZoomLen > -0.01f && GNg::GN_InputState.aLastZoomLen < 0.01f) {
                    GNg::GN_InputState.aInputNotIsMapping = 0;
                    GF_SetMouseState(GM_RIGHT, 1, GN::MouseMX, GN::MouseMY);
                    GF_SetMouseState(GM_RIGHT, 0, GN::MouseMX, GN::MouseMY);
                    GNg::GN_InputState.aInputNotIsMapping = 1;
                }
            }
            if(point_id <= 1) GNg::GN_InputState.aLastZoomLen = 0.0f;
        }
    }
    //--------------------------------------------------------------------------------------------
    if(GNg::GN_InputState.aInputRecOn) { //记录触摸点击输入
        GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
        inp.type = GT_eINPUT_TOUCH_KEY;
        inp.parame[0] = (short)point_id;
        inp.parame[1] = (short)down;
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
    }
    GN::TouchZ[point_id] = down;

    GT_IdleTime = 0;
}
GN_INIT_API void GF_SetTouchMoveState(uint8 point_id, uint8 down_z, short new_x, short new_y) {
    if(GNg::GN_InputState.aTouch0DownTime /* > 240*/ >= GNg::GN_InputState.aMouseInterval) { //当有首个触摸点长按时就映射为鼠标左键双击消息
        GNg::GN_InputState.aInputNotIsMapping = false;
        GF_SetMouseState(GM_LEFT, 1, GN::MouseMX, GN::MouseMY);
        GN::Mouse2[GM_LEFT] = 1;
        GF_SetMouseState(GM_LEFT, 0, GN::MouseMX, GN::MouseMY);
        GNg::GN_InputState.aInputNotIsMapping = true;
        GNg::GN_InputState.aTouch0DownTime = 0;
    }
    short *touch_x = GN::TouchX, *touch_y = GN::TouchY;
    if(touch_x[point_id] == new_x && touch_y[point_id] == new_y && (GN::TouchZ[point_id] == 255 || GN::TouchZ[point_id] == down_z)) return; //忽略没更变的消息 以免浪费性能

    if(GNg::GN_InputState.aTouchMappingMKey < GM_TOP) {
        if(point_id == 0) { //如是首个触摸点就映射为鼠标事件
            short crs_x = new_x - touch_x[point_id], crs_y = new_y - touch_y[point_id];
            if(crs_x && crs_y) {
                GNg::GN_InputState.aInputNotIsMapping = 0;
                if(GNg::GN_InputState.aTouch0DownTime) {
                    short dhr = (short)GNg::GN_InputState.aDitherRange;
                    if(crs_x > dhr || crs_x < -dhr || crs_y > dhr || crs_y < -dhr) {
                        GF_SetMouseState(GNg::GN_InputState.aTouchMappingMKey, 1, (int)new_x, (int)new_y);
                        GNg::GN_InputState.aTouch0DownTime = 0;
                    }
                }
                GF_SetMouseMoveState(GNg::GN_InputState.aTouchMappingMKey == GM_LEFT ? GMM_LEFT : GMM_RIGHT, (int)new_x, (int)new_y);
                GNg::GN_InputState.aInputNotIsMapping = 1;
            }
        }
    } else { //映射滚轮操作模式
        if(point_id <= 1 && GN::TouchZ[point_id] > 1) {
            GNg::sVEC2 tchp0_1, tchp0__1;
            if(point_id == 0) {
                tchp0_1.x = (float)(touch_x[0] - touch_x[1]), tchp0_1.y = (float)(touch_y[0] - touch_y[1]);
                tchp0__1.x = (float)(new_x - touch_x[1]), tchp0__1.y = (float)(new_y - touch_y[1]);
            } else {
                tchp0_1.x = (float)(touch_x[1] - touch_x[0]), tchp0_1.y = (float)(touch_y[1] - touch_y[0]);
                tchp0__1.x = (float)(new_x - touch_x[0]), tchp0__1.y = (float)(new_y - touch_y[0]);
            }
            GNg::GN_InputState.aLastZoomLen += GNg::Vec2Length(&tchp0__1) - GNg::Vec2Length(&tchp0_1);

            GNg::GN_InputState.aInputNotIsMapping = 0;
            if(GNg::GN_InputState.aLastZoomLen > 65.0f) {
                GNg::GN_InputState.aLastZoomLen -= 65.0f;
                GF_SetMouseState(GM_TOP, 1, GN::MouseMX, GN::MouseMY);
            } else if(GNg::GN_InputState.aLastZoomLen < -65.0f) {
                GNg::GN_InputState.aLastZoomLen += 65.0f;
                GF_SetMouseState(GM_BOTTOM, 1, GN::MouseMX, GN::MouseMY);
            }
            GNg::GN_InputState.aInputNotIsMapping = 1;
        }
    }
    //--------------------------------------------------------------------------------------------
    if(GNg::GN_InputState.aInputRecOn) { //记录触摸拖动输入
        GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
        inp.type = GT_eINPUT_TOUCH_MOVE;
        inp.parame[0] = (short)point_id;
        inp.parame[1] = (short)down_z;
        inp.parame[2] = new_x;
        inp.parame[3] = new_y;
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
    }
    // B7-5：GNg::GN_InputState.aLastTouchTime 已迁 GNg::GN_InputState.aLastTouchTime。
    short vx, vy; /*模版代码:计算在额定时间内的位置差*/
    if(GT_RunTotalTime - GNg::GN_InputState.aLastTouchTime > 250) {
        vx = 0;
        vy = 0;
    } else {
        vx = new_x - touch_x[point_id];
        vy = new_y - touch_y[point_id];
    }
    if(GN::TouchZ[point_id] != 255) GN::TouchZ[point_id] = down_z;
    touch_x[point_id] = new_x;
    touch_y[point_id] = new_y;
    GN::TouchM = true;

#ifdef _GN_2D_H_
    GN::sLIST<GNg::cVIEW2D *>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
    GNg::cVIEW2D *pv;
    for(; itv != etv; ++itv) {
        pv = (*itv);
        pv->touchX[point_id] = pv->CoordWindowToViewX(new_x);
        pv->touchY[point_id] = pv->CoordWindowToViewY(new_y);
        pv->touchSX[point_id] = (short)(vx * pv->zScreenRatioW);
        pv->touchSY[point_id] = (short)(vy * pv->zScreenRatioH);
    }
#endif
    GNg::GN_InputState.aLastTouchTime = GT_RunTotalTime;
    GT_IdleTime = 0;
}
/*************************************************************************************************************************************/
GN_INIT_API void GF_SetKeyState_(int key, bool is_down) {
    if(GNg::GN_InputState.aInputRecOn && GN::Key[key] != is_down) { //记录按键盘输入
        GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
        inp.type = GT_eINPUT_KEYBOARD;
        inp.parame[0] = key;
        inp.parame[1] = is_down ? 1 : 0;
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
    }
    if(!is_down) GN::Key0[key] = 1;
    GN::Key1[key] = GN::KeyDown = !GN::Key[key];
    GN::Key11[key] = is_down;
    GN::Key[key] = is_down;
    GT_IdleTime = 0;
}
GN_INIT_API void (*GF_SetKeyState)(int key, bool is_down) = GF_SetKeyState_;

GN_INIT_API void GF_SetInFvState(short start_infv_id, const float *infv, short in_count) {
    if(GNg::GN_InputState.aInputRecOn) { //记录浮点数值更新输入
        GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
        inp.type = GT_eINPUT_INFV_UPDATE;
        inp.parame[0] = start_infv_id;
        inp.parame[1] = in_count;
        memcpy(inp.parame + 2, infv, in_count << 2 /* *sizeof(float) */);
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
    }
    memcpy(GNio::InFv + start_infv_id, infv, in_count << 2);
    //GT_IdleTime = 0;
}



GN_INIT_API void GF_SetInPortState(unsigned short port_id, bool is_on) {
    GNio::InPort[port_id] = is_on;
    if(GNg::GN_InputState.aIoMappingKeys && GNg::GN_InputState.aIoMappingKeys[port_id])
        GF_SetKeyState(GNg::GN_InputState.aIoMappingKeys[port_id], is_on);

    GNg::GN_InputState.aInputLastPort = port_id;
    GNg::GN_InputState.aInputTestTime = 0;
}
GN_INIT_API void GF_ReSetViewport(short new_w, short new_h) {
    if(GNg::GN_InputState.aInputRecOn) { //记录窗口大小改变
        GT_sINPUT_PARAME inp;
        inp.time = GT_RunTotalTimeApp - GNg::GN_InputState.aInputRecStartTime;
        inp.type = GT_eINPUT_VIEW_SIZE;
        inp.parame[0] = new_w;
        inp.parame[1] = new_h;
        GNg::GN_InputState.aInputRecList[GNg::GN_InputState.aInputRecId].push_back(inp);
    }
    GN::ViewReSized = true;
#ifdef _GN_2D_H_
    GNg::ViewCur->ReSize(0, 0, new_w, new_h);
#ifdef GT_TMP_COMPATI_DX_DDRAW /*模版代码:重架设2D渲染状态*/
    if(!strcmp(GN_GetInterfaceInfoRender(), "DirectDraw")) {
        GNg::ViewCur->BeginRender2D();
    } else
#endif
        GNg::ViewCur->BeginRender2D();
#endif
}
GN_INIT_API void GN_SetMouseDoubleClick(unsigned int Interval, unsigned int deviation) {
    GNg::GN_InputState.aMouseInterval = Interval;
    GNg::GN_InputState.aMousePosDeviation = deviation;
}
GN_INIT_API void GN_SetMouseOutViewRePos(GNg::cVIEW2D *view, bool xpos, bool ypos) {
    GNg::GN_InputState.aMouseRePosView = view;
    GNg::GN_InputState.aMouseRePosX = xpos;
    GNg::GN_InputState.aMouseRePosY = ypos;
}
/**************************************************************外部IO的东西*************************************************************************************************/

GN_INIT_API void GNio::SetIOMappingKey(short IO_port, unsigned short GK_key) {
#ifdef GN_DEBUG
    if(!GNg::GN_InputState.aIoMappingKeys) {
        GN_Log("调用 SetIOMappingKey 失败! 还未初始化外部IO 请先调用 GN_InitInput ...\n");
        return;
    }
#endif
    if(GK_key < GN_MAX_KEY_NUM && IO_port < (short)GetInPortNum()) GNg::GN_InputState.aIoMappingKeys[IO_port] = GK_key;
}
GN_INIT_API void GNio::OutPutAllON() {
    uint8 i = 0, n = GetOutPutNum();
    for(; i < n; ++i) OutPut(i, 1);
}
GN_INIT_API void GNio::OutPutAllOFF() {
    uint8 i = 0, n = GetOutPutNum();
    for(; i < n; ++i) OutPut(i, 0);
}
GN_INIT_API bool GNio::OutPutTestDisplay(unsigned int nTime, unsigned int port_chg_time) {
    // B7-5：输出测试状态已迁 GNg::GN_InputState（aOutPutTestTime/aOutPutTestChange）。
    GNg::GN_InputState.aOutPutTestTime += nTime;
    unsigned int port = GNg::GN_InputState.aOutPutTestTime / port_chg_time;
    if(port != GNg::GN_InputState.aOutPutTestChange) {
        if(port < 4) {
            switch(port) { //开头全部开两遍
                case 0:
                case 2: OutPutAllON(); break;
                case 1:
                case 3: OutPutAllOFF(); break;
            }
        } else { //然后一个接一个地开
            int tp = (int)(port - 4);
            if(tp) OutPut(tp - 1, 0);
            if(tp < (int)GetOutPutNum()) OutPut(tp, 1);
            else {
                GNg::GN_InputState.aOutPutTestTime = 0;
                GNg::GN_InputState.aOutPutTestChange = 10000;
                return 0;
            }
        }
        GNg::GN_InputState.aOutPutTestChange = port;
    }
    if(port >= 4) GN_RunValueCheck(port - 4, "OUT PORT: ", port - 4);
    return 1;
}
GN_INIT_API int GNio::InPutTestDisplay(unsigned int nTime, unsigned int show_delay_time) {
#ifdef _GN_INIT_H
    unsigned int fcolor = GT_AppDefaultColorFont;
    unsigned int lcolor = GT_AppDefaultColorFocus;
#else
    unsigned int fcolor = 0xffffffff;
    unsigned int lcolor = 0xffaf4f00;
#endif
#ifdef _GN_RUNINFO_H_ //文字输入出端口号提示
    if(GNg::GN_InputState.aInputTestTime < show_delay_time)
        GN_RunValueCheck(GNg::GN_InputState.aInputLastPort, "          IN  PORT: ", GNg::GN_InputState.aInputLastPort);
#endif
#ifdef _GN_G2D_H_
    short i = 0, n = (short)GetInPortNum();
    short w = GNg::ViewCur->orthoW / n, h = GNg::ViewCur->orthoH >> 3;
    for(; i < n; ++i) { //显示出当前输入状态
        if(InPort[i]) GNg::ImageDraw(0, GNg::ViewCur->ortho_left + w * i, GNg::ViewCur->ortho_top, w, h, lcolor);
        GNg::LineFrame2dDraw(GNg::ViewCur->ortho_left + w * i, GNg::ViewCur->ortho_top, w, h, 3, fcolor);
    }
#endif
    GNg::GN_InputState.aInputTestTime += nTime;
    return GNg::GN_InputState.aInputLastPort;
}
extern uint8 GT_MouseCkN = 0;


GN_INIT_API void GN::SetWeightSensorMappingToKey(bool yes) {
    GNg::GN_InputState.aWsMappingToKey = yes;
}
GN_INIT_API void GN::SetTouchMappingToMouseDitherRange(unsigned short dr) {
    GNg::GN_InputState.aDitherRange = dr;
}

/*******************************************************************************************************************************************/
GN_INIT_API void GN_InitInput() {
    if(GT_SystemInited & (1 << 1)) return;
    uint8 tn = GN::GetTouchMaxNum();
    if(!GN::TouchX) GN::TouchX = new short[tn];
    if(!GN::TouchY) GN::TouchY = new short[tn];
    if(!GN::TouchZ) GN::TouchZ = new uint8[tn];
    memset(GN::TouchZ, 0, tn);

    if(!GNg::GN_InputState.aIoMappingKeys) GNg::GN_InputState.aIoMappingKeys = new unsigned short[GNio::GetInPortNum()];
    memset(GNg::GN_InputState.aIoMappingKeys, 0, GNio::GetInPortNum() << 1);

    GT_SystemInited |= 1 << 1;
}
GN_INIT_API void GN_UnInitInput() {
    GN::InputRecClear();
    GN::InputRecClearPlay();
    GN_ValueChangeClearAll();
#ifdef _GN_INPUTACT_H_
#if defined(GN_DEBUG) && defined(_GN_RUNINFO_H_)
    if(GN::IA::sOBJ::zTotalList.size()) GN_Log("还有输入动作没被移除!!\n");
#endif
#endif
    GN_DeleteArray(GNg::GN_InputState.aIoMappingKeys);
    GN_DeleteArray(GN::TouchX);
    GN_DeleteArray(GN::TouchY);
    GN_DeleteArray(GN::TouchZ);
    GT_SystemInited -= 1 << 1;
}
GN_INIT_API void GN_InputProc(unsigned int nTime) {
    static unsigned int sensorCaps = GNio::GetSensorCaps(); //用静态变量以仅执行1次 减少性能开销
    if(sensorCaps) {
        if(GNg::GN_InputState.aWsMappingToKey) { //感应器信号映射为键盘信号
            GN::Key[GK_A] = GNio::InFv[GI_WY] > 0.7f;
            GN::Key[GK_D] = GNio::InFv[GI_WY] < -0.7f;
            GN::Key[GK_W] = GNio::InFv[GI_WX] > -3.0f;
            GN::Key[GK_S] = GNio::InFv[GI_WX] < -5.0f;

            // B7-5：感应器基准已迁 GNg::GN_InputState（aSensorRx/aSensorRz）。
            if(!GNg::GN_InputState.aSensorRx && GNio::InFv[GI_DX]) GNg::GN_InputState.aSensorRx = GNio::InFv[GI_DX];
            if(!GNg::GN_InputState.aSensorRz && GNio::InFv[GI_DZ]) GNg::GN_InputState.aSensorRz = GNio::InFv[GI_DZ];
            GN::Key[GK_LEFT] = GNio::InFv[GI_DX] - GNg::GN_InputState.aSensorRx < -1.0f;
            GN::Key[GK_RIGHT] = GNio::InFv[GI_DX] - GNg::GN_InputState.aSensorRx > 1.0f;
            GN::Key[GK_UP] = GNio::InFv[GI_DZ] - GNg::GN_InputState.aSensorRz > 1.0f;
            GN::Key[GK_DOWN] = GNio::InFv[GI_DZ] - GNg::GN_InputState.aSensorRz < -1.0f;
        }
    }
    if(GN::MouseT[0]) GN::MouseT[0] += nTime;
    if(GN::MouseT[1]) GN::MouseT[1] += nTime;
    if(GN::MouseT[2]) GN::MouseT[2] += nTime;
    if(GN::MouseT[3]) GN::MouseT[3] += nTime;
    if(GN::MouseT[4]) GN::MouseT[4] += nTime;

    GNg::GN_InputState.aLastOnMouseTime[0] += nTime;
    GNg::GN_InputState.aLastOnMouseTime[1] += nTime;
    GNg::GN_InputState.aLastOnMouseTime[2] += nTime;
    GNg::GN_InputState.aLastOnMouseTime[3] += nTime;
    GNg::GN_InputState.aLastOnMouseTime[4] += nTime;
    if(GNg::GN_InputState.aTouch0DownTime) GNg::GN_InputState.aTouch0DownTime += nTime;
    if(GT_MouseCkN < 5) {
        if(GT_MouseCkN & 1) {
            if(GN::MouseQ[0]) ++GT_MouseCkN;
            else if(GN::Mouse0[0]) GT_MouseCkN = 0;
        } else {
#define GT_TIME_OUTV(TOV) #TOV
            if(GN::MouseT[0] > 6000) ++GT_MouseCkN;
            else if(GN::Mouse0[0]) GT_MouseCkN = 0;
        }
    }
    GN::GetImeContext().aImeOn = false;
    GN::GetImeContext().aImeOnRecv = false;
    GT_LastRunTotalTimeApp = GT_RunTotalTimeApp;
    GT_RunTotalTime += nTime; /*更新应用运行时间*/
    GT_RunTotalTimeApp += nTime;
    ++GT_RunTotalFpsApp;
    GT_IdleTime += nTime;
    GN::TimeRatioL = GN::TimeRatio = (float)nTime * 0.0625f /* /16.0f */;
    if(GT_MouseCkN >= 5) { GN::RunRec.OutText(GT_TIME_OUTV(11282076), 0, 160, 160); }
    if(GN::TimeRatioL > 3.0f) GN::TimeRatioL = 3.0f;

    GN_ValueChangeUpdateAll();

#if defined _GN_2D_H_ && defined _GN_MAIN_H_
    GN::sLIST<GNg::cVIEW2D *>::iterator itv = GNg::cVIEW2D::zTotalList.begin(), etv = GNg::cVIEW2D::zTotalList.end();
    GNg::cVIEW2D *pv;
    GNg::sRECT *_rng = GN_GetSoftKeyboardRange();
    if(_rng) { //如有软键盘 就传递它的范围到各个窗口
        for(; itv != etv; ++itv) {
            pv = (*itv);
            pv->softKeyboardX = pv->CoordWindowToViewX(_rng->left);
            pv->softKeyboardW = pv->CoordWindowToViewX(_rng->right) - pv->softKeyboardX;
            pv->softKeyboardY = pv->CoordWindowToViewY(_rng->top);
            pv->softKeyboardH = pv->CoordWindowToViewY(_rng->bottom) - pv->softKeyboardY;
        }
    } else {
        for(; itv != etv; ++itv) {
            pv = (*itv);
            pv->softKeyboardX = -32768; //short 装不下 -100000（溢出截断成 +31072，符号反了）；用 SHRT_MIN 当屏外哨兵
            pv->softKeyboardW = 0;
            pv->softKeyboardY = -32768; //同上
            pv->softKeyboardH = 0;
        }
    }
#endif
#ifdef _GN_INPUTACT_H_
    GN::IA::sOBJ::zUpdateAll(nTime);
#endif
#ifdef _GN_UI_H_
    GN_Swap(GNu::iUI_OBJECT *, GNu::RecvInputUiObject, GNu::RequestInputUiObject);
#endif
}
#endif // _GN_INPUT_H_
