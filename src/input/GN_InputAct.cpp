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
#ifdef _GN_INPUTACT_H_
GN::sLIST<GN::IA::sOBJ *> GN::IA::sOBJ::zTotalList;

void GN::IA::sOBJ::zUpdateAll(unsigned int uptime) {
    if(zTotalList.size()) {
        sLIST<sOBJ *>::iterator ita = zTotalList.begin(), eta = zTotalList.end();
        for(; ita != eta; ++ita) (*ita)->zUpDate(uptime); //更新所有相关对象
    }
}
void GN::IA::sMOUSE_DOWN_SKIP::zUpDate(unsigned int uptime) {
    aOn = false;
    if(Mouse[zMsKey]) {
        if(zMsDownTime != 0xffffffff) {
            if(zMsDownTime >= zMsSkipTime) {
                aOn = true;
                zMsDownTime = 0xffffffff;
            }
            zMsDownTime += uptime;
            if(MouseM) zMsDownTime = 0xffffffff;
        }
    } else zMsDownTime = 0;
}
void GN::IA::sMOUSE_DOWN_SHAKE::zUpDate(unsigned int uptime) {
    aOn = false;
    if(Mouse[zMsKey]) {
        if(zMsDownTime != 0xffffffff) {
            if(!zMsDownTime) {
                zMsDownSX = 0;
                zMsDownSY = 0;
                zMsDownTime = 1;
                zMsMoveNum = 0;
            }
            if(zMsDownTime > 300) {
                zMsDownTime = 0xffffffff;
                if(zMsMoveNum > 1) aOn = true;
            } else {
                short msx = GNg::ViewCur->mouseMSX, msy = GNg::ViewCur->mouseMSY;
                if((zMsDownSX > 0 && msx < 0) || (zMsDownSX < 0 && msx > 0) || (zMsDownSY > 0 && msy < 0) || (zMsDownSY < 0 && msy > 0))
                    ++zMsMoveNum; //记录鼠标左键按紧额定时间内来回摞动数增加1

                zMsDownTime += uptime;
                zMsDownSX = msx;
                zMsDownSY = msy;
            }
        }
    } else zMsDownTime = 0;
}
#endif