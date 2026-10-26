#pragma once
#ifndef _GN_INPUT_H_
#define _GN_INPUT_H_

/* 请在编译环境里的预处理定义添加以下其中一个宏来确定代码的应用模式 */
//#define GN_INIT_USING_SDK                       //SDK方式使用
//#define GN_INIT_USING_LIB_EXPORTS               //动态库导出
//#define GN_INIT_USING_SOURCE_CODE               //源代码编辑

#ifdef GN_INIT_USING_SOURCE_CODE //源代码编辑
#define GN_INIT_API
#define GN_INIT_CALL
#elif defined GN_INIT_USING_LIB_EXPORTS //动态库导出
#if defined _WIN32_ || defined _WIN32_WCE
#define GN_INIT_API __declspec(dllexport)
#define GN_INIT_CALL __declspec(dllimport)
#else
#define GN_INIT_API extern
#define GN_INIT_CALL extern
#endif
#else // --------------------------------- //SDK 方式使用
#if defined _WIN32_ || defined _WIN32_WCE
#define GN_INIT_API __declspec(dllimport)
#define GN_INIT_CALL __declspec(dllexport)
#else
#define GN_INIT_API
#define GN_INIT_CALL
#endif
#endif
/*-----------------------------------------------------------------------------------------------------------------------------------
#if !defined(GN_INIT_USING_SOURCE_CODE) && !defined(GN_USING_SDK_INTEGRATION) && !defined(GN_INIT_USING_LIB_EXPORTS)
#pragma comment(lib, "GN_Init_win32.lib")
#endif
*/
/****** 键盘键位标志表 ********/ /*************************** PC键盘 ********************************/ /******************* 手机键盘 ***********************/

//                             //  WINDOWS键控码                          GLUT键控码   SDL键控码    //   ANDROID键控码   IOS键控码
#define GK_LBUTTON 0 //  0x01									                            //   102
#define GK_RBUTTON 1 //  0x02									                            //   103
#define GK_CANCEL 2 //  0x03									                            //
#define GK_MBUTTON 3 //  0x04									                            //   110
#define GK_BACK 4 //  0x08									   8            8           //   67                         //这个是手机里的DEL键
#define GK_TAB 5 //  0x09									   9            9           //   61                         //TAB键
#define GK_CLEAR 6 //  0x0C									                12          //   28                         //清除
#define GK_RETURN 7 //  0x0D //回车							   13           13          //   66                         //回车
    //    									                            //
#define GK_SHIFT 8 //  0x10									                            //
#define GK_CONTROL 9 //  0x11									                            //
#define GK_MENU 10 //  0x12									                319         //
#define GK_PAUSE 11 //  0x13									                19          //
#define GK_CAPITAL 12 //  0x14									                301         //

#define GK_KANA 13 //  0x15									                            //
#define GK_HANGEUL 14 //									   							    //
#define GK_HANGUL 15 //									   							    //
#define GK_JUNJA 16 //  0x17									                            //
#define GK_FINAL 17 //  0x18									                            //
#define GK_HANJA 18 //  0x19									                            //
#define GK_KANJI 19 //									   							    //

#define GK_ESCAPE 20 //  0x1B //退出ESC						   27           27          //   4                          //退出

#define GK_CONVERT 21 //  0x1C									                            //
#define GK_NONCONVERT 22 //  0x1D									                            //
#define GK_ACCEPT 23 //  0x1E									                            //
#define GK_MODECHANGE 24 //  0x1F									                            //

#define GK_SPACE 25 //  0x20 //空格							   32           32          //   62                         //空格
#define GK_PRIOR 26 //  0x21									                            //
#define GK_NEXT 27 //  0x22									                            //
#define GK_END 28 //  0x23									   307          279         //   6                          //电话盖线
#define GK_HOME 29 //  0x24									   306          278         //   3               3          //★返回主屏
#define GK_LEFT 30 //  0x25									   300          276         //   21                         //左方向
#define GK_UP 31 //  0x26									   301          273         //   19                         //上方向
#define GK_RIGHT 32 //  0x27									   302          275         //   22                         //右方向
#define GK_DOWN 33 //  0x28									   303          274         //   20                         //下方向
#define GK_SELECT 34 //  0x29									                            //
#define GK_PRINT 35 //  0x2A									                316         //
#define GK_EXECUTE 36 //  0x2B									                            //
#define GK_SNAPSHOT 37 //  0x2C									                            //
#define GK_INSERT 38 //  0x2D									   308          277         //
#define GK_DELETE 39 //  0x2E									   127          127         //
#define GK_HELP 40 //  0x2F									                315         //

