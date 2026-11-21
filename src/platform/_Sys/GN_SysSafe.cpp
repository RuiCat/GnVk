// GN_SysSafe.cpp — 平台层带容量上限的安全字符串/路径变体（B5f 收尾，旧 ABI 保留在 GN_Sys.cpp）
#include <GN.h>

// ── B5f 收尾：带容量上限的安全变体（旧函数保留 ABI，新调用点用 _s） ──
GN_SYS_API void GN_StrToSysFilePath_s(char des[], int cap, const char *src, const char *open_mode, bool is_g_dir) {
    if(!des || cap <= 0) return;
    char tmp[384] = {0};
    GN_StrToSysFilePath(tmp, src, open_mode, is_g_dir);
    GN_StrCopyToLimit(des, cap, tmp);
}
GN_SYS_API int GT_GetAppCurPath_s(char *dir_out, int cap) {
    if(!dir_out || cap <= 0) return 0;
    char tmp[256] = {0};
    const int r = GT_GetAppCurPath(tmp);
    GN_StrCopyToLimit(dir_out, cap, tmp);
    return r;
}
GN_SYS_API void GT_GetAppFileName_s(char *name_out, int cap) {
    if(!name_out || cap <= 0) return;
    char tmp[256] = {0};
    GT_GetAppFileName(tmp);
    GN_StrCopyToLimit(name_out, cap, tmp);
}
GN_SYS_API void GN_GetDeviceInfoCpu_s(char *info, int cap) {
    if(!info || cap <= 0) return;
    char tmp[256] = {0};
    GN_GetDeviceInfoCpu(tmp);
    GN_StrCopyToLimit(info, cap, tmp);
}
