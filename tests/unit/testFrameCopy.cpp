// testFrameCopy.cpp — cFRAME_RC 的复制语义：operator= 逐字段复制 + 安全指针浅共享，
// CopyData 深复制事件/索引/子筐，自赋值不破坏数据。对应 DEFECT_ANALYSIS DA-M1
// （原实现对含虚函数与非平凡成员的类做 memcpy/逐字节覆写，属 UB）。
#include "testFramework.h"

#include <GN.h>

namespace {

// 给 src 的全部成员填上有辨识度的值；新增成员时必须同步加进这里与 testCheckAllFields。
void fillAllFields(GNg::cFRAME_RC &f) {
    f.zRuntime = 11;
    f.zNowtime = 22;
    f.zEndtime = 33;
    f.zStarttime = 44;
    f.zAm_mode = 3;
    f.zAm_s_e = true;
    f.zHasTRS = true;
    f.zPlaying = false;
    f.zSegmentID = 7;
    f.ztAm_mode = 9;
    f.ztEndtime = 88;
    f.zTargetTime = 1234;
    f.zStartRunTime = 99;

    f.zFrame_ts.x = 1.5f;
    f.zFrame_ts.y = 2.5f;
    f.zFrame_ts.sx = 3.5f;
    f.zFrame_ts.sy = 4.5f;
    f.zFrame_ts.r = 0.25f;
    f.zRcLoc.left = 10.0f;
    f.zRcLoc.top = 20.0f;
    f.zRcLoc.right = 130.0f;
    f.zRcLoc.bottom = 140.0f;
    f.zRcS.left = 0.25f;
    f.zRcS.top = 0.5f;
    f.zRcS.right = 0.75f;
    f.zRcS.bottom = 1.0f;
    f.zRcS0.left = -1.0f;
    f.zRcS0.top = -2.0f;
    f.zRcS0.right = -3.0f;
    f.zRcS0.bottom = -4.0f;
    f.zColor = 0x11223344u;
    f.zTexID = 5;
    f.zRcS_w_num = 3;
    f.zRcS_h_num = 4;
    f.zTexOnceTime = 40;
    f.zTargetID = 77;
    f.zFrameTsReItem = 17;

    f.aLevel_range.left = 100.0f;
    f.aLevel_range.top = 200.0f;
    f.aLevel_range.right = 300.0f;
    f.aLevel_range.bottom = 400.0f;
    f.aLevel_ts.x = 5.0f;
    f.aLevel_ts.y = 6.0f;
    f.aLevel_ts.sx = 7.0f;
    f.aLevel_ts.sy = 8.0f;
    f.aLevel_ts.r = 0.5f;
    f.aLevel_color = 0xaabbccddu;
    f.aFrame_ts.x = 9.0f;
    f.aFrame_ts.y = 10.0f;
    f.aFrame_ts.sx = 11.0f;
    f.aFrame_ts.sy = 12.0f;
    f.aFrame_ts.r = 1.25f;
    f.aColor = 0x55667788u;
    f.aBlendmode = 2;
#ifdef _GN_SOUND_H_
    f.zSouObj = &GNa::Sound;
    f.zSouStartTime = 55;
    f.zSouEndTime = 66;
#endif
}

void checkAllFields(const GNg::cFRAME_RC &a, const GNg::cFRAME_RC &b) {
    CHECK_EQ(a.zRuntime, b.zRuntime);
    CHECK_EQ(a.zNowtime, b.zNowtime);
    CHECK_EQ(a.zEndtime, b.zEndtime);
    CHECK_EQ(a.zStarttime, b.zStarttime);
    CHECK_EQ(a.zAm_mode, b.zAm_mode);
    CHECK_EQ(a.zAm_s_e, b.zAm_s_e);
    CHECK_EQ(a.zHasTRS, b.zHasTRS);
    CHECK_EQ(a.zPlaying, b.zPlaying);
    CHECK_EQ(a.zSegmentID, b.zSegmentID);
    CHECK_EQ(a.ztAm_mode, b.ztAm_mode);
    CHECK_EQ(a.ztEndtime, b.ztEndtime);
    CHECK_EQ(a.zTargetTime, b.zTargetTime);
    CHECK_EQ(a.zStartRunTime, b.zStartRunTime);

    CHECK_EQ(a.zFrame_ts.x, b.zFrame_ts.x);
    CHECK_EQ(a.zFrame_ts.y, b.zFrame_ts.y);
    CHECK_EQ(a.zFrame_ts.sx, b.zFrame_ts.sx);
    CHECK_EQ(a.zFrame_ts.sy, b.zFrame_ts.sy);
    CHECK_EQ(a.zFrame_ts.r, b.zFrame_ts.r);
    CHECK_EQ(a.zRcLoc.left, b.zRcLoc.left);
    CHECK_EQ(a.zRcLoc.top, b.zRcLoc.top);
    CHECK_EQ(a.zRcLoc.right, b.zRcLoc.right);
    CHECK_EQ(a.zRcLoc.bottom, b.zRcLoc.bottom);
    CHECK_EQ(a.zRcS.left, b.zRcS.left);
    CHECK_EQ(a.zRcS.top, b.zRcS.top);
    CHECK_EQ(a.zRcS.right, b.zRcS.right);
    CHECK_EQ(a.zRcS.bottom, b.zRcS.bottom);
    CHECK_EQ(a.zRcS0.left, b.zRcS0.left);
    CHECK_EQ(a.zRcS0.top, b.zRcS0.top);
    CHECK_EQ(a.zRcS0.right, b.zRcS0.right);
    CHECK_EQ(a.zRcS0.bottom, b.zRcS0.bottom);
    CHECK_EQ(a.zColor, b.zColor);
    CHECK_EQ(a.zTexID, b.zTexID);
    CHECK_EQ(a.zRcS_w_num, b.zRcS_w_num);
    CHECK_EQ(a.zRcS_h_num, b.zRcS_h_num);
    CHECK_EQ(a.zTexOnceTime, b.zTexOnceTime);
    CHECK_EQ(a.zTargetID, b.zTargetID);
    CHECK_EQ(a.zFrameTsReItem, b.zFrameTsReItem);

    CHECK_EQ(a.aLevel_range.left, b.aLevel_range.left);
    CHECK_EQ(a.aLevel_range.top, b.aLevel_range.top);
    CHECK_EQ(a.aLevel_range.right, b.aLevel_range.right);
    CHECK_EQ(a.aLevel_range.bottom, b.aLevel_range.bottom);
    CHECK_EQ(a.aLevel_ts.x, b.aLevel_ts.x);
    CHECK_EQ(a.aLevel_ts.y, b.aLevel_ts.y);
    CHECK_EQ(a.aLevel_ts.sx, b.aLevel_ts.sx);
    CHECK_EQ(a.aLevel_ts.sy, b.aLevel_ts.sy);
    CHECK_EQ(a.aLevel_ts.r, b.aLevel_ts.r);
    CHECK_EQ(a.aLevel_color, b.aLevel_color);
    CHECK_EQ(a.aFrame_ts.x, b.aFrame_ts.x);
    CHECK_EQ(a.aFrame_ts.y, b.aFrame_ts.y);
    CHECK_EQ(a.aFrame_ts.sx, b.aFrame_ts.sx);
    CHECK_EQ(a.aFrame_ts.sy, b.aFrame_ts.sy);
    CHECK_EQ(a.aFrame_ts.r, b.aFrame_ts.r);
    CHECK_EQ(a.aColor, b.aColor);
    CHECK_EQ(a.aBlendmode, b.aBlendmode);
#ifdef _GN_SOUND_H_
    CHECK_EQ(a.zSouObj, b.zSouObj);
    CHECK_EQ(a.zSouStartTime, b.zSouStartTime);
    CHECK_EQ(a.zSouEndTime, b.zSouEndTime);
#endif
}

GNg::sAM_EVENT *makeEvents() {
    GNg::sAM_EVENT *evs = new GNg::sAM_EVENT[2];
    evs[0].Init(GNg::eEVENT_MOVE_X, 10, 20, 1.0f, 2.0f, 0.5f, 1);
    evs[1].Init(GNg::eEVENT_COLOR, 30, 40, 3.0f, 4.0f, 0.1f, 2);
    return evs;
}

} // namespace