//英文数字键
#define GK_0 41 //  '0'									   48           48          //   7                          //主键盘 0
#define GK_1 42 //  '1'									   49           49          //   8                          //主键盘 1
#define GK_2 43 //  '2'									   50           50          //   9                          //主键盘 2
#define GK_3 44 //  '3'									   51           51          //   10                         //主键盘 3
#define GK_4 45 //  '4'									   52           52          //   11                         //主键盘 4
#define GK_5 46 //  '5'									   53           53          //   12                         //主键盘 5
#define GK_6 47 //  '6'									   54           54          //   13                         //主键盘 6
#define GK_7 48 //  '7'									   55           55          //   14                         //主键盘 7
#define GK_8 49 //  '8'									   56           56          //   15                         //主键盘 8
#define GK_9 50 //  '9'									   57           57          //   16                         //主键盘 9
#define GK_A 51 //  'A'									   97           97          //   29                         //主键盘 A
#define GK_B 52 //  'B'									   98           98          //   30                         //主键盘 B
#define GK_C 53 //  'C'									   99           99          //   31                         //主键盘 C
#define GK_D 54 //  'D'									   100          100         //   32                         //主键盘 D
#define GK_E 55 //  'E'									   101          101         //   33                         //主键盘 E
#define GK_F 56 //  'F'									   102          102         //   34                         //主键盘 F
#define GK_G 57 //  'G'									   103          103         //   35                         //主键盘 G
#define GK_H 58 //  'H'									   104          104         //   36                         //主键盘 H
#define GK_I 59 //  'I'									   105          105         //   37                         //主键盘 I
#define GK_J 60 //  'J'									   106          106         //   38                         //主键盘 J
#define GK_K 61 //  'K'									   107          107         //   39                         //主键盘 K
#define GK_L 62 //  'L'									   108          108         //   40                         //主键盘 L
#define GK_M 63 //  'M'									   109          109         //   41                         //主键盘 M
#define GK_N 64 //  'N'									   110          110         //   42                         //主键盘 N
#define GK_O 65 //  'O'									   111          111         //   43                         //主键盘 O
#define GK_P 66 //  'P'									   112          112         //   44                         //主键盘 P
#define GK_Q 67 //  'Q'									   113          113         //   45                         //主键盘 Q
#define GK_R 68 //  'R'									   114          114         //   46                         //主键盘 R
#define GK_S 69 //  'S'									   115          115         //   47                         //主键盘 S
#define GK_T 70 //  'T'									   116          116         //   48                         //主键盘 T
#define GK_U 71 //  'U'									   117          117         //   49                         //主键盘 U
#define GK_V 72 //  'V'									   118          118         //   50                         //主键盘 V
#define GK_W 73 //  'W'									   119          119         //   51                         //主键盘 W
#define GK_X 74 //  'X'									   120          120         //   52                         //主键盘 X
#define GK_Y 75 //  'Y'									   121          121         //   53                         //主键盘 Y
#define GK_Z 76 //  'Z'									   122          122         //   54                         //主键盘 Z

#define GK_LWIN 77 //  0x5B									                311         //   57                         //L_ALT键
#define GK_RWIN 78 //  0x5C									                312         //   58                         //R_ALT键
#define GK_APPS 79 //  0x5D									                            //

#define GK_SLEEP 80 //  0x5F									                            //

