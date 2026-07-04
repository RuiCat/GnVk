#include<GN.h>

GNa::sSOUND  So;                    //装声音的对象
GNg::sIMAGE  Bg;					//背景 
GNg::sIMAGE  Img,     InsectImg;    //荷叶 虫图象
GNg::cFRAME  Am1,     InsectAm;     //荷叶 虫图动画筐
GNg::sAM_EVENT *Ev = 0;             //动画筐怎样运动的事件

unsigned char InsectIndex[]={ 0,1,2, 3,4,3,4,3,4,3,4, 2,1,0,0,1,2,2,1,0,0,1,2,2,1,0,0,1,2,2,1,0,0,1,2,2,1,0 }; //索引图片编号数据

P<GNg::sAM_NODE> AmNodeData; //定义动画段描述数据

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	Bg.InitToData("samples/base/image/bg");   
	Img.InitToData("samples/base/image/leaf");				 //读入荷叶图象
	InsectImg.InitToData("samples/base/image/chong",5);       //读入虫子动画图组 这里仅读取5张 其它通过索引数据来重复引用
	So.InitToData("samples/base/sound/hhhh.wav");             //读入音频文件

	Ev = new GNg::sAM_EVENT[5];
	Ev[4].Init(GNg::eEVENT_SOUND, 500); //声音事件可以只设置开始时间
	
	//InsectAm装填图象(指定图象,动画事件个数, 动画事件数组,  变换信息(x,y),     图心点对位x,y, 混合模式,				 颜色,       播放完就停住)
	InsectAm.Init(&InsectImg,     1,            &Ev[4],    GNg::sTRANSF2D(100,450),0.5f,0.5f,      GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_ONCE_KEEP);
	//设置索引图数据（有几个索引数据,     索引数组,   每个索引停留的时间,	播放完就停住)
	InsectAm.InitTID(sizeof(InsectIndex), InsectIndex,       120,         GNg::eAM_ONCE_KEEP); 
	InsectAm.InitSoundToData(&So); //插入声音

	//设置事件( 事件类型,                         开始时间,  结束时间,     开始值,  结束值,  缓动值,缓动类型)
	Ev[0].Init(GNg::eEVENT_MOVE_X,                      0,     1000,       0.0f,     100.0f,  0.0f,  GNg::L_START);
	Ev[1].Init(GNg::eEVENT_SIZE_Y,                   1000,     2000,       1.0f,     -0.6f);
	Ev[2].Init(GNg::eEVENT_ROTATION,                 2000,     3000,       0.0f,    360.0f);
	Ev[3].InitColor(                                 3000,     4000,   0xffffffff, 0xffff0000);
	
	//为动画筐Am1装填图象(指定图象, 动画事件个数, 动画事件数组,	 变换信息(x,y))
	Am1.Init(            &Img,        4,             Ev,	  GNg::sTRANSF2D(350,400)); 
	
	AmNodeData.New(3); //在动画段内定3个节点就可以把动画段分割为4段了
	//设置节点数据 (  节点所在时间,播到该节点以后跳转到那个时间0xffffffff:为在原段内继续播放,该节点段的播放模式
 	AmNodeData[0].Init(1000,                 0xffffffff,                                       GNg::eAM_REPEAT);
    AmNodeData[1].Init(2000,                 0xffffffff,                                       GNg::eAM_REPEAT);
	AmNodeData[2].Init(3000,                 0xffffffff,                                       GNg::eAM_REPEAT);
	Am1.InitSegment(AmNodeData); //Am1设置段落节点
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	InsectAm.Release();          //这里筐对象也要释放是因为在插入声音那个函数有“ToData”字样
	AmNodeData.Delete();         //安全释放内存
	GN_DeleteArray(Ev);          //安全释放内存
	InsectImg.Release();         //释放图组数据
	Img.Release();               //释放图象数据
	Bg.Release();
	So.Release();   
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::ImageDraw(&Bg, GNg::ViewCur->ortho_left, GNg::ViewCur->ortho_top, GNg::ViewCur->orthoW, GNg::ViewCur->orthoH); //这样用视口动态坐标能确保背景图总能填满整个屏幕 无论窗口怎么缩放
   
	if(GN::Key1[GK_1]){ Am1.ReSetPlaytime(); Am1.SeekSegment(0); } //设置动画跳转段落
	if(GN::Key1[GK_2]){ Am1.ReSetPlaytime(); Am1.SeekSegment(1); }
	if(GN::Key1[GK_3]){ Am1.ReSetPlaytime(); Am1.SeekSegment(2); }
	if(GN::Key1[GK_4]){ Am1.ReSetPlaytime(); Am1.SeekSegment(3); }
	Am1.PlayFrame(nTime);	    //播放Am1动画

	if(GN::Key[GK_A]) InsectAm.aFrame_ts.x -= 3.0f;  //.aFrame_ts 可以动态控制动画筐 动画筐事件动画与这里的动态控制可以一起互相使用不影响
	if(GN::Key[GK_D]) InsectAm.aFrame_ts.x += 3.0f;
	if(GN::Key[GK_W]) InsectAm.aFrame_ts.y -= 3.0f;
	if(GN::Key[GK_S]) InsectAm.aFrame_ts.y += 3.0f;    
	if(GN::Key1[GK_R])InsectAm.ReSetPlaytime();		//重新播发动画
	InsectAm.PlayFrame(nTime);  //播放InsectAm动画
	
	//利用调试信息跟踪查看变量值 (  变量,        放置顺序
	GN_RunValueCheck2(             Am1.zSegmentID, 1);
	GN_RunValueCheck2((unsigned int)Am1.zEndtime,  2);
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}