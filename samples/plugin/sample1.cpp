#include<GN.h> //使用GN必需包含GN.h头文件

#pragma pack(push,2)			//设置编译器编译结构体两字节对齐
struct GT_sBMP_HEADER				//bmp文件头
{
	unsigned short       type;           //类型表示
    unsigned int         size;           //文件字节大小
    unsigned short       reserved1;      //保留占位
    unsigned short       reserved2;      //保留占位
    unsigned int         offBits;        //图象数据地址相对文件开始地址的偏移字节
};
#pragma pack(pop)				//取消刚才编译结构体对齐设置
struct GT_sBMP_INFO					//bmp文件信息
{
    unsigned int         size;           //信息结构的大小字节数
    int                  w;              //图象宽
    int                  h;              //图象高
    unsigned short       planes;         //图象帧数
    unsigned short       bit;            //颜色位数
    unsigned int         compression;	 //压缩方式 0为无压缩 也是bmp数据常态
    unsigned int         sizeImage;		 //图象数据大小字节
    int                  xPelsPerMeter;  //水平分辨率
    int                  yPelsPerMeter;  //垂直分辨率
    unsigned int         clrUsed;		 //调色板颜色数
    unsigned int         clrImportant;	 //保留占位
};
//	uint8	rgbBlue,rgbGreen,rgbRed,rgbReserved; //GT_sBMP_INFO 之后如有调色板 接下来的均是调色板各个颜色 BGRX

class iFILE_IMAGE_BMP :public GNg::iFILE_IMAGE //bmp图像文件支持插件 继承自接口类 GNg::iFILE_IMAGE
{public:
//				 iFILE_IMAGE_BMP(); //构造插件并把它接入到插件链 FileImage 的前面 并使 FileImage = 新构建的插件
//	virtual		~iFILE_IMAGE_BMP(); //释放插件资源并断开在插件链的连接

	//读取图片文件信息(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否输出缩略图,使用内存模式指定大小)返回缩略图数据指针 返回0表示缩略图读取失败 返回数据内容用完后记得delete []data;
	virtual uint8* LoadInfo(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool min_out ,int memsize);
	//读取图片文件数据(图片文件名或内存地址,返回颜色格式,返回宽, 返回高, 是否翻转列象素,使用内存模式指定大小)返回图片数据的指针 返回0表示读取失败 返回数据内容用完后记得delete []data;
	virtual uint8* LoadData(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize);
	//把图象数据保存为文件（保存文件名,图象数据指针, 原图象颜色格式, 图象宽, 图象高,是否翻转列象素号, 压缩等级0-5 0:不压缩 1:无损压缩 其它值越大压缩越多) 返回保存是否成功
	virtual  bool  Save(char *file_name,uint8 *data,GNg::COLOR_FORMAT srcformat,int w,int h,bool col_turn=false, uint8 comps_lv=0);