#define GK_NUMPAD0 81 //  0x60									                256         //
#define GK_NUMPAD1 82 //  0x61									                257         //
#define GK_NUMPAD2 83 //  0x62									                258         //
#define GK_NUMPAD3 84 //  0x63									                259         //
#define GK_NUMPAD4 85 //  0x64									                260         //   1                          //软左键
#define GK_NUMPAD5 86 //  0x65									                261         //   23                         //中方向键
#define GK_NUMPAD6 87 //  0x66									                262         //   2                          //软右键
#define GK_NUMPAD7 88 //  0x67									                263         //
#define GK_NUMPAD8 89 //  0x68									                264         //
#define GK_NUMPAD9 90 //  0x69									                265         //
#define GK_NUM_ENTER 91 //  									                    271			//
#define GK_MULTIPLY 92 //  0x6A //NUM：乘						   42           268         //
#define GK_ADD 93 //  0x6B //NUM：加						                270         //   24              24         //音量加
#define GK_SEPARATOR 94 //  0x6C									                            //
#define GK_SUBTRACT 95 //  0x6D //NUM：减						                269         //   25              25         //音量减
#define GK_DECIMAL 96 //  0x6E //NUM：点						                266         //
#define GK_DIVIDE 97 //  0x6F //NUM：除						                267         //
//功能键
#define GK_F1 98 //  0x70									   201          282         //   26              26         //电源Power键
#define GK_F2 99 //  0x71									   202          283         //   27                         //相机键
#define GK_F3 100 //  0x72									   203          284         //   84                         //查找
#define GK_F4 101 //  0x73									   204          285         //   82                         //菜单选项
#define GK_F5 102 //  0x74									   205          286         //
#define GK_F6 103 //  0x75									   206          287         //
#define GK_F7 104 //  0x76									   207          288         //
#define GK_F8 105 //  0x77									   208          289         //
#define GK_F9 106 //  0x78									   209          290         //
#define GK_F10 107 //  0x79									   210          291         //
#define GK_F11 108 //  0x7A									   211          292         //
#define GK_F12 109 //  0x7B									   212          293         //
#define GK_F13 110 //  0x7C									   213          294         //
#define GK_F14 111 //  0x7D									   214          295         //
#define GK_F15 112 //  0x7E									   215          296         //
#define GK_F16 113 //  0x7F									   216                      //
#define GK_F17 114 //  0x80									   217                      //
#define GK_F18 115 //  0x81									   218                      //
#define GK_F19 116 //  0x82									   219                      //
#define GK_F20 117 //  0x83									   220                      //
#define GK_F21 118 //  0x84									   221                      //
#define GK_F22 119 //  0x85									   222                      //
#define GK_F23 120 //  0x86									   223                      //
#define GK_F24 121 //  0x87									   224                      //

#define GK_NUMLOCK 122 //  0x90									                300         //   63                         //SYM输入符号切换
#define GK_SCROLL 123 //  0x91									                302         //   5                          //打电话CALL

#define GK_OEM_NEC_EQUAL 124 //  0x92 //等于号						   61           272         //   324                        //等于号

#define GK_OEM_FJ_JISHO 125 //									   							    //
#define GK_OEM_FJ_MASSHOU 126 //  0x93									                            //
#define GK_OEM_FJ_TOUROKU 127 //  0x94									                            //
#define GK_OEM_FJ_LOYA 128 //  0x95									                            //
#define GK_OEM_FJ_ROYA 129 //  0x96									                            //

#define GK_LSHIFT 130 //  0xA0									                304         //   59                         //L_SHIFT键
#define GK_RSHIFT 131 //  0xA1									                303         //   60                         //R_SHIFT键
#define GK_LCONTROL 132 //  0xA2									                306         //
#define GK_RCONTROL 133 //  0xA3									                305         //
#define GK_LMENU 134 //  0xA4									                308         //
#define GK_RMENU 135 //  0xA5									                307         //

#define GK_OEM_1 136 //  0xBA // ';:' for US					   59           59          //
#define GK_OEM_PLUS 137 //  0xBB // '+' any country				   43           43          //
#define GK_OEM_COMMA 138 //  0xBC // 豆号','						   44           44          //   55                         // 豆号','
#define GK_OEM_MINUS 139 //  0xBD // '-' any country				   45           45          //
#define GK_OEM_PERIOD 140 //  0xBE // 点号'.'						   46           46          //   56                         // 点号'.'
#define GK_OEM_2 141 //  0xBF // /号'/?' for US				   47           47          //   76                         // /号'/?' for US
#define GK_OEM_3 142 //  0xC0 // '`~' for US					   96           96          //

