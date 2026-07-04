#include<GN.h>

GNg::cTEXT_ART    Fnum,     Fascii,    Fgb32;     //分别定义3个文本输出器
GNg::sIMAGE       FnumImg,  FasciiImg;             //定义字体式样图
GNg::sIMAGE       BgImg;						  //定义背景图

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	BgImg.InitToData("samples/base/image/louyu");			//装入背景图
	FnumImg.InitToData("samples/base/image/number",1);		//装入式仅数字的样图
    FasciiImg.InitToData("samples/base/image/fontmap_",1);   //装入Ascii式样图

	//定义排列格式(			是否填满文本筐,   文本水平靠位,  文本垂直靠位, 字间距, 行距, 单字宽, 单字高,  自动换行模式,     竖方向输出, 反转输出)
	GNg::sARRANGE_FMT asciifmt(true,              0.5f,         0.0f,        0,    10);
	GNg::sARRANGE_FMT gb32_fmt(false,             0.0f,         0.0f,        0,    12,    14,     16,	GNg::eENTER_LEFT_RIGHT,  false,   false);

	//设置文本对象（字体式样图, 排列格式）
    Fascii.Init(&FasciiImg,   &asciifmt);
	Fgb32.Init (nullptr,      &gb32_fmt);
	Fgb32.SetFreeTypeFont(16);  // FreeType UTF-8 中文
	Fnum.Init  (&FnumImg);  //!!没指定排版格式为使用默认排版方式 一般数字都是用这个方式即可
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Fnum.Release();
	Fascii.Release();
	Fgb32.Release();
	FnumImg.Release();
	FasciiImg.Release();
	BgImg.Release();
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	static int csx, csy;
	--csy; //控制筐内中文自动上滚
	if(csy < -230)csy = 130;
	
	//按键盘A,D,W,S控制筐内中文移动
	if(GN::Key[GK_A])csx -= 1; 
	if(GN::Key[GK_D])csx += 1;
	if(GN::Key[GK_W])csy -= 1; 
	if(GN::Key[GK_S])csy += 1;

	GNg::ImageDraw(&BgImg,0,0,800,600); //绘制背景用以清屏

	//输出文本注意事项 输出的字符不能超出式样字库所能显示的范围 不能用使用数字的式样来显示英文 反之可以
	//因为ASCII包含有数字 中文也包含英文数字 但是中文式样字库占内存大 请合理使用
	
	int dx = 400 - (FnumImg.width / 10 * 3);
	//输出数字（       数字,  坐标x, 坐标y, 文本筐宽, 文本筐高,    颜色,   混合模式）
	Fnum.TextOutNumber(230525, dx ,   70,     0,         0,    0xffffeebb, GNg::eBLEND_NORMAL); //斋数字可以这样方便输出 其他的符号文字格式要先转换为文本才行
    
    GNg::LineFrame2dDraw(250,170,300,100,1,0x33ffffff); //画一个边筐 以代表英文的文本筐
	//输出英文文本
	Fascii.OutText(
		"GN Engine\n--* user code sample!! *--\nby:2023.05.25", //文本
		 0,					 //输出的个数 0表示全输出
		 250,				 //文本筐左上角坐标X
		 170,				 //文本筐左上角坐标Y
		 300,				 //文本筐宽度 值0即不使用文本筐
		 100,				 //文本筐高度 值0即不使用文本筐
		 0xffffffff,		 //文本颜色
		 GNg::eBLEND_NORMAL, //混合模式
		 0,					 //在文本筐内的水平偏移量 修改该值可以实现在筐内滚动文本效果 如果排列格式设置了填满文本筐这值不起作用
		 0					 //在文本筐内的垂直偏移量
		 );
    
	GNg::LineFrame2dDraw(268,330,264,130,1,0x33ffffff); //画一个边筐 以代表中文的文本筐
	//输出中英文本
	Fgb32.OutText(
		"GN Engine Sample:\n大尺寸中文字图会比较占显存空间,您应合理使用! 这里暂时仅用默认字体显示,如想使用更丰富的文字式样!!\n请在以下网址下载式样模板自行制作:\nwww.img-power.com/load/style/FontMapTemplate.rar\n...",
		 0,					 //输出的个数 0表示全输出
		 268,				 //文本筐左上角坐标X
		 330,				 //文本筐左上角坐标Y
		 264,				 //文本筐宽度 值0即不使用文本筐
		 130,				 //文本筐高度 值0即不使用文本筐
		 0xfffebf20,		 //文本颜色
		 GNg::eBLEND_NORMAL, //混合模式
		 csx,				 //在文本筐内的水平偏移量 修改该值可以实现在筐内滚动文本效果
		 csy				 //在文本筐内的垂直偏移量 
		 );
     
	//把文字输出两次可模拟阴影浮雕效果的字体
	Fgb32.OutText("在外输出一个没文本筐的文本",0,309,502,0,0,0xffa0703b); //先输出背后一个深色的文本
	Fgb32.OutText("在外输出一个没文本筐的文本",0,308,500,0,0,0xffefeecc); //再输出前面白色的文本
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}