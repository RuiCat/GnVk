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
#ifdef _GN_INIT_H
/********************************************************* 动态库模块 **************************************************************************/
class GN_cMODULE //动态库程度模块
{
    int *zModule; //模块

  public:
    inline GN_cMODULE() : zModule(0) {}
    inline ~GN_cMODULE() {
        if(zModule) {
            GT_ModuleClose(zModule);
            zModule = 0;
        }
    }

    //初始化装载一个程序模块(模块名)返回是否成功
    bool InitToData(const char *mod_name);

    //从模块里获取一个函数( 函数名 ) 失败返回0, 成功返回该函数指针
    GT_PFUNC_VOID GetFunction(const char *func_name);
};
bool GN_cMODULE::InitToData(const char *mod_name) {
    if(zModule) GT_ModuleClose(zModule);
    zModule = GT_ModuleOpen(mod_name);
    return zModule ? true : false;
}
GT_PFUNC_VOID GN_cMODULE::GetFunction(const char *func_name) {
    if(zModule) return GT_ModuleGetFunction(zModule, func_name);
    else return 0;
}
P<char> GT_UserModuleFileName; //用户程序模块路径文件名 不包含.dll等
P<GN_cMODULE> GT_UserModule; //用户程序模块

/************************************************************* 特殊目录与文件 ****************************************************************************/
char GT_SysDir[384] = "assets/";
char *GT_SysFile = GT_SysDir + 7;

GN_INIT_API char *const GN_SysDir(const char *pathfile) {
    if(pathfile) {
        strncpy(GT_SysFile, pathfile, sizeof(GT_SysDir) - (GT_SysFile - GT_SysDir) - 1);
        GT_SysFile[sizeof(GT_SysDir) - (GT_SysFile - GT_SysDir) - 1] = 0;
    }
    return (char *const)GT_SysDir;
}
/*------------------------------------------------------------ 用户应用资源目录 -------------------------------------------------------------------------*/
char GT_AppDir[384] = "\0";
char *GT_AppFile = GT_AppDir;

GN_INIT_API char *const GN_AppDir(const char *pathfile) {
    if(pathfile) {
        strncpy(GT_AppFile, pathfile, sizeof(GT_AppDir) - 1);
        GT_AppFile[sizeof(GT_AppDir) - 1] = 0;
    } else *GT_AppFile = 0;
    return (char *const)GT_AppDir;
}
GN_INIT_API void GN_AppDirSet(const char *dir) {
    if(dir && dir[0]) GT_AppFile = sprintf(GT_AppDir, "%s/", dir) + GT_AppDir;
    else {
        GT_AppDir[0] = 0;
        GT_AppFile = GT_AppDir;
    }
}
GN_INIT_API void GN_AppDirGet(char *dir_out) {
    if(GT_AppFile == GT_AppDir) *dir_out = 0;
    else {
        char *_end = GT_AppFile - 1;
        *_end = 0; //临时断开文件名
        strncpy(dir_out, GT_AppDir, 383);
        dir_out[383] = 0;
        *_end = '/';
    }
}
/************************************************************** 需提前定义的一些东西 *************************************************************************/
static void GT_UnInitAll();
void GT_FuncEmpty(unsigned int nTime) {}
void GT_FuncEmpty2(void) {}

//内部主回调函数指针
void (*GT_pFuncLoad)(void) = GT_FuncEmpty2;
void (*GT_pFuncExit)(void) = GT_FuncEmpty2;
void (*GT_pFuncDisplay2D)(unsigned int) = GT_FuncEmpty;
void (*GT_pFuncDisplay3D)(unsigned int) = GT_FuncEmpty;
void (*GT_pFuncRun)(unsigned int) = GT_FuncEmpty;
void (*GT_pFuncPreFrame)(unsigned int) = GT_FuncEmpty; // pass前回调(Vulkan安全init)

