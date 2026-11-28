// GN_mini 平台实现片段：进程枚举/核心数/剪贴板/应用当前路径。
// 2026-11-26 自 GN_Sys.cpp 第 698-964 行 move-only 拆出（REFACTOR_PLAN §13 B6）。
// 由 GN_Sys.cpp 按原顺序包含（单编译单元，#ifdef 平台分支语义不变）；不是独立头文件。
GN_SYS_API P<GT_sPROC_INFO> GT_GetSysRunProcAll() {
    GN::sLIST<GT_sPROC_INFO> proc_list;
#if defined(_WIN32_) || defined(_WIN32_WCE)
    HANDLE hSysSnapshot = NULL;
    hSysSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSysSnapshot != (HANDLE)-1) {
        PROCESSENTRY32 proc;
        proc.dwSize = sizeof(proc);
        if(Process32First(hSysSnapshot, &proc)) {
            GT_sPROC_INFO *cur_proc;
            do {
                cur_proc = proc_list.add(GT_sPROC_INFO());
                cur_proc->name(proc.szExeFile, 0);
                cur_proc->pid = proc.th32ProcessID;

            } while(Process32Next(hSysSnapshot, &proc));
        }
        CloseHandle(hSysSnapshot);
    }
#elif defined _MAC_
    GT_sPROC_INFO *cur_proc;
    GT_TMP_FIND_PROCS_INFOS(
        cur_proc = proc_list.add(GT_sPROC_INFO());
        cur_proc->name(result[pi].kp_proc.p_comm, 0);
        cur_proc->pid = (unsigned int)result[pi].kp_proc.p_pid;)
