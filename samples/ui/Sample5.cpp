#include<GN.h>		   //使用GN库必需包含这个头文件

P<GNu::cUI_SCENE>			UiScene;			//UI场景
GNu::cSCROLL_LIST_GUI 	   *ScrollBlock;		//滚动嵌套UI块
#define BLOCK_SIZE 			142					//单个子UI块数值占用大小字节

//构建用户信息存档数据(装填存档数据的缓冲, 存档数)返回存档数据总大小
unsigned int MakeUserInfoData(uint8 *data, unsigned int count){
	const char *names[] = { "猪肉荣", "李狗蛋", "小疯子", "王老五", "吉十兆", "大头根", "刘菲菲" }; //定义几个常用的用户名
	uint8 *wpos = data, i2;	//起始写位置
	unsigned int i = 0;
	for(; i < count; ++i, wpos += BLOCK_SIZE){ //历遍各个存档来写入用户信息 以便之后滚动筐时读取
		i2 = (uint8)(GN_RandomF0_1() * 5.0f) + 1;					//计算得一个固定随机1到6的值
		memcpy(wpos, GNg::cFRAME_LAUNCHER_FONT::aPicIndex + i, i2); //在起始64个字节区间里写入i2个拥有物品图标ID
		wpos[i2] = -1;												//图标ID以-1为结束标记
		strcpy((char*)(wpos+64), names[i%8]);						//在下一64个字节区间里写入用户名
		*((int*)(wpos+128)) = i2;									//在之后位置里写入物品数 占4个字节
		*((unsigned int*)(wpos+132)) = GNg::GetColorChange(51);		//又在之后位置里写入幻彩Lv底颜色 占4个字节
		*((int*)(wpos+136)) = i;									//在之后位置里又再写入Lv数 占4个字节
		*((short*)(wpos+140)) = GN_Random_n(25, 180);				//最后写入随机25到180的hp长度 占2个字节
	}
	return wpos - data;  //最尾的写位置减数据起始位置即为数据总大小
}
extern "C"{
GN_CALL void AppInit(){ /* Vulkan: UI延迟到AppDisplay2D首帧初始化 */ }
static bool s_sample5InitDone = false;
static void Sample5Init(){
	if(s_sample5InitDone) return; s_sample5InitDone = true;
	UiScene.New(1);
	float progr;
	UiScene->InitLoadToData("samples/ui/ScrollBlock/ScrollBlock.gui", progr);
	ScrollBlock = CUP(GNu::cSCROLL_LIST_GUI, UiScene->Search("UiBlock"));
	if(!ScrollBlock->aSubUiArg){
		P<GNu::cVALUE_EXTRACT_FILE> pextract_file(1);
		GNu::cVALUE_EXTRACT_FILE *extract_file = pextract_file;
		extract_file->extract_descs.New(13);					//新建13个数值提取描述 依次一一对应子区块UI组里的各个UI组件
		GN::sVALUE_LAYOUT2 *_extract_descs = extract_file->extract_descs; //临时拿出数值提取描述裸指针以访问更快
		uint8 uobj_buf[8];	/*以下代码教您怎么按本例读取要求手动配置数值提取描述*/
		_extract_descs[ 0].stride = -1;							  //首个为子区块UI组 指定-1为忽略处理
		_extract_descs[ 1].stride = BLOCK_SIZE;					  //其它UI组件取值访问跨距都为142个字节
		_extract_descs[ 1].size   = GNu::eLAYOUT_VALUE_PIMG0000;  //指示静态图像图片路径简单接上"00000000_"序号后续名来读取
		_extract_descs[ 1].pos    = 0;							  //该UI组件取值存放位置偏移
		_extract_descs[ 1].pos_tg = (uint8*)&(((GNu::sUI_IMAGE*)uobj_buf)->img) - uobj_buf; //计算出取值应用目标位置偏移
		_extract_descs[ 2].stride = BLOCK_SIZE;
		_extract_descs[ 2].size   = GNu::eLAYOUT_VALUE_MIN_STR;	  //此项为小字符串 用于字筐发射器的内容整个更新替换
		_extract_descs[ 2].pos    = 0;
		_extract_descs[ 2].pos_tg = (uint8*)((GNu::cUI_FRAME_LAUNCHER_FONT*)uobj_buf) - uobj_buf; //计算出取值应用目标位置偏移
		_extract_descs[ 3].stride = BLOCK_SIZE;
		_extract_descs[ 3].size   = GNu::eLAYOUT_VALUE_MIN_STR;   //此项为小字符串 用于用户名静态文本内容整个更新替换
		_extract_descs[ 3].pos    = 64;							  //在之上的字筐发射器内容已占64字节 所以此项数值存放在64字节偏移处
		_extract_descs[ 3].pos_tg = (uint8*)&(((GNu::cUI_TEXT_ART*)uobj_buf)->text) - uobj_buf;   //计算出取值应用目标位置偏移
		_extract_descs[ 4].stride = -1;							  //此项为 "拥有物品数:" 纯静态文本 忽略处理
		_extract_descs[ 5].stride = BLOCK_SIZE;
		_extract_descs[ 5].size   = 4|((strlen("个")+1)<<8);	  //物品数占4字节 还包含它后面的字符大小标记
		_extract_descs[ 5].pos    = 128;						  //存放在128字节偏移处
		_extract_descs[ 5].pos_tg = (uint8*)&(((GNu::cUI_TEXT_ART*)uobj_buf)->text) - uobj_buf;	  //计算出取值应用目标位置偏移
		_extract_descs[ 6].stride = BLOCK_SIZE;
		_extract_descs[ 6].size   = 4;							  //此项为颜色值 用于Lv底片的静态图像颜色
		_extract_descs[ 6].pos    = 132;
		_extract_descs[ 6].pos_tg = (uint8*)&(((GNu::sUI_IMAGE*)uobj_buf)->c) - uobj_buf;		  //计算出取值应用目标位置偏移
		_extract_descs[ 7].stride = -1;							  //此项为 "Lv:" 纯静态文本 忽略处理
		_extract_descs[ 8].stride = BLOCK_SIZE;
		_extract_descs[ 8].size   = 4|((strlen(".")+1)<<8);		  //等级数占4字节 还包含它后面的字符大小标记
		_extract_descs[ 8].pos    = 136;
		_extract_descs[ 8].pos_tg = (uint8*)&(((GNu::cUI_TEXT_ART*)uobj_buf)->text) - uobj_buf;	  //计算出取值应用目标位置偏移
		_extract_descs[ 9].stride = -1;							  //此项为 "hp:" 纯静态文本 忽略处理
		_extract_descs[10].stride = -1;							  //此项为hp底筐条 纯静态图像 忽略处理
		_extract_descs[11].stride = BLOCK_SIZE;
		_extract_descs[11].size   = 2;							  //hp条宽度值占2字节
		_extract_descs[11].pos    = 140;
		_extract_descs[11].pos_tg = (uint8*)&(((GNu::sUI_IMAGE*)uobj_buf)->w) - uobj_buf;  //计算出取值应用目标位置偏移
		_extract_descs[12].stride = -1;							  //此项为下划分界线 纯静态图像 忽略处理

		extract_file->filename.New(BLOCK_SIZE * ScrollBlock->GetZoningCount()); //创建用户信息存档数据空间
		MakeUserInfoData((uint8*)((char*)extract_file->filename), ScrollBlock->GetZoningCount());  //构建用户信息存档数据
		extract_file->fp = GNf::Open(extract_file->filename, "rb", extract_file->filename.Size()); //把存档数据内存空间映射为文件来统一方式读取 !这种方式加载快还灵活
		extract_file->data_pos = 0;	 //存档数据区起始位置偏移为0 因为内存映射文件无需文件头等信息
		ScrollBlock->SetSubUiCallBack(ScrollBlock->GetSubUi(), pextract_file, GNu::SubUiUpdateSyncProc); //设置子区块UI与数值提取文件和UI更新回调
	}else{
		ScrollBlock->aSubUiCallBack = GNu::SubUiUpdateSyncProc;  //指定使用默认的UI更新回调
		ScrollBlock->SubUiExecCallBack(GNu::eSUBUI_LOAD);		 //使各个子区块UI执行加载对应的存档数值 
	}
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次
	UiScene.Delete();			//删除UI场景和相关数据
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
    Sample5Init(); // Vulkan: ViewCur在首帧才可用, 延迟到此处初始化
	if(GN::Key1[GK_1]){			//当有按下键盘1键
		GNu::cVALUE_EXTRACT_FILE *extract_file = CVP(GNu::cVALUE_EXTRACT_FILE)(ScrollBlock->aSubUiArg); //临时拿出UI数值提取文件裸指针以访问更快
		if(extract_file->fp){		//提取文件已准备好
		  if(extract_file->fp->type >= GNf::eFILE_MEM){		//是内存映射文件
			MakeUserInfoData((uint8*)((char*)extract_file->filename), ScrollBlock->GetZoningCount());    //重构建用户信息存档数据 由于里面有些随机数 所以每次重构值都会不一样
			ScrollBlock->SubUiExecCallBack(GNu::eSUBUI_LOAD);	//使各个子区块UI执行加载对应的存档数值
		  }else{	//是标准文件
			GNf::Close(extract_file->fp);	//先关闭文件
			extract_file->fp = GNf::Open(extract_file->filename, "r+b"); //再以可读可写方式打开该文件
			GNf::Seek(extract_file->fp, extract_file->data_pos, GNf::eSEEK_START);  //跳转到存档数据区 因为从编辑器里做好导入的UI数值提取文件含有文件头信息 需跳过
			uint8 local_buf[8192];					//定义一个临时数据缓冲
			GN::sTBUF_INT tbuf(local_buf, 8192);	//把该临时缓冲给高速缓冲分配器使用 之后您用它来临时分配的空间不超出8192就用临时缓冲里的空间 超出就 new 新内存空间 但您无需手动去 delete 它所分配到的空间
			uint8 *buf = (uint8*)tbuf.GetSpace(BLOCK_SIZE * ScrollBlock->GetZoningCount()); //分配用户信息存档数据空间
			unsigned int data_size = MakeUserInfoData(buf, ScrollBlock->GetZoningCount());  //重构建用户信息存档数据 由于里面有些随机数 所以每次重构值都会不一样
			GNf::Write((char*)buf, 1, data_size, extract_file->fp);	//把用户信息存档数据写入进文件 以覆盖原有的数据
			ScrollBlock->SubUiExecCallBack(GNu::eSUBUI_LOAD);   //使各个子区块UI执行加载对应的存档数值
		} }
	}
	UiScene->UpdatePlay(nTime);		//更新播放UI场景
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}