	//按文件扩展名检测图像格式是否被引擎支持( 文件扩展名 )如能支持返回读取该格式的图像读取方法 否则返回0
	virtual GNg::PF_LOAD_IMG_FILE_DATA CheckFmtSupportFromExtName(const char *extname);
	//获得能够被引擎支持的图像文件扩展名(返回各个不同的扩展名, 是否追加返回大写的名)
	virtual void GetSupportFmtFileExtName(char *out, bool upper);
};
//读取BMP图片文件信息(bmp文件信息, 图片文件名或内存地址,使用内存模式指定大小, 读完是否一并关闭文件 否则就把文件指针跳转到数据区)返回正在打开的文件 返回0表示读取失败
GNf::sFILE *LoadBmpFileInfo2(char *filename_mem, GNg::COLOR_FORMAT *format, int *w,int *h,int *fdsize, bool close, int memsize){
    GNf::sFILE *fp = GNf::Open(filename_mem,"rb",memsize); //打开文件 filename_mem 和 memsize 可以是磁盘文件 或压缩包 或 内存数据块 具体看 memsize 怎么表示
	if (!fp)return 0;			//文件打开失败
	GT_sBMP_HEADER bmp_head;	//定义bmp文件头结构
	char *type_str = (char*)(&bmp_head);
	GNf::Read(type_str, sizeof(GT_sBMP_HEADER), 1, fp);  //读取文件头
	if(type_str[0] != 'B' || type_str[1] != 'M'){		 //首两个字节如不是 BM 即不是bmp文件
		GNf::Close(fp);			//关闭文件
		return 0;				//返回效验失败
	}
	GT_sBMP_INFO   bmp_info;	//定义bmp文件信息块结构
	GNf::Read((char*)(&bmp_info), sizeof(GT_sBMP_INFO), 1, fp);  //读取文件信息块
	if(bmp_info.h < 0)bmp_info.h = -bmp_info.h;					 //图象高有些软件输出可能为负数就反转为正数
	*w = bmp_info.w; *h = bmp_info.h;			  //返回图片的宽高
	*format = GNg::ImgFmtGetFormat(bmp_info.bit); //把颜色位数转为引擎表示的颜色格式
	*fdsize  = bmp_info.sizeImage;				  //返回图像文件数据大小

	if(close)GNf::Close(fp); //要求关闭就关闭文件
	else GNf::Seek(fp, bmp_head.offBits, GNf::eSEEK_START); //否则使文件指针跳转到数据区 以便接下来读取
	return fp;				 //返回文件
}
//读取BMP图片文件数据(图片文件名或内存地址,返回颜色格式,返回宽,返回高,是否翻转列象素,使用内存模式指定大小)返回图片数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
uint8* LoadBmpFileData2(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
	int fdsize;
	GNf::sFILE *fp = LoadBmpFileInfo2(filename_mem, format, w, h, &fdsize, false, memsize); //读取bmp文件信息并获得正在打开的文件
	int tw = *w, th = *h;				//宽高接下来频繁访问 把它们传到局部变量处理更高效
	unsigned int px_byte = ImgFmtGetByteNum(*format);  //获取指定颜色格式的单个像素占字节数
	unsigned int  pitch  = px_byte * (unsigned int)tw; //计算到图像一行数据所占字节数
	int			  size   = th * pitch;  //计算到图像数据总大小
	uint8 *tmp_data = new uint8[size];  //分配一块数据以用来保存图像
	unsigned int  sk = 0;
	if(fdsize > size/* + 2*/)sk = (fdsize - (size/* + 2*/))/th; //计算出图像文件数据里一行多出的字节数 bmp文件数据通常每行以4字节的倍数对齐
    //GN_Log("sizeImage=%d, sk=%d\n", bmp_info.sizeImage, sk);	//打印图像文件数据大小和一行多出的字节数到 RunLog.txt 文件里

	int i;
	if(col_turn && sk){  //当要求翻转列象素且有行多出的字节
		for(i = th-1; i >= 0; --i){	//接收数据从最末行开始历遍到首行
			GNf::Read((char*)(tmp_data+(i*pitch)), 1, pitch, fp); //读出一行行数据
			GNf::Seek(fp,sk,GNf::eSEEK_CUR);					  //跳过多余的对齐字节
		}
	}else if(col_turn){  //当要求翻转列象素
	    for(i = th-1; i >= 0; --i)  //接收数据从最末行开始历遍到首行
			GNf::Read((char*)(tmp_data+(i*pitch)), 1, pitch, fp); //读出一行行数据

	}else if(sk){		 //仅有行多出字节
	    for(i = 0; i < th; ++i){	//接收数据从首行开始历遍到最末行
			GNf::Read((char*)(tmp_data+(i*pitch)), 1, pitch, fp); //读出一行行数据
			GNf::Seek(fp,sk,GNf::eSEEK_CUR);					  //跳过多余的对齐字节
		}
	}else{				 //没反转也没行多出的字节
		if(GNf::Read((char*)tmp_data, 1, size, fp) <= 0){ //整片数据一次过读取
			GN_DeleteArray(tmp_data);				//当读取失败时删除刚分配的数据 并使 tmp_data 为0 以便待会返回0
			GN_Log("读取 bmp 文件数据时有错!\n");	//打印图像读取失败信息到 RunLog.txt 文件里
			GNf::Close(fp);						    //退出前先关闭文件
			return 0;								//失败退出
		}
	} 
	// ###### 注释掉这两行以区分本插件的读取方法和引擎默认的bmp文件读取方法
	//uint8 *_data = tmp_data, *endd = tmp_data + size;
	//for(; _data < endd; _data += px_byte)GN_Swap(uint8,_data[0],_data[2]); //历遍每个像素数据 使红蓝分量值调换一下 即把bmp颜色数据默认的BGR转为引擎用的RGB !您也可以调用GNg::ImgFmtConvertTs函数来转换 但该函数输入输出参数不能为同一指针

    GNf::Close(fp);		 //完成就关闭文件
	return tmp_data;	 //返回图像数据
}
uint8* iFILE_IMAGE_BMP::LoadInfo(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool min_out,int memsize){
	//按插件调用约定 传入的 *format 会定位到文件名的扩展名位置
	int yes = GNf::IsTypeFile(filename_mem, *format, "bmp", "BM", 2, memsize); //判断提供的文件名或文件内容是否指定类型的文件
	if(yes){ //如是 或不确定
		int fdsize;
		GNf::sFILE *fp = LoadBmpFileInfo2(filename_mem, format, w, h, &fdsize, true, memsize); //尝试读取文件信息后就要求顺便关闭文件
		if(fp || yes == 1){ //如调佣成功 或 已确定是bmp文件
			uint8 *min_data = 0;
			if(fp && min_out){ //如调佣成功且有要求返回缩略图
				//在这里读取您得缩略图数据
			} 
			return min_data;  //返回缩略图数据
		}
	} //以上情况都不能获取到信息
	return next_node->LoadInfo(filename_mem,format,w,h,min_out,memsize); //传给插件队列的下个插件来处理
}
uint8* iFILE_IMAGE_BMP::LoadData(char *filename_mem,GNg::COLOR_FORMAT *format,int *w,int *h,bool col_turn,int memsize){
	int yes = GNf::IsTypeFile(filename_mem, *format, "bmp", "BM", 2, memsize); //判断提供的文件名或文件内容是否指定类型的文件
	if(yes){ //如是 或不确定
		uint8 *data = LoadBmpFileData2(filename_mem,format,w,h,col_turn,memsize); //尝试读取文件数据
		if(data || yes == 1)return data; //如调佣成功 或 已确定是bmp文件 就返回数据 无论数据有否
	} 
	//以上情况都不能获取到数据
	return next_node->LoadData(filename_mem,format,w,h,col_turn,memsize); //传给插件队列的下个插件来处理
}
bool iFILE_IMAGE_BMP::Save(char *file_name,uint8 *data,GNg::COLOR_FORMAT srcformat,int w,int h,bool col_turn, uint8 comps_lv){
	char *ext_name = GN_StrGetExtName(file_name); //获取文件扩展名
	if(!ext_name || strcmp(ext_name, "bmp")){ //如无扩展名或不是bmp
		return next_node->Save(file_name,data,srcformat,w,h,col_turn,comps_lv); //传给插件队列的下个插件来处理
	}
	GNf::sFILE *fp = GNf::Open(file_name,"wb",GNf::eFILE_STD); //打开创建文件(路径文件名, 以写入二进制方式, 创建于标准本地磁盘文件)
	if(!fp){ GN_Log("写入 bmp 文件 %s 失败! 文件创建失败!\n", file_name); return false; }  //文件创建失败就打印失败原因到 RunLog.txt 文件里
	GT_sBMP_HEADER bmp_head;	//定义bmp文件头结构
	GT_sBMP_INFO   bmp_info;	//定义bmp文件信息结构
	int sbh = sizeof(GT_sBMP_HEADER);
	int sbi = sizeof(GT_sBMP_INFO);
	memset(&bmp_head,0,sbh);	//使文件头结构里的所有属性先设为0
	memset(&bmp_info,0,sbi);	//使文件信息结构里的所有属性先设为0
	int pixel_byte = (int)ImgFmtGetByteNum(srcformat); //获取指定颜色格式的单个像素占字节数
	bmp_info.bit = (unsigned short)pixel_byte << 3;	   //颜色位数=像素占字节数乘8   <<3相当于乘8 但性能更好
	bmp_info.w = w;  bmp_info.h = h;  //设置图片宽高
	bmp_info.planes = 1;			  //只有1帧图像
	bmp_info.size = 40;				  //文件信息结构
	bmp_info.xPelsPerMeter = 2874;	  //常规水平分辨率
	bmp_info.yPelsPerMeter = 2874;	  //常规水平分辨率
	unsigned int data_size = (unsigned int)pixel_byte * (unsigned int)w * (unsigned int)h; //计算到图像数据总大小
	bmp_info.sizeImage = data_size;   //指定图像数据大小
	//unsigned int   v = data_size % 4;
	//if(v)bmpI.sizeImage += 4 - v;
	bmp_head.type = 0x4d42;			  //文件效验标签 "BM" 的16进制
	bmp_head.offBits = 54;			  //无调色板的数据区在文件开始的偏移字节位置
	bmp_head.size = 54 +  bmp_info.sizeImage; //文件总大小字节
	GNf::Write((char*)&bmp_head,sbh,1,fp);	  //写入文件头信息
    GNf::Write((char*)&bmp_info,sbi,1,fp);    //写入文件信息
	uint8 *data2 = new uint8[data_size];	  //分配一块图像数据大小的内存
	GNg::ImgFmtConvertTs(data2, srcformat, data, srcformat, w * h, true); //使红蓝分量值调换一下 即把引擎用的颜色数据RGB转为bmp默认的BGR

	if(col_turn){  //当要求翻转列象素
	    unsigned int pitch = pixel_byte * (unsigned int)w; //计算到图像一行数据所占字节数
        int i = h - 1;  //写入数据从最末行开始历遍到首行
		for(; i >= 0; --i)GNf::Write((char*)(data2+(i*pitch)),1,pitch,fp); //要翻转图象就要一行行写

	}else GNf::Write((char*)data2,1,data_size,fp); //一次过写入整片图像数据

	GNf::Close(fp);		//写完关闭文件
	delete [] data2;	//删除临时数据
	return true;		//返回成功
}
GNg::PF_LOAD_IMG_FILE_DATA iFILE_IMAGE_BMP::CheckFmtSupportFromExtName(const char *extname){
	if(!strcmp(extname,"bmp"))return LoadBmpFileData2;		//判断扩展名如是bmp就返回本插件的图像读取方法
	return next_node->CheckFmtSupportFromExtName(extname);  //其它情况 传给插件队列的下个插件来处理
}
void iFILE_IMAGE_BMP::GetSupportFmtFileExtName(char *out, bool upper){
	strcpy(out, upper?"bmp,BMP,":"bmp,");	//复制支持的扩展名到缓冲里
	next_node->GetSupportFmtFileExtName(out+(upper?8:4), upper); //把缓冲地址移动到内容末端再传给插件队列的下个插件来继续添加扩展名
}
/************************************************************************************************************************************************/
extern "C"{
#if 0 //导出为引擎插件 --------------------------------------------------------------------------------------------------------------------------
GN_CALL GNg::iFILE_IMAGE *Create(){ return new iFILE_IMAGE_BMP; }	 //必须导出一个创建插件对象的函数给引擎调用 并返回自定义插件类对象
GN_CALL char*const Version(){ return GN_INTERFACE_VERSION; }		 //必须导出一个版本函数给引擎调用 并返回接口版本的宏
																	 //把编译好的插件动态库和它依赖到的资源放到一个与插件动态库同名的文件夹下 并将该文件夹放到引擎目录 Plugins/FileImage/ 下就可以被编辑器加载到 但您用SDK开发的应用需要手动调用 GN_PluginsLoad 系列函数来选择加载那些插件
#else //插件使用例子 ----------------------------------------------------------------------------------------------------------------------------

char  *ImgFileName = "PluginSampleFileImage.bmp";
GNg::sIMAGE	 Img;	  //定义一个图像

GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	new iFILE_IMAGE_BMP;	//创建一个自定义的bmp图像文件支持插件 并把它接入到插件链 FileImage 的前面 并使 FileImage = 该插件
	GNg::COLOR_FORMAT format;
	int w, h;
	char *src_img_filename = "samples/base/image/s11_0000.png";	//定义图片源文件路径
   #if 0
	format = (GNg::COLOR_FORMAT)GNf::GetExtNamePos(src_img_filename); //找出扩展名的位置偏移 要调用文件支持插件的 ->LoadData 或 ->LoadInfo 函数必须的约定
	uint8 *img_data = GNg::FileImage->LoadData(src_img_filename,&format, &w, &h);	 //调用引擎所有支持含插件的图像读取方法来读取指定文件 如果插件里有支持的文件类型但引擎默认里也有 会优先调用最后接入的插件里的方法 如调用引擎里的函数GNg::LoadPngFileData来直接读取 将会忽略插件
   #else
	uint8 *img_data = GNg::LoadImgFileData(GNg::eIMG_FILE_UNKNOWN, src_img_filename,&format, &w, &h); //该调用等价于上面两行 指定 eIMG_FILE_UNKNOWN 未知类型 引擎就会用所有支持含插件的图像读取方法来读取
   #endif
	GNg::FileImage->Save(ImgFileName, img_data, format, w, h, true); //把图像数据用插件优先的方法来保存成bmp并存到引擎跟目录里 这个等价于 GNg::SaveImgFile
	GN_DeleteArray(img_data);	  //删除图像数据

	Img.InitToData(ImgFileName);  //读取刚刚保存的图像 当接入插件之后所有没有明确文件类型的读取方法都能得到插件的功能加成
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Img.Release();			//释放图像空间
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//显示一个比较大的黑色矩型(使用的图片, x坐标, y坐标, 宽,  高,   颜色调)
	GNg::ImageDraw(      0,        -300,   -300,  1100, 1100, 0xff000000);
    
	static bool usPlugin = true;  //插件当前启用状态
	if(GN::Key1[GK_SPACE]){				//当按下空格键
		usPlugin = !usPlugin;				//变更一下插件启用状态
		if(usPlugin)new iFILE_IMAGE_BMP;	//要求启用时就在创建插件
		else delete GNg::FileImage;			//不想使用时就删除插件释放空间
		Img.InitToData(ImgFileName);		//再重新读取一下bmp图片  该方法比 ImageLoad更便捷 您无需手动调用 Release 内部帮您调用了
	}
	GNg::ImageDraw(&Img, (GNg::ViewCur->orthoW - Img.width) / 2, (GNg::ViewCur->orthoH - Img.height) / 2); //按屏幕截取范围宽高来计算出图像能居中显示的XY坐标来绘制图片

	GN_RunValueCheck2(usPlugin, 19);						   //显示插件当前启用状态
	GN_RunValueCheck2(GNg::GetSupportImgFmtFileExtName(), 20); //显示插件和引擎支持的所有图象文件扩展名 越后接入的插件支持的类型显示在越前
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
#endif
}