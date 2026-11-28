// GN_mini 图像编解码片段：PNG 读取器（libpng）+ 加载/保存。
// 2026-11-26 自 GN_FormatImgFile.cpp 第 415-606 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_FormatImgFile.cpp 按原顺序包含（单编译单元）；不是独立头文件。
    struct GT_sRPNG_FILE //PNG文件数据读取器
    {
        GNf::sFILE *fp; //文件
        uint8 zPngReadBuf[16]; //用来交换gim宽和高参数用
        unsigned int zPngReadN; //用来作跨函数传递临时参数用, 读取gim文件时用到
    };
    inline void GT_PngReadCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
        GT_sRPNG_FILE *rpf = (GT_sRPNG_FILE *)(png_ptr->io_ptr);
        GNf::Read((char *)data, 1, length, rpf->fp);
        rpf->zPngReadN++;
        if(rpf->zPngReadN <= 3) {
            uint8 ihdr[] = {0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52}; //增补PNG第二个头标识 "IHDR"
            switch(rpf->zPngReadN) {
                case 1: memcpy(data, ihdr, length); break;
                case 2: memcpy(data, ihdr + 4, length); break;
                case 3: memcpy(data, rpf->zPngReadBuf, length); break;
            }
        }
    }
    GN_API uint8 *GNg::LoadPngFileInfo(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool min_out, int memsize) {
        *w = GN_INT_MIN;
        *h = 2147483637 /*GN_INT_MAX-10*/; //用于提示仅获取信息即可
        return LoadPngFileData(filename_mem, format, w, h, 0, memsize);
    }
    GN_API uint8 *GNg::LoadPngFileData(char *filename_mem, COLOR_FORMAT *format, int *w, int *h, bool col_turn, int memsize) {
        GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize);
        if(!fp) {
            //GN_Log("读取 png 文件失败! 不能打开该文件或该文件不存在!\n""read_png_file File  could not be opened for reading\n");
            return 0;
        }
        uint8 header[16];
        GNf::Read((char *)header, 1, 4, fp);
        GNf::Seek(fp, 0, GNf::eSEEK_START);
        GT_sRPNG_FILE rpf;
        rpf.fp = fp;
        rpf.zPngReadN = 1000000;
        GNf::Read((char *)header, 1, 16, fp);
        if(png_sig_cmp((png_bytep)header, 0, 8) || strncmp((char *)(header + 12), "IHDR", 4)) {
            GN_Log("读取 png 文件失败! 文件不是有效的PNG格式文件\n");
            GNf::Close(fp);
            return 0;
        }
        GNf::Seek(fp, 8, GNf::eSEEK_START);

        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); //获取图像头数据
        if(!png_ptr) {
            GN_Log("读取 png 文件失败! 创建读取头失败!\n");
            GNf::Close(fp);
            return 0;
        }
        png_infop info_ptr = png_create_info_struct(png_ptr); //获取图像信息头
        if(!info_ptr) {
            GN_Log("读取 png 文件失败! 创建信息头失败!\n");
            png_destroy_read_struct(&png_ptr, &info_ptr, 0);
            GNf::Close(fp);
            return 0;
        }
        png_set_read_fn(png_ptr, &rpf, GT_PngReadCallback); //设置读数据的回调函数
        png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);
        if(info_ptr->bit_depth != 8) {
            GN_Log("读取 png 文件失败! 该文件使用了非 8位/通道 的格式!\n");
            png_destroy_read_struct(&png_ptr, &info_ptr, 0);
            GNf::Close(fp);
            return 0;
        }
        int th = info_ptr->height;
        switch(info_ptr->color_type) {
            case PNG_COLOR_TYPE_RGB_ALPHA: *format = GNg::CR_RGBA; break;
            case PNG_COLOR_TYPE_RGB: *format = GNg::CR_RGB; break;
            case PNG_COLOR_TYPE_GRAY_ALPHA: *format = GNg::CR_ALPHA; break;
            case PNG_COLOR_TYPE_GRAY: *format = GNg::CR_LUMINANCE; break;
            case PNG_COLOR_TYPE_PALETTE: *format = GNg::CR_PALETTE; break;
        }
        uint8 *tmp_data = 0;
        if(*w == GN_INT_MIN && *h == 2147483637) {
            ;
        } else {
            uint8 loc_buf[65536]; //64K
            GN::sTBUF_INT temp_buf(loc_buf, 65536);
            int row_bytes = info_ptr->rowbytes;
            tmp_data = new uint8[th * row_bytes]; //分配数据资源
            uint8 **row_ptrs = (uint8 **)temp_buf.GetSpace(sizeof(uint8 *) * th), *data1;
            int i = 0, th1 = th - 1;
            if(col_turn)
                for(data1 = tmp_data + (th1 * row_bytes); i < th; ++i, data1 -= row_bytes) row_ptrs[i] = data1;
            else /*----*/
                for(data1 = tmp_data; i < th; ++i, data1 += row_bytes) row_ptrs[i] = data1;
            png_read_image(png_ptr, row_ptrs); //读取图像数据
            //png_read_end(png_ptr, info_ptr);  //读取PNG结束字串
        }
        *w = info_ptr->width;
        *h = th;
        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        GNf::Close(fp);
        return tmp_data;
    }
    inline void GT_PngWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
        GNf::Write((char *)data, 1, length, (GNf::sFILE *)png_ptr->io_ptr);
    }
    GN_API bool GNg::SavePngFile(char *file_name, uint8 *data, COLOR_FORMAT srcformat, int w, int h, bool col_turn, unsigned int dateID) {
        GNf::sFILE *fp = GNf::Open(file_name, "wb", GNf::eFILE_STD);
        if(!fp) {
            GN_Log(GT_ERR_FileCreateFail, "png", file_name);
            return false;
        }
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if(!png_ptr) {
            GN_Log("写入 png 文件失败! 创建写入头失败!\n");
            GNf::Close(fp);
            return false;
        }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if(!info_ptr) {
            GN_Log("写入 png 文件失败! 创建信息头失败!\n");
            png_destroy_write_struct(&png_ptr, &info_ptr);
            GNf::Close(fp);
            return false;
        }
        //png_init_io(png_ptr, (FILE*)fp->handle_buf);
        png_set_write_fn(png_ptr, fp, GT_PngWriteCallback, 0); /*设置写数据的回调函数*/
        if(setjmp(png_jmpbuf(png_ptr))) {
            GN_Log("写入 png 文件失败! 写入信息头失败!\n");
            png_destroy_write_struct(&png_ptr, &info_ptr);
            GNf::Close(fp);
            return false;
        }
        int row_byte;
        png_byte color_type;
        uint8 *tmp_data = 0;
        if(srcformat == CR_RGB565 || srcformat == CR_LUMINANCE_ALPHA) { //转换为PNG读写规格并转换图象数据
            COLOR_FORMAT d_format;
            if(srcformat == CR_RGB565) {
                d_format = CR_RGB;
                color_type = PNG_COLOR_TYPE_RGB;
            } else {
                d_format = CR_RGBA;
                color_type = PNG_COLOR_TYPE_RGB_ALPHA;
            }
            row_byte = w * ImgFmtGetByteNum(d_format);
            tmp_data = new uint8[h * row_byte];
            ImgFmtConvertTs(tmp_data, d_format, data, srcformat, h * w);
            data = tmp_data;
        } else { //转换为PNG读写规格
            switch(srcformat) { //各种颜色类型的处理
                case CR_RGBA: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
                case CR_RGB: color_type = PNG_COLOR_TYPE_RGB; break;
                case CR_ALPHA: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
                case CR_LUMINANCE:
                    color_type = PNG_COLOR_TYPE_GRAY;
                    break;
                    //case CR_RGBA4444: break;
                    //case CR_RGBA5551: break;
                            default: break; // B9-3: 未列举枚举值兜底（保行为）
}
            row_byte = w * ImgFmtGetByteNum(srcformat);
        }
        uint8 loc_buf[65536]; //64K
        GN::sTBUF_INT temp_buf(loc_buf, 65536);
        uint8 **row_ptrs = (uint8 **)temp_buf.GetSpace(sizeof(uint8 *) * h);
        int i, h1 = h - 1;
        if(col_turn)
            for(i = 0; i < h; ++i) row_ptrs[i] = data + ((h1 - i) * row_byte);
        else /*----*/
            for(i = 0; i < h; ++i) row_ptrs[i] = data + (i * row_byte);

        png_set_IHDR(png_ptr, info_ptr, w, h,
                     8, color_type, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        bool ret = true;
        png_write_info(png_ptr, info_ptr);
        /*if(setjmp(png_jmpbuf(png_ptr))){
	    ret = false;
		GN_Log("写入 png 文件失败! 写入数据时有错!\n");
		goto GT_GOTO_ENDwritePNG;
	}*/
        png_write_image(png_ptr, row_ptrs); //写数据

        if(setjmp(png_jmpbuf(png_ptr))) {
            ret = false;
            GN_Log("写入 png 文件失败! 写入结束标记时有错!\n");
        }
        //结束并释放写缓冲
        png_write_end(png_ptr, info_ptr);
            png_destroy_write_struct(&png_ptr, &info_ptr);

        //清除缓存数据
        if(tmp_data) delete[] tmp_data;
        GNf::Close(fp);
        return ret;
    }
