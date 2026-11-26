// GN_UiBrowserContext.h — ui 文件浏览子系统上下文（B7-4，c4 状态收敛，2026-11-27）。
//
// 文件浏览器进程级状态（DA-M2 批 2a 的 sUI_FB_CONTEXT + 文件级 GT_FileBrowserLast）收敛进一个上下文，
// 由 sEngineContext 持有（GN_GetEngineContext().ui）；老 API 经全局 GN_UiBrowserState 工作，行为不变。
// 预览缩图上下文（sUI_FB_PREVIEW_CONTEXT）在 GN_USING_FILE_PREVIEW 关闭时不参与编译，未随本批迁移（B7-5）。
#ifndef _GN_UIBROWSER_CONTEXT_H_
#define _GN_UIBROWSER_CONTEXT_H_

#include <GN.h> // P<> / GNg::sIMAGE / GNg::cFRAME / GNg::sRECT / GNg::COLOR_FORMAT

namespace GNu { class cFILE_BROWSER; } // 定义在 ui/GN_UiFBrowser.h；指针成员只需前置声明

namespace GNg {

struct GN_UiBrowserContext {
    P<GNg::sIMAGE> aDefIconImg;
    uint8 *aIconBackData = 0;
    uint8 *aIconFrameData = 0;
    GNg::COLOR_FORMAT aIconBackCfmt;
    GNg::COLOR_FORMAT aIconFrameCfmt;
    int aIconBackW = -1;
    int aIconBackH;
    int aIconFrameX;
    int aIconFrameY;
    int aIconFrameW;
    int aIconFrameH;
    GNg::sRECT aPicInIconRange;
    bool aShowPreview = false;
    P<GNg::cFRAME> aDefFrameAct;
    P<P<char>> aExtNameTable;
    GNu::cFILE_BROWSER *aLastFileBrowser = 0; // 原 GT_FileBrowserLast：文件批处理的目标浏览器
};

extern GN_UiBrowserContext GN_UiBrowserState; // 默认上下文（仿 GN_Vk / GN_Core）

} // namespace GNg

#endif // _GN_UIBROWSER_CONTEXT_H_
