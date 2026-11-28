// GN_FormatPlugin.cpp — 文件格式插件链与格式表：图像格式注册表/缓存、iFILE_IMAGE 插件基类与增删（原 GN_Format.cpp 拆分）。
#include "GN.h"
#include "GN_FormatInternal.h"
#include "GN_CoreContext.h" // B7-3：图像扩展名缓存收敛进 GNg::GN_Core
#ifdef _GN_FORMAT_H_
    GN_API bool GNg::CheckImgFmtSupportFromFileName(const char *filename) {
        char *ext_name = GN_StrGetExtName(filename);
        if(ext_name) return FileImage->CheckFmtSupportFromExtName(ext_name) ? true : false;
        return false;
    }
    // B7-3：扩展名缓存（原 static GT_SupportExtNamesImage[160]）已并入 GNg::GN_Core.aImageExtNames。
    GN_API char * GNg::GetSupportImgFmtFileExtName(bool upper) {
        if(!GNg::GN_Core.aImageExtNames[0]) FileImage->GetSupportFmtFileExtName(GNg::GN_Core.aImageExtNames, upper);
        return (char *)GNg::GN_Core.aImageExtNames;
    }
    GN_API const char *GNg::ImgFileTypeNames[] = {"bmp", "png", "tga", "gim", "hdr"};

    /*----------------------------------------------------- 默认图像文件插件功能 --------------------------------------------------------------*/
    GNg::iFILE_IMAGE::iFILE_IMAGE() : module(0) {
        GT_MX_PushFront(FileImage, this);
        GNg::GN_Core.aImageExtNames[0] = 0;
    }
    GNg::iFILE_IMAGE::~iFILE_IMAGE() {
        GT_TMP_REMOVE_PLUGINS_NODE(iFILE_IMAGE, FileImage)
        GNg::GN_Core.aImageExtNames[0] = 0;
    }
    uint8 *GNg::iFILE_IMAGE::LoadInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out, int memsize) {
#define GT_TMP_FILE_IMAGE_LOAD(PROC, ARG1, ARG2, ADDPROC)                                  \
    unsigned short ext_pos = *format;                                                      \
    uint8 *data;                                                                           \
    int yes = GNf::IsTypeFile(filename_mem, ext_pos, "png", "塒NG", 4, memsize);           \
    if(yes) {                                                                              \
        data = LoadPngFile##PROC(filename_mem, format, w, h, ARG1, memsize);               \
        if(data || yes == 1) return data;                                                  \
    } else {                                                                               \
        uint8 gim_head[] = {0x11, 0x02, 0x28, 0x02};                                       \
        yes = GNf::IsTypeFile(filename_mem, ext_pos, "gim", (char *)gim_head, 4, memsize); \
        if(yes) {                                                                          \
            data = LoadPngFile##PROC(filename_mem, format, w, h, ARG1, memsize);           \
            if(data || yes == 1) return data;                                              \
        }                                                                                  \
    }                                                                                      \
    yes = GNf::IsTypeFile(filename_mem, ext_pos, "bmp", "BM", 2, memsize);                 \
    if(yes) {                                                                              \
        data = LoadBmpFile##PROC(filename_mem, format, w, h, ARG2, memsize);               \
        if(data || yes == 1) return data;                                                  \
    }                                                                                      \
    yes = GNf::IsTypeFile(filename_mem, ext_pos, "hdr", "#?", 2, memsize);                 \
    if(yes) {                                                                              \
        float ins = -8.0f; (void)ins; /*亮度提高*/                                         \
        data = ADDPROC;                                                                    \
        if(data || yes == 1) return data;                                                  \
    }                                                                                      \
    return LoadTgaFile##PROC(filename_mem, format, w, h, ARG2, memsize);

        GT_TMP_FILE_IMAGE_LOAD(Info, min_out, min_out, LoadHdrFileInfo(filename_mem, format, w, h, min_out, memsize))
    }
    uint8 *GNg::iFILE_IMAGE::LoadData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        GT_TMP_FILE_IMAGE_LOAD(Data, col_turn, !col_turn, LoadHdrFileData(filename_mem, format, w, h, !col_turn, memsize, &ins))
    }
    bool GNg::iFILE_IMAGE::Save(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn, uint8 comps_lv) {
        bool save_ok = false;
        char *ext_name = GN_StrGetExtName(file_name);
        if(ext_name) {
            if(!strcmp(ext_name, "bmp")) {
                save_ok = SaveBmpFile((char *)file_name, data, srcformat, w, h, col_turn);
            } else if(!strcmp(ext_name, "png") || !strcmp(ext_name, "gim")) {
                save_ok = SavePngFile((char *)file_name, data, srcformat, w, h, col_turn);
            } else if(!strcmp(ext_name, "tga")) {
                save_ok = SaveTgaFile((char *)file_name, data, srcformat, w, h, col_turn, comps_lv);
            } else if(!strcmp(ext_name, "hdr")) {
                save_ok = SaveHdrFile((char *)file_name, data, srcformat, w, h, col_turn, comps_lv);
            }
        } else {
            GN_Log("保存文件名没有类型扩展名,无法确定保存的文件类型!\n");
        }
        return save_ok;
    }
    uint8 *GT_LoadBmpFileData(char *filename_mem, GNg::COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        return GNg::LoadBmpFileData(filename_mem, format, w, h, !col_turn, memsize);
    }
    uint8 *GT_LoadTgaFileData(char *filename_mem, GNg::COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        return GNg::LoadTgaFileData(filename_mem, format, w, h, !col_turn, memsize);
    }
    uint8 *GT_LoadHdrFileData(char *filename_mem, GNg::COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        float ins = -8.0f; /*亮度提高*/
        return GNg::LoadHdrFileData(filename_mem, format, w, h, !col_turn, memsize, &ins);
    }
    GNg::PF_LOAD_IMG_FILE_DATA GNg::iFILE_IMAGE::CheckFmtSupportFromExtName(const char *extname) {
        /*if(GNf::sFILE::EnableDedicatedOnly){
		if(!strcmp(extname,"gim"))return LoadPngFileData;
	}else*/
        {
            if(extname[0] > 'Z') {
                if(!strcmp(extname, "png") || !strcmp(extname, "gim")) return LoadPngFileData;
                else if(!strcmp(extname, "bmp")) return GT_LoadBmpFileData;
                else if(!strcmp(extname, "tga")) return GT_LoadTgaFileData;
                else if(!strcmp(extname, "hdr")) return GT_LoadHdrFileData;
            } else {
                if(!strcmp(extname, "PNG")) return LoadPngFileData;
                else if(!strcmp(extname, "BMP")) return GT_LoadBmpFileData;
                else if(!strcmp(extname, "TGA")) return GT_LoadTgaFileData;
                else if(!strcmp(extname, "HDR")) return GT_LoadHdrFileData;
            }
        }
        return 0;
    }
    void GNg::iFILE_IMAGE::GetSupportFmtFileExtName(char *out, bool upper) {
        // 已核（STRCPY 审计）：调用方缓存 160 字节（GNg::GN_Core.aImageExtNames），本函数写 20+16=36 字节。
        memcpy(out, "png,bmp,tga,hdr,gim", sizeof("png,bmp,tga,hdr,gim"));
        if(upper) memcpy(out + 19, ",PNG,BMP,TGA,HDR", sizeof(",PNG,BMP,TGA,HDR"));
    }
    GNg::iFILE_IMAGE GT_FileImage;
    GN_API GNg::iFILE_IMAGE *GNg::FileImage = &GT_FileImage;

    GN_API int GN_PluginsFileImageRemove(){
        GT_TMP_REMOVE_PLUGIN(GNg::FileImage, GT_FileImage)}
#endif //_GN_FORMAT_H_