/********************************************************* 显示错误原因来选择修复 *****************************************************************************/
//开始运行应用回调 一并加载用户资源
/*static*/ void GT_RunAppCallBackFlush() {
    GT_LastRunTotalTimeApp = 1;
    GT_RunTotalTimeApp = 1;
    GT_RunTotalFpsApp = 0;
    GN::ViewReSized = true;
    GT_pFuncLoad(); // >>> 开始加载用户资源
}
//开始尝试运行应用回调处理
static void GT_RunAppCallBackTry() {
    if(GN_CallBackFuncIsNull()) return;
    GT_RunAppCallBackFlush();
}
GN_INIT_API unsigned short GT_AppDefault_ExtModule = 0;
GN_INIT_API uint8 GT_AppDefault_MsgCode = 0;
GN_INIT_API uint8 GT_AppDefaultFontStyle = 1;
GN_INIT_API unsigned int GT_AppDefaultColorFont = 0xffffffff;
GN_INIT_API unsigned int GT_AppDefaultColorBack = 0x88000000;
GN_INIT_API unsigned int GT_AppDefaultColorFocus = 0xffaf4f00;

GN_INIT_API P<char> GT_AppDefaultSrcDownloadURL;
GN_INIT_API P<char> GT_AppDefaultSrcDownloadDir;

/*static*/ void GT_SystemDownloadDirSafeSet(const char *dir) {
    if(dir && dir[0] && dir[0] != '/' && dir[0] != '\\' && !strstr(dir, "../") && !GN_StrIsGlobalPath(dir)) GT_AppDefaultSrcDownloadDir(dir, 0);
    else GT_AppDefaultSrcDownloadDir.Delete();
}
GN_INIT_API void (*GT_RunBreakLoopProc)() = 0; //指向在程序刚退出循环时被调用的函数
GN_INIT_API void (*GT_RunEndProc)() = 0; //指向在程序运行到最后时被调用的函数

GN_INIT_API bool GN_CallBackFuncIsNull() {
    return (GT_pFuncDisplay2D == GT_FuncEmpty && GT_pFuncDisplay3D == GT_FuncEmpty && GT_pFuncRun == GT_FuncEmpty && GT_pFuncLoad == GT_FuncEmpty2 && GT_pFuncExit == GT_FuncEmpty2);
}
GN_INIT_API void GN_CallBackLoadFuncIsNull(bool *nulls /*[2]*/) {
    nulls[0] = GT_pFuncLoad == GT_FuncEmpty2;
    nulls[1] = GT_pFuncExit == GT_FuncEmpty2;
}
GN_INIT_API void GN_CallBackLoopFuncIsNull(bool *nulls /*[3]*/) {
    nulls[0] = GT_pFuncDisplay2D == GT_FuncEmpty;
    nulls[1] = GT_pFuncDisplay3D == GT_FuncEmpty;
    nulls[2] = GT_pFuncRun == GT_FuncEmpty;
}
/************************************************************* 用户应用程序的回调方法 *********************************************************************************/
GN_INIT_API void GN_InitLoadCallBackFunc(void (*load_func)(), void (*exit_func)()) {
    GT_pFuncLoad = load_func ? load_func : GT_FuncEmpty2;
    GT_pFuncExit = exit_func ? exit_func : GT_FuncEmpty2;
}
GN_INIT_API void GN_InitLoopCallBackFunc(void (*display2D_func)(unsigned int), void (*display3D_func)(unsigned int), void (*run_func)(unsigned int)) {
    GT_pFuncDisplay2D = display2D_func ? display2D_func : GT_FuncEmpty;
    GT_pFuncDisplay3D = display3D_func ? display3D_func : GT_FuncEmpty;
    GT_pFuncRun = run_func ? run_func : GT_FuncEmpty;
}
GN_INIT_API bool GN_LoadCallBackFuncFromModule(const char *mod_name, GT_PFUNC_VOID &load_func, GT_PFUNC_VOID &exit_func, GT_VOID_PFUNC_UINT &display2D_func, GT_VOID_PFUNC_UINT &display3D_func, GT_VOID_PFUNC_UINT &run_func) {
    char md_name[384];
    strncpy(md_name, mod_name, sizeof(md_name) - 1);
    md_name[sizeof(md_name) - 1] = 0;
    strncat(md_name, GN_StrGetModuleExtName(GN_eMODULE_DLL), sizeof(md_name) - strlen(md_name) - 1);
    {
        GT_UserModule.New(1);
        GN_cMODULE *_md = GT_UserModule;
        if(_md->InitToData(md_name)) { //加载用户应用程序动态库
            GT_UserModuleFileName(mod_name, 0);
            load_func = (GT_PFUNC_VOID)_md->GetFunction("AppInit");
            exit_func = (GT_PFUNC_VOID)_md->GetFunction("AppUnInit");
            display2D_func = (GT_VOID_PFUNC_UINT)_md->GetFunction("AppDisplay2D");
            display3D_func = (GT_VOID_PFUNC_UINT)_md->GetFunction("AppDisplay3D");
            run_func = (GT_VOID_PFUNC_UINT)_md->GetFunction("AppRun");
            return true;
        }
    }
    GN_CallBackFuncModuleRelease();
    return false;
}
GN_INIT_API bool GN_InitCallBackFuncFromModule(const char *mod_name) {
    GT_PFUNC_VOID pf_init, pf_uninit;
    GT_VOID_PFUNC_UINT pf_disp2D, pf_disp3D, pf_run;
    if(GN_LoadCallBackFuncFromModule(mod_name, pf_init, pf_uninit, pf_disp2D, pf_disp3D, pf_run)) {
        GN_InitLoadCallBackFunc(pf_init, pf_uninit);
        GN_InitLoopCallBackFunc(pf_disp2D, pf_disp3D, pf_run);
        return true;
    }
    return false;
}
GN_INIT_API void GN_CallBackFuncModuleRelease() {
    GT_UserModuleFileName.Delete();
    GT_UserModule.Delete();
}
/*************************************************************** 供循环框架调用的处理 ***********************************************************************************************************/
GN_INIT_API void GF_FrameCycleProc1(unsigned int nTime) {
    GT_pFuncPreFrame(nTime); // pass前回调(Vulkan: 在此安全init GPU资源)
}

