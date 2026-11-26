// GN_mini 图像编解码片段：BMP 文件头结构 + 加载/保存。
// 2026-11-26 自 GN_FormatImgFile.cpp 第 22-163 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_FormatImgFile.cpp 按原顺序包含（单编译单元）；不是独立头文件。

#pragma pack(push, 2) //设置编译器编译结构体两字节对齐
    struct GT_sBMP_HEADER //bmp文件头
    {
        unsigned short type; //类型表示
        unsigned int size; //文件字节大小
        unsigned short reserved1; //保留占位
        unsigned short reserved2; //保留占位
        unsigned int offBits; //图象数据地址相对文件开始地址的偏移字节
    };
#pragma pack(pop) //取消刚才编译结构体对齐设置
    struct GT_sBMP_INFO //bmp文件信息
    {
        unsigned int size; //信息结构的大小字节数
        int w; //图象宽
        int h; //图象高
        unsigned short planes; //图象帧数
        unsigned short bit; //颜色位数
        unsigned int compression; //压缩方式 0为无压缩 也是bmp数据常态
        unsigned int sizeImage; //图象数据大小字节
        int xPelsPerMeter; //水平分辨率
        int yPelsPerMeter; //垂直分辨率
        unsigned int clrUsed; //调色板颜色数
        unsigned int clrImportant; //保留占位
    };
    //	uint8	rgbBlue,rgbGreen,rgbRed,rgbReserved; //GT_sBMP_INFO 之后如有调色板 接下来的均是调色板各个颜色 BGRX

    const char *GT_ERR_FileCreateFail = "写入 %s 文件 %s 失败! 文件创建失败!\n";

    //读取BMP图片文件信息(bmp文件信息, 图片文件名或内存地址,使用内存模式指定大小, 读完是否一并关闭文件 否则就把文件指针跳转到数据区)返回正在打开的文件 返回0表示读取失败
    GNf::sFILE *GT_LoadBmpFileInfo(char *filename_mem, GNg::COLOR_FORMAT *format, int *w, int *h, int *fdsize, bool close, int memsize) {
        GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize); //打开文件 filename_mem 和 memsize 可以是磁盘文件 或压缩包 或 内存数据块 具体开 memsize 怎么表示
        if(!fp) return 0; //文件打开失败
        GT_sBMP_HEADER bmp_head; //定义bmp文件头结构
        char *type_str = (char *)(&bmp_head);
        GNf::Read(type_str, sizeof(GT_sBMP_HEADER), 1, fp); //读取文件头
        if(type_str[0] != 'B' || type_str[1] != 'M') { //首两个字节如不是 BM 即不是bmp文件
            GNf::Close(fp); //关闭文件
            return 0; //返回效验失败
        }
        GT_sBMP_INFO bmp_info; //定义bmp文件信息块结构
        GNf::Read((char *)(&bmp_info), sizeof(GT_sBMP_INFO), 1, fp); //读取文件信息块
        if(bmp_info.h < 0) bmp_info.h = -bmp_info.h; //图象高有些软件输出可能为负数就反转为正数
        *w = bmp_info.w;
        *h = bmp_info.h; //返回图片的宽高
        *format = GNg::ImgFmtGetFormat(bmp_info.bit); //把颜色位数转为引擎表示的颜色格式
        *fdsize = bmp_info.sizeImage; //返回图像文件数据大小

        if(close) GNf::Close(fp); //要求关闭就关闭文件
        else GNf::Seek(fp, bmp_head.offBits, GNf::eSEEK_START); //否则使文件指针跳转到数据区 以便接下来读取
        return fp; //返回文件
    }
    GN_API uint8 *GNg::LoadBmpFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out, int memsize) {
        int fdsize;
        GT_LoadBmpFileInfo(filename_mem, format, w, h, &fdsize, true, memsize); //读取文件信息后就要求顺便关闭文件
        return 0; //暂时不处理缩略图返回0
    }
    GN_API uint8 *GNg::LoadBmpFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        int fdsize;
        GNf::sFILE *fp = GT_LoadBmpFileInfo(filename_mem, format, w, h, &fdsize, false, memsize); //读取bmp文件信息并获得正在打开的文件
        if(!fp) return 0;
        int tw = *w, th = *h; //宽高接下来频繁访问 把它们传到局部变量处理更高效
        unsigned int px_byte = ImgFmtGetByteNum(*format); //获取指定颜色格式的单个像素占字节数
        unsigned int pitch = px_byte * (unsigned int)tw; //计算到图像一行数据所占字节数
        int size = th * pitch; //计算到图像数据总大小
        uint8 *tmp_data = new uint8[size]; //分配一块数据以用来保存图像
        unsigned int sk = 0;
        if(fdsize > size /* + 2*/) sk = (fdsize - (size /* + 2*/)) / th; //计算出图像文件数据里一行多出的字节数 bmp文件数据通常每行以4字节的倍数对齐
        //GN_Log("sizeImage=%d, sk=%d\n", bmp_info.sizeImage, sk);	//打印图像文件数据大小和一行多出的字节数到 RunLog.txt 文件里

        int i;
        if(col_turn && sk) { //当要求翻转列象素且有行多出的字节
            for(i = th - 1; i >= 0; --i) { //接收数据从最末行开始历遍到首行
                GNf::Read((char *)(tmp_data + (i * pitch)), 1, pitch, fp); //读出一行行数据
                GNf::Seek(fp, sk, GNf::eSEEK_CUR); //跳过多余的对齐字节
            }
        } else if(col_turn) { //当要求翻转列象素
            for(i = th - 1; i >= 0; --i) //接收数据从最末行开始历遍到首行
                GNf::Read((char *)(tmp_data + (i * pitch)), 1, pitch, fp); //读出一行行数据

        } else if(sk) { //仅有行多出字节
            for(i = 0; i < th; ++i) { //接收数据从首行开始历遍到最末行
                GNf::Read((char *)(tmp_data + (i * pitch)), 1, pitch, fp); //读出一行行数据
                GNf::Seek(fp, sk, GNf::eSEEK_CUR); //跳过多余的对齐字节
            }
        } else { //没反转也没行多出的字节
            if(GNf::Read((char *)tmp_data, 1, size, fp) <= 0) { //整片数据一次过读取
                GN_DeleteArray(tmp_data); //当读取失败时删除刚分配的数据 并使 tmp_data 为0 以便待会返回0
                GN_Log("读取 bmp 文件数据时有错!\n"); //打印图像读取失败信息到 RunLog.txt 文件里
                GNf::Close(fp); //退出前先关闭文件
                return 0; //失败退出
            }
        }
        uint8 *_data = tmp_data, *endd = tmp_data + size;
        for(; _data < endd; _data += px_byte) GN_Swap(uint8, _data[0], _data[2]); //历遍每个像素数据 使红蓝分量值调换一下 即把bmp颜色数据默认的BGR转为引擎用的RGB !您也可以调用GNg::ImgFmtConvertTs函数来转换 但该函数输入输出参数不能为同一指针

        GNf::Close(fp); //完成就关闭文件
        return tmp_data; //返回图像数据
    }
    GN_API bool GNg::SaveBmpFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn) {
        GNf::sFILE *fp = GNf::Open(file_name, "wb", GNf::eFILE_STD); //打开创建文件(路径文件名, 以写入二进制方式, 创建于标准本地磁盘文件)
        if(!fp) {
            GN_Log(GT_ERR_FileCreateFail, "bmp", file_name);
            return false;
        } //文件创建失败就打印失败原因到 RunLog.txt 文件里
        GT_sBMP_HEADER bmp_head; //定义bmp文件头结构
        GT_sBMP_INFO bmp_info; //定义bmp文件信息结构
        int sbh = sizeof(GT_sBMP_HEADER);
        int sbi = sizeof(GT_sBMP_INFO);
        memset(&bmp_head, 0, sbh); //使文件头结构里的所有属性先设为0
        memset(&bmp_info, 0, sbi); //使文件信息结构里的所有属性先设为0
        int pixel_byte = (int)ImgFmtGetByteNum(srcformat); //获取指定颜色格式的单个像素占字节数
        bmp_info.bit = (unsigned short)pixel_byte << 3; //颜色位数=像素占字节数乘8   <<3相当于乘8 但性能更好
        bmp_info.w = w;
        bmp_info.h = h; //设置图片宽高
        bmp_info.planes = 1; //只有1帧图像
        bmp_info.size = 40; //文件信息结构
        bmp_info.xPelsPerMeter = 2874; //常规水平分辨率
        bmp_info.yPelsPerMeter = 2874; //常规水平分辨率
        unsigned int data_size = (unsigned int)pixel_byte * (unsigned int)w * (unsigned int)h; //计算到图像数据总大小
        bmp_info.sizeImage = data_size; //指定图像数据大小
        //unsigned int   v = data_size % 4;
        //if(v)bmpI.sizeImage += 4 - v;
        bmp_head.type = 0x4d42; //文件效验标签 "BM" 的16进制
        bmp_head.offBits = 54; //无调色板的数据区在文件开始的偏移字节位置
        bmp_head.size = 54 + bmp_info.sizeImage; //文件总大小字节
        GNf::Write((char *)&bmp_head, sbh, 1, fp); //写入文件头信息
        GNf::Write((char *)&bmp_info, sbi, 1, fp); //写入文件信息
        uint8 *data2 = new uint8[data_size]; //分配一块图像数据大小的内存
        GNg::ImgFmtConvertTs(data2, srcformat, data, srcformat, w * h, true); //使红蓝分量值调换一下 即把引擎用的颜色数据RGB转为bmp默认的BGR

        if(col_turn) { //当要求翻转列象素
            unsigned int pitch = pixel_byte * (unsigned int)w; //计算到图像一行数据所占字节数
            int i = h - 1; //写入数据从最末行开始历遍到首行
            for(; i >= 0; --i) GNf::Write((char *)(data2 + (i * pitch)), 1, pitch, fp); //要翻转图象就要一行行写

        } else GNf::Write((char *)data2, 1, data_size, fp); //一次过写入整片图像数据

        GNf::Close(fp); //写完关闭文件
        delete[] data2; //删除临时数据
        return true; //返回成功
    }