// operator= 是逐字段复制：全部成员值相等、外部指针浅共享（不另分配）。
TEST_CASE(frameCopyAssignIsFieldWise) {
    GNg::sAM_EVENT *evs = makeEvents();
    unsigned char *tidx = new unsigned char[3];
    tidx[0] = 1;
    tidx[1] = 2;
    tidx[2] = 3;
    {
        GNg::cFRAME_RC src;
        fillAllFields(src);
        src.zFEvent = evs;
        src.zEventcount = 2;
        src.zTexIndex = tidx;
        src.zTexIndexCount = 3;
        src.zPlay_obj = &GNg::BackScreen128; // 与默认 &Image 不同，用于验证该字段确实被复制
        src.zNodes.New(4);

        GNg::cFRAME_RC dst;
        dst = src;
        checkAllFields(dst, src);
        CHECK(dst.zFEvent == evs); // 浅共享，不复制数组
        CHECK(dst.zTexIndex == tidx);
        CHECK(dst.zPlay_obj == &GNg::BackScreen128);
        CHECK(dst.zNodes.zData == src.zNodes.zData); // 安全指针共享引用计数
        CHECK_EQ(dst.zNodes.Us(), 2u);
    }
    delete[] evs;
    delete[] tidx;
}

// 自赋值不得清空自身（原 memcpy 实现会先把自身 Release 掉再自我覆盖）。
TEST_CASE(frameCopyAssignSelfIsSafe) {
    GNg::sAM_EVENT *evs = makeEvents();
    GNg::cFRAME_RC f;
    fillAllFields(f);
    f.zFEvent = evs;
    f.zEventcount = 2;
    f = f;
    checkAllFields(f, f);
    CHECK(f.zFEvent == evs);
    CHECK_EQ(f.zEventcount, 2u);
    delete[] evs;
}