GN_INIT_API void GF_FrameCycleProc2(unsigned int nTime) {
    if(!GNg::GN_VkBeginFrame()) return; // Vulkan: start frame, skip rendering on failure
    GN_SysProc(nTime);
    GN_InputProc(nTime); //更新输入处理和一些杂务
    GN_TaskIdleProc(nTime, false);

#ifdef _GN_NETTRANS_H_
    GN_NetworkProc(nTime);
#endif

#ifdef _GN_SOUND_H_
#ifdef _GN_AUDIO_H_
    GN_SoundPlayBegin2D(); //准备播放2D音效
#else
    GT_ReSetSoundMode2D();
#endif
#endif

    GT_pFuncRun(nTime); //▲▲▲运行用户后台处理

#ifdef _GN_2D_H_
    GN_RenderBegin2D(); //准备绘制2D内容
    GT_pFuncDisplay2D(nTime); //▲▲▲显示用户2D内容

#ifdef _GN_UI_H_
    GNu::cUI_SCENE::zDrawFinishProc();
#endif
#ifdef _GN_RUNINFO_H_
    GN::RunRec.Show(nTime); //显示运行信息记录
#endif
    GNg::RenderFinish();
#endif
#ifdef _GN_PARALLEL_H_
    GN::Parallel.TaskListClear();
#endif
}
GN_INIT_API void GF_Sleep(unsigned int ms) {
#ifdef _GN_PARALLEL_H_
    GN::Parallel.Sleep(ms);
#endif
    GN_TimeSleep(ms);
}
GN_INIT_API void GF_FrameCycleBeginProc() {
#ifdef _GN_MAIN_H_
    if(!(GT_SystemInited & (1 << 2))) { //有循环框架时最少也要确保可以使用图形设备
        int *_hwnd0;
        GN_InitWindow(&_hwnd0);
        GN_InitGraphics(_hwnd0);
    }
#endif
    GN_AppDirSet(GT_AppDefaultSrcDownloadDir); //设置用户应用资源下载目录

#ifdef _GN_RUNINFO_H_
    GN_RunErrorOutputOn = false;
    GN::RunRec.InitToData();
    GN_RunErrorOutputOn = true;
#endif
    {
        if(GT_AppDefaultSrcDownloadDir && GN_CallBackFuncIsNull()) {
            char mou_fname[384]; //有指定用户下载目录且没有手动设置回调就常试用默认名来加载程序模块来获取回调
            sprintf(mou_fname, "%s/%s", GT_AppDefaultSrcDownloadDir.Begin(), GN_APP_MODULE_NAME);
            bool init_func_err = !GN_InitCallBackFuncFromModule(mou_fname);
        }
    }
    GNg::sIMAGE start_logo;
    if(GNg::ImageLoad("TempProject/stplogo.png", 1, &start_logo)) { //加载开机LOGO
        GNg::ImageDraw(&start_logo, GNu::NearCx(-start_logo.width >> 1), GNu::NearCy(-start_logo.height >> 1), start_logo.width, start_logo.height);
        GNg::RenderFinish();
        GN_ScreenUpData(); //手动把屏幕后台绘制内容提交给前台显示
        GN_TimeSleep(5000); //显示5秒
    }
    GT_RunAppCallBackTry(); //开始去尝试运行应用回调
}
GN_INIT_API void GF_FrameCycleEndProc() {
    if(GT_RunBreakLoopProc) GT_RunBreakLoopProc();
    GT_pFuncExit(); //▲▲▲释放用户资源

#ifdef _GN_RUNINFO_H_
    GN::RunRec.Release(); //释放运行信息记录相关资源
#endif
    GN_CallBackFuncModuleRelease();
    GT_AppDefaultSrcDownloadURL.Delete();
    GT_AppDefaultSrcDownloadDir.Delete();

    GT_UnInitAll(); //释放承担引擎运行的所有设备资源
    GN_ExecArgs.Delete();
    if(GT_RunEndProc) GT_RunEndProc(); //最后回调结束的一个处理
}
/************************************************************** 初始化参数 *******************************************************************/
void GN_sINIT::InitDefault() {
    vieww = 800;
    viewh = 600;
    colorbit = 0;
    rangeleft = 0;
    rangetop = 0;
    rangeright = 0;
    rangebottom = 0;
    depthbuffer = 1;
    msbuffer = 1;
    zoom = 1;
    full = 0;
    showmouse = 1;
    extmodule = 0;
    msgcode = 0;
    fontstyle = 1;
    fontcolor = 0xffffffff;
    focuscolor = 0xffaf4f00;
    backcolor = 0x88000000;

    src[0] = 0;
    caption[0] = 0;
}
bool GN_sINIT::InitLoadFromFile(const char *ini_filename) {
    int content_size;
    char *cnt = GNf::FileFullLoadToMemoryText((char *)ini_filename, &content_size);
    if(cnt) {
        int v;
        //只有独立应用程序才需配置的参数 ------------------------------------------
        caption[0] = 0;
        GN_StrStringRead(cnt, caption, "caption");
        v = 800;
        GN_StrValueRead(cnt, &v, "vieww");
        vieww = v;
        v = 600;
        GN_StrValueRead(cnt, &v, "viewh");
        viewh = v;
        v = 0;
        GN_StrValueRead(cnt, &v, "colorbit");
        colorbit = v;
        v = 1;
        GN_StrValueRead(cnt, &v, "depthbuffer");
        depthbuffer = v ? true : false;
        v = 1;
        GN_StrValueRead(cnt, &v, "msbuffer");
        msbuffer = v;

        //以下是通用配置的参数 ----------------------------------------------------
        v = 0;
        GN_StrValueRead(cnt, &v, "rangeleft");
        rangeleft = v;
        v = 0;
        GN_StrValueRead(cnt, &v, "rangetop");
        rangetop = v;
        v = 0;
        GN_StrValueRead(cnt, &v, "rangeright");
        rangeright = v;
        v = 0;
        GN_StrValueRead(cnt, &v, "rangebottom");
        rangebottom = v;

        v = 0;
        GN_StrValueRead(cnt, &v, "full");
        full = v ? true : false;
        v = 1;
        GN_StrValueRead(cnt, &v, "zoom");
        zoom = v ? true : false;
        v = 1;
        GN_StrValueRead(cnt, &v, "showmouse");
        showmouse = v ? true : false;

        v = 1;
        GN_StrValueRead(cnt, &v, "fontstyle");
        fontstyle = v;
        unsigned int v1;
        v1 = 0xffffffff;
        GN_StrValueRead(cnt, "%x", &v1, "fontcolor");
        fontcolor = v1;
        v1 = 0x88000000;
        GN_StrValueRead(cnt, "%x", &v1, "backcolor");
        backcolor = v1;
        v1 = 0xffaf4f00;
        GN_StrValueRead(cnt, "%x", &v1, "focuscolor");
        focuscolor = v1;

        v = 0;
        GN_StrValueRead(cnt, &v, "msgcode");
        msgcode = v;
        v = 0;
        GN_StrValueRead(cnt, &v, "extmodule");
        extmodule = v;

        src[0] = 0;
        GN_StrStringRead(cnt, src, "src"); //读取资源连接
        if(src[0]) { //有指定资源连接
            v = strlen(src) - 1;
            if(src[v] == '@') src[v] = 0; //去除连接尾部的@符号
        }
        delete[] cnt;
        return true;
    }
    return false;
}
/************************************************************* 方便的引擎系统初始化方法 *******************************************************/
#ifdef GN_USING_UTILITY_LIB
GN_INIT_API void GN_InitParameDefaultStyle(unsigned char fontstyle, unsigned int fontcolor, unsigned int backcolor, unsigned int focuscolor) {
    GT_AppDefaultFontStyle = fontstyle;
    GT_AppDefaultColorFont = fontcolor;
    GT_AppDefaultColorBack = backcolor;
    GT_AppDefaultColorFocus = focuscolor;
}
#endif
GN_INIT_API void GN_InitParameExtSrc(const char *src, const char *tg_dir, unsigned short ext_module, unsigned char msg_code) {
    if(src && src[0]) GT_AppDefaultSrcDownloadURL(src, 0);
    else GT_AppDefaultSrcDownloadURL.Delete();
    GT_SystemDownloadDirSafeSet(tg_dir);
    GT_AppDefault_ExtModule = ext_module;
    GT_AppDefault_MsgCode = msg_code;
}
static void GT_UnInitAll() {
#ifdef _GN_MAIN_H_
#ifdef _GN_SOUND_H_
#ifdef _GN_AUDIO_H_
    GN_UnInitSound(); //释放声音设备资源
#else
    GT_UnInitSound();
#endif
#endif

#ifdef _GN_NET_H_
#ifdef _GN_NETTRANS_H_
    GN_UnInitNetwork(); //释放网络设备资源
#else
    GT_UnInitNetwork();
#endif
#endif
    GN_TaskIdleClearAll();
    GN_UnInitInput(); //释放输入设备资源

    GN_UnInitGraphics(); //释放图形设备资源

    if(GT_SystemInited & (1 << 0)) GN_UnInitWindow(GNg::sWINDOW::Hwnd); //释放并销毁窗口

#ifdef _GN_SYS_H_
    GN_UnInitSys(); //释放系统其它设备资源
#endif

#if defined(GN_DEBUG) && defined(_GN_RUNINFO_H_)
    GN_ExitInfoPrint(); //打印一些退出还没处理的信息 例如沉余内存等
#else
    GN_ErrorInfoClear();
#endif
    //GN_Words.Delete();
    GT_SystemInited -= 1 << 31;
#endif
}
static void GT_InitAllAndRun(const int *window, short view_w, short view_h, short color_bit, short back_src_left, short back_src_top, short back_src_right, short back_src_bottom, bool full_screen, bool window_zoom, bool dp_buf, uint8 ms_buf) {
    dp_buf = 0;
    ms_buf = 0; //2D不使用深度和多重采样缓冲
#ifdef _GN_MAIN_H_
#ifdef _GN_SYS_H_
    GN_InitSys(); //初始化系统其它设备接口
#endif
    GN_SetAppKey(0x77777777);
    GN_InitWords();
    GN_InitGraphics(window, view_w, view_h, color_bit, back_src_left, back_src_top, back_src_right, back_src_bottom, full_screen, window_zoom, dp_buf, ms_buf);
    GN_InitInput();

#ifdef _GN_NET_H_
#ifdef _GN_NETTRANS_H_
    GN_InitNetwork(); //初始化网络资源接口
#else
    GT_InitNetwork();
#endif
#endif

#ifdef _GN_SOUND_H_
#ifdef _GN_AUDIO_H_
    GN_InitSound(); //初始化声音设备资源
#else
    GT_InitSound();
#endif
#endif
    GT_SystemInited |= 1 << 31;
    GN_GameLoop(true); // >>> 进入循环
#endif
}
#ifdef GT_USING_GLUT
extern GN_MAIN_API bool GT_InitArgDpBuf;
extern GN_MAIN_API uint8 GT_InitArgMsBuf;
#endif
GN_INIT_API void GN_StartRun(const char *caption, short view_w, short view_h, short color_bit, bool full_screen, bool window_zoom, bool cursor_show, short back_src_left, short back_src_top, short back_src_right, short back_src_bottom, bool dp_buf, uint8 ms_buf) {
    // Vulkan: 直接创建窗口并运行主循环
    int *hwnd = 0;
    GN_InitWindow(&hwnd, caption, view_w, view_h, full_screen, window_zoom, cursor_show);
    GT_InitAllAndRun(hwnd, view_w, view_h, color_bit,
                     back_src_left, back_src_top, back_src_right, back_src_bottom,
                     full_screen, window_zoom, dp_buf, ms_buf);
}
GN_INIT_API void GN_StartRunFromWindow(const GN_sINIT *_init_arg, const char *tg_dir, const int *hwnd) {
    if(GT_SystemInited & (1 << 31)) return;
    GN_sINIT init_arg;
    memcpy(&init_arg, _init_arg, sizeof(GN_sINIT));

    if(init_arg.src[0]) GT_AppDefaultSrcDownloadURL(init_arg.src, 0);
    else GT_AppDefaultSrcDownloadURL.Delete();
    GT_SystemDownloadDirSafeSet(tg_dir);

    GT_AppDefault_ExtModule = init_arg.extmodule;
    GT_AppDefault_MsgCode = init_arg.msgcode;

    GT_AppDefaultFontStyle = init_arg.fontstyle;
    GT_AppDefaultColorFont = init_arg.fontcolor;
    GT_AppDefaultColorFocus = init_arg.focuscolor;
    GT_AppDefaultColorBack = init_arg.backcolor;

    int *window;
    if(hwnd) { //调整一下现有窗口
        window = (int *)hwnd;
        if(!init_arg.rangeright) init_arg.rangeright = init_arg.rangeleft + init_arg.vieww;
        if(!init_arg.rangebottom) init_arg.rangebottom = init_arg.rangetop + init_arg.viewh;
        if(!init_arg.zoom) {
            init_arg.rangeright = init_arg.rangeleft + init_arg.vieww;
            init_arg.rangebottom = init_arg.rangetop + init_arg.viewh;
        } else init_arg.zoom = 0;
    } else { //需创建新窗口
#ifdef GT_USING_GLUT
        GT_InitArgDpBuf = init_arg.depthbuffer;
        GT_InitArgMsBuf = init_arg.msbuffer;
#endif
        GN_InitWindow(&window, init_arg.caption[0] ? init_arg.caption : 0, init_arg.vieww, init_arg.viewh, init_arg.full, init_arg.zoom, init_arg.showmouse);
    }
    GT_InitAllAndRun(window, init_arg.vieww, init_arg.viewh, init_arg.colorbit,
                     init_arg.rangeleft, init_arg.rangetop, init_arg.rangeright, init_arg.rangebottom,
                     init_arg.full, init_arg.zoom, init_arg.depthbuffer, init_arg.msbuffer);
}
GN_INIT_API void GN_StartRunFromFile(const char *ini_filename) {
    GN_sINIT init_arg;
    char *app_dir = 0;
    if(init_arg.InitLoadFromFile(ini_filename)) { //获得ini文件所在目录位置
        char *ini_fename = GN_StrGetFileName(ini_filename);
        if(ini_fename > ini_filename) { //必需要有另外的用户应用资源目录 不能为引擎主目录
            unsigned int fnlen = ini_fename - ini_filename;
            app_dir = (char *)malloc(fnlen--);
            strncpy(app_dir, ini_filename, fnlen);
            app_dir[fnlen] = 0;
        }
    } else init_arg.InitDefault();

    GN_InitParameDefaultStyle(init_arg.fontstyle, init_arg.fontcolor, init_arg.backcolor, init_arg.focuscolor);
    GN_InitParameExtSrc(init_arg.src, app_dir, init_arg.extmodule, init_arg.msgcode);
    GN_MemFree(app_dir); //用完释放临时空间

    GN_StartRun(init_arg.caption[0] ? init_arg.caption : 0, init_arg.vieww, init_arg.viewh, init_arg.colorbit, init_arg.full, init_arg.zoom, init_arg.showmouse, init_arg.rangeleft, init_arg.rangetop, init_arg.rangeright, init_arg.rangebottom, init_arg.depthbuffer, init_arg.msbuffer);
}
/* --------------------------------------------------------------- 退出程序运行 -----------------------------------------------------------------------------------------------------*/
#ifdef _GN_MAIN_H_
void (*GT_GameBreakLoopProcB)() = 0;
void (*GT_GameBreakLoopProcB2)() = 0;
void (*GT_pFuncBreakRunOtherProc)() = 0;

