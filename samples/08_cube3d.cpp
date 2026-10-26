// Test 08: Terrain — Perlin Noise 分形地形
// 经典 Perlin 2D 噪声 + 多 octave 叠加 (FBM)
// 80×80 网格, 24×24 世界空间, Blinn-Phong 渲染
#include "common/test_utils.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>

static GN3d::cMODEL *g_terrain = nullptr;
static GN3d::cLIGHT  *g_light   = nullptr;

static float g_camYaw=0, g_camPitch=0.6f, g_camDist=30.0f;
static GNg::sVEC3 g_target = {0,2,0};
static float g_dt = 0.016f;

static inline float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ═══════════════════════════════════════════════════════════
//  2D Perlin Noise 实现
// ═══════════════════════════════════════════════════════════

// 排列表 (Ken Perlin 原始)
static const unsigned char g_perm[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
    184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

// 平滑曲线
static inline float Fade(float t) { return t*t*t*(t*(t*6-15)+10); }
static inline float Lerp(float a, float b, float t) { return a + t*(b-a); }

// 梯度向量点积
static float Grad(int hash, float x, float y) {
    int h = hash & 3;
    float u = h<2 ? x : y;
    float v = h<2 ? y : x;
    return ((h&1) ? -u : u) + ((h&2) ? -v : v);
}

// 2D Perlin 噪声 (单 octave)
static float PerlinNoise(float x, float y) {
    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float u = Fade(xf), v = Fade(yf);

    const unsigned char *p = g_perm;
    int a = p[p[X  ]+Y  ], b = p[p[X+1]+Y  ];
    int c = p[p[X  ]+Y+1], d = p[p[X+1]+Y+1];

    return Lerp(Lerp(Grad(a, xf,   yf),   Grad(b, xf-1, yf),   u),
                Lerp(Grad(c, xf,   yf-1), Grad(d, xf-1, yf-1), u), v);
}

// 分形布朗运动 (FBM): 多 octave 叠加
static float FBM(float x, float y, int octaves, float lacunarity, float gain) {
    float value = 0, amplitude = 1.0f, frequency = 1.0f, maxVal = 0;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * PerlinNoise(x * frequency, y * frequency);
        maxVal += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return value / maxVal;  // 归一化到 [-1, 1]
}

// ── 最终高度函数 ─────────────────────────────────────
static float Height(float x, float z) {
    // 低 gain(0.35) + 低基频(0.08) = 大尺度起伏 + 无尖峰
    float h = FBM(x*0.08f, z*0.08f, 7, 2.3f, 0.35f);
    h = (h + 1.0f) * 0.5f;
    return h * 8.0f;   // 最高 8 单位
}

// ═══════════════════════════════════════════════════════════
//  水力侵蚀 (Hydraulic Erosion)
//  模拟雨滴沿梯度流动: 侵蚀坡面, 在低洼处沉积
// ═══════════════════════════════════════════════════════════
static void HydraulicErosion(float *heightmap, int grid, int iterations,
                              float erosionRate, float depositionRate,
                              float evaporation, float minSlope, int maxSteps) {
    for (int iter = 0; iter < iterations; iter++) {
        // 随机起始位置
        int cx = rand() % grid;
        int cz = rand() % grid;
        float water = 1.0f, sediment = 0.0f, velocity = 1.0f;

        for (int step = 0; step < maxSteps; step++) {
            int ix = cx, iz = cz;

            // 找最低的邻居
            float h0 = heightmap[iz * grid + ix];
            int nx = ix, nz = iz;
            float lowest = h0;
            for (int dz = -1; dz <= 1; dz++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dz == 0) continue;
                    int tx = ix + dx, tz = iz + dz;
                    if (tx < 0 || tx >= grid || tz < 0 || tz >= grid) continue;
                    float h = heightmap[tz * grid + tx];
                    if (h < lowest) { lowest = h; nx = tx; nz = tz; }
                }
            }
            if (nx == ix && nz == iz) break; // 局部最低点

            float dh = h0 - lowest;
            float slope = dh;  // 简化: 假设单位网格间距

            // 侵蚀: 水流带走泥沙
            float erode = erosionRate * dh * water;
            heightmap[iz * grid + ix] -= erode;
            sediment += erode;

            // 沉积: 水流携带超过容量的泥沙
            float capacity = fmaxf(slope, minSlope) * water * velocity;
            if (sediment > capacity) {
                float deposit = (sediment - capacity) * depositionRate;
                heightmap[iz * grid + ix] += deposit;
                sediment -= deposit;
            }

            // 移动 + 蒸发
            cx = nx; cz = nz;
            water *= (1.0f - evaporation);
            velocity = sqrtf(velocity * velocity + dh);
        }

        // 剩余泥沙沉积在终点
        if (cx >= 0 && cx < grid && cz >= 0 && cz < grid)
            heightmap[cz * grid + cx] += sediment * depositionRate;
    }
}

