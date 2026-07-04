#include<GN.h>

struct GAME_INFO    //游戏运行信息
{
	unsigned int RunTotalTime;  //总运行时间
	int          RunNum;        //程序打开运行次数
};
GAME_INFO	 GameInfo;
GNg::sIMAGE  BgImg;
GNg::sIMAGE  ComboImg;

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	BgImg.InitToData("samples/base/image/bg");  //读入背景图
	ComboImg.InitToData("samples/base/image/combo", 2);

	GNf::sFILE *file = 0;
	
	//打开文件(           文件名,          打开模型,      安全模式)     //安全模式是分两个文件来交替校验使用 以防有一个文件坏掉造成数据丢失
	file = GNf::Open("SampleSafeSave.ddd",  "rb",     GNf::eFILE_SAFE); //以2进制只读方式常试打开文件 模式必需设为"r" 如果设了"r+"或"w" 在不文件不存在时会自动创建一个空文件 但这样不能做文件存在检查
	
	if(file){ //文件存在
		//读取文件数据(要读入数据的对象,   该类型所占字节大小,  1个这样的数据,   所读取文件)
		GNf::Read(      (char*)&GameInfo,    sizeof(GAME_INFO),            1,      file);  //注意文件取数据起始点刚打开时在文件的首部 当读了或写了数据之后会自动移动到读写量的结尾处
		
		GNf::Close(file);	  //使用完就关闭文件
		
		GameInfo.RunNum += 1; //在当前值上加一 来记录程序执行次数

	}else{ //文件不存在
		GN_Log("open file error!"); //输出错误文本信息  

		GameInfo.RunNum = 0;
        GameInfo.RunTotalTime = 0;  //没文件时候把这些值重设为0
	}
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	GNf::sFILE *file = 0;   //定义个文件指针
	
	file = GNf::Open("SampleSafeSave.ddd",  "wb"  ,GNf::eFILE_SAFE); //以2进制只写方式安全打开文件
	
	//写数据到文件(要写数据的对象,  该类型所占字节大小,  写1个这样的数据, 写入数据文件)
	GNf::Write(  (char*)&GameInfo,   sizeof(GAME_INFO),       1,			file);
    
	GNf::Close(file);		//使用完就关闭文件

	ComboImg.Release();	
	BgImg.Release();		//释放背景数据
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GameInfo.RunTotalTime += nTime; //累积每帧间隔时间
	
	GNg::ImageDraw(&BgImg,0,0,800,600); //绘制背景以填充屏幕

	//绘制精灵图  !用总运行时间来控制精灵图位移  运行次数来控制精灵的混合模式
	GNg::ImageDraw(&ComboImg,(GameInfo.RunTotalTime&4095)/4-100, 230, 0, 0, 0xffffffff, GN::TimeGetRunFpsCount()&1, GameInfo.RunNum % (GNg::eBLEND_BRIGHT_MODU+1)); 

	//显示这写值(       值,                显示顺序编号);
	GN_RunValueCheck2(GameInfo.RunNum,       1);
	GN_RunValueCheck2(GameInfo.RunTotalTime, 2);
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}