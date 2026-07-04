#include<GN.h>		   //使用GN库必需包含这个头文件

GNg::sIMAGE   Logo;				  //logo图
float         dxf = 500.0f;       //浮点插值的变量
int           dxi = 500;          //整型插值的变量
int			  len = 80;			  //绘制长度
unsigned int  cr  = 0xff71682a;   //颜色插值的变量
bool          crChange = false;   //记录颜色是否正在更改中  

void ValueChangeEnd(void *us_parame){ //插值结束回调
	int *_len = (int*)us_parame;
	if(_len[0] > 100)_len[0] = 80;  //使绘制长度变短
	else			 _len[0] = 160; //或变长
}
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	Logo.InitToData("samples/base/image/combo", 2); //读入图像 图序中有两张图
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Logo.Release();	  //释放图片空间
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//   绘制矩型( 没图片,   当前视口的左边,           当前视口的上边,        当前视口宽度,       当前视口高度,         灰色) !这样就能使矩形绘制大小刚好匹配窗口大小改变
	GNg::ImageDraw(0,  GNg::ViewCur->ortho_left, GNg::ViewCur->ortho_top, GNg::ViewCur->orthoW, GNg::ViewCur->orthoH, 0xff888888);
	
	//按下空格键进行一次插值处理
	if(GN::Key1[GK_SPACE]){
        dxf = 100.0f;
        //变量插值更变(   要修改的变量, 改成的目标值, 完成插值时的处理函数, 用户参数, 值之间误差范围内视为完成插值, 插值变化倍速)
		GN::ValueChange     (  &dxf,     500.0f   ,     ValueChangeEnd,     &len,           0.5f,                     0.05f  );
        dxi = 100;
		GN::ValueChange     (  &dxi,     600      ,     0,                  0,              0.5f,                     0.05f  );
		cr = 0xff3388ff;
        GN::ValueChangeColor(  &cr,    0xffffcc88 ,     0,                  &crChange,      0.5f,                     0.05f  ); //如果插值结束回调为0 此时用户参数可以传入一个bool类型或单字节的变量指针 这样在插值结束后引擎会自动设置该变量为0以告诉您插值完成!
	}
    //用经过插值后的变量来绘制矩型		这里宽度为负 可以水平翻转来显示								获取应用程序当前运行帧号 取它最末1位是1还是0来控制图序显示
	GNg::ImageDraw(&Logo,(int)dxf, 100,		-Logo.width,				 Logo.height,  0xffffffff, GN::TimeGetRunFpsCount()&1);
	GNg::ImageDraw(&Logo,     dxi, 300,		-Logo.width,				 len,			cr		 , GN::TimeGetRunFpsCount()&1);

    //在屏幕上显示出变量值( 变量 ,         变量显示名,       在屏幕上的位布局编号)
    GN_RunValueCheck(      len,		       "length",          0);
	
	GN_RunValueCheck2(    crChange,						      1);  //显示颜色是否正在更改的标记  !这个调用可以不设置变量显示名  显示的名称将于变量名一样
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}