// ═══════════════════════════════════════════════════════════
//  地形网格生成 (带侵蚀)
// ═══════════════════════════════════════════════════════════
static GN3d::cMODEL *CreateTerrain(int grid, float size) {
    int vc = grid * grid;
    int tc = (grid-1)*(grid-1)*2;
    int ic = tc * 3;

    // 第1步: 生成高度图
    float *hm = new float[vc];
    float step = size / (grid-1), half = size*0.5f;
    for (int iz=0; iz<grid; iz++)
        for (int ix=0; ix<grid; ix++)
            hm[iz*grid+ix] = Height(-half+ix*step, -half+iz*step);

    // 第2步: 高斯平滑 (抑制尖峰)
    float *sm = new float[vc];
    for (int pass = 0; pass < 2; pass++) {
        memcpy(sm, hm, vc * sizeof(float));
        for (int iz = 1; iz < grid-1; iz++) {
            for (int ix = 1; ix < grid-1; ix++) {
                int idx = iz * grid + ix;
                hm[idx] = (sm[idx]*4 + sm[idx-1] + sm[idx+1]
                         + sm[idx-grid] + sm[idx+grid]) / 8.0f;
            }
        }
    }
    delete[] sm;

    // 第3步: 水力侵蚀
    HydraulicErosion(hm, grid,
                     25000,      // 雨滴数
                     0.08f,      // 侵蚀率
                     0.3f,       // 沉积率
                     0.02f,      // 蒸发率
                     0.01f,      // 最小坡度
                     30);        // 最大流动步数

    // 第3步: 生成网格
    float *v = new float[vc * 11];
    uint32_t *idx = new uint32_t[ic];
    float eps = step * 0.5f;

    for (int iz=0; iz<grid; iz++) {
        for (int ix=0; ix<grid; ix++) {
            int vi = (iz*grid + ix)*11;
            float wx = -half + ix*step;
            float wz = -half + iz*step;
            float wy = hm[iz*grid+ix];
            v[vi+0]=wx; v[vi+1]=wy; v[vi+2]=wz;

            // 法线: 用侵蚀后的高度图差分
            auto H = [&](int x, int z) {
                x = x<0?0:(x>=grid?grid-1:x);
                z = z<0?0:(z>=grid?grid-1:z);
                return hm[z*grid+x];
            };
            float nR=H(ix+1,iz), nL=H(ix-1,iz);
            float nU=H(ix,iz+1), nD=H(ix,iz-1);
            float nx=-(nR-nL)/(2*step), nz=-(nU-nD)/(2*step), ny=1;
            float len=sqrtf(nx*nx+ny*ny+nz*nz);
            v[vi+3]=nx/len; v[vi+4]=ny/len; v[vi+5]=nz/len;
            v[vi+6]=1; v[vi+7]=0; v[vi+8]=0;
            v[vi+9]=(float)ix/(grid-1); v[vi+10]=(float)iz/(grid-1);
        }
    }
    int ii=0;
    for(int iz=0; iz<grid-1; iz++)
        for(int ix=0; ix<grid-1; ix++) {
            int a=iz*grid+ix, b=a+grid;
            idx[ii++]=a; idx[ii++]=b; idx[ii++]=a+1;
            idx[ii++]=a+1; idx[ii++]=b; idx[ii++]=b+1;
        }

    delete[] hm;
    GN3d::cMODEL *m = new GN3d::cMODEL();
    strcpy(m->aName,"Terrain"); m->aMeshCount=1;
    m->aMeshes = new GN3d::sMESH[1];
    m->aMeshes[0].aVertices=v; m->aMeshes[0].aIndices=idx;
    m->aMeshes[0].aVertexCount=vc; m->aMeshes[0].aIndexCount=ic;
    m->aMeshes[0].aDiffuse[0]=0.20f;
    m->aMeshes[0].aDiffuse[1]=0.48f;
    m->aMeshes[0].aDiffuse[2]=0.15f;
    strcpy(m->aMeshes[0].aName,"mesh");
    m->UpdateTransform();
    return m;
}