#elif defined _LINUX_ /* && !defined GT_USING_GLes*/
    GT_TMP_FIND_PROCSS
    GT_sPROC_INFO *cur_proc = proc_list.add(GT_sPROC_INFO());
    cur_proc->name(name, 0);
    cur_proc->pid = (unsigned int)atol(next->d_name);
}
fclose(status);
}
}
closedir(dir);
}
#endif
    if(proc_list.empty()) return P_();
    else return P<GT_sPROC_INFO>(proc_list);
}
GN_SYS_API unsigned int GT_GetSysCoreCount() {
#if defined(_WIN32_) || defined(_WIN32_WCE)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (unsigned int)si.dwNumberOfProcessors;
#else /*if defined (_LINUX_)*/
    return sysconf(_SC_NPROCESSORS_CONF);
#endif
}
// DA-M2 批 3：剪贴板文本缓冲收敛进文件内上下文（跨平台各分支共用同一字段）。
struct sClipboardContext {
    char *aStrPasteBuffer = 0;
};
static sClipboardContext gClipboardContext;
#if defined(_WIN32_) || defined(_WIN32_WCE)
GN_SYS_API char const *GN_StrGetPasteText() {
    GN_DeleteArray(gClipboardContext.aStrPasteBuffer);
    if(IsClipboardFormatAvailable(CF_TEXT)) { //确定剪贴板是否含有CF_TEXT格式的数据
        OpenClipboard(0 /*(HWND)GNg::sWINDOW::Hwnd*/); //打开剪贴板
        HGLOBAL hGlobal = GetClipboardData(CF_TEXT); //获得引用文本的内存块句柄
        size_t clipSize = GlobalSize(hGlobal);
        gClipboardContext.aStrPasteBuffer = new char[clipSize]; //分配一块与剪贴板数据块大小相同的内存块
        LPVOID pGlobal = GlobalLock(hGlobal);
        strncpy(gClipboardContext.aStrPasteBuffer, (char *)pGlobal, clipSize - 1);
        gClipboardContext.aStrPasteBuffer[clipSize - 1] = 0;
        GlobalUnlock(hGlobal);
        CloseClipboard();
    }
    return gClipboardContext.aStrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text) {
    OpenClipboard(0); //打开剪贴板
    EmptyClipboard(); //清空剪贴板
    if(text) {
        unsigned int len = strlen(text);
        HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, (SIZE_T)(len + 1));
        LPVOID pGlobal = GlobalLock(hGlobal);
        strncpy((char *)pGlobal, text, len);
        ((char *)pGlobal)[len] = 0;
        GlobalUnlock(hGlobal);
        SetClipboardData(CF_TEXT, hGlobal); //把内存句柄交给剪贴板
    }
    CloseClipboard();
}
#elif defined(GT_USING_ANDROID_JNI)
GN_SYS_API char const *GN_StrGetPasteText() {
    return 0;
}
GN_SYS_API void GN_StrSetPasteText(const char *text) {
}
#elif defined _MAC_
GN_SYS_API char const *GN_StrGetPasteText() {
    char const *text = macGetPasteText();
    GN_StrcpyData(gClipboardContext.aStrPasteBuffer, text);
    return gClipboardContext.aStrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text) {
    macSetPasteText(text ? text : "");
}
#elif defined(_LINUX_)
#ifdef GN_HAS_X11_CLIPBOARD
#include <X11/Xlib.h>
#include <X11/Xatom.h>
struct GT_ClipboardX11 {
    Display *disp;
    Window win;
    Atom clip, utf8, targets, sel_prop;
    bool init;
    GT_ClipboardX11() : disp(0), win(0), init(false) {}
    ~GT_ClipboardX11() {
        if(win) XDestroyWindow(disp, win);
        if(disp) XCloseDisplay(disp);
    }
    void Setup() {
        if(init) return;
        disp = XOpenDisplay(NULL);
        if(!disp) return;
        int scr = DefaultScreen(disp);
        win = XCreateSimpleWindow(disp, RootWindow(disp, scr), 0, 0, 1, 1, 0, 0, 0);
        clip = XInternAtom(disp, "CLIPBOARD", False);
        utf8 = XInternAtom(disp, "UTF8_STRING", False);
        targets = XInternAtom(disp, "TARGETS", False);
        sel_prop = XInternAtom(disp, "GN_SEL_PROP", False);
        init = true;
    }
    char *ReadSelection(Atom selection) {
        Setup();
        if(!disp) return 0;
        XConvertSelection(disp, selection, utf8, sel_prop, win, CurrentTime);
        XFlush(disp);
        XEvent ev;
        for(int retry = 0; retry < 40; ++retry) {
            if(XCheckTypedWindowEvent(disp, win, SelectionNotify, &ev)) {
                if(ev.xselection.property != None) {
                    Atom type;
                    int fmt;
                    unsigned long nitems, after;
                    unsigned char *data = 0;
                    XGetWindowProperty(disp, win, sel_prop, 0, 65536 / 4, True, AnyPropertyType, &type, &fmt, &nitems, &after, &data);
                    if(data && type == utf8 && fmt == 8) {
                        char *out = new char[nitems + 1];
                        memcpy(out, data, nitems);
                        out[nitems] = 0;
                        XFree(data);
                        return out;
                    }
                    if(data) XFree(data);
                }
                break;
            }
            usleep(5000);
        }
        return 0;
    }
};
static GT_ClipboardX11 GT_X11Clip;
GN_SYS_API char const *GN_StrGetPasteText() {
    GN_DeleteArray(gClipboardContext.aStrPasteBuffer);
    char *text = GT_X11Clip.ReadSelection(GT_X11Clip.clip);
    if(!text) text = GT_X11Clip.ReadSelection(XA_PRIMARY);
    if(text) {
        gClipboardContext.aStrPasteBuffer = text;
        return gClipboardContext.aStrPasteBuffer;
    }
    FILE *fp = popen("wl-paste -t text 2>/dev/null", "r");
    if(fp) {
        char buf[4096] = {0};
        if(fread(buf, 1, sizeof(buf) - 1, fp) > 0) GN_StrcpyData(gClipboardContext.aStrPasteBuffer, buf);
        pclose(fp);
    }
    return gClipboardContext.aStrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text) {
    GN_StrcpyData(gClipboardContext.aStrPasteBuffer, text ? text : "");
    if(GT_X11Clip.Setup(), GT_X11Clip.disp) {
        XSetSelectionOwner(GT_X11Clip.disp, XA_PRIMARY, GT_X11Clip.win, CurrentTime);
        XSetSelectionOwner(GT_X11Clip.disp, GT_X11Clip.clip, GT_X11Clip.win, CurrentTime);
        XEvent ev;
        while(XCheckTypedWindowEvent(GT_X11Clip.disp, GT_X11Clip.win, SelectionRequest, &ev)) {
            XSelectionEvent sev{};
            sev.type = SelectionNotify;
            sev.display = ev.xselectionrequest.display;
            sev.requestor = ev.xselectionrequest.requestor;
            sev.selection = ev.xselectionrequest.selection;
            sev.target = ev.xselectionrequest.target;
            sev.property = ev.xselectionrequest.property;
            sev.time = ev.xselectionrequest.time;
            if(ev.xselectionrequest.target == GT_X11Clip.targets) {
                Atom types[] = {GT_X11Clip.utf8, XA_STRING};
                XChangeProperty(GT_X11Clip.disp, ev.xselectionrequest.requestor,
                                ev.xselectionrequest.property, XA_ATOM, 32, PropModeReplace, (unsigned char *)types, 2);
            } else {
                XChangeProperty(GT_X11Clip.disp, ev.xselectionrequest.requestor,
                                ev.xselectionrequest.property, ev.xselectionrequest.target, 8, PropModeReplace,
                                (unsigned char *)gClipboardContext.aStrPasteBuffer, strlen(gClipboardContext.aStrPasteBuffer));
            }
            XSendEvent(GT_X11Clip.disp, ev.xselectionrequest.requestor, False, 0, (XEvent *)&sev);
        }
        XFlush(GT_X11Clip.disp);
    }
    if(text && text[0]) {
        FILE *fp = popen("wl-copy 2>/dev/null", "w");
        if(fp) {
            fputs(text, fp);
            pclose(fp);
        }
    }
}
#else
GN_SYS_API char const *GN_StrGetPasteText() {
    GN_DeleteArray(gClipboardContext.aStrPasteBuffer);
    FILE *fp = popen("wl-paste -t text 2>/dev/null", "r");
    if(fp) {
        char buf[4096] = {0};
        if(fread(buf, 1, sizeof(buf) - 1, fp) > 0) GN_StrcpyData(gClipboardContext.aStrPasteBuffer, buf);
        pclose(fp);
    }
    return gClipboardContext.aStrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text) {
    GN_StrcpyData(gClipboardContext.aStrPasteBuffer, text ? text : "");
    if(text && text[0]) {
        FILE *fp = popen("wl-copy 2>/dev/null", "w");
        if(fp) {
            fputs(text, fp);
            pclose(fp);
        }
    }
}
#endif
#else
GN_SYS_API char const *GN_StrGetPasteText() {
    return gClipboardContext.aStrPasteBuffer;
}
GN_SYS_API void GN_StrSetPasteText(const char *text) {
    GN_StrcpyData(gClipboardContext.aStrPasteBuffer, text);
}
#endif