GN_INIT_API void GN_BreakRun() {
    if(GT_pFuncBreakRunOtherProc) GT_pFuncBreakRunOtherProc();
    else GN_GameBreakLoop();
}
void GT_BreakRunBeforeChangeProc() {
    if(GT_GameBreakLoopProcB2) GT_GameBreakLoopProcB2();
    else GT_pFuncBreakRunOtherProc();
}
GN_INIT_API void GN_BreakRunBeforeProcSet(void (*proc)()) {
    if(GT_GameBreakLoopProcB) GT_GameBreakLoopProcB2 = proc;
    else GN_GameBreakLoopProc = proc;
}
GN_INIT_API bool GN_BreakRunProcChange(void (*other_proc)()) {
    if(other_proc) {
        if(GN_GameBreakLoopProc != GT_BreakRunBeforeChangeProc) {
            GT_GameBreakLoopProcB = GN_GameBreakLoopProc;
            GN_GameBreakLoopProc = GT_BreakRunBeforeChangeProc; //将程序退出前处理改为这里预设的跳转处理
            GT_pFuncBreakRunOtherProc = other_proc;
            return true;
        }
    } else {
        if(GN_GameBreakLoopProc == GT_BreakRunBeforeChangeProc) {
            GN_GameBreakLoopProc = GT_GameBreakLoopProcB; //恢复之前程序退出前的处理函数
            GT_GameBreakLoopProcB = 0;
            GT_GameBreakLoopProcB2 = 0;
            GT_pFuncBreakRunOtherProc = 0;
            return true;
        }
    }
    return false;
}
#else
GN_INIT_API void GN_BreakRun() {}
GN_INIT_API void GN_BreakRunBeforeProcSet(void (*break_proc)()) {}
GN_INIT_API void GN_BreakRunProcChange(void (*other_proc)()) {}
#endif
GN_INIT_API P<P<char>> GN_ExecArgs;

GN_INIT_API void GF_AppArgsSet(const int *app, int argc, char **argv, const char *cmdstr) {
    GNg::sWINDOW::Hinst = (int *)app;
    if(cmdstr) {
        if(*cmdstr) GN_ExecArgs = GN_StrSplitToData(cmdstr);
    } else if(argc && argv) {
        GN_ExecArgs.New(argc);
        int ari = 0;
        for(; ari < argc; ++ari) (GN_ExecArgs[ari])(argv[ari], 0);
    }
}
#endif