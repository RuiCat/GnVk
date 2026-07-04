#include<GN.h>

P<GNg::sIMAGE>             ImgMin (1);   //用来装图标序列的图像
P<GNg::sIMAGE>             ImgMin2(1);   //用来装图标序列的图像
P<GNg::sIMAGE>             ImgFont(1);   //字图图像
P<GNg::cTEXT_ART>		   AsciiText(1); //文本输出器
P<GNu::cSELECT_LIST_ICON>  SL (1);       //图标选择列表 用于单选
P<GNu::cSELECT_LIST_ICON>  SL2(1);       //图标选择列表 用于多选
P<GNu::cSELECT_LIST_TEXT>  ST (1);       //文本选择列表 用于多选

short dx2 = 50, dx3 = 370, dy2 = 48, dw , dw2 = 128, dh2 = 530;  //定义一些列表的初始摆放坐标

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	ImgFont->InitToData("samples/base/image/fontmap_");  //读入英文字图
	ImgMin ->InitToData("samples/base/image/chong",7);   //读入一组图片到图象序列里
	ImgMin2->InitToData("samples/base/image/chong",5);   //读入一组图片到图象序列里 比上个少两张
	dw = (short)ImgMin->width;   //单选图标列表的图标与图片原来一致

	//初始化图标列表   (图标图,  是否能多选, 列表坐标X,    坐标Y, 列表宽度, 高度, 是否带默认动画效果, 列表里图标的宽度, 高度, 选中焦点颜色)
	SL ->InitStyleToData(ImgMin,   false,     dx2 ,          dy2,   dw,     dh2,     true ,              dw,            dw);
	SL2->InitStyleToData(ImgMin2,  true,      dx2 + dw +50,  dy2,   dw2,    dh2,     true ,              dw2,           dw2,   0x889b6822);
	SL->aSelectFrameStyle = GNu::eRECT_DRAW_WIRE; //这个用线筐来表示已选项

	// 定义选择标签的内容 ------------
	P< P<char> > lables(16);
	lables[ 0].New("Lable 01...",0);
	lables[ 1].New("Lable 02.....",0);
	lables[ 2].New("Lable 03.........",0);
	lables[ 3].New("Lable 04...",0);
	lables[ 4].New("Lable 05....",0);
	lables[ 5].New("Lable 06...---...---...",0);
	lables[ 6].New("Lable 07....",0);
	lables[ 7].New("Lable 08.............",0);
	lables[ 8].New("Lable 09....",0);
	lables[ 9].New("Lable 10........",0);
	lables[10].New("Lable 11.....",0);
	lables[11].New("Lable 12.......",0);
	lables[12].New("Lable 13............",0);
	lables[13].New("Lable 14...........",0);
	lables[14].New("Lable 15.......",0);
	lables[15].New("Lable 16........",0);

	AsciiText->Init(ImgFont); //把字图传给输出器

	//初始化文本选择列表(标签选择组, 文字式样,   文字颜色   是否能多选, 列表的坐标X,     Y, 列表宽度, 高度, 是否带默认动画效果, 列表里标签文本的宽度, 高度 0为自动适配, 选中焦点颜色)
	ST->InitStyleToData( lables,     AsciiText,  0xffffffdd,   true,        dx3 ,       dy2 ,   0,    360,        true,               0,                0,				0x88223677);
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	SL .Delete();        //删除单选图标列表
	SL2.Delete();        //删除多选图标列表
	ST .Delete();        //删除多选文本列表
	AsciiText.Delete();  //删除文本输出器
	ImgMin .Delete();    //删除小图标图
	ImgMin2.Delete();    //删除小图标图
	ImgFont.Delete();    //删除字图
}
//显示指定选择列表的选择装态(选择列表, 信息打印格式)
void ShowSelectListSatae(GNu::cSELECT_LIST_ICON *sl, const char *pfmt){
	char info[128];
    short fx, fy, fw, fh;
	//获取图标列表(返回位置X,返回Y, 宽度 , 高度 )
    sl->GetPos(    &fx,     &fy,    &fw,   &fh);
   
	GNg::LineFrame2dDraw(  (int)fx-2, (int)fy-2, (int)fw+4, (int)fh+4, 2, 0x99ffffff); //在图标列表位置里画一个跟滚栏颜色一样的线筐
	sprintf(info, pfmt, sl->aSelected); //将选择值按格式转为字符串
	GN_RunTextOut(info, 0, fx-1, fy-30, 0, 0, 0xffffffff, 1); //显示选择值
	GN_RunTextOut(info, 0, fx,   fy-30, 0, 0, 0xffffffff, 1);
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::ImageDraw(0,0,0,800,600,0xff777777);  //显示一个灰色的矩型作为背景色清屏

	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_RIGHT); //使接下来的2D绘制都为向右渐淡的
	GNg::ImageDraw(0,0,0,430,600, 0x66aa8822);		  //绘制一个半个屏大渐变普蓝色块
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_NULL);  //取消渐变绘制

	ShowSelectListSatae(SL,  "SINGLE SELECT: %d");  //显示单选图标列表所选状态值
	ShowSelectListSatae(SL2, "MULTI  SELECT: %d");  //显示多选图标列表所选状态值
	ShowSelectListSatae(ST,  "TEXT   SELECT: %d");  //显示标签列表所选状态值

	if(GN::Key[GK_LEFT]) { dx2 -= 3; SL->SetPos(dx2,dy2,dw,dh2); } //按住键盘左方向键左移图标列表
    if(GN::Key[GK_RIGHT]){ dx2 += 3; SL->SetPos(dx2,dy2,dw,dh2); } //按住键盘右方向键右移图标列表
	if(GN::Key[GK_UP])   { dy2 -= 3; SL->SetPos(dx2,dy2,dw,dh2); } //按住键盘上方向键上移图标列表
    if(GN::Key[GK_DOWN]) { dy2 += 3; SL->SetPos(dx2,dy2,dw,dh2); } //按住键盘下方向键下移图标列表
	if(GN::Key1[GK_J])SL ->RePos();								   //按下键盘J键重设图标列表的偏移 使它恢复为开始时的状态
	if(GN::Key1[GK_H])SL ->OkHide();                               //按下键盘H键使图标列表筐里的内容都隐藏起来
	if(GN::Key1[GK_L])SL2->RePos();								   //按下键盘L键重设多选图标列表的偏移 使它恢复为开始时的状态
	if(GN::Key1[GK_K])SL2->OkHide();                               //按下键盘K键使多选图标列表筐里的内容都隐藏起来
	if(GN::Key1[GK_4]){ SL->aSelected = 4;  SL2->aSelected = 4; }  //按下键盘4键显式设置选择值以选取图表列表的选项
    if(GN::Key1[GK_5]){ SL->aSelected = 5;  SL2->aSelected = 5; }  //按下键盘5键显式设置选择值以选取图表列表的选项
    if(GN::Key1[GK_I])SL2->InitStyleToData(ImgMin2, true, dx2 + dw +50, dy2, dw2, dh2, true , dw2, dw2); //按下键盘I键重设多选图标列表的图标

	SL ->UpdateRun(nTime);   //运行单选图标列表
	SL2->UpdateRun(nTime);   //运行多选图标列表
	ST ->UpdateRun(nTime);   //运行文本列表
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}