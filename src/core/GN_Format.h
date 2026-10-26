#ifndef _GN_FORMAT_H_
#define _GN_FORMAT_H_

extern GN_API bool GN_AddUpperFmtName; //获得引擎支持的文件格式扩展名时是否追加大写名输出 这个在特定系统的文件路径需严格区分大小写时默认值为true否则false

/********************************************************* 颜色格式 ************************************************************************/
namespace GNg {
extern GN_API const char *ColorFmtNamesEn[]; //象素颜色格式名 -- 对应下面的每一个举枚

enum COLOR_FORMAT { //象素颜色格式
    CR_DEFAULT_FORMAT = 0, //默认图像的颜色
    CR_RGBA = 1, //红绿蓝透明信息
    CR_RGB = 2, //红绿蓝信息
    CR_RGB565 = 3, //压缩红绿蓝信息
    //	 CR_RGBA4444             = 4,      //压缩红绿蓝透明信息
    //	 CR_RGBA5551             = 5,      //压缩红绿蓝透明信息
    CR_LUMINANCE_ALPHA = 6, //只有灰度和透明信息
    CR_ALPHA = 7, //只有透明信息
    CR_LUMINANCE = 8, //只有灰度信息
    CR_RED = 9, //只有红信息
    CR_GREEN = 10, //只有绿信息
    CR_BLUE = 11, //只有蓝信息
    CR_PALETTE = 12, //调色版索引信息

    CR_RGBA16 = 13, //每通道16位的红绿蓝透明浮点数信息
    CR_RGB16 = 14, //每通道16位的红绿蓝浮点数信息

    CR_DEFAULT_L_AND_A = 255, //默认图像的颜色合并为亮度 如果图像有透明也包含透明
};
extern GN_API const char *ImgFileTypeNames[]; //图片文件格式类型名 -- 对应下面的每一个举枚

enum eIMG_FILE_TYPE { //图片文件格式类型
    eIMG_FILE_BMP, //.bmp文件
    eIMG_FILE_PNG, //.png文件
    eIMG_FILE_TGA, //.tga文件
    eIMG_FILE_GIM, //.gim文件
    eIMG_FILE_HDR, //.hdr文件

