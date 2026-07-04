#include<GN.h>

unsigned int SensorFlags = 0;

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	SensorFlags = GNio::GetSensorCaps(); //获得设备所拥有的传感器信息
    GNio::EnableSensors(SensorFlags);    //开启该设备所有的传感器
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	if(GN::Key1[GK_F4])GNio::EnableSensors(GI_HAS_WEIGHT|GI_HAS_DIRECTION); //如果按了菜单键之后关闭其它感应器 只开启重力感应器和方向感应器

   GNg::ImageDraw(0, //画一个视图大小的黑色矩型在底来清屏幕
                        GNg::ViewCur->ortho_left,
                        GNg::ViewCur->ortho_top,
                        GNg::ViewCur->orthoW,
                        GNg::ViewCur->orthoH,
                        0xff000000);

   if(SensorFlags & GI_HAS_WEIGHT){     //显示出重力感应器的值
       GN_RunValueCheck(GNio::InFv[GI_WX],"weight_x",0);
       GN_RunValueCheck(GNio::InFv[GI_WY],"weight_y",1);
       GN_RunValueCheck(GNio::InFv[GI_WZ],"weight_z",2);
   }
   if(SensorFlags & GI_HAS_DIRECTION){  //显示出方向感应器的值
       GN_RunValueCheck(GNio::InFv[GI_DX],"direction_x",4);
       GN_RunValueCheck(GNio::InFv[GI_DY],"direction_y",5);
       GN_RunValueCheck(GNio::InFv[GI_DZ],"direction_z",6);
   }
   if(SensorFlags & GI_HAS_FIELD){      //显示出磁场轴感应器的值
       GN_RunValueCheck(GNio::InFv[GI_FX],"field_x",8);
       GN_RunValueCheck(GNio::InFv[GI_FY],"field_y",9);
       GN_RunValueCheck(GNio::InFv[GI_FZ],"field_z",10);
   }
   if(SensorFlags & GI_HAS_GYROSCOPE){  //显示出陀螺仪感应器的值
       GN_RunValueCheck(GNio::InFv[GI_GX],"gyroscope_x",12);
       GN_RunValueCheck(GNio::InFv[GI_GY],"gyroscope_y",13);
       GN_RunValueCheck(GNio::InFv[GI_GZ],"gyroscope_z",14);
   }
   if(SensorFlags & GI_HAS_PROXIMITY){  //显示出接近感应器的值
       GN_RunValueCheck(GNio::InFv[GI_PROXIMITY],"proximity",16);
   }
   if(SensorFlags & GI_HAS_AMB_LIGHT){  //显示出环境光感应器的值
       GN_RunValueCheck(GNio::InFv[GI_AMB_LIGHT],"amb_light",18);
   }
   if(SensorFlags & GI_HAS_TEMPERATURE){//显示出温度感应器的值
       GN_RunValueCheck(GNio::InFv[GI_TEMPERATURE],"temperature",20);
   }
   if(SensorFlags & GI_HAS_PRESSURE){   //显示出压力感应器的值
       GN_RunValueCheck(GNio::InFv[GI_PRESSURE],"pressure",22);
   }
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}