#define GK_OEM_4 143 //  0xDB // '[{' for US					   91           91          //
#define GK_OEM_5 144 //  0xDC // '\|' for US					   92           92          //
#define GK_OEM_6 145 //  0xDD // ']}' for US					   93           93          //
#define GK_OEM_7 146 //  0xDE // ''"' for US					   39           39          //
#define GK_OEM_8 147 //  0xDF									                            //

#define GK_OEM_AX 148 //  0xE1 // 'AX' key on Japanese AX kbd                              //   77                         //键盘空格左边的 @
#define GK_OEM_102 149 //  0xE2 // "<>" or "\|" on RT 102-key kbd                           //
#define GK_ICO_HELP 150 //  0xE3 // Help key on ICO                                          //
#define GK_ICO_00 151 //  0xE4 // 00 key on ICO                                            //

#define GK_ICO_CLEAR 152 //  0xE6                                                             //

#define GK_OEM_RESET 153 //  0xE9                                                             //
#define GK_OEM_JUMP 154 //  0xEA                                                             //
#define GK_OEM_PA1 155 //  0xEB									                            //
#define GK_OEM_PA2 156 //  0xEC									                            //
#define GK_OEM_PA3 157 //  0xED									                            //
#define GK_OEM_WSCTRL 158 //  0xEE									                            //
#define GK_OEM_CUSEL 159 //  0xEF									                            //
#define GK_OEM_ATTN 160 //  0xF0									                            //
#define GK_OEM_FINISH 161 //  0xF1									                            //
#define GK_OEM_COPY 162 //  0xF2									                            //
#define GK_OEM_AUTO 163 //  0xF3									                            //
#define GK_OEM_ENLW 164 //  0xF4									                            //
#define GK_OEM_BACKTAB 165 //  0xF5									                            //

#define GK_ATTN 166 //  0xF6									                            //
#define GK_CRSEL 167 //  0xF7									                            //
#define GK_EXSEL 168 //  0xF8									                            //
#define GK_EREOF 169 //  0xF9									                            //
#define GK_PLAY 170 //  0xFA									                            //
#define GK_ZOOM 171 //  0xFB									                            //
#define GK_NONAME 172 //  0xFC									                            //
#define GK_PA1 173 //  0xFD									                            //
#define GK_OEM_CLEAR 174 //  0xFE					 		                                    //

//-------------------------- !!!各系统平台能使用的键位是有差异的 只有在对应平台栏里有值的键位才能使用!!! -------------------------------------------------

//WINDOWS 平台键控码: 可用键位最多的

//SDL     平台键控码: 配备常用的键位

//GLUT    平台键控码有点乱:
/*由于他是按ASCII码形式输入的(适合作检测字符输入处理) 而并不是按不同键位分开的, 所以作游戏控制设置键时最好选择一些意义较唯一的键位
  尽量不要用“Shift”,“Ctrl”,“Alt”和 NumLock这边的键位
  要想程序也能兼容到其它PC系统平台[Linux, MAC]等, 应只选择使用大家都共同存在的键位来开发 */

//ANDROID 和 IOS 平台键控码:
/*该系统的设备不会太多键用到, Android 系统按键的主要用处用来输入文字, 在设计程序时其它控制尽量不要使用按键,
  因很多机器可能没真实的键盘,系统用摸以键盘代替,摸以键盘不能检测到长按键和只能单步按下一个键,
  而且都是按键松开以后系统才传来按下消息,以真实键盘处理不一样,容易引起混乱,
  
  要想程序也能兼容Apple IOS移动系统的设备[iphone, ipad, itouch]等, 应只选择使用大家都共同有的键位 如: [  GK_HOME:返回主屏键 ] 
  其它控制优先考虑在屏幕上画UI控件来处理 */

