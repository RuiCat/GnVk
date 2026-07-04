#include<GN.h>

// ==================== 猫猫状态 ====================
static float neko_x = 350, neko_y = 350;
static int   happiness = 70;
static int   fish_treats = 5;
static float anim_timer = 0;
static int   walk_frame = 0;
static bool  fed_this_press = false;
static bool  pet_this_press = false;

// ==================== 爱心粒子 ====================
struct Heart { float x,y,vy,life; int size; };
static Heart hearts[30];
static int   heart_count = 0;

// ==================== 颜色常量 (ARGB) ====================
#define C_SKY         0xff87CEEB
#define C_WALL        0xffFFF8E7
#define C_FLOOR       0xffDEB887
#define C_WINDOW      0xffADD8E6
#define C_WINFRAME    0xffF0F0F0
#define C_NEKO_BODY   0xffFFD700
#define C_NEKO_EAR    0xffFFB6C1
#define C_NEKO_DARK   0xff443322
#define C_WHITE       0xffFFFFFF
#define C_BED         0xffE6B8C2
#define C_PILLOW      0xffFFFFFF
#define C_FISH        0xffFF8C42
#define C_HEART       0xffFF4477
#define C_HAPPY_BAR   0xff44DD44
#define C_HAPPY_MID   0xffDDA444
#define C_HAPPY_LOW   0xffDD4444
#define C_HAPPY_BG    0xff555555
#define C_TABLE       0xff8B7355
#define C_SHADOW      0x44000000
#define C_BORDER      0xffDAA520

// ==================== 辅助绘图函数 ====================
static void drawRect(int x, int y, int w, int h, unsigned int color){
    GNg::ImageDraw(0, x, y, w, h, color);
}
static void drawBox(int x, int y, int w, int h, int lw, unsigned int color){
    GNg::LineFrame2dDraw(x, y, w, h, lw, color, GNg::eBLEND_NORMAL);
}
static void spawnHeart(float x, float y){
    if(heart_count < 30){
        hearts[heart_count].x = x + (heart_count%5 - 2)*4.0f;
        hearts[heart_count].y = y;
        hearts[heart_count].vy = -1.5f - (heart_count%3)*0.5f;
        hearts[heart_count].life = 50;
        hearts[heart_count].size = 3 + (heart_count % 4) * 2;
        heart_count++;
    }
}

// ==================== 绘制猫猫 ====================
static void drawNeko(float cx, float cy, int frame){
    int x = (int)cx, y = (int)cy;
    int bounce = (frame % 2) ? 2 : 0;

    // 阴影
    drawRect(x-14, y+34, 28, 5, C_SHADOW);

    // 尾巴 (S形)
    int tx = x + 10, ty = y + 10 + bounce;
    drawRect(tx,   ty,    6, 22, C_NEKO_BODY);
    drawRect(tx-2, ty+8,  8, 8,  C_NEKO_BODY);
    drawRect(tx+2, ty-6,  4, 10, C_NEKO_BODY);

    // 身体
    drawRect(x-16, y+4, 32, 28, C_NEKO_BODY);
    drawBox (x-16, y+4, 32, 28, 2, C_BORDER);

    // 前腿
    drawRect(x-11, y+26, 9, 10, C_NEKO_BODY);
    drawRect(x+2,  y+26, 9, 10, C_NEKO_BODY);

    // 头
    drawRect(x-12, y-20 + bounce, 24, 22, C_NEKO_BODY);
    drawBox (x-12, y-20 + bounce, 24, 22, 2, C_BORDER);

    // 耳朵 (三角近似)
    drawRect(x-14, y-32 + bounce, 7, 13, C_NEKO_BODY);
    drawRect(x-11, y-34 + bounce, 4, 10, C_NEKO_EAR);
    drawRect(x+7,  y-32 + bounce, 7, 13, C_NEKO_BODY);
    drawRect(x+7,  y-34 + bounce, 4, 10, C_NEKO_EAR);

    // 眼睛
    drawRect(x-8, y-16 + bounce, 5, 6, C_NEKO_DARK);
    drawRect(x+3, y-16 + bounce, 5, 6, C_NEKO_DARK);
    drawRect(x-7, y-16 + bounce, 2, 3, C_WHITE);
    drawRect(x+4, y-16 + bounce, 2, 3, C_WHITE);

    // 鼻子
    drawRect(x-2, y-9 + bounce, 4, 3, C_NEKO_EAR);

    // 嘴
    drawRect(x-4, y-6 + bounce, 8, 1, C_NEKO_DARK);

    // 胡子
    drawRect(x-17, y-11 + bounce, 5, 1, C_NEKO_DARK);
    drawRect(x-16, y-9  + bounce, 4, 1, C_NEKO_DARK);
    drawRect(x+12, y-11 + bounce, 5, 1, C_NEKO_DARK);
    drawRect(x+12, y-9  + bounce, 4, 1, C_NEKO_DARK);
}