extern "C" {

GN_CALL void AppInit() {
    srand(42);
    g_terrain = CreateTerrain(100, 48.0f);  // 100×100 网格, 48×48 世界空间
    g_light = new GN3d::cLIGHT();
    GNg::sVEC3 a={0.25f,0.28f,0.30f}; g_light->SetAmbient(a,1);
    GNg::sVEC3 d={0.4f,-0.8f,0.3f}, c={1,0.95f,0.82f};
    g_light->AddDirectional(d,c,1.8f,false);
    GNg::sVEC3 p={0,10,0}, pc={0.4f,0.42f,0.5f};
    g_light->AddPoint(p,35,pc,0.5f);
}

GN_CALL void AppUnInit() { delete g_terrain; delete g_light; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    float cp=cosf(g_camPitch),sp=sinf(g_camPitch);
    float sy=sinf(g_camYaw),cy=cosf(g_camYaw);
    GNg::sVEC3 pos; pos.x=g_target.x+g_camDist*cp*sy;
    pos.y=g_target.y+g_camDist*sp; pos.z=g_target.z+g_camDist*cp*cy;
    GNg::sVEC3 up={0,1,0};
    GNg::sMAT44 vm,pm; GNg::Mat44LookAtRH(&vm,&pos,&g_target,&up);
    GNg::Mat44PerspectiveVK(&pm,0.785398f,800.0f/600,0.1f,200); GNg::sMAT44 vp;
    GNg::Mat44Mul(&vp,&pm,&vm);
    GNg::GN_VkBegin3DPass(&vp,&pos,g_light->GetUBO(),sizeof(GN3d::sLIGHT_UBO));
    g_terrain->Draw();
    GNg::GN_VkEnd3DPass();
}

GN_CALL void AppDisplay3D(unsigned int t) { (void)t; }

GN_CALL void AppRun(unsigned int nTime) {
    static unsigned int l=0;
    g_dt=(float)(nTime-l)/1000.0f; l=nTime;
    if(g_dt<=0||g_dt>0.5f)g_dt=0.016f;
    float o=1.0f*g_dt;
    if(GN::Key[GK_W])g_camPitch+=o; if(GN::Key[GK_S])g_camPitch-=o;
    if(GN::Key[GK_A])g_camYaw-=o;   if(GN::Key[GK_D])g_camYaw+=o;
    if(GN::Key[GK_UP])g_target.y+=2*g_dt;
    if(GN::Key[GK_DOWN])g_target.y-=2*g_dt;
    if(GN::Mouse[GM_LEFT]&&GN::MouseM){
        g_target.x-=GNg::ViewCur->mouseMSX*0.03f;
        g_target.z-=GNg::ViewCur->mouseMSY*0.03f;
    }
    if(GN::Key[GK_ADD]||GN::Mouse[GM_TOP])g_camDist-=6*g_dt;
    if(GN::Key[GK_SUBTRACT]||GN::Mouse[GM_BOTTOM])g_camDist+=6*g_dt;
    g_camPitch=Clamp(g_camPitch,-1.4f,1.4f); g_camDist=Clamp(g_camDist,3,60);
    if(GN::Key1[GK_R]){g_camYaw=0;g_camPitch=0.4f;g_camDist=18;
        g_target.x=0;g_target.y=2;g_target.z=0;}
}

} // extern "C"