    eIMG_FILE_TYPE_NUM, //文件格式类型数
    eIMG_FILE_UNKNOWN = 0xff //未知类型
};

//获得指定颜色格式是否含有透明通道信息( 颜色格式 )
GN_API bool ImgFmtHasAlpha(COLOR_FORMAT format);
//获得指定颜色格式是否属于灰度类格式( 颜色格式 )
GN_API bool ImgFmtIsLuminance(COLOR_FORMAT format);

//按颜色位数来获取颜色格式( 颜色位数 )
GN_API COLOR_FORMAT ImgFmtGetFormat(short bit_num);
//得到图象颜色格式所占用的字节数( 颜色格式 )
GN_API short ImgFmtGetByteNum(COLOR_FORMAT format);

//图象格式转换传输(目标图象指针, 想要转换的格式, 原图象指针, 原图象的颜色格式, 图象总宽长, RGB颜色值是否进行翻转)  !!两个图象的总宽必须一致并不超出两个图象的数组范围 还有原格式一定要指定清楚 否则会出轨 !暂不支持浮点数颜色格式
GN_API void ImgFmtConvertTs(uint8 *pdesc_data, COLOR_FORMAT descFormat, const uint8 *psrc_data, COLOR_FORMAT srcFormat, int img_w, bool RGB_turn = false);
//图象格式转换RGB555为RGB565(目标图象指针, 原图象指针, 图象总宽长, RGB颜色值是否进行翻转)  !!两个图象的总宽必须一致并不超出两个图象的数组范围
GN_API void ImgFmtRGB555toRGB565(unsigned short *pdesc_data, const unsigned short *psrc_data, int img_w, bool RGB_turn = false);
//图象格式转换RGB565为RGB555(目标图象指针, 原图象指针, 图象总宽长, RGB颜色值是否进行翻转)  !!两个图象的总宽必须一致并不超出两个图象的数组范围
GN_API void ImgFmtRGB565toRGB555(unsigned short *pdesc_data, const unsigned short *psrc_data, int img_w, bool RGB_turn = false);

//单个像素格式转换RGB565为BGR555(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtRGB565toBGR555(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换RGBA为BGRA(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtRGBAtoBGRA(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换RGB为BGR(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtRGBtoBGR(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换LA为BGRA(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtLAtoBGRA(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换A为BGRA(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtAtoBGRA(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换L为BGR(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtLtoBGR(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换R为BGR(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtRtoBGR(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换G为BGR(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtGtoBGR(uint8 *pdesc, const uint8 *psrc);
//单个像素格式转换B为BGR(目标像素颜色指针, 原像素颜色指针)
GN_API void PixFmtBtoBGR(uint8 *pdesc, const uint8 *psrc);

//把24位颜色直接转为32位指数颜色(24位颜色, 32位指数颜色)
GN_API void PackRGBE0(uint8 *rgb, uint8 *rgbe /*[4]*/);
//把RGB浮点颜色打包为32位指数颜色(RGB浮点颜色, 32位指数颜色)
GN_API void PackRGBE(/*GNg::sVEC3* */ uint8 *rgb, uint8 *rgbe /*[4]*/);
//把32位指数颜色解包为 RGB浮点颜色(RGB浮点颜色, 32位指数颜色)
GN_API void UnpkRGBE(/*GNg::sVEC3* */ float *rgbf, uint8 *rgbe /*[4]*/);

typedef void (*PFUNC_PIX_CV)(uint8 *, const uint8 *); //像素格式转换功能函数指针类型
//按特定颜色格式来的得到使像素颜色RB分量调换的功能函数指针( 特定颜色格式 ) 返回功能函数指针
GN_API PFUNC_PIX_CV PixRGBturnFuncChoose(COLOR_FORMAT format);

//从hdr图像数据转为线性空间的图像数据(hdr图像数据, hdr图像宽, 高, 每像素颜色分量数, 曝光补偿, 加码)返回线性空间的图像数据 !用完后记得delete []data;
GN_API uint8 *ImgDataHdrToLinearData(const float *data, int w, int h, int chn, float exp = 1.0f, float gamma = 0.75f);

//从两个图象格式结合为一个新格式(新图象指针, 原图象指针,  原2图象指针, 图象总宽长, 是否倒置A通道的值) !!3个图象的总宽必须一致并不超出3个图象的数组范围,这个暂不支持CR_RGB565等压缩的格式 这个明确格式的调用比通用版本快
GN_API void ImgFmtMergeRGBandRGBtoRGBA(uint8 *pdesc_data, const uint8 *psrc_data, const uint8 *psrc_data2, int img_w, bool inv_a = false);
//从两个图象格式结合为一个新格式(新图象指针, 原图象指针,  原2图象指针, 图象总宽长, 是否倒置A通道的值) !!3个图象的总宽必须一致并不超出3个图象的数组范围,这个暂不支持CR_RGB565等压缩的格式 这个明确格式的调用比通用版本快
GN_API void ImgFmtMergeRGBandRGB_toRGBA(uint8 *pdesc_data, const uint8 *psrc_data, const uint8 *psrc_data2, int img_w, bool inv_a = false);
//从两个图象格式结合为一个新格式(新图象指针, 原图象指针,  原2图象指针, 图象总宽长, 是否倒置A通道的值) !!3个图象的总宽必须一致并不超出3个图象的数组范围,这个暂不支持CR_RGB565等压缩的格式 这个明确格式的调用比通用版本快
GN_API void ImgFmtMergeRGBandRGBtoLA(uint8 *pdesc_data, const uint8 *psrc_data, const uint8 *psrc_data2, int img_w, bool inv_a = false);
//从图象的RGB转为A与指定亮度值结合为一个LA格式(新图象指针, 原图象指针, 原图象单个象素占用的字节数, 亮度值,  图象总宽长, 是否倒置A通道的值) !!2个图象的总宽必须一致并不超出2个图象的数组范围 这个暂不支持CR_RGB565等压缩的格式 这个明确格式的调用比通用版本快
GN_API void ImgFmtMergeRGBto_A(uint8 *pdesc_data, const uint8 *psrc_data, int src_px_byte, uint8 lum, int img_w, bool inv_a = false);
//从两个图象格式结合为一个新格式(新图象指针, 新图象单个象素占用的字节数, 原图象指针, 原图象单个象素占用的字节数, 原图象单个象素转换为的字节数, 插入到新象素的偏移字节数, 原2图象指针, 原2图象单个象素占用的字节数, 原2图象单个象素转换为的字节数, 插入到新象素的偏移字节数, 图象总宽长) !!3个图象的总宽必须一致并不超出3个图象的数组范围,这个暂不支持CR_RGB565等压缩的格式  !如果原象素转换为的字节数 < 实际象素占用的字节数 将会取均值 反之 如果 R3,G7 转为 RGBA 为 R3,G7,B7,A7
GN_API void ImgFmtMerge(uint8 *pdesc_data, int desc_px_byte, const uint8 *psrc_data, int src_px_byte, int src_to_px_byte, int src_offset, const uint8 *psrc_data2, int src_px_byte2, int src_to_px_byte2, int src_offset2, int img_w);
//图象数据缩放(目标图象指针, 目标图象宽, 目标图象高, 原图象指针, 原图象宽, 原图象高, 图象数据单个象素占用的字节数, 平滑质量:0-2 值越大质越好)
GN_API void ImgScale(uint8 *pdesc_data, int desc_w, int desc_h, const uint8 *psrc_data, int src_w, int src_h, short pix_byte, uint8 smooth = 2);
//复制图象数据(原图象数据地址, 图象一行的字节数, 图象高, 是否翻转列象素) 返回新复制出来的图象数据 !用完后记得delete []data;
GN_API uint8 *ImgCopyToData(const uint8 *pdata, int pitch, int h, bool col_turn = false);

//由源图象数据块直接覆盖到另一个图象里(原图象指针, 原图象宽, 原数据块偏移位置X, 数据块偏移位置Y, 转移的数据块宽, 转移的数据块高, 目标覆盖图象指针,目标图象宽, 覆盖偏移位置X, 覆盖偏移位置Y, 原图象的颜色格式) !该函数不会作数据访问出轨检测的 所以你须确定目标图象尺寸在指定位置上能够容纳覆盖来的数据块区划
GN_API void ImgBlt(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, COLOR_FORMAT srcFormat);
//由源图象数据块转换覆盖到另一个图象里(原图象指针, 原图象宽, 原数据块偏移位置X, 数据块偏移位置Y, 转移的数据块宽, 转移的数据块高, 目标覆盖图象指针,目标图象宽, 覆盖偏移位置X, 覆盖偏移位置Y, 原图象的颜色格式, 覆盖目标的颜色格式,RGB颜色值是否进行翻转) !该函数不会作数据访问出轨检测的 所以你须确定目标图象尺寸在指定位置上能够容纳覆盖来的数据块区划
GN_API void ImgTsBlt(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, COLOR_FORMAT srcFormat, COLOR_FORMAT descFormat, bool RGB_turn = false);
//由源图象数据块缩放覆盖到另一个图象里(原图象指针, 原图象宽, 原数据块偏移位置X, 数据块偏移位置Y, 转移的数据块宽, 转移的数据块高, 目标覆盖图象指针,目标图象宽, 覆盖偏移位置X, 覆盖偏移位置Y, 覆盖宽度, 覆盖高度, 原图象的颜色格式, 缩放临时处理缓冲:如果需要连续调用该函数时指定这个可以提高处理速度~这个缓冲应该预先分配好总是大于原转移数据块空间加覆盖数据块空间的两倍) !该函数不会作数据访问出轨检测的 所以你须确定目标图象尺寸在指定位置上能够容纳覆盖来的数据块区划
GN_API void ImgScaleBlt(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, int _desc_w, int _desc_h, COLOR_FORMAT srcFormat, uint8 *proc_buf = 0);

//使图象数据的指定区域乘上一定因子(图象数据地址, 图象的颜色格式, 图象宽, 处理区域位置X, 位置Y, 区域宽, 区域高, 因子0-255, 是否仅乘到透明通道里面如果有透明通道)
GN_API void ImgMultMultiplyRatio(uint8 *pdata, COLOR_FORMAT cfmt, int img_w, int x, int y, int w, int h, uint8 ratio, bool to_alpha = true);

//以下的混合函数处理开销比较大如果混合模式是直接覆盖 那么请使用 ImgBlt 系列函数
//由源图象数据块使用常规方式混合到另一个图象里(原图象指针, 原图象宽, 原数据块偏移位置X, 数据块偏移位置Y, 转移的数据块宽, 转移的数据块高, 目标覆盖图象指针,目标图象宽, 覆盖偏移位置X, 覆盖偏移位置Y) 这里两图象数据必须都是RGBA格式 这样无需转换的调用相对处理快一点 !!该函数不会作数据访问出轨检测的 所以你须确定目标图象尺寸在指定位置上能够容纳覆盖来的数据块区划
GN_API void ImgBlendRGBAtoRGBAusNormal(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y);
//由源图象数据块使用自定义方式混合到另一个图象里(原图象指针, 原图象宽, 原数据块偏移位置X, 数据块偏移位置Y, 转移的数据块宽, 转移的数据块高, 目标覆盖图象指针,目标图象宽, 覆盖偏移位置X, 覆盖偏移位置Y, 原图象每像素占字节数, 目标覆盖图象每像素占字节数, 用户混合函数(当前处理的原图象像素指针, 当前处理的目标覆盖图象像素指针)) !!该函数不会作数据访问出轨检测的 所以你须确定目标图象尺寸在指定位置上能够容纳覆盖来的数据块区划
GN_API void ImgBlend(const uint8 *psrc_data, int src_w, int _src_x, int _src_y, int _src_w, int _src_h, uint8 *pdesc_data, int desc_w, int _desc_x, int _desc_y, int src_px_size, int desc_px_size, void (*blend_func)(const uint8 *src_pix, uint8 *desc_pix));

/*-------------------- !! 图象文件读写操作函数 引擎的BMP文件暂不支持256色索引格式,但追加了一些引擎里特有的颜色格式 !! --------------------------------------------------------------------*/

//读取BMP图片文件信息(图片文件名或内存地址,返回颜色格式,返回宽,返回高,是否输出缩略图,使用内存模式指定大小)返回缩略图数据指针 返回0表示缩略图读取失败 返回数据内容用完后记得delete []data;
GN_API uint8 *LoadBmpFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out = false, int memsize = GNf::sFILE::DefaultType);
//读取BMP图片文件数据(图片文件名或内存地址,返回颜色格式,返回宽,返回高,是否翻转列象素,使用内存模式指定大小)返回图片数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
GN_API uint8 *LoadBmpFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn = false, int memsize = GNf::sFILE::DefaultType);
//把图象数据保存为BMP文件(保存文件名,图象数据指针,原图象颜色格式,图象宽,图象高,是否翻转列象素) 返回保存是否成功
GN_API bool SaveBmpFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn = false);

