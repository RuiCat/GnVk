#include<GN.h>

#define MAX_W 2048

P<GNg::sIMAGE>		 Img, Img2;		//定义两个图像对象
GNg::COLOR_FORMAT	 ImgFmt;		//记录图片像素格式用
int					 ImgW, ImgH, ImgW2, ImgH2, ImgW3, ImgH3;
uint8               *PngData = 0;
uint8               *SclData = 0;

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	Img.New(1); Img2.New(1);
	PngData = GNg::LoadPngFileData("samples/base/image/s11_0000.png", &ImgFmt, &ImgW, &ImgH); //读入图片数据到内存 并指派给PngData
	Img->InitToData("samples/base/image/s11_"); //读入图像
	//Img->CreateEmptyToData (ImgW,ImgH,1,ImgFmt,true);
	//Img->DataFill(PngData,ImgFmt,ImgW,ImgH);
    Img2->CreateEmptyToData(MAX_W,MAX_W,1,ImgFmt,false); //创建指定大小的空图像
	Img2->DataFill(PngData,ImgFmt,ImgW,ImgH);			 //把图片数据装填到 Img2 里
	
	SclData = new uint8[MAX_W * MAX_W * GNg::ImgFmtGetByteNum(ImgFmt)]; //分配一个较大的临时处理缓冲
    ImgW3 = ImgW2 = ImgW, ImgH3 = ImgH2 = ImgH;
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Img.Delete(); Img2.Delete();  //删除图像
	GN_DeleteArray(SclData);	  //释放中转数据
	GN_DeleteArray(PngData);	  //释放图片数据
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//用 A D W S 键来控制Img的显示大小  并限制最大不能超过 MAX_W
	if     (GN::Key[GK_A]){ ImgW3 -= 3; if(ImgW3 < 1    )ImgW3 = 1;     }
	else if(GN::Key[GK_D]){ ImgW3 += 3; if(ImgW3 > MAX_W)ImgW3 = MAX_W; }
	if     (GN::Key[GK_W]){ ImgH3 -= 3; if(ImgH3 < 1    )ImgH3 = 1;     }
	else if(GN::Key[GK_S]){ ImgH3 += 3; if(ImgH3 > MAX_W)ImgH3 = MAX_W; }
	
	//用 左右上下 键来控制图片数据的缩放尺寸 并限制最大不能超过 MAX_W
	bool size_change = false;
	if     (GN::Key[GK_LEFT]) { ImgW2 -= 3; if(ImgW2 < 1    )ImgW2 = 1;     size_change = true; }
	else if(GN::Key[GK_RIGHT]){ ImgW2 += 3; if(ImgW2 > MAX_W)ImgW2 = MAX_W; size_change = true; }
	if     (GN::Key[GK_UP])   { ImgH2 -= 3; if(ImgH2 < 1    )ImgH2 = 1;     size_change = true; }
	else if(GN::Key[GK_DOWN]) { ImgH2 += 3; if(ImgH2 > MAX_W)ImgH2 = MAX_W; size_change = true; }
	
	if(size_change){ //当要缩放数据时
        short pb = GNg::ImgFmtGetByteNum(ImgFmt);
		GN_RUNTSTART(1);
		GNg::ImgScale(SclData,ImgW2,ImgH2,PngData,ImgW,ImgH,pb,2); //把原图片数据用CPU计算缩放至指定大小
        GN_RUNTEND(); //统计处理时间并显示在屏幕左下角
		Img2->DataFill(SclData,ImgFmt,ImgW2,ImgH2); //把缩放好的图片数据再装填到 Img2 里
	}
	GNg::cVIEW2D *view = GNg::ViewCur;
	GNg::ImageDraw(0,   view->ortho_left,view->ortho_top,view->orthoW,view->orthoH,0xff000000); //绘制一个黑色屏幕大小的矩形以清屏
    GNg::ImageDraw(Img, view->ortho_left,view->ortho_top, ImgW3, ImgH3);     //用当前修改过的显示大小来绘制图像  这里的缩放用3D硬件加速的纹理缩放

	GNg::ImageDraw(Img2,view->ortho_left+(view->orthoW>>1),view->ortho_top); //以临时图的原大小来显示它 但它里面的数据并不是满画幅的 因为会被缩放过
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}