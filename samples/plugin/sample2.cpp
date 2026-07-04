#include<GN.h> //使用GN必需包含GN.h头文件

class cSTREAM_WAV :public GNa::iFILE_AUDIO::cSTREAM  //wav音频文件流 继承自接口类 GNa::iFILE_AUDIO::cSTREAM
{
	GNf::sFILE		*fp;			//文件
   public:
	cSTREAM_WAV(GNf::sFILE	*_fp=0):fp(_fp){} //构造时先把文件指针设为0

	//跳转到指定音频进度(进度0.0f至1.0f)
	virtual void Seek(float progre);
	//读取文件流解码后的数据(存放数据的内存, 需读取解码后的数据字节数)返回读取解码后的字节数 返回0表示完结
	virtual unsigned int Read(char *buf, unsigned int count);
	//写入一段音频数据(数据, 数据大小字节)返回写入了个数 失败返回0
	virtual unsigned int Write(const char *buf, int count);
	//关闭音频文件流(音频文件流)
	virtual void Close();
};
class iFILE_AUDIO_WAV :public GNa::iFILE_AUDIO //wav音频文件支持插件 继承自接口类 GNa::iFILE_AUDIO
{public:
//				 iFILE_AUDIO_WAV(); //构造插件并把它接入到插件链 FileAudio 的前面 并使 FileAudio = 新构建的插件
//	virtual		~iFILE_AUDIO_WAV(); //释放插件资源并断开在插件链的连接

	//打开音频文件流并获取文件信息(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频文件流 返回0表示打开失败
	virtual GNa::iFILE_AUDIO::cSTREAM *Open(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize);
	//创建音频文件流并先存储文件信息 以便接下来写入音频数据(保存文件名,音频格式,声道数,声音位数,频率)返回音频文件流 返回0表示创建失败
	virtual GNa::iFILE_AUDIO::cSTREAM *Create(const char *file_name,unsigned short format,unsigned short channels,unsigned short bit,unsigned int freq);


    //读取音频文件信息(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频播放时间 返回0表示读取失败
	virtual unsigned int LoadInfo(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize);
	//读取音频文件数据(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
	virtual uint8*		 LoadData(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize);
	//把音频数据保存为文件(  保存文件名  ,  音频格式,声道数,声音位数,频率,数据字节大小, 数据)返回保存是否成功
	virtual bool		 Save(const char *file_name,  unsigned short  format,unsigned short  channels,unsigned short  bit, unsigned int freq,unsigned int  size, uint8 *data);

