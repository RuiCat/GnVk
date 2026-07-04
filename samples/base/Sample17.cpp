#include<GN.h>

P<GNg::sIMAGE>            FontImg(1);  //文字图
P<GNg::sIMAGE>            LogoImg(1);  //LOGO图
P<GNu::cSCROLL_RECT>      Srect(1);    //简单滚动筐
P<GNu::cSCROLL_RECTEX>    SrectEx(1);  //增强滚动筐

//滚动筐位置 X      Y             宽度      高度
short dx = 450,   dy = 100,   dw = 260,   dh = 400;
//													 增强滚动筐的子区划分格高度
short dx2=  90,   dy2= 100,   dw2= 290,   dh2= 400,   zh = 60;

int   LogoSize = 50;   //LOGO图标原始显示大小

//增强滚动筐的子分格区划绘制(子区划位置X,  Y,   子区划宽度,  高度,    子区划的底色,     当前绘制的是第几个子区划, 当前绘制的是第几个可视区划, 是否有选择焦点, 回传给您的参数) !该函数在初始化增强滚动筐时传給它 增强滚动筐会在适当的时候帮你回调该函数来正确显示内容
void DrawZoning(            int x,       int y,    int w,    int h,  unsigned int color, unsigned int zid,	unsigned int vid,		   bool has_focus, void  *us_arg){
    GNg::ImageDraw(0, x+2, y+2, w-4, h-4, color); //用底色画一个比子区划小一点的矩型作底板色

	float ratio_w = (float)w / (float)dw2,  ratio_h = (float)h / (float)zh;   //计算单位宽高比率
    int c = y + (h>>1), img_w = (int)((float)LogoSize * ratio_w), img_h = (int)((float)LogoSize * ratio_h); //经过比率缩放计算图标显示大小
	unsigned int draw_color = (0xff000000 & color)|0x00ffffff;  //绘制颜色为白色 不透明度和底色一样,
    
	//绘制显示子区划左边的图标(图标图, 位置X,           Y,        图标宽, 高,   颜色)
	GNg::ImageDraw((GNg::sIMAGE*)us_arg, x + (20.0f*ratio_w) , c-(img_h>>1), img_w, img_h, draw_color);  

	int font_h = (int)GN::RunRec.aFont.GetFontHeight(); //获得GN内部默认显示文本的字体高度
	char str[128];
	sprintf(str,"lv: %03d  color: %08x", zid, color);   //拼装好要输出的字符串

    //用GN内部默认显示文本的字体来显示(字符内容,   0:表示输出全部,			位置X,                   Y,        文本筐宽,   高,     字颜色) 
	GN::RunRec.aFont.OutText(              str,             0 ,        x +(90.0f * ratio_w),   c-(font_h>>1),     0       ,0,    draw_color); 

	//有焦点的鼠标位置表明落在当前绘制的子区划范围里
    if(has_focus)GNg::ImageDraw(0, x+2, y+2, w-4, h-4, 0xaaffffff); //那么就绘制一个半透明白色的矩型叠在子区划上 以表示当前选中子区划
}
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	FontImg->InitToData("samples/base/image/fontmap_",1);  //读入字图
	LogoImg->InitToData("samples/base/image/s40",1);       //读入Logo图标
	Srect->SetOffsetBorderEF(600);                        //设置简单滚动区的滚栏闪动间隔时间

    //初始化简单滚动筐( 位置X, Y,   筐宽,  筐高, 筐内可滚动的总高度,  滚栏颜色,   滚栏混合模式）
	Srect->InitStyleToData(dx, dy,   dw,   dh,          600,          0xddffffff,  GNg::eBLEND_BRIGHT);
	
	//初始化增强滚动筐( 绘制子区划的处理函数, 位置X,  Y,  筐宽, 筐高,  是否含默认动画效果, 子区划宽度,  高度,   子区划总个数即,  滚栏颜色,  滚栏混合模式）
	SrectEx->InitStyleToData(0,       dx2,  dy2, dw2,  dh2,        true,               dw2,     zh,            50,     0xddffffff,  GNg::eBLEND_BRIGHT);
	SrectEx->SetZoningDrawCall(DrawZoning, (GNg::sIMAGE*)LogoImg); //设置区划绘制回调
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Srect.Delete();     //删除简单滚动筐
    SrectEx.Delete();   //删除增强滚动筐
	FontImg.Delete();   //删除简单滚动筐内滚动的底图
	LogoImg.Delete();   //删除小图标图
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::ImageDraw(0,0,0,800,600,0xff777777);  //显示一个灰色的矩型作为背景色清屏
	
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_TOP);  //使接下来的2D绘制都为向上渐淡的
	GNg::ImageDraw(0,0,300,800,500,0xff556fb5);	    //绘制一个渐变普蓝色块
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_NULL); //取消渐变绘制

    short fx, fy, fw, fh;
	//获取简单滚动筐范围(返回位置X, 返回Y, 返回宽度, 返回高度)
	Srect->GetPos(		   &fx,      &fy,     &fw,     &fh);  
	GNg::LineFrame2dDraw((int)fx-2, (int)fy-2, (int)fw+4, (int)fh+4,2,Srect->GetFocusChangeColor()); //在简单滚动筐位置里画一个与滚栏颜色一样的线筐
	
	GNg::ViewCur->ScissorRectSetup(fx,fy,fw,fh); //加入一个剪裁区划范围 这样使接下来的绘制内容只在指定区划里
	GNg::ImageDraw(FontImg, (int)fx, (int)fy+Srect->GetColOffset(),(int)fw, (int)fh,0xffffffff); //按简单滚动筐返回的偏移高度来显示图片
    GNg::ViewCur->ScissorRectRemove(); //移除刚才加入的剪裁区划 

	//获取增强滚动筐范围(返回位置X, 返回Y, 返回宽度, 返回高度 )
    SrectEx->GetPos(	   &fx,      &fy,   &fw,	   &fh);
    GNg::LineFrame2dDraw((int)fx-2, (int)fy-2, (int)fw+4, (int)fh+4,2,Srect->GetFocusChangeColor()); //在增强滚动筐位置里画一个与滚栏颜色一样的线筐
    
	//....增强滚动筐的其它子区划内容在绘制回调那里计算处理

	if(GN::Key[GK_LEFT]) { dx -= 3; Srect->SetPos(dx,dy,dw,dh); } //当按住键盘左方向键左移简单滚动筐
    if(GN::Key[GK_RIGHT]){ dx += 3; Srect->SetPos(dx,dy,dw,dh); } //当按住键盘右方向键右移简单滚动筐
	if(GN::Key[GK_UP])   { dy -= 3; Srect->SetPos(dx,dy,dw,dh); } //当按住键盘上方向键上移简单滚动筐
    if(GN::Key[GK_DOWN]) { dy += 3; Srect->SetPos(dx,dy,dw,dh); } //当按住键盘下方向键下移简单滚动筐
	
	if(GN::Key1[GK_K])Srect->RePos();    //当按住键盘K键重设简单滚动筐的偏移 使它恢复为开始时的状态

	if(GN::Key[GK_A]){ dx2 -= 3; SrectEx->SetPos(dx2,dy2,dw2,dh2); } //当按住键盘A键左移增强滚动筐
    if(GN::Key[GK_D]){ dx2 += 3; SrectEx->SetPos(dx2,dy2,dw2,dh2); } //当按住键盘D键右移增强滚动筐
	if(GN::Key[GK_W]){ dy2 -= 3; SrectEx->SetPos(dx2,dy2,dw2,dh2); } //当按住键盘W键上移增强滚动筐
    if(GN::Key[GK_S]){ dy2 += 3; SrectEx->SetPos(dx2,dy2,dw2,dh2); } //当按住键盘S键下移增强滚动筐
	
	if(GN::Key1[GK_J])SrectEx->RePos();	 //当按住键盘J键重设增强滚动筐的偏移,使它恢复为开始时的状态
	if(GN::Key1[GK_H])SrectEx->OkHide(); //当按住键盘H键确定提示增强滚动筐里的内容都要隐藏起来
	
	if(GN::Key1[GK_N]){				     //当按住键盘N键重设置增强滚动筐的显示区块为10个
		SrectEx->InitStyleToData(0, dx2, dy2, dw2, dh2, true, dw2, zh, 10, 0xddffffff, 2);  
		SrectEx->SetZoningDrawCall(DrawZoning, (GNg::sIMAGE*)LogoImg);  //你得再指定一下回调
	}
	Srect->UpdateRun(nTime);    //更新运行简单滚动筐
	SrectEx->UpdateRun(nTime);  //更新运行增强滚动筐
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}