#define GT_TEMP_CUR_APP_DIR_ADD_SPLIT(DIR, LEN) \
    LEN = (unsigned char)strlen(DIR);           \
    DIR[LEN] = '/';                             \
    LEN++;                                      \
    DIR[LEN] = 0

GN_SYS_API void GF_AppCurPathChange(char *app_path, char *doc_path) {
    strncpy(GT_CurAppDir, app_path, sizeof(GT_CurAppDir) - 1);
    GT_CurAppDir[sizeof(GT_CurAppDir) - 1] = 0;
    GT_TEMP_CUR_APP_DIR_ADD_SPLIT(GT_CurAppDir, GT_CurAppDirLen);
    if(doc_path) {
        strncpy(GT_CurAppDirW, doc_path, sizeof(GT_CurAppDirW) - 1);
        GT_CurAppDirW[sizeof(GT_CurAppDirW) - 1] = 0;
        GT_TEMP_CUR_APP_DIR_ADD_SPLIT(GT_CurAppDirW, GT_CurAppDirLenW);
    }
}
GN_SYS_API int GT_GetAppCurPath(char dir_out[]) {
    if(GT_CurAppDir[0]) {
        // 契约（B5f）：dir_out 缓冲区 ≥256 字节。
        strncpy(dir_out, GT_CurAppDir, 255);
        dir_out[255] = 0;
        dir_out[GT_CurAppDirLen - 1] = 0;
        return (int)(GT_CurAppDirLen - 1);
    } else {
        dir_out[0] = 0;
        return 0;
    }
}