	//按文件扩展名检测音频格式是否被引擎支持( 文件扩展名 )
	virtual bool CheckFmtSupportFromExtName(const char *extname);
	//获得能够被引擎支持的音频文件扩展名(返回各个不同的扩展名, 是否追加返回大写的名)
	virtual void GetSupportFmtFileExtName(char *out, bool upper);
};
//读取Wav音频文件信息(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频播放时间 返回0表示读取失败
unsigned int LoadWavFileInfo2(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
   #define GT_TMP_SET_WAV_INFO \
	*format   = *(unsigned short*)(td+20); /*格式*/\
	*channels = *(unsigned short*)(td+22); /*声道数*/\
	*bit      = *(unsigned short*)(td+34); /*声道位*/\
	*freq     = *(unsigned int  *)(td+24); /*频率*/\
	*size     = *(unsigned int  *)(td+40); /*音频数据的大小*/

   #define GT_TMP_LOAD_WAV_HEAD \
	GNf::sFILE *fp = GNf::Open(filename_mem,"rb",memsize);\
    if(!fp)return 0;  /*文件打开失败返回0*/\
	char td[44];\
	GNf::Read(td,1,44,fp); /*读取wav头信息共44个字节*/\
	if(td[0]!='R'||td[1]!='I'||td[2]!='F'||td[3]!='F'){ /*判断是否不是wav格式文件*/\
		GNf::Close(fp);\
		return 0;\
	}\
	GT_TMP_SET_WAV_INFO

    GT_TMP_LOAD_WAV_HEAD;
	GNf::Close(fp);		  //关闭文件
	return GNa::ComputePlayTime(*channels , *bit, *freq, *size); //计算返回音频总播放时间 单位毫秒
}
//读取Wav音频文件数据(音频文件名或内存地址,音频格式,声道数,声音位数,频率,数据字节大小,使用内存模式指定大小)返回音频数据的指针 返回0表示读取失败  返回数据内容用完后记得delete []data;
uint8* LoadWavFileData2(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	GT_TMP_LOAD_WAV_HEAD;				   //读取wav 文件头信息
	uint8 *data = new uint8[*size];		   //分配数据内存空间
	GNf::Read((char*)data, 1, *size, fp);  //一次过读取音频数据 !wav无压缩数据比较简单
	GNf::Close(fp);						   //关闭文件
	return data;						   //返回数据
}
void cSTREAM_WAV::Close(){ 
	if(!fp->buf_size){	 //文件流是以写方式打开的
		unsigned int size = GNf::Tell(fp) - 44; //写完获得当前文件写位置偏移减去文件头大小就是wav音频数据大小
		GNf::Seek(fp, 40, GNf::eSEEK_START);	//跳回文件第40个字节处为wav文件储存音频数据大小值的位置
		GNf::Write((char*)&size, 1, 4, fp);		//写入数据大小的值
	}
	GNf::Close(fp); delete this;				//完成写入关闭文件 并删除自己的内存空间
}
void cSTREAM_WAV::Seek(float progre){
	GNf::Seek(fp, 44 + (int)(fp->buf_size * progre), GNf::eSEEK_START); //使文件读取跳转到音频播放进度对应的文件位置 44表示文件头所占字节
}
unsigned int cSTREAM_WAV::Read(char *buf, unsigned int count){
	short tmp_data[65536];	 //临时缓冲
	count = GNf::Read((char*)tmp_data, 1, count, fp); //在当前文件位置读出指定大小的数据到临时缓冲
	short *data1 = (short*)buf;			  //16位音频即short类型
	static short L = 0, R = 0;			  //用于缓存涩后的声音震幅
	unsigned int si = 0, smn = count / 2; //计算到样本数 short 占两个字节
	//使本次读取的数据增加混响效果
	for(; si < smn; si += 2){ 
		L += (tmp_data[si  ] - L) >> 4;   //计算到左声道涩后的声音震幅
		data1[si  ] = L * 3 / 2;		  //音量乘1.5倍

		R += (tmp_data[si+1] - R) >> 4;   //计算到右声道涩后的声音震幅
		data1[si+1] = R * 3 / 2;		  //音量乘1.5倍
	}
	return count;						  //返回本次读取到的字节数
}
unsigned int cSTREAM_WAV::Write(const char *buf, int count){
	count = GNf::Write(buf, 1, count, fp);	//在当前文件位置写入指定大小的数据
	return count > 0 ? count : 0;			//使返回值最小为0
}
GNa::iFILE_AUDIO::cSTREAM * iFILE_AUDIO_WAV::Open(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	int yes = GNf::IsTypeFile(filename_mem, *format, "wav", "RIFF", 4, memsize);  //判断提供的文件名或文件内容是否指定类型的文件
	if(yes){ //如是 或不确定
		GNf::sFILE *fp = GNf::Open(filename_mem,"rb",memsize); //打开文件 filename_mem 和 memsize 可以是磁盘文件 或压缩包 或 内存数据块 具体看 memsize 怎么表示
		if(fp){ //打开成功
			char td[44];
			GNf::Read(td, 1, 44, fp);	  //读取wav文件头信息
			if(!strncmp(td, "RIFF", 4)){  //校验文件头信息 判断是否wav格式文件
				GT_TMP_SET_WAV_INFO		  //返回文件头的信息到输出参数
				fp->buf_size = *size;	  //文件该属性指定为数据大小 以便之后读取跳转 计算出文件读取位置
				return new cSTREAM_WAV(fp); //创建文件流对象并返回
			}
		}else if(yes == 1)return 0; //文件打开失败且已确定是wav文件就返回失败
	}
	return next_node->Open(filename_mem,format,channels,bit,freq,size,memsize); //传给插件队列的下个插件来处理
}
GNa::iFILE_AUDIO::cSTREAM * iFILE_AUDIO_WAV::Create(const char *file_name,unsigned short format,unsigned short channels,unsigned short bit,unsigned int freq){
	char *ext_name = GN_StrGetExtName(file_name);	//获得文件扩展名
	if(ext_name && !strcmp(ext_name, "wav")){
		unsigned int size = 0;					//初始音频数据大小为0

	   #define GT_TMP_SAVE_WAV_HEAD \
		GNf::sFILE *fp = GNf::Open((char*)file_name,"wb",GNf::eFILE_STD); /*创建并打开文件(路径文件名, 以写入二进制方式, 创建于标准本地磁盘文件)*/\
		if(!fp){ GN_Log("写入wav文件 %s 失败! 文件创建失败!\n", file_name); return false; } /*文件创建失败就打印失败原因到 RunLog.txt 文件里*/\
		char td[44]; /*44字节临时变量空间*/\
		strncpy(td, "RIFF", 4); /*首4个字节为wav的文件效验标签*/\
		memset(td+4, 0, 40);	/*其它40个字节先全设为0*/\
		*(unsigned short*)(td+20) = format  ;	/*格式*/\
		*(unsigned short*)(td+22) = channels;	/*声道数*/\
		*(unsigned short*)(td+34) = bit     ;	/*声道位*/\
		*(unsigned int  *)(td+24) = freq    ;	/*频率*/\
		*(unsigned int  *)(td+40) = size    ;	/*音频数据的大小*/\
		GNf::Write(td, 1, 44, fp);  //写入文件头信息

		GT_TMP_SAVE_WAV_HEAD					//先保存wav文件头信息
		fp->buf_size = 0;						//文件该属性设为0 标记是写文件
		return new cSTREAM_WAV(fp);	//创建文件流对象并返回
	}
	return next_node->Create(file_name,format,channels,bit,freq); //传给插件队列的下个插件来处理
}
unsigned int iFILE_AUDIO_WAV::LoadInfo(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	int yes = GNf::IsTypeFile(filename_mem, *format, "wav", "RIFF", 4, memsize);  //判断提供的文件名或文件内容是否指定类型的文件
	if(yes){ //如是 或不确定
		unsigned int play_time = LoadWavFileInfo2(filename_mem,format,channels,bit,freq,size,memsize); //尝试读取wav音频文件信息
		if(play_time || yes == 1)return play_time;  //如调佣成功 或 已确定是wav文件 就返回音频播放时间 无论有否
	}
	return next_node->LoadInfo(filename_mem,format,channels,bit,freq,size,memsize); //传给插件队列的下个插件来处理
}
uint8* iFILE_AUDIO_WAV::LoadData(char *filename_mem,unsigned short *format,unsigned short *channels,unsigned short *bit,unsigned int *freq,unsigned int *size,int memsize){
	int yes = GNf::IsTypeFile(filename_mem, *format, "wav", "RIFF", 4, memsize);  //判断提供的文件名或文件内容是否指定类型的文件
	if(yes){ //如是 或不确定
		uint8 *data = LoadWavFileData2(filename_mem,format,channels,bit,freq,size,memsize); //尝试读取wav音频文件数据
		if(data || yes == 1)return data;			//如调佣成功 或 已确定是wav文件 就返回音频数据 无论有否
	}
	return next_node->LoadData(filename_mem,format,channels,bit,freq,size,memsize); //传给插件队列的下个插件来处理
}
bool iFILE_AUDIO_WAV::Save(const char *file_name,  unsigned short  format,unsigned short  channels,unsigned short  bit, unsigned int freq,unsigned int  size, uint8 *data){
	char *ext_name = GN_StrGetExtName(file_name); //获取文件扩展名
	if(!ext_name || strcmp(ext_name, "wav")){ //如无扩展名或不是wav
		return next_node->Save(file_name,format,channels,bit,freq,size,data); //传给插件队列的下个插件来处理
	}
	GT_TMP_SAVE_WAV_HEAD		//先保存文件头信息
	GNf::Write((char*)data, 1, size, fp); //再一次过写入音频数据
	GNf::Close(fp);				//关闭文件
	return true;				//返回成功
}
bool iFILE_AUDIO_WAV::CheckFmtSupportFromExtName(const char *extname){
	if(!strcmp(extname, "wav"))return true;	 //扩展名是wav就返回支持
	return next_node->CheckFmtSupportFromExtName(extname); //其它情况 传给插件队列的下个插件来处理
}
void iFILE_AUDIO_WAV::GetSupportFmtFileExtName(char *out, bool upper){
	strcpy(out, upper?"wav,WAV,":"wav,");	//复制支持的扩展名到缓冲里
	next_node->GetSupportFmtFileExtName(out+(upper?8:4), upper); //把缓冲地址移动到内容末端再传给插件队列的下个插件来继续添加扩展名
}
/************************************************************************************************************************************************/
extern "C"{
#if 0 //导出为引擎插件 --------------------------------------------------------------------------------------------------------------------------
GN_CALL GNg::iFILE_AUDIO *Create(){ return new iFILE_AUDIO_WAV; }	 //必须导出一个创建插件对象的函数给引擎调用 并返回自定义插件类对象
GN_CALL char*const Version(){ return GN_INTERFACE_VERSION; }		 //必须导出一个版本函数给引擎调用 并返回接口版本的宏
																	 //把编译好的插件动态库和它依赖到的资源放到一个与插件动态库同名的文件夹下 并将该文件夹放到引擎目录 Plugins/FileAudio/ 下就可以被编辑器加载到 但您用SDK开发的应用需要手动调用 GN_PluginsLoad 系列函数来选择加载那些插件
#else //插件使用例子 ----------------------------------------------------------------------------------------------------------------------------

char  *BgmFileName = "PluginSampleFileAudio.wav";
GNa::cBGM_MUSIC  Bgm;            //音频流播放器 该对象以流式播放不会加载整个文件到内存

GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	new iFILE_AUDIO_WAV;	//创建一个自定义的wav音频文件支持插件 并把它接入到插件链 FileAudio 的前面 并使 FileAudio = 该插件
	unsigned short format, chl, bit;
	unsigned int freq, size;
	char *src_bgm_filename = "samples/base/sound/018.mp3";	//定义音频源文件路径

	format = GNf::GetExtNamePos(src_bgm_filename); //找出扩展名的位置偏移 要调用文件支持插件的 ->LoadData 或 ->LoadInfo 函数必须的约定
	uint8 *bgm_data = GNa::FileAudio->LoadData(src_bgm_filename,&format,&chl,&bit,&freq,&size);	 //调用引擎所有支持含插件的音频读取方法来读取指定文件 如果插件里有支持的文件类型但引擎默认里也有 会优先调用最后接入的插件里的方法 如调用引擎里的函数GNa::LoadMp3FileData来直接读取 将会忽略插件
   
	GNa::FileAudio->Save(BgmFileName,format,chl,bit,freq,size,bgm_data); //把音频数据用插件优先的方法来保存成wav并存到引擎跟目录里
	GN_DeleteArray(bgm_data);	  //删除音频数据

	Bgm.InitToData(BgmFileName);  //打开刚刚保存的wav准备播放 当接入插件之后所有没有明确文件类型的读取方法都能得到插件的功能加成
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Bgm.Release();			//释放音频流播放器空间
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//显示一个比较大的黑夜红矩型(使用的图片, x坐标, y坐标, 宽,  高,   颜色调)
	GNg::ImageDraw(      0,        -300,   -300,  1100, 1100, 0xff300026);
    
	static bool usPlugin = true;  //插件当前启用状态
	if(GN::Key1[GK_SPACE]){				//当按下空格键
		Bgm.Release();						//释放音频流播放器空间
		usPlugin = !usPlugin;				//变更一下插件启用状态
		if(usPlugin)new iFILE_AUDIO_WAV;	//要求启用时就在创建插件
		else delete GNa::FileAudio;			//不想使用时就删除插件释放空间
		Bgm.InitToData(BgmFileName);		//再重新打开一下wav音频流
	}
	Bgm.UpdataPlay(nTime);	//更新播放音频流
	GN_RunValueCheck2(usPlugin, 19);						   //显示插件当前启用状态
	GN_RunValueCheck2(GNa::GetSupportAudioFmtFileExtName(), 20); //显示插件和引擎支持的所有音频文件扩展名 越后接入的插件支持的类型显示在越前
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
#endif
}