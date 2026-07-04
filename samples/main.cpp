// GN_mini 测试程序通用入口
// 编译: make test N=01
// 或: g++ -DTEST_01=1 ... samples/main.cpp -o test

#include <GN.h>

// ── 选择测试 (编译时指定 -DTEST_01 等) ──────
#if defined(TEST_01)
#include "01_hello_vulkan.cpp"
#elif defined(TEST_02)
#include "02_texture2d.cpp"
#elif defined(TEST_03)
#include "03_texture_atlas.cpp"
#elif defined(TEST_04)
#include "04_font.cpp"
#elif defined(TEST_05)
#include "05_input.cpp"
#elif defined(TEST_06)
#include "06_particle.cpp"
#elif defined(TEST_07)
#include "07_scroll.cpp"
#elif defined(TEST_08)
#include "08_cube3d.cpp"
#elif defined(TEST_09)
#include "09_model3d.cpp"
#elif defined(TEST_10)
#include "10_scene3d.cpp"
#elif defined(TEST_11)
#include "11_audio.cpp"
#elif defined(TEST_12)
#include "12_video.cpp"
#elif defined(TEST_13)
#include "13_button.cpp"
#elif defined(TEST_14)
#include "14_list.cpp"
#elif defined(TEST_15)
#include "15_dialog.cpp"
#elif defined(TEST_16)
#include "16_scene.cpp"
#else
// 默认: 显示可用测试列表
extern "C" {
GN_CALL void AppInit() {}
GN_CALL void AppUnInit() {}
GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    ClearScreen(TEST_GRAY);
    GN_RunTextOut("GN_mini Test Suite", 0, Vcx()-100, Vcy()-80, 0,0, TEST_WHITE, GNg::eBLEND_NORMAL);
    GN_RunTextOut("compile with: make test N=01", 0, Vcx()-120, Vcy()-30, 0,0, TEST_YELLOW, GNg::eBLEND_NORMAL);
    GN_RunTextOut("test 01-04: 2D rendering", 0, Vcx()-100, Vcy()+10, 0,0, TEST_CYAN, GNg::eBLEND_NORMAL);
    GN_RunTextOut("test 05-07: input & effects", 0, Vcx()-100, Vcy()+30, 0,0, TEST_CYAN, GNg::eBLEND_NORMAL);
    GN_RunTextOut("test 08-10: 3D rendering", 0, Vcx()-100, Vcy()+50, 0,0, TEST_CYAN, GNg::eBLEND_NORMAL);
    GN_RunTextOut("test 11-12: media", 0, Vcx()-100, Vcy()+70, 0,0, TEST_CYAN, GNg::eBLEND_NORMAL);
    GN_RunTextOut("test 13-16: UI components", 0, Vcx()-100, Vcy()+90, 0,0, TEST_CYAN, GNg::eBLEND_NORMAL);
}
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
#endif

// ── 引擎入口 ───────────────────────────────
GN_MAIN;
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
GN_StartRun("GN Test", 800, 600, 0, false, true, true, 0,0,0,0);
GN_EXIT;
