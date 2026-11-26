// testScrollListRegistration.cpp — 滚动列表绘制回调注册签名回归（P0.2 / DA-1126-15）。
//
// 旧实现的分派看 zUsArg 是否非空，而不是回调注册时的签名：
//   * InitStyleToData（8 参）不清 zUsArg ⇒ 9 参注册留下的用户参数会让 8 参回调被按 9 参调用；
//   * SetZoningDrawCall(fn9, 0) ⇒ zUsArg 为空，9 参回调被按 8 参调用（尾参是垃圾值）。
// 修复：按注册签名标志 zDrawCallHasUsArg 分派。这里只验状态迁移（纯 CPU，不跑绘制路径）。
#include "testFramework.h"

#include <GN.h>

namespace {

void draw8(int, int, int, int, unsigned int, unsigned int, unsigned int, bool) {}
void draw9(int, int, int, int, unsigned int, unsigned int, unsigned int, bool, void *) {}

// zUsArg/zDrawCallHasUsArg 是 protected：测试经派生类 using 暴露，不改生产代码可见性。
struct sTEST_SCROLL : public GNu::cSCROLL_LIST {
    using GNu::cSCROLL_LIST::zUsArg;
    using GNu::cSCROLL_LIST::zDrawCallHasUsArg;
};

} // namespace

TEST_CASE(scrollListDrawCallSignatureFlag) {
    sTEST_SCROLL list;
    int marker = 0;
    list.SetZoningDrawCall(draw9, &marker);
    CHECK(list.zDrawCallHasUsArg);
    CHECK(list.zUsArg == &marker);
    // 换 8 参注册：必须清掉用户参数与签名标志（旧实现残留 zUsArg ⇒ 错签名分派）
    list.InitStyleToData(draw8, 0, 0, 100, 100, false, 10, 10, 0, 0xffffffff, 0);
    CHECK(!list.zDrawCallHasUsArg);
    CHECK(list.zUsArg == 0);
    // usarg==0 的 9 参注册：仍必须按 9 参分派（旧实现看 zUsArg 会按 8 参调）
    list.SetZoningDrawCall(draw9, 0);
    CHECK(list.zDrawCallHasUsArg);
}