//读取TGA图片文件信息(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否输出缩略图,使用内存模式指定大小)返回缩略图数据指针 返回0表示缩略图读取失败 返回数据内容用完后记得delete []data;
GN_API uint8 *LoadTgaFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out = false, int memsize = GNf::sFILE::DefaultType);
//读取TGA图片文件数据(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否翻转列象素,使用内存模式指定大小,是否解压16位RGB为24位)返回图片数据的指针 返回0表示读取失败 返回数据内容用完后记得delete []data;
GN_API uint8 *LoadTgaFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn = false, int memsize = GNf::sFILE::DefaultType, bool unpk = true);
//把图象数据保存为TGA文件（保存文件名,图象数据指针, 原图象颜色格式, 图象宽, 图象高,是否翻转列象素号, 压缩等级0-5 0:不压缩 1:无损压缩 其它值越大压缩越多) 返回保存是否成功
GN_API bool SaveTgaFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn = false, uint8 comps_lv = 0);

//读取PNG图片文件信息(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否输出缩略图,使用内存模式指定大小)返回缩略图数据指针 返回0表示缩略图读取失败 返回数据内容用完后记得delete []data;
GN_API uint8 *LoadPngFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out = false, int memsize = GNf::sFILE::DefaultType);
//读取PNG图片文件数据(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否翻转列象素,使用内存模式指定大小)返回图片数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
GN_API uint8 *LoadPngFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn = false, int memsize = GNf::sFILE::DefaultType);
//把图象数据保存为PNG文件（保存文件名,图象数据指针, 原图象颜色格式, 图象宽, 图象高,是否翻转列象素,gim文件更新序号) 返回保存是否成功    如果文件名后为.gim 则保存为GIM格式
GN_API bool SavePngFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn = false, unsigned int dateID = 0);