/**************************************************  鼠标键位标志表  ***************************************************************************************/

#define GM_LEFT 0 //左键
#define GM_CENTRE 1 //滚轮压键
#define GM_RIGHT 2 //右键
#define GM_TOP 3 //上滚
#define GM_BOTTOM 4 //下滚

//鼠标拖动键状态位标识

#define GMM_LEFT 1 //左键拖动
#define GMM_CENTRE 2 //滚轮下压拖动
#define GMM_RIGHT 4 //右键拖动

//某些系统没有鼠标但有触摸屏的触摸信号可以被引擎系统自动转换位鼠标信号 但点击信号只映射为鼠标左键

//************************************************* 感应器支持标记位表 *****************************************************************************************/

#define GI_HAS_WEIGHT 1 //含有重力感应器
#define GI_HAS_DIRECTION 2 //含有方向感应器
#define GI_HAS_FIELD 4 //含有磁场轴感应器
#define GI_HAS_GYROSCOPE 8 //含有陀螺仪感应器
#define GI_HAS_PROXIMITY 16 //含有接近感应器
#define GI_HAS_AMB_LIGHT 32 //含有环境光感应器
#define GI_HAS_TEMPERATURE 64 //含有温度感应器
#define GI_HAS_PRESSURE 128 //含有压力感应器

//使用方式              GNio::EnableSensors( GI_HAS_WEIGHT | GI_HAS_DIRECTION | GI_HAS_AMB_LIGHT );
//判断设备是否有该功能  if(GNio::GetSensorCaps() & GI_HAS_WEIGHT)执行自定义处理;

/******************************************* 传感器和其它通用外设输入值索引表 ******************************************************************************/

#define GI_WX 0 //重力加速方向X  最大值为10.0f 按机子的屏幕朝天放在一个水平面Z为-10.0f 机子左右倾斜X变化 上下倾斜Y变化 即相当于重力珠在OpenGL的坐标系那样
#define GI_WY 1 //重力加速方向Y
#define GI_WZ 2 //重力加速方向Z

#define GI_DX 3 //方向左右旋转角度  最大值为360.0f 按机子的屏幕朝天 机顶部向正北方时 这3个旋转角度都为0.0f  !ipod itunch 没有这个设备
#define GI_DY 4 //方向上下旋转角度  目前仅 android 有效
#define GI_DZ 5 //方向倾侧旋转角度  目前仅 android 有效

#define GI_FX 6 //电磁场轴方向X  最大值为50.0f 该轴将参考一个正Y轴向北再往下45度角 正X轴向东的一个坐标系里  !ipod itunch 没有这个设备
#define GI_FY 7 //电磁场轴方向Y
#define GI_FZ 8 //电磁场轴方向Z

#define GI_GX 9 //陀螺仪转动加速角度X  !iphone4时代之前的设备可能不支持这个
#define GI_GY 10 //陀螺仪转动加速角度Y
#define GI_GZ 11 //陀螺仪转动加速角度Z

#define GI_PROXIMITY 12 //靠近传感器     当有物体靠近机子前面时值为0.0f  否则为其他值 仅 android 有 apple 的设备可能不提供这个给应用程序开发控制
#define GI_AMB_LIGHT 13 //环境光感应器   用于测量外界光暗度 仅 android 有这个  apple 的设备可能不提供这个给应用程序开发控制
#define GI_TEMPERATURE 14 //温度感应器                        仅 android 有这个  apple 的设备可能不提供这个给应用程序开发控制

#define GI_PRESSURE 15 //压力感应 ...                      仅 android 有这个  apple 的设备可能不提供这个给应用程序开发控制

//使用方式 if(GNio::InFv[ GI_WX ] > 0.0f)执行自定义处理;

/************************************************************************************************************************************************************/
#define GN_MAX_KEY_NUM 176 //最大键盘键数
#define GN_MAX_MK_NUM 5 //最大鼠标键数

