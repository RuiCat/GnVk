#include<GN.h>

GNg::sIMAGE			BgImg;		 //背景图
GNg::sIMAGE			SquareImg;	 //9宫格图		
GNg::sIMAGE			Gb2312Img;   //字库图象对象
GNg::cTEXT_ART		Gb2312Text;  //文本式样对象
GNu::cTEXT_INPUT    TextIn;      //文本输入对象
 
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	BgImg.InitToData("samples/base/image/bg");		    //读入背景图
	SquareImg.InitToData(GN_SysDir("ui/bubbleB"),9, GNg::CR_ALPHA); //读入9宫格图 包含9个图序

	// FreeType UTF-8 中文 (替代 GB2312 PNG 字库)
	Gb2312Text.Init(nullptr);
	Gb2312Text.SetFreeTypeFont(16);

	//初始终化文本输出式样(字库图)
	// Gb2312Text.Init(&Gb2312Img);  // 由 FreeType 替代

	//初始化文本输入式样(文体式样, 文本筐X坐标, 文本筐Y坐标, 文本筐宽, 文本筐高, 文本颜色,   焦点颜色,   文本混合模式, 焦点混合模式)
	TextIn.InitStyle( &Gb2312Text,    220,         200,        360,       300 ,  0xff5566aa, 0xff771133);

	//初始化文本输入格式(        输入形式,      筐内开始预设的文本内容,  输入筐最大输入数)
	TextIn.InitInputFmtToData(GNu::eTX_IN_TEXT,      0               /*, ...*/);

	//在当前焦点位置输入文本(文本内容, 字节数:0为全部输入)
	TextIn.InputText("Please input your text!");   
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	TextIn.Release();     //释放文本筐的内容
	Gb2312Text.Release(); //释放文本式样内容
    Gb2312Img.Release();  //释放字库图内容
	SquareImg.Release();  //释放9宫格图
	BgImg.Release();	  //释放背景图
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::ImageDraw(&BgImg, 0, 0, 800, 600, 0xffffffff);  //绘制背景以清屏
	
	short ix,iy,iw,ih; //获取文本筐位置(返回坐标X, 返回坐标Y, 返回宽, 返回高)
	TextIn.GetPos(						  &ix,       &iy,      &iw,    &ih);	
	
	GNg::ImageDraw9Square(&SquareImg, ix-24, iy-24, iw+48, ih+48, 0x66ffffff); //绘制一个比文本筐稍大的圆角矩形
	GNg::LineFrame2dDraw(ix, iy, iw, ih, 1, 0x60ffffff);  //绘制文本边筐
	
	TextIn.UpdateRun(nTime);  //挂起更新运行文本输入筐
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}