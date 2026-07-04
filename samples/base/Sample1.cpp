#include<GN.h>
extern "C"{
GN_CALL void AppInit(){}
GN_CALL void AppUnInit(){}
GN_CALL void AppDisplay2D(unsigned int nTime){
    GN_RunTextOut("Hello Vulkan!", 0, 150, 100, 0, 0, 0xffff3434, GNg::eBLEND_NORMAL);
}
GN_CALL void AppDisplay3D(unsigned int nTime){}
GN_CALL void AppRun(unsigned int nTime){}
}