//读取HDR图片文件信息(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否输出缩略图,使用内存模式指定大小)返回缩略图数据指针 返回0表示缩略图读取失败 返回数据内容用完后记得delete []data;
GN_API uint8 *LoadHdrFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out = false, int memsize = GNf::sFILE::DefaultType);
//读取HDR图片文件数据(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否翻转列象素,使用内存模式指定大小, 如有指定这个将强制转为24位颜色格式并压缩亮度至不大于1 并在此返回原数据里最大的亮度值 若不超过1则不会压缩亮度 若该参数指向为一个负值将使用该值作为曝光补偿来把数据转为线性空间 而不返回最大亮度)返回图片数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
GN_API uint8 *LoadHdrFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn = false, int memsize = GNf::sFILE::DefaultType, float *max_ins = 0);
//把图象数据保存为HDR文件(保存文件名,图象数据指针, 原图象颜色格式, 图象宽, 图象高,是否翻转列象素号, 压缩等级0-5 0:不压缩 1:无损压缩 其它值越大压缩越多) 返回保存是否成功
GN_API bool SaveHdrFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn = false, uint8 comps_lv = 0);
//判断文件路径是否为hdr文件(文件名)
GN_API bool IsHdrFile(char *filename);

//读取指定类型图片文件数据(图片类型, 图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 使用内存模式指定大小)返回图片数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
GN_API uint8 *LoadImgFileData(eIMG_FILE_TYPE file_type, char *filename_mem, COLOR_FORMAT *format, int *w, int *h, int memsize = GNf::sFILE::DefaultType);
//把图象数据保存为扩展名指定类型的图片文件(保存文件名, 图象数据指针, 原图象颜色格式, 图象宽, 图象高, 是否翻转列象素号, 压缩等级0-5 0:不压缩 1:无损压缩 其它值越大压缩越多) 返回保存是否成功  如果文件名后为.gim 则保存为GIM格式
GN_API bool SaveImgFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn = false, uint8 comps_lv = 0);

//图片文件缩放(缩放后保存为的目标文件名:保存的文件类型自动按该名的扩展名来定, 要缩放到的宽度, 要缩放到的高度, 源图片文件名或内存地址, 源图片是什么类型的文件, 使用内存模式指定大小, 压缩等级0-5 0:不压缩 1:无损压缩 其它值越大压缩越多) 返回是否成功
GN_API bool ImgFileScale(const char *dst_file_name, int dst_w, int dst_h, char *filename_mem, eIMG_FILE_TYPE file_type, int memsize = GNf::sFILE::DefaultType, uint8 comps_lv = 0);

//按文件扩展名获取支持的图象类型( 路径文件名 )返回图象类型 返回 eIMG_FILE_UNKNOWN 表示未知
GN_API eIMG_FILE_TYPE GetImgFmtTypeFromFileName(const char *filename);

//按文件扩展名检测图象格式是否被引擎支持( 路径文件名 ) 返回是否
GN_API bool CheckImgFmtSupportFromFileName(const char *filename);

//获得能够被引擎支持的图象文件扩展名(是否追加返回大写的名)  返回各个不同的扩展名之间用,隔开 如 "gim,png,bmp,tga"
GN_API char *const GetSupportImgFmtFileExtName(bool upper = GN_AddUpperFmtName);

//生成圆数据到内存只有单色通道(目标内存地址, 宽高, 边界大小, 硬度0.0f - 1.0f, 是否实圆)
GN_API void GenSphereDataToMem(uint8 *p_memory, int wh, int b = 0, float value = 0.0f, bool is_solid = false);

//生成圆数据到内存只有单色通道(目标内存地址, 宽高, 边界大小, 圆心到圆外的明暗插值关键点数据 x分量表示位置y分量表示值, 关键点数)返回关键点中最大的值 !!注意关键点位置和值都不能为负数
GN_API float GenSphereDataToMem(uint8 *p_memory, int wh, int b, const float /*sVEC2*/ *keys, int kn);

//生成立体3D空间圆数据到内存只有单色通道(目标内存地址, 宽高深, 边界大小, 硬度0.0f - 1.0f, 平方衰减调整值1.0f - 255.0f, 是否半边) 平方衰减调整值0.0f为不使用平方衰减 如果指定该值那么最亮的地方就为该值
GN_API void GenSphereDataToMem3D(uint8 *p_memory, int whd, int b = 0, float value = 0.0f, float sqv = 0.0f, bool is_half = false);

//生成圆柱数据到内存只有单色通道(目标内存地址, 宽, 高, 硬度0.0f - 1.0f)
GN_API void GenCylinderDataToMem(uint8 *p_memory, int w, int h, float value = 0.0f);

//生成矩型数据到内存只有单色通道(目标内存地址, 宽, 高, 渐变起始位置x, 渐变起始位置y, 渐变方向x 该值越大渐变越短 通常设为1.0/w渐变就能覆盖到整图, 渐变方向y 该值越大渐变越短 通常设为1.0/h渐变就能覆盖到整图)
GN_API void GenRectDataToMem(uint8 *p_memory, int w, int h, float px = 0.0f, float py = 0.0f, float vx = 1.0f, float vy = 0.0f);

//生成棋盘格数据到内存只有单色通道(目标内存地址, 单格大小, 宽格数, 高格数, 黑格亮度)
GN_API void GenCheckerboardDataToMem(uint8 *p_memory, int cb_size, int wn, int hn, uint8 lum);

