#include<GN.h> //使用GN必需包含GN.h头文件

//定义一组颜色用于区分不同的触摸点      红          橙          黄         黄绿        绿          青          天蓝         蓝            深蓝       紫           紫红         洋红
unsigned int     PColor[20] = { 0xffff0000, 0xffff8800, 0xffffff00, 0xff88ff00, 0xff00ff00, 0xff00ff88, 0xff00ffff, 0xff0088ff, 0xff0000ff, 0xff8800ff , 0xffff00ff, 0xffff0088 };

GNg::sIMAGE StarPoint; //用来读入图片

//显示触摸点内容（    点坐标x,   点坐标y,       点坐标z,           第几粒点)
void ShowTouchPoint(short xx, short yy, unsigned char zz, unsigned int id){
    short p_size = 80; //点的半径大小

    //显示星星图片（       图对象,        x坐标, y坐标,       宽,        高,                颜色调              )
    GNg::ImageDraw(&StarPoint, xx-p_size, yy-p_size, p_size<<1, p_size<<1,(((PColor[id] & 0x00ffffff)|(zz << 24))));
}
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	//读入star4_0000.png图片文件(文件名）
    StarPoint.InitToData("samples/base/image/star4_"); 
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	StarPoint.Release();   //释放图片对象
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//显示一个比较大的黑色矩型(使用的图片, x坐标, y坐标, 宽,  高,   颜色调)
	GNg::ImageDraw(      0,        -300,   -300,  1100, 1100, 0xff000000);
    
    uint8 i, tn = GN::GetTouchMaxNum();    //获取设备支持的最大触摸点数
    GN_RunValueCheck(tn,"TouchMaxNum", 0); //显示支持的最大触摸点数
	
	for(i = 0; i <  tn; ++i){ //历遍所有触摸点
        //当这个值有的时候代表该触摸点生效
        if(GN::TouchZ[i])
            ShowTouchPoint(GNg::ViewCur->touchX[i],GNg::ViewCur->touchY[i],GN::TouchZ[i], i); //用不同的颜色与不透明度显示所有生效的触摸点
    }
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}