namespace GN { /* 键盘与鼠标触摸等状态键变量数组 -----------------------------------------------------------------------------------*/

//使用方式 if(GN::Key [GK_RETURN])执行自定义处理;

extern GN_INIT_API bool Key[GN_MAX_KEY_NUM]; //键盘按紧响应键
extern GN_INIT_API bool Key0[GN_MAX_KEY_NUM]; //键盘松开响应键
extern GN_INIT_API bool Key1[GN_MAX_KEY_NUM]; //键盘单次响应键
extern GN_INIT_API bool Key11[GN_MAX_KEY_NUM]; //键盘连续响应键
extern GN_INIT_API bool KeyDown; //标记当前键盘的任意键是否有按下

//使用方式 if(GN::Mouse [GM_LEFT])执行自定义处理;

extern GN_INIT_API bool Mouse[GN_MAX_MK_NUM]; //鼠标5个按紧键
extern GN_INIT_API bool Mouse0[GN_MAX_MK_NUM]; //鼠标5个松开键
extern GN_INIT_API bool Mouse1[GN_MAX_MK_NUM]; //鼠标5个单次键
extern GN_INIT_API bool Mouse2[GN_MAX_MK_NUM]; //鼠标5个双击键
extern GN_INIT_API bool MouseQ[GN_MAX_MK_NUM]; //鼠标5个原位松开键
extern GN_INIT_API bool MouseI[GN_MAX_MK_NUM]; //鼠标5个按下立即松开键 !界面通常用这个来代替 单次键 会更好地兼容触摸屏操作
extern GN_INIT_API unsigned int MouseT[GN_MAX_MK_NUM]; //鼠标5个键原位按紧不动的累计时间

extern GN_INIT_API bool MouseM; //标记当前鼠标是否发生移动
extern GN_INIT_API int MouseMX; //标记鼠标点击或移动的窗口新位置X  //!!值得注意 在SDL平台下创建了固定窗口大小后 再拉大时传回的鼠标位置也会局限在原来的窗口大小内
extern GN_INIT_API int MouseMY; //标记鼠标点击或移动的窗口新位置Y  //!!值得注意 同上

extern GN_INIT_API unsigned short MouseMKS; //鼠标拖动键状态 标记当前鼠标有按住那些键正在拖动 这个值里的标识是可按位组合的如 GMM_LEFT | GMM_CENTRE | GMM_RIGHT

extern GN_INIT_API bool TouchM; //标记当前触摸是否发生过拖动
extern GN_INIT_API short *TouchX; //点触摸时当前窗口位置X
extern GN_INIT_API short *TouchY; //点触摸时当前窗口位置Y
extern GN_INIT_API unsigned char *TouchZ; //点触摸时当前窗口位置Z  255:刚点下去 1:刚提起来 0:没有接触 其它值:越大点击接触范围越大  !!当按下去之后就算按紧值最大为254

extern GN_INIT_API bool ViewReSized; //标记当前主视口是否发生过大小更变

struct sOPERATE_TIP //操作提示
{
    uint8 device_type; //设备类型 0:键盘 1:鼠标 2:触屏  0xff:其他
    uint8 action; //对应设置的操作动作 当设备为键盘选值有GK_LBUTTON...等  当设备为鼠标选值有GM_LEFT...等
    P<char> desc; //操作描述
};
//设置触摸点自动映射为鼠标信号的防手抖容忍范围(容忍范围 单位为象素) 默认时为:16 值大一点相对手指来点更容易点中 但这不利于压感笔这类设备 防斗值大意味着你按下后不能拖动出少于该值的线条 如果是笔等设备值设少一点比较好
GN_INIT_API void SetTouchMappingToMouseDitherRange(unsigned short dr = 16);
//设置重力感应器信号是否自动映射到一些键盘特定键(是否) 会被映射的键位有 GK_D GK_A GK_W GK_S GK_LEFT GK_RIGHT GK_UP GK_DOWN !!调用这个你需在有重力感应器的设备并重力感应器已经被开启的情况下才会起作用 默认时为:false 不自动映射
GN_INIT_API void SetWeightSensorMappingToKey(bool yes);

//记录控制操作 摸以自动操作功能函数 分别对应两个消息队列数据 一个用于记录操作 一个用于播放操作用 -------------------------------------

//从新开始记录输入操作()
GN_INIT_API void InputRecReStart();
//在末尾加上退出消息() !这可以让下次演示时能自动退出程序
GN_INIT_API void InputRecExitMsg();
//结束记录输入操作()
GN_INIT_API void InputRecEnd();
//把操作记录保存到文件(保存文件名, 文件更新序号)
GN_INIT_API void InputRecSave(const char *file_name, unsigned int dateID = 0);
//清除输入记录数据()
GN_INIT_API void InputRecClear();

//打开输入操作记录播放文件(打开文件名, 文件更新序号输出, 记录开始时间输出) 返回是否读取成功
GN_INIT_API bool InputRecOpenPlay(const char *file_name, unsigned int *dateID = 0, unsigned int *time = 0);
//从新开始演示播放输入记录 实现模以自动输入()
GN_INIT_API void InputRecRePlay();
//停止演示播放输入操作()
GN_INIT_API void InputRecStopPlay();
//清除输入播放记录()
GN_INIT_API void InputRecClearPlay();

//使录制队列数据交换到播放队列以准备演示播放 同时使两个队列都停止录制和播放 必需调用InputRecRePlay();才能继续播放 调用InputRecReStart();继续录制
GN_INIT_API void InputRecSwapToPlay();

//获取在输入记录里最后一个键盘松开键位()返回键位 返回0xff表示没找到
GN_INIT_API uint8 InputRecLastKeyUpGet();
}; // namespace GN
/*-------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

//设置鼠标双击或触摸点长按映射鼠标双击的间隔(间隔时间, XY的位置误差)
GN_INIT_API void GN_SetMouseDoubleClick(unsigned int time, unsigned int deviation);

//设置鼠标离开视口是否自动重置回中间位置(视口, X位置是否需重置, Y位置是否需重置)
GN_INIT_API void GN_SetMouseOutViewRePos(GNg::cVIEW2D *view, bool xpos, bool ypos);

/***************************************************************  文字输入  ************************************************************************************************/