// ==================== App 回调 ====================
extern "C" {

GN_CALL void AppInit(){
    neko_x = 350; neko_y = 350;
    happiness = 70;
    fish_treats = 5;
    heart_count = 0;
    anim_timer = 0;
}

GN_CALL void AppUnInit(){}

GN_CALL void AppDisplay2D(unsigned int nTime){
    // ====== 天空渐变 ======
    for(int i = 0; i < 280; i += 4){
        int r = 130 - i/6; if(r<30) r=30;
        int g = 180 - i/8; if(g<50) g=50;
        int b = 220 - i/4; if(b<100) b=100;
        drawRect(0, i, 800, 4, 0xff000000 | (r<<16) | (g<<8) | b);
    }

    // ====== 墙壁 ======
    drawRect(0, 90, 800, 510, C_WALL);

    // ====== 地板 ======
    drawRect(0, 460, 800, 140, C_FLOOR);
    drawRect(0, 457, 800, 5, 0xffC4A06C);

    // ====== 窗户 ======
    drawRect(60, 15, 180, 140, C_WINFRAME);
    drawBox (60, 15, 180, 140, 4, 0xffD0D0D0);
    drawRect(68, 23, 164, 124, C_WINDOW);
    // 太阳
    drawRect(178, 33, 36, 36, 0xffFFEE44);
    drawBox (178, 33, 36, 36, 2, 0xffFFCC00);
    // 十字框
    drawRect(148, 23, 4, 124, C_WINFRAME);
    drawRect(68, 80, 164, 4, C_WINFRAME);

    // ====== 小床 ======
    drawRect(570, 370, 170, 90, C_BED);
    drawBox (570, 370, 170, 90, 2, 0xffC4909E);
    // 枕头
    drawRect(582, 358, 75, 16, C_PILLOW);
    drawRect(658, 358, 75, 16, C_PILLOW);
    // 被子
    drawRect(578, 385, 152, 68, 0xffD4A0AE);

    // ====== 小桌子 ======
    drawRect(240, 420, 130, 8, C_TABLE);
    drawRect(258, 428, 7, 35, C_TABLE);
    drawRect(340, 428, 7, 35, C_TABLE);

    // ====== 小鱼干 ======
    for(int i = 0; i < fish_treats && i < 10; i++){
        int fx = 252 + i * 24;
        drawRect(fx+2, 408, 12, 7,  C_FISH);       // 身体
        drawRect(fx+4, 404, 8, 14,  C_FISH);       // 身体
        drawRect(fx-3, 410, 6, 5,  C_FISH);        // 尾巴
        drawRect(fx+9, 408, 2, 2,  C_NEKO_DARK);   // 眼睛
    }

    // ====== 猫猫 ======
    drawNeko(neko_x, neko_y, walk_frame);

    // ====== 爱心粒子 ======
    for(int i = 0; i < heart_count; i++){
        if(hearts[i].life > 0){
            int a = (int)(hearts[i].life * 255 / 50);
            if(a > 255) a = 255;
            unsigned int hc = ((unsigned int)a << 24) | (C_HEART & 0x00FFFFFF);
            int s = hearts[i].size;
            drawRect((int)hearts[i].x - s,   (int)hearts[i].y - s/2, s*2, s,   hc);
            drawRect((int)hearts[i].x - s/2, (int)hearts[i].y - s,   s,   s*2, hc);
        }
    }

    // ====== HUD ======
    // 幸福度背景
    drawRect(555, 8, 225, 24, C_HAPPY_BG);
    drawBox (555, 8, 225, 24, 2, 0xff777777);
    // 幸福度条
    int bw = happiness * 221 / 100;
    unsigned int bc = happiness > 60 ? C_HAPPY_BAR : happiness > 30 ? C_HAPPY_MID : C_HAPPY_LOW;
    drawRect(557, 10, bw, 20, bc);

    // 文字信息
    GN_RunTextOut("幸福度", 0, 560, 0, 0, 0, 0xffEEEEEE, GNg::eBLEND_NORMAL);
    GN_RunTextOut("小鱼干 x", 0, 660, 0, 0, 0, 0xffEEEEEE, GNg::eBLEND_NORMAL);

    // 心情文字
    if(happiness > 70)
        GN_RunTextOut("喵~好幸福呀！", 0, 300, 535, 0, 0, 0xff884422, GNg::eBLEND_NORMAL);
    else if(happiness > 40)
        GN_RunTextOut("主人陪我玩嘛~", 0, 300, 535, 0, 0, 0xff996633, GNg::eBLEND_NORMAL);
    else if(happiness > 15)
        GN_RunTextOut("有点孤单呢...", 0, 300, 535, 0, 0, 0xffBB6644, GNg::eBLEND_NORMAL);
    else
        GN_RunTextOut("呜...主人不要猫猫了吗...", 0, 300, 535, 0, 0, 0xffDD5555, GNg::eBLEND_NORMAL);

    // 操作提示
    GN_RunTextOut("方向键移动 | F键喂食 | P键摸摸 | ESC退出", 0, 160, 575, 0, 0, 0xff998877, GNg::eBLEND_NORMAL);
}

GN_CALL void AppDisplay3D(unsigned int nTime){}

GN_CALL void AppRun(unsigned int nTime){
    anim_timer += 1.0f;

    // ====== 移动 ======
    float speed = 3.5f;
    bool moving = false;
    if(GN::Key[GK_LEFT]  || GN::Key['A']){ neko_x -= speed; moving = true; }
    if(GN::Key[GK_RIGHT] || GN::Key['D']){ neko_x += speed; moving = true; }
    if(GN::Key[GK_UP]    || GN::Key['W']){ neko_y -= speed; moving = true; }
    if(GN::Key[GK_DOWN]  || GN::Key['S']){ neko_y += speed; moving = true; }

    if(moving && ((int)anim_timer % 6 == 0)) walk_frame = (walk_frame + 1) % 4;
    if(!moving) walk_frame = 0;

    // 区域限制
    if(neko_x < 25)  neko_x = 25;
    if(neko_x > 760) neko_x = 760;
    if(neko_y < 130) neko_y = 130;
    if(neko_y > 440) neko_y = 440;

    // ====== 喂食 (F键) ======
    if(GN::Key1[GK_F] && !fed_this_press){
        fed_this_press = true;
        if(fish_treats > 0){
            fish_treats--;
            happiness += 12; if(happiness > 100) happiness = 100;
            for(int i = 0; i < 8; i++) spawnHeart(neko_x - 12 + i*3, neko_y - 30);
        }
    }
    if(!GN::Key1[GK_F]) fed_this_press = false;

    // ====== 摸摸 (P键) ======
    if(GN::Key1[GK_P] && !pet_this_press){
        pet_this_press = true;
        happiness += 4; if(happiness > 100) happiness = 100;
        for(int i = 0; i < 3; i++) spawnHeart(neko_x, neko_y - 25);
    }
    if(!GN::Key1[GK_P]) pet_this_press = false;

    // ====== 幸福度自然衰减 ======
    if((int)anim_timer % 150 == 0 && happiness > 0) happiness--;

    // ====== 更新爱心 ======
    for(int i = 0; i < heart_count; i++){
        if(hearts[i].life > 0){
            hearts[i].y += hearts[i].vy;
            hearts[i].vy -= 0.04f;
            hearts[i].life--;
        }
    }
    // 压缩数组
    int w = 0;
    for(int i = 0; i < heart_count; i++)
        if(hearts[i].life > 0){ if(w != i) hearts[w] = hearts[i]; w++; }
    heart_count = w;
}

} // extern "C"