//生成 0123456789 号码单通道图像数据(返回单字符宽, 返回单字符高)返回图像数据 图像数据大小= 字宽x字高x10  !用完后记得delete [] data;
GN_API uint8 *GenNumberImgData(int &fw_out, int &fh_out);

//生成箭头数据到内存只有单色通道( 目标内存地址, 宽, 高, 式样0:圆头1:尖头 , 生成为空心边框的边宽度 默认0为实心的)
GN_API void GenArrowDataToMem(unsigned char *p_memory, int w, int h, bool style = 0, int frame_w = 0);
}; // namespace GNg
namespace GNa {

//计算音频播放时间(声道数, 声音位数, 频率, 数据字节大小)返回播放时间
inline unsigned int ComputePlayTime(unsigned short channels, unsigned short bit, unsigned int freq, unsigned int size) {
    return (size / (channels * (bit >> 3)) * 1000 / freq);
}

//读取Wav音频文件信息(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频播放时间 返回0表示读取失败
GN_API unsigned int LoadWavFileInfo(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize = GNf::sFILE::DefaultType);
//读取Wav音频文件数据(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
GN_API uint8 *LoadWavFileData(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize = GNf::sFILE::DefaultType);
//把音频数据保存为Wav文件(保存文件名, 音频格式, 声道数, 声音位数, 频率, 数据字节大小, 数据) 返回保存是否成功
GN_API bool SaveWavFile(char *file_name, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq, unsigned int size, uint8 *data);

//读取Mp3音频文件信息(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频播放时间 返回0表示读取失败
GN_API unsigned int LoadMp3FileInfo(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize = GNf::sFILE::DefaultType);
//读取Mp3音频文件数据(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
GN_API uint8 *LoadMp3FileData(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize = GNf::sFILE::DefaultType);

//按文件扩展名检测音频格式是否被引擎支持( 路径文件名 ) 返回是否
GN_API bool CheckAudioFmtSupportFromFileName(const char *filename);
//获得能够被引擎支持的音频文件扩展名(是否追加返回大写的名) 返回各个不同的扩展名之间用,隔开 如 "wav,mp3,ogg"
GN_API char *const GetSupportAudioFmtFileExtName(bool upper = GN_AddUpperFmtName);
}; // namespace GNa
namespace GNf {

//找出扩展名的位置偏移(文件名或内存地址, 使用内存模式指定大小)参数1如是有扩展名的文件路径就偏移 否则返回0
inline unsigned short GetExtNamePos(char *filename_mem, int memsize = sFILE::DefaultType) {
    if(memsize < eFILE_MEM) {
        char *ext_name = GN_StrGetExtName(filename_mem);
        if(ext_name) return ext_name - filename_mem;
    }
    return 0;
}
//判断提供的文件名或文件内容是否指定类型的文件(文件名或内存地址, 文件扩展名在文件名里的字节偏移 否则值给0, 指定类型的文件扩展名, 指定类型文件头标签, 标签大小字节, 使用内存模式指定大小)返回是否 返回-1表示不确定
inline int IsTypeFile(char *filename_mem, unsigned short ext_name_pos, const char *extname, const char *tag, int tag_size, int memsize = sFILE::DefaultType) {
    if(ext_name_pos)
        return strcmp(filename_mem + ext_name_pos, extname) ? 0 : 1;
    else if(memsize >= eFILE_MEM)
        return strncmp(filename_mem, tag, tag_size) ? 0 : 1;
    else return -1;
}
}; // namespace GNf
/********************************************************* 音频文件支持插件功能 **************************************************************/
namespace GNa {
/*
	以下插件接口一系列具有文件名或内存地址参数 filename_mem 的函数会被一连串地调用, 以续个尝试能否打开读取特定类型的文件, 
	因此造成性能和IO带宽浪费, 为提升判断速度, 最好先用 GetExtNamePos 找出文件扩展名位置, 
	再通过这些函数的 format 参数传入位置, 以便实现里可以用 IsTypeFile 快速判断是否能支持的类型
*/
class GN_API iFILE_AUDIO //音频文件支持插件接口
{
  public:
    iFILE_AUDIO *next_node; //指向下个插件 通常是该类的一系列插件链 当有多个插件加载后 就用这个来把一个个插件连起来 最后一个为0表示结束
    int *module; //插件程序模块

    iFILE_AUDIO(); //构造插件并把它接入到插件链 FileImage 的前面 并使 FileImage = 新构建的插件
    virtual ~iFILE_AUDIO(); //释放插件资源并断开在插件链的连接

    class cSTREAM //音频文件流
    {
      public:
        //跳转到指定音频进度(进度0.0f至1.0f)
        virtual void Seek(float progre) = 0;
        //读取文件流解码后的数据(存放数据的内存, 需读取解码后的数据字节数)返回读取解码后的字节数 返回0表示完结
        virtual unsigned int Read(char *buf, unsigned int count) = 0;
        //写入一段音频数据(数据, 数据大小字节)返回写入了个数 失败返回0
        virtual unsigned int Write(const char *buf, int count) = 0;
        //关闭音频文件流(音频文件流)
        virtual void Close() = 0;
    };
    //打开音频文件流并获取文件信息(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频文件流 返回0表示打开失败
    virtual cSTREAM *Open(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize = GNf::sFILE::DefaultType);
    //创建音频文件流并先存储文件信息 以便接下来写入音频数据(保存文件名,音频格式,声道数,声音位数,频率)返回音频文件流 返回0表示创建失败
    virtual cSTREAM *Create(const char *file_name, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq);