//检测当前是否在接受输入()返回是否
GN_INIT_API bool GN_ImeIsOpen();

//系统软键盘是否需要打开() !在没真实键盘的系统中按照该值得知是否要打开软键盘 !如果你是开发系统接口的需检测该值来作处理
GN_INIT_API bool GN_SkbNeedOpen();

/**************************************************************外部IO的东西*************************************************************************************************/
namespace GNio {
GN_INIT_API void OutPutAllON(); //IO输出全部开启()
GN_INIT_API void OutPutAllOFF(); //IO输出全部关闭()

//IO输出测试(帧间隔时间, 换端口间隔时间)返回0表示结束 !这个要在游戏循环里挂起
GN_INIT_API bool OutPutTestDisplay(unsigned int nTime, unsigned int port_change_time);
//IO输入测试(帧间隔时间, 指示延迟时间)返回最近输入的端口号 !这个要在游戏循环里挂起
GN_INIT_API int InPutTestDisplay(unsigned int nTime, unsigned int show_delay_time);

//设置IO输入的键盘映射键(IO端口, 键盘键位)
GN_INIT_API void SetIOMappingKey(short IO_port, unsigned short GK_key);
}; // namespace GNio
/****************************************************** 常用时间函数  ****************************************************************************/

extern GN_INIT_API unsigned int GT_IdleTime; //程序操作空闲时间
extern GN_INIT_API unsigned int GT_RunTotalTime; //程序开始运行经过的总时间
extern GN_INIT_API unsigned int GT_RunTotalTimeApp; //程序开始运行用户应用经过的总时间  由1开始
extern GN_INIT_API unsigned int GT_LastRunTotalTimeApp; //记录上帧运行用户应用经过的总时间  由1开始
extern GN_INIT_API unsigned int GT_RunTotalFpsApp; //程序开始运行用户应用经过的总帧数

