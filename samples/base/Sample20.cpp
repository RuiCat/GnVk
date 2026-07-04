#include<GN.h>        //使用GN库必需包含这个头文件
#define                USE_MAPPING_KEY     //使用映射到键盘键的全平台操作 !注释掉这个就用触屏专用的高精度虚拟操控杆反馈数据来处理

P<GNu::cJOYSTICK>	   VtJoystick;         //虚拟操控杆
P<GNu::cBUTTON_T>	   VtButton;           //虚拟升降按钮
P<GNg::sIMAGE>         Sprite;             //精灵动画图序
GNg::cFRAME            StarAm;             //星星动画
GN::sLIST<GNg::cFRAME> StarList;           //记录所有活动的星星动画的链表

//重设虚拟按钮位置
void ReVtButtonPos(){
    GNu::cBUTTON_T *_butn = VtButton;       //临时拿出安全指针里的原始指针 这样在局部计算里可以更快
    short icon_h = _butn->aIcon->height; 
    _butn[1].aRngL = _butn->aRngL = GNg::ViewCur->ortho_left; //使按钮总是靠视口左边
    _butn[1].aRngR = _butn->aRngR = _butn->aRngL + _butn->aIcon->width; //按钮的范围右边等于按钮左边加按钮图标宽度
    short vtb_y = GNu::NearVy(0.624f); //获得视口大半高度位置 参数0.0视口顶部 1.0视口底部
    _butn->aRngT = vtb_y - icon_h;  _butn->aRngB = vtb_y;
    vtb_y = GNu::NearVy(0.824f);       //获得视口更加靠底的位置
    _butn[1].aRngT = vtb_y; _butn[1].aRngB = vtb_y + icon_h;
    
    icon_h = _butn[2].aIcon->height;
    _butn[2].aRngL = GNg::ViewCur->ortho_right  - 80; //在视口右边左移80像素的位置
    _butn[2].aRngT = GNg::ViewCur->ortho_bottom - 80; //在视口下边上移80像素的位置
    _butn[2].aRngR = _butn[2].aRngL + icon_h * 2;     //该按钮的右边等于左边加两倍的图标大小
    _butn[2].aRngB = _butn[2].aRngT + icon_h * 2;     //该按钮的下边等于上边加两倍的图标大小
}
extern "C"{
GN_CALL void AppInit(){      //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次
    Sprite.New(1);                //分配图像空间
    Sprite->InitToData("samples/base/image/combo", 2); //读取精灵图片(图片文件路径, 图序里含两张图)
    
    GNg::sIMAGE *star_img = new GNg::sIMAGE;          //分配星星图像空间
    star_img->InitToData("samples/base/image/star4_"); //读取星星图片(图片文件路径)
    GNg::sAM_EVENT *amev = new  GNg::sAM_EVENT[2];    //分配两个动画事件
    //设置向下移动事件( Y方向移动, 开始时间, 结束时间, 开始值,     结束值,   缓动值,  缓动类型)
    amev->Init(GNg::eEVENT_MOVE_Y,    0,       2000,   0.0f,       260.0f,     0.0f,  GNg::L_END);
    //设置颜色变化事件
    amev[1].InitColor(                0,       2000, 0xffffff00,  0xaaff6600);
    //StarAm装填图象(指定图象,动画事件个数,动画事件数组, 初始变换信息(x,y), 以图中心为旋转定点, 旋转定点,	混合模式,		   颜色,		   播放完消失)
	StarAm.Init(       star_img,  2,           amev,  GNg::sTRANSF2D(0,0),	   0.5f,		     0.5f,  GNg::eBLEND_NORMAL, 0xffffffff,  GNg::eAM_ONCE_HIDE);
    StarAm.KeepExternObjs();    //保管外部连接对象 star_img 和 amev 这样你就不必手动 delete 它们了
    
    // 定义图像      方向箭头     底盘           上落按钮
    P<GNg::sIMAGE> arrow_img(1), disk_img(1), up_button_img(1);
    
    //创建空的图像(图像宽, 图像高, 图像序列数, 使用仅不透明格式以节省空间)
    arrow_img->CreateEmptyToData(24, 24, 1, GNg::CR_ALPHA);
    uint8 arrow_data[24*24*1];
    //计算生成箭头图像数据(数据输出, 图像宽, 图像高, 尖头式样, 非线框的实体箭头)
    GNg::GenArrowDataToMem(arrow_data,  24,    24,      1,           0);
    arrow_img->DataFill(arrow_data, GNg::CR_ALPHA); //图像数据填充  (图像数据, 图像数据为仅不透明格式)
    
    //创建空的图像(图像宽, 图像高, 图像序列数, 使用仅不透明格式以节省空间)
    disk_img->CreateEmptyToData(100, 100, 1, GNg::CR_ALPHA);
    uint8 disk_data[100*100*1];
    //计算生成圆图像数据(数据输出,     圆宽高, 边界大小, 硬度, 非实心圆)
    GNg::GenSphereDataToMem(disk_data,  100,      1,     1.0f,  false);
    disk_img->DataFill(disk_data, GNg::CR_ALPHA); //图像数据填充  (图像数据, 图像数据为仅不透明格式)
    
    up_button_img->CreateEmptyToData(50, 50, 1, GNg::CR_ALPHA);
    uint8 button_data[50*50*1];
    //计算生成矩型图像数据(数据输出, 图像宽, 高, 渐变起始位置x, 渐变起始位置y, 渐变方向x, 渐变方向y)
    GNg::GenRectDataToMem(button_data, 50,   50,    0.0f,           0.0f,        0.0f,    1.0f/50.0f);
    up_button_img->DataFill(button_data, GNg::CR_ALPHA);
    
	VtButton.New(3);                    //分配三个虚拟按钮 一个上速度 一个降速度 一个攻击
	GNu::cBUTTON_T *_butn = VtButton;
	_butn->aIcon   = up_button_img;     //使两按钮使用同一个按钮图像
	_butn[1].aIcon = up_button_img;
	_butn[2].aIcon.New(1);
	_butn[2].aIcon->InitToData("samples/base/image/sys"); //读取攻击按钮图片(图片文件路径)
	#ifdef USE_MAPPING_KEY
	_butn -> aMappingKey = GK_E;        //指定虚拟按钮按下后同时将相关消息映射到键盘的E键
	_butn[1].aMappingKey = GK_Q;        //指定虚拟按钮按下后同时将相关消息映射到键盘的Q键
	_butn[2].aMappingKey = GK_SPACE;    //指定虚拟按钮按下后同时将相关消息映射到键盘的空格键
	#endif
	ReVtButtonPos();                    //初始布置升降虚拟按钮位置
	
	VtJoystick.New(1);                  //分配虚拟操控杆对象
	GNu::cJOYSTICK *_joyk = VtJoystick;
	_joyk->aBackDiskColor = GNg::MultiplyAlphaRatio(0xffffffff,0.333f); //控杆底盘使用一个白色调到0.333不透明度的半透白色
	_joyk->aArrowColor    = GNg::MultiplyAlphaRatio(0xffffffff,0.7f);   //指定控杆方向箭头颜色
	_joyk->aBackDiskImg = disk_img;     //指定底盘式样啊图
	_joyk->aArrowImg = arrow_img;       //指定方向箭头图
	GNg::sVEC2 rng_auv(0.3f, 0.6f);
	_joyk -> Init(rng_auv);             //设置控杆放置管控范围为自动贴近视口左下边角的范围对角点UV UV范围意义: u0.0视口左边 u1.0视口右边 v0.0视口上边 v1.0视口下边 该方法会自动根据UV与视口四个角间的最短距离来选择靠在那个角
	#ifdef USE_MAPPING_KEY
	_joyk -> aVecMappingKeyType = 1;    //设置控杆方向映射到键盘键位为 AWDS 键
	_joyk -> aVectorMaxLen = 0.8f;      //设置控杆方向量绝对值不能长于0.8 超出时底盘会被拉动以确保方向量不会超出该值
    #endif
}
GN_CALL void AppUnInit(){    //▲释放应用必须数据 程序运行结束前会调用它一次
    StarList.clear();                   //清空链表里所有星动画
    StarAm.Release();                   //释放星动画内存空间
    VtButton.Delete();                  //删除虚拟按钮   以释放内存空间
    VtJoystick.Delete();                //删除虚拟操控杆 以释放内存空间
    Sprite.Delete();                    //删除精灵图序   以释放内存空间
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它
    GNg::cVIEW2D *view = GNg::ViewCur;
	GNg::ImageDraw(0, view->ortho_left,view->ortho_top,view->orthoW,view->orthoH,0xff000000); //绘制一个黑色屏幕大小的矩形以清屏
	
	GNu::cBUTTON_T *_butn = VtButton;
    GNg::sIMAGE *_butn_img = _butn->aIcon; 
    if(GN::ViewReSized)ReVtButtonPos();     //当视口大小或横竖切换有更变时就重设升降虚拟按钮位置 使它始终靠左边
    _butn->UpdateRun(nTime);                //更新显示上虚拟按钮(帧间隔时间)
    _butn_img->vRange = -_butn_img->vRange; //使图像垂直占用范围为负值 即可上下翻转来显示
    _butn[1].UpdateRun(nTime);              //更新显示下虚拟按钮(帧间隔时间)
    _butn_img->vRange = -_butn_img->vRange; //恢复原值
    _butn[2].UpdateRun(nTime);              //更新显示攻击虚拟按钮(帧间隔时间)
    
    GNu::cJOYSTICK *_joyk = VtJoystick;
    _joyk->UpdateRun(nTime);                //更新显示操控杆(帧间隔时间)
    
    static bool dir_left = true;                  //定义精灵默认向左
    static int dx = 320, dy = 180, speed = 3;     //定义精灵1绘制坐标 和精灵移动速度
    #ifdef USE_MAPPING_KEY
    if     (GN::Key1[GK_Q] && speed>1)speed -= 1; //当Q键有刚按下信号时 且速度大于1 就使速度减1
    else if(GN::Key1[GK_E] && speed<5)speed += 1; //当E键有刚按下信号时 且速度小于5 就使速度加1
    if     (GN::Key[GK_A]){ dx -= speed; dir_left = true; }   //当A键有按下信号时 使精灵坐标往左移动速度个像素 且设置方向为向左
    else if(GN::Key[GK_D]){ dx += speed; dir_left = false; }  //当D键有按下信号时 使精灵坐标往右移动速度个像素 且设置方向为非左
    if     (GN::Key[GK_W])  dy -= speed;                      //当W键有按下信号时 使精灵坐标往上移动速度个像素 
    else if(GN::Key[GK_S])  dy += speed;                      //当S键有按下信号时 使精灵坐标往下移动速度个像素 
    #else
    static float dxf = 320.0f, dyf = 180.0f;                  //为防止精度损失 用浮点类型坐标来控制
    if     (_butn -> aTouchZ >= 255 && speed<5)speed += 1;    //当上虚拟按钮下压力>=255时 且速度小于5 就使速度加1
    else if(_butn[1].aTouchZ >= 255 && speed>1)speed -= 1;    //当下虚拟按钮下压力>=255时 且速度大于1 就使速度减1
    if     (_joyk->aVector.x){                                //当虚拟操控杆方向x有值时
        dir_left = _joyk->aVector.x < 0.0f;                   //按操控杆方向x是否为负值来确定精灵方向是否为左
        dxf += _joyk->aVector.x * (float)speed;               //使精灵坐标x加上速度乘方向x 这样能起到带压感的方向控制
    }
        dyf += _joyk->aVector.y * (float)speed;               //使精灵坐标y加上速度乘方向y 以实现控制精灵升降
    dx = (int)dxf;   dy = (int)dyf;                           //把浮点类型坐标转为整形坐标
    #endif
    int dw = Sprite->width; 
    //绘制显示精灵(精灵图序, 方向为左时 X位置在左边否则在右边, X位置, 方向为左时宽度正常否则为负宽度 亦可翻转来绘制, 高度0表示使用原图高度,   原颜色,   按程序运行帧数 每4帧来轮替图序ID)
    GNg::ImageDraw(Sprite,    dir_left?dx:dx+dw,                dy,         dir_left?dw:-dw,                            0,                  0xffffffff, (GN::TimeGetRunFpsCount()&4)?1:0);
    
    #ifdef USE_MAPPING_KEY
    if(GN::Key1[GK_SPACE]){      //当空格键有刚按下信号时
    #else
    if(_butn[2].aTouchZ >= 255){ //当攻击虚拟按钮下压力>=255时 
    #endif
        GNg::cFRAME *star0 = StarList.add(StarAm);      //在链表里加入一个星星动画副本
        star0->aFrame_ts.x = dx + dw / 2;               //设置星星动画副本的X位置为精灵当前中心
        star0->aFrame_ts.y = dy + Sprite->height - 25;  //设置星星动画副本的y位置为精灵下面往上一点
    }
    if(StarList.size()){ //链表里有星星动画
        GN::sLIST<GNg::cFRAME>::iterator itam = StarList.begin(), etam = StarList.end();
        for(; itam != etam; ){ //链表检索位置不等于末尾的那个时
            if(itam->PlayFrame(nTime))++itam;  //播放当前检索位置的动画筐 当返回非0时动画还未结束就跳到下一个检索位置
            else itam = StarList.erase(itam);  //返回0时表示动画结束 就移除当前位置的动画筐 并指向下一个检索位置
        }
    }
    GN_RunValueCheck2((int)StarList.size(), 0);//在屏幕上快捷显示链表里装有动画筐的数量
    GN_RunValueCheck2(speed, 1);               //在屏幕上快捷显示精灵当前移动速度
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}