    //读取音频文件信息(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频播放时间 返回0表示读取失败
    virtual unsigned int LoadInfo(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize = GNf::sFILE::DefaultType);
    //读取音频文件数据(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
    virtual uint8 *LoadData(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize = GNf::sFILE::DefaultType);
    //把音频数据保存为文件(  保存文件名  ,  音频格式,声道数,声音位数,频率,数据字节大小, 数据)返回保存是否成功
    virtual bool Save(const char *file_name, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq, unsigned int size, uint8 *data);

    //按文件扩展名检测音频格式是否被引擎支持( 文件扩展名 )
    virtual bool CheckFmtSupportFromExtName(const char *extname);
    //获得能够被引擎支持的音频文件扩展名(返回各个不同的扩展名, 是否追加返回大写的名) !扩展名之间用,隔开 如 "wav,mp3,ogg"
    virtual void GetSupportFmtFileExtName(char *out, bool upper = GN_AddUpperFmtName);
};
extern GN_API iFILE_AUDIO *FileAudio; //首选音频文件插件
}; // namespace GNa
/*-------------------------------------------------------- 图像文件支持插件功能 -------------------------------------------------------------*/
namespace GNg {
typedef uint8 *(*PF_LOAD_IMG_FILE_DATA)(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize);

class GN_API iFILE_IMAGE //图像文件支持插件接口
{
  public:
    iFILE_IMAGE *next_node; //指向下个插件 通常是该类的一系列插件链 当有多个插件加载后 就用这个来把一个个插件连起来 最后一个为0表示结束
    int *module; //插件程序模块

    iFILE_IMAGE(); //构造插件并把它接入到插件链 FileImage 的前面 并使 FileImage = 新构建的插件
    virtual ~iFILE_IMAGE(); //释放插件资源并断开在插件链的连接

    //读取图片文件信息(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否输出缩略图,使用内存模式指定大小)返回缩略图数据指针 返回0表示缩略图读取失败 返回数据内容用完后记得delete []data;
    virtual uint8 *LoadInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out = false, int memsize = GNf::sFILE::DefaultType);
    //读取图片文件数据(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否翻转列象素,使用内存模式指定大小)返回图片数据的指针 返回0表示读取失败 返回数据内容用完后记得delete []data;
    virtual uint8 *LoadData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn = false, int memsize = GNf::sFILE::DefaultType);
    //把图象数据保存为文件（保存文件名,图象数据指针, 原图象颜色格式, 图象宽, 图象高,是否翻转列象素号, 压缩等级0-5 0:不压缩 1:无损压缩 其它值越大压缩越多) 返回保存是否成功
    virtual bool Save(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn = false, uint8 comps_lv = 0);

    //按文件扩展名检测图像格式是否被引擎支持( 文件扩展名 )如能支持返回读取该格式的图像读取方法 否则返回0
    virtual PF_LOAD_IMG_FILE_DATA CheckFmtSupportFromExtName(const char *extname);
    //获得能够被引擎支持的图像文件扩展名(返回各个不同的扩展名, 是否追加返回大写的名)  !扩展名之间用,隔开 如 "gim,png,bmp,tga"
    virtual void GetSupportFmtFileExtName(char *out, bool upper = GN_AddUpperFmtName);
};
extern GN_API iFILE_IMAGE *FileImage; //首选图像文件插件
}; // namespace GNg
/********************************************************* 变量值调整信息 ****************************************************************/
namespace GN {
enum eVALUE_TYPE { //变量类型
    eVALUE_INT, //整形值
    eVALUE_FLOAT, //浮点值
    eVALUE_ULONG, //无符号整形值
    eVALUE_BYTE //单字节无符号整形值
};
inline unsigned int ValueTypeSize(eVALUE_TYPE t) {
    unsigned int tsizes[] = {4, 4, 4, 1};
    return tsizes[t];
}
struct sVALUE_ADDR //值地址
{
    unsigned int size; //值占大小字节
    void *addr; //地址
};
//从数据包里读入一组值(一组值地址 最尾一个size为0表示结束,  数据包)返回读取完数据包的末尾指针
inline uint8 *ValuesRead(sVALUE_ADDR *values, uint8 *data) {
    while(values->size) {
        memcpy(values->addr, data, values->size);
        ++values;
        data += values->size;
    }
    return data;
}
//写入一组值到数据包里(一组值地址 最尾一个size为0表示结束,  数据包)返回写入完数据包的末尾指针
inline uint8 *ValuesWrite(sVALUE_ADDR *values, uint8 *data) {
    while(values->size) {
        memcpy(data, values->addr, values->size);
        ++values;
        data += values->size;
    }
    return data;
}
struct sVALUE_NAME //变量名
{
    const char *name; //变量名
    uint8 type; //变量类型
};
template <class T, uint8 TT>
struct sADJUST_VALUE : public sVALUE_NAME //调整变量
{
    T *ptr; //指定修改值地址
    T max_value; //最大值
    T min_value; //最小值
    T add_value; //每次增加量

    sADJUST_VALUE() { type = TT; }
    sADJUST_VALUE(const char *_name, T *_ptr, T _max_v, T _min_v, T _add_v) : ptr(_ptr), max_value(_max_v), min_value(_min_v), add_value(_add_v) {
        name = _name;
        type = TT;
    }
};
template <class T1, uint8 T1T>
struct sADJUST_VALUE_I : public sADJUST_VALUE<T1, T1T> //调整变量I
{
    const char *enum_name; //列举名:"YES|NO"用这格式可以填入变量筐里面