namespace GN {
//获得程序空闲了多长时间没有鼠标 触摸 键盘输入操作 调用速度快 单位毫秒
inline unsigned int TimeGetIdleCount() {
    return GT_IdleTime;
}
//获得程序已经运行用户应用所经过的总时间() 调用速度快 该时间不包含系统开始时自动更新和设置的运行时间 时间由实际的用户应用运行开始算起 由1开始 单位毫秒
inline unsigned int TimeGetRunCount() {
    return GT_RunTotalTimeApp;
}
//获得程序已经运行用户应用所经过的总帧数() 调用速度快 该时间不包含系统开始时自动更新和设置的运行帧数 帧数由实际的用户应用运行开始算起
inline unsigned int TimeGetRunFpsCount() {
    return GT_RunTotalFpsApp;
}

//获取系统时间值() 这个精度高但调用慢些 单位毫秒
inline double TimeGetCount() {
    return GN_TimeGetCount();
}
}; // namespace GN
/*************************************************** 引用 US_LIB 的功能 **************************************************************************/
#ifdef GN_USING_US_LIB
#ifndef WIN32
#define WIN32
#endif
#include <us_lib/us_global.h>
#include <us_lib/us_other.h>
#include <us_lib/us_iniset.h>
#endif
/*************************************************************************************************************************************************/

GN_INIT_API void GN_InitInput(); //初始化引擎外部IO设备()
GN_INIT_API void GN_UnInitInput(); //释放引擎外部IO设备()
GN_INIT_API void GN_InputProc(unsigned int update_time); //挂起处理运行外部IO一些杂务(更新间隔时间)

/************ 输入状态更改通知 !!以下这些函数主要供系统平台接口调用以便把有用的系统的消息传达给引擎 应用开发可以不用理会 ****************************************/
extern "C" {
//重设全部输入状态以便准备正确接收输入( 传递系统IME输入文本给引擎的处理函数(引擎给你装填文本内容的指针, 装填文本字符串长度) )  !!在主循环内接收输入消息前必须调用
GN_INIT_API void GF_ReSetInputState(void (*get_ime_func)(char *ime_str, uint8 *ime_len));

//通知按下的键位全部松开() !这个通常在窗口失去操作焦点后要调用的
GN_INIT_API void GF_DownKeyAllLoosen();

//设置鼠标键状态(键位, 是否按下, 点击时的X坐标,点击时的Y坐标) !!参数键位在鼠标键位标志里有定义 如: GM_LEFT
GN_INIT_API void GF_SetMouseState(uint8 mkey, bool is_down, int px, int py);

//设置鼠标移动状态(拖动键位, 新位置X, 新位置Y) //参数拖动键位可以有多个组合 如: GMM_LEFT | GMM_RIGHT
GN_INIT_API void GF_SetMouseMoveState(unsigned short keys_state, int new_x, int new_y);

//设置触摸点击状态(触点ID , 是否按下还是松开)  参数down 255:按下 1:松开
GN_INIT_API void GF_SetTouchState(uint8 point_id, uint8 down);

//设置触摸滑动状态(触点ID , 新压力Z量, 新位置X, 新位置Y) 参数down_z 取值:254 - 2  值越大点击接触范围越大  !!当按下去之后就算按紧值最大为254
GN_INIT_API void GF_SetTouchMoveState(uint8 point_id, uint8 down_z, short new_x, short new_y);

//设置按键状态(键位, 是否按下) !! 键位值在 键盘键位标志表 里有定义 如: GK_RETURN
GN_INIT_API void GF_SetKeyState_(int key, bool is_down);

extern GN_INIT_API void (*GF_SetKeyState)(int key, bool is_down);

//输入更新浮点值状态(要更新浮点值的起始索引, 输入数据,  输入数据个数) !!参数infv_start_id的参考值在 传感器输入和其它通用外设一栏里有定义 如: GI_WX GI_DX 等
GN_INIT_API void GF_SetInFvState(short infv_start_id, const float *infv, short in_count);

//设置外部IO状态（端口ID, 是否为开)
GN_INIT_API void GF_SetInPortState(unsigned short port_id, bool is_on);

//重设绘图主窗口大小(新宽度, 新高度)
GN_INIT_API void GF_ReSetViewport(short new_vw, short new_vh);
}
#endif //_GN_INPUT_H_