// CopyData 是深复制：事件/纹理索引/子筐各自新分配，内容相等但指针不同；拷贝体拥有资源。
TEST_CASE(frameCopyDataDeepCopies) {
    GNg::sAM_EVENT *evs = makeEvents();
    unsigned char *tidx = new unsigned char[3];
    tidx[0] = 7;
    tidx[1] = 8;
    tidx[2] = 9;
    GNg::cFRAME_RC *subs = new GNg::cFRAME_RC[2];
    subs[0].zRcLoc.right = 64.0f;
    subs[1].zRcLoc.right = 96.0f;
    {
        GNg::cFRAME_RC src;
        fillAllFields(src);
        src.zFEvent = evs;
        src.zEventcount = 2;
        src.zTexIndex = tidx;
        src.zTexIndexCount = 3;
        src.InitSubFrame(subs, 2);

        GNg::cFRAME_RC dst;
        dst.CopyData(src);

        checkAllFields(dst, src);
        CHECK(dst.zFEvent != src.zFEvent);
        CHECK(dst.zFEvent != 0);
        CHECK_EQ(dst.zEventcount, 2u);
        CHECK_EQ(dst.zFEvent[0].event_mode, evs[0].event_mode);
        CHECK_EQ(dst.zFEvent[1].start_time, evs[1].start_time);
        CHECK(dst.zTexIndex != src.zTexIndex);
        CHECK_EQ(dst.zTexIndex[0], (unsigned char)7);
        CHECK(dst.zSubFrame != src.zSubFrame);
        CHECK_EQ(dst.zSubCount, (unsigned short)2);
        CHECK_EQ(dst.SubFrame(0).zRcLoc.right, (float)64.0f);
        CHECK_EQ(dst.SubFrame(1).zRcLoc.right, (float)96.0f);
        CHECK_EQ(dst.zObjsKeep.Us(), 1u); // 拷贝体拥有自己的一份资源
    }
    delete[] evs;
    delete[] tidx;
    delete[] subs;
}

// CopyData 用于自身时不得破坏数据。
TEST_CASE(frameCopyDataSelfIsSafe) {
    GNg::sAM_EVENT *evs = makeEvents();
    GNg::cFRAME_RC f;
    fillAllFields(f);
    f.zFEvent = evs;
    f.zEventcount = 2;
    f.CopyData(f);
    checkAllFields(f, f);
    CHECK(f.zFEvent == evs); // 早退，不重分配
    delete[] evs;
}