    sADJUST_VALUE_I() {}
    sADJUST_VALUE_I(const char *_name, T1 *_ptr, T1 _max_v, T1 _min_v, T1 _add_v, const char *_enum_name) : sADJUST_VALUE<T1, T1T>(_name, _ptr, _max_v, _min_v, _add_v), enum_name(_enum_name) {}
};
typedef sADJUST_VALUE_I<int, eVALUE_INT> sADJUST_VALUE_INT; //整形调整变量
typedef sADJUST_VALUE<float, eVALUE_FLOAT> sADJUST_VALUE_FLOAT; //浮点调整变量
typedef sADJUST_VALUE<unsigned int, eVALUE_ULONG> sADJUST_VALUE_ULONG; //无符号整形调整变量
typedef sADJUST_VALUE_I<uint8, eVALUE_BYTE> sADJUST_VALUE_BYTE; //单字节无符号整形调整变量

struct GN_API sADJUST_VALUE_GROUP //调整变量集
{
    sVALUE_NAME **args; //变量列表 你可先判断 sVALUE_NAME::type 以确定对应的数据结构 sADJUST_VALUE_INT 或其它相关类型 以便转换指针
    uint8 argn; //变量个数

    sADJUST_VALUE_GROUP() : args(0) {}
    ~sADJUST_VALUE_GROUP();
};
//从数据包里读入一组变量值(数据包, 变量列表, 变量数)返回已读取字节数
GN_API unsigned int ValuesRead(const uint8 *data, sVALUE_NAME **args, uint8 argn);

//写入一组变量值到数据包里(数据包 需分配好足够空间, 变量列表, 变量数)返回写入字节数 !参数data可指定为0 这样就只统计数据包需要的字节数而不写入
GN_API unsigned int ValuesWrite(uint8 *data, sVALUE_NAME **args, uint8 argn);

//检测一组变量值与对应数据包里的值使不相同的执行指定回调((数据包, 变量列表, 变量数, 回调方法(告诉您有差异变量, 告诉您对应数据包值地址, 传回给您的用户参数), 用户参数)
GN_API void ValuesNotEqualExec(const uint8 *data, sVALUE_NAME **args, uint8 argn, void (*proc)(sVALUE_NAME *, void *, void *), void *us_arg);

//打印变量名和值(变量, 另外的打印值, 是否使用另外的打印值)
GN_API void ValuePrint(sVALUE_NAME *value, void *v, void *ptv);

//设置对象指定属性给其它多个同类型对象(多个同类型对象, 对象地址, 对象指定属性地址, 属性大小字节)
inline void GN_SetObjAttrbToObjs(P<P_> &objs, uint8 *p_obj, uint8 *p_attrb, unsigned int size) {
    P_ *_objs = objs;
    unsigned int i = 0, n = objs.Num(), mem_ofs = p_attrb - p_obj;
    for(; i < n; ++i) memcpy((uint8 *)(_objs[i].zData->pmem) + mem_ofs, p_attrb, size);
}
template <class T>
//保存对象组的引用ID数据(当前写入位置, 对象组, 收集到的不相同对象)返回最后的写完位置
uint8 *ObjectRefIdSave(uint8 *wpos, P<T> &objs, sLIST<T> &collect) {
    unsigned int obi = 0, obn = objs.Num();
    *((unsigned int *)wpos) = obn;
    wpos += 4;
    T *_objs = objs;
    for(; obi < obn; ++obi) {
        *((unsigned int *)wpos) = (_objs[obi] ? collect.add_no_eq2(_objs[obi]) : 0xffffffff);
        wpos += 4;
    }
    return wpos;
}
template <class T>
//读取对象组的引用ID数据 并设置它引用到的对象(当前读取位置, 对象组, 对象引用源)返回最后的读完位置
uint8 *ObjectRefIdLoad(uint8 *rpos, P<T> &objs, P<T> &objs_ref) {
    unsigned int id, obi = 0, obn = *((unsigned int *)rpos);
    rpos += 4;
    objs.New(obn);
    T *_objs = objs;
    for(; obi < obn; ++obi) {
        if(*((unsigned int *)rpos) != 0xffffffff) _objs[obi] = objs_ref[*((unsigned int *)rpos)];
        rpos += 4;
    }
    return rpos;
}
template <class T>
struct sVALUE_LAYOUT_ //数值布局
{
    T pos_tg; //数值安插到目标的位置字节偏移
    T pos; //数值所在位置字节偏移
    T size; //数值大小字节
    T stride; //下个同类数值访问跨距字节
};
typedef sVALUE_LAYOUT_<int> sVALUE_LAYOUT; //高精度数值布局
typedef sVALUE_LAYOUT_<short> sVALUE_LAYOUT2; //中精度数值布局
}; // namespace GN
/**************************************************** zip 压缩解压摸块 *********************************************************************/
#ifdef GN_USING_UNZIP
extern GN::sNUMBER<uint8> GT_ZipIDs;
extern void *GT_ZipPacks[32];

namespace GNd {

class GN_API cZIP {
    void *zFile; //压缩文件
    int zLevel; //压缩率值
  public:
    cZIP() : zFile(0) {}
    ~cZIP() {
        if(zFile) Close();
    }

    //创建打开压缩文件(压缩包文件名或内存地址, 使用内存模式就指定大小, 压缩等级值:0-9值越大压缩率越高但占时间越多 值:-1自动适应, 压缩文件是否添加到一个旧有的任意文件尾部:该功能通常用于制作自解压安装程序) 返回0:成功 -1:失败
    int Open(const char *filename_mem, int memsize = GNf::eFILE_STD, int level = -1, int is_add = 0);
    //关闭压缩文件以便结束文件添加()返回小于等于-1:失败 >= 0:成功  如果调用 cZIP::Open 时设定了压缩到内存时成功则返回内存压缩包的大小 当所有添加文件操作都完成时调用这个关闭压缩文件
    int Close();

    /*    !!!以下函数ZIP包里的相对路径名前面都不要包含'/' 或'\\' 最好目录分隔符都统一用'/' 包里的路径名和实在路径名不一致也可以 不过这样通过解压出来的目录结构跟原本会不一样  */
    //添加一个目录(在ZIP包里的相对路径名, 目录实在的全局路径名:默认值表示该目录是新虚构成的, 如果目录是实存在的是否把该目录下的所有子内容都一并添加到压缩包里) 返回0:成功 小于等于-1:失败
    int AddDir(const char *in_zip_name, const char *g_name = 0, bool add_all_sub__ = true);
    //添加一个目录(在ZIP包里的相对路径名, 目录信息结构指针 需指定好里面的全局路径, 是否把该目录下的所有子内容都一并添加到压缩包里) 返回0:成功 小于等于-1:失败
    int AddDir(const char *in_zip_name, const GNf::sFILE_INFO *f_info, bool add_all_sub__ = true);
    //添加一个文件(在ZIP包里的相对路径名, 文件实在的全局路径名或内存地址, 使用内存模式就指定大小)返回0:成功 小于等于-1:失败  !!如果这里所指定的包里相对路径文件名用到在包里还没创建的目录 会自动帮您创建好这些目录 使文件总能放到期待的位置上
    int AddFile(const char *in_zip_name, const char *g_name_mem, int memsize = GNf::eFILE_STD);
    //添加一个文件(在ZIP包里的相对路径名, 文件信息结构指针 )返回0:成功 小于等于-1:失败  !!如果这里所指定的包里相对路径文件名用到在包里还没创建的目录 会自动帮您创建好这些目录 使文件总能放到期待的位置上
    int AddFile(const char *in_zip_name, const GNf::sFILE_INFO *f_info);
};

//打开并锁定一个zip压缩文件( zip包文件名或内存地址, 使用内存模式就指定大小) 成功返回该zip的索引ID号 失败返回0  该返回值常是提供给GNf::Open函数中的参数us_type_size 这样以后在压缩包里面的文件能映射为象操作普通文件一样的使用(仅限于读取) 最多同时打开32个压缩包
GN_API int ZipPackOpen(const char *zip_pak_name_mem, int memsize = GNf::eFILE_STD);

//关闭一个指定索引ID的zip压缩文件( zip索引ID )
GN_API void ZipPackClose(int zp_id);

//在zip包内读取指定名的文件( zip包文件名或内存地址, 索取的包内文件名, 返回读出文件的大小, 使用内存模式就指定大小) 返回文件数据内容用完后记得delete []data;
GN_API uint8 *UnzipLoadFileData(const char *zip_pak_name_mem, const char *inzipfile, unsigned int *size, int memsize = GNf::eFILE_STD);

//解压zip包到指定目录路径(zip包文件名或内存地址, 解压到的目录路径:0为当前目录下, 使用内存模式就指定大小, 解压后是否自动删除包文件)  成功返回0 失败返回-1:创建输出目录失败  -2:获取当前文件信息失败  -3:创建子目录失败  -4:读取压缩包里的文件失败  -5:解压数据时失败 错误原因会写到 GN_ErrorLast 里
GN_API int Unzip(const char *zip_pak_name_mem, const char *out_path = 0, int memsize = GNf::eFILE_STD, bool remove_pak = false);

//当 cZIP::AddDir 执行时 该变量用来记录当前正在压缩的文件名相对于压缩包内的路径 当Unzip 函数 执行时 该变量用来记录当前正在解压的文件名
extern GN_API char ZipCurProcFileName[];

//跟踪正在解压或压缩的文件进度 0.0f - 1.0f
extern GN_API float ZipCurProcFileProgress;

//当解压缩文件解压到一个文件开始时的处理函数指针(当前解压到的文件路径, 当前正在解压缩的文件对象, 当前准备要解压的文件大小)要求返回是否要解压这个文件出来 !!这里告诉你的文件路径参数 没必要自己另外再设置文件读写目录状态 直接拿来用即可
extern GN_API bool (*OnUnZipFileBeginCallBack)(const char *tofilename, const void **unz, int f_size);
//当解压缩文件解压完一个文件之后的处理函数指针(当前解压完的文件路径) !!这里告诉你的文件路径参数 没必要自己另外再设置文件读写目录状态 直接拿来用即可
extern GN_API void (*OnUnZipFileEndCallBack)(const char *tofilename);

}; // namespace GNd
//以下这几个函数只是简单在 libzip 里引出而已

//打开zip包里的特定文件( 包文件句柄 )
GN_API int GNd_UnzOpenCurrentFile(void *file);
//读取zip包里的特定文件( 包文件句柄, 存放数据的内存, 读取大小字节数 )
GN_API int GNd_UnzReadCurrentFile(void *file, void *buf, unsigned len);
//关闭zip包里的特定文件( 包文件句柄 )
GN_API int GNd_UnzCloseCurrentFile(void *file);
#endif

#ifndef GN_USING_ADDITIONAL_LIB
GN_API P<uint8> GN_SecrecyToData(uint8 *data_in, unsigned int size, unsigned int sk);
GN_API P<uint8> GN_SecrecyUnData(uint8 *data_in, unsigned int size, unsigned int sk);
GN_API char *GN_SecrecyToText(unsigned int &new_len_out, char *text_in, unsigned int len, uint8 sk);
GN_API char *GN_SecrecyUnText(unsigned int &new_len_out, char *text_in, unsigned int len, uint8 sk);
#endif
#endif //endif _GN_FORMAT_H_
