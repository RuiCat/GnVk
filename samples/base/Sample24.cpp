#include<GN.h>
#ifdef GN_USING_3D
#include"GN_3Dlight.h"
#include"GN_3Dmodel.h"
using namespace GNg;
using namespace GN3d;

// ═══════════════════════════════════════════════════════════
//  Sample24 — PBR + 阴影映射 + HDR/Bloom 完整演示
//  使用修复后的渲染管线全部功能
// ═══════════════════════════════════════════════════════════

static cMODEL *gSphere   = 0;
static cMODEL *gFloor    = 0;
static cLIGHT *gLight    = 0;
static float   gAngle    = 0.0f;

// ── 构建光源 VP 矩阵 (用于阴影映射) ──
static void BuildShadowVP(sMAT44 *out, const sVEC3 *center){
    sVEC3 lightDir = sVEC3{0.5f, -1.0f, 0.3f};
    sVEC3 lightPos = sVEC3{center->x - lightDir.x * 10.0f,
                            center->y - lightDir.y * 10.0f,
                            center->z - lightDir.z * 10.0f};
    sVEC3 up = sVEC3{0, 1, 0};
    sMAT44 lightView, lightProj;
    Mat44LookAtRH(&lightView, &lightPos, center, &up);
    Mat44OrthoVK(&lightProj, -6.0f, 6.0f, -6.0f, 6.0f, 0.1f, 30.0f);
    Mat44Mul(out, &lightProj, &lightView);
}

extern "C"{
GN_CALL void AppInit(){
    // ── 方向光 (主光 + 阴影投射) ──
    gLight = new cLIGHT();
    gLight->AddDirectional(sVEC3{0.5f, -1.0f, 0.3f},
                           sVEC3{1.0f, 0.95f, 0.85f}, 1.5f);
    gLight->SetAmbient(sVEC3{0.15f, 0.18f, 0.25f}, 1.0f);
    gLight->SetShadowMapSize(2048.0f);

    // ── 金属球体 (PBR: roughness=0.3, metallic=0.9) ──
    gSphere = CreateSphereModel(1.0f, 32, 0.8f, 0.3f, 0.2f);
    gSphere->SetPosition(0, 1.0f, 0);
    gSphere->UploadAllToGPU();
    // 材质: 低粗糙度金属质感
    if(gSphere->aMeshes){
        sMESH &m = gSphere->aMeshes[0];
        m.aDiffuse[0]  = 0.9f; m.aDiffuse[1] = 0.35f; m.aDiffuse[2] = 0.25f;
        m.aSpecular[0] = 1.0f; m.aSpecular[1] = 1.0f; m.aSpecular[2] = 1.0f;
        m.aShininess   = 64.0f;
        // PBR 参数通过 aMaterial 可选设置:
        // roughness=0.3, metallic=0.9 → 类金属外观
    }

    // 地板 — 暗面也需要 ambient 可见。增大 albedo + 设置位置
    gFloor = CreatePlaneModel(8.0f, 8.0f, 0.7f, 0.7f, 0.75f);
    gFloor->SetPosition(0, 0.0f, 0);
    gFloor->UploadAllToGPU();
    if(gFloor->aMeshes){
        gFloor->aMeshes[0].aDiffuse[0] = 0.7f;
        gFloor->aMeshes[0].aDiffuse[1] = 0.7f;
        gFloor->aMeshes[0].aDiffuse[2] = 0.75f;
    }
}

GN_CALL void AppUnInit(){
    if(gSphere){ gSphere->ReleaseGPU(); delete gSphere; gSphere = 0; }
    if(gFloor) { gFloor->ReleaseGPU();  delete gFloor;  gFloor  = 0; }
    if(gLight) { delete gLight; gLight = 0; }
}

GN_CALL void AppDisplay2D(unsigned int nTime){
    if(!gLight || !gSphere || !gFloor) return;

    // ── 帧率无关旋转 ──
    static unsigned int s_last = 0;
    unsigned int dt = nTime - s_last; s_last = nTime;
    if(dt > 100) dt = 16;
    gAngle += dt * 0.0004f;
    if(gAngle > 6.283f) gAngle -= 6.283f;

    // ── 摄像机: 绕场景旋转 ──
    float cd = 7.0f;
    sVEC3 eye{sinf(gAngle)*cd, 3.5f, cosf(gAngle)*cd};
    sVEC3 center{0, 0.5f, 0};
    sVEC3 up{0, 1, 0};

    sMAT44 view, proj, vp;
    Mat44LookAtRH(&view, &eye, &center, &up);
    Mat44PerspectiveVK(&proj, 0.785f, 800.0f/600.0f, 0.1f, 100.0f);
    Mat44Mul(&vp, &proj, &view);

    // ═══════════════════════ Phase 1: 阴影映射 ═══════════════
    sMAT44 shadowVP;
    BuildShadowVP(&shadowVP, &center);
    gLight->SetShadowLightVP(shadowVP);

    GN_VkShadowPassBegin(&shadowVP);
    gSphere->Draw();
    gFloor->Draw();
    GN_VkShadowPassEnd();

    // ═══════════════════ Phase 2: 延迟渲染 (G-Buffer + Lighting) ═════
    GN_VkBegin3DPassDeferred(&vp, &eye, gLight->GetUBO(), sizeof(sLIGHT_UBO));
    GN_VkDrawSkybox(&vp);
    // 手动调用 G-Buffer 绘制 (不能使用 DrawPBR, 因为管线不同)
    GN_VkDraw3D_GBuffer(gFloor->aMeshes[0].aVertices, gFloor->aMeshes[0].aVertexCount,
                         gFloor->aMeshes[0].aIndices, gFloor->aMeshes[0].aIndexCount,
                         &gFloor->aTransform,
                         gFloor->aMeshes[0].aDiffuse[0], gFloor->aMeshes[0].aDiffuse[1], gFloor->aMeshes[0].aDiffuse[2]);
    GN_VkDraw3D_GBuffer(gSphere->aMeshes[0].aVertices, gSphere->aMeshes[0].aVertexCount,
                         gSphere->aMeshes[0].aIndices, gSphere->aMeshes[0].aIndexCount,
                         &gSphere->aTransform,
                         gSphere->aMeshes[0].aDiffuse[0], gSphere->aMeshes[0].aDiffuse[1], gSphere->aMeshes[0].aDiffuse[2]);
    GN_VkDeferredLighting(&vp, &eye, gLight->GetUBO(), sizeof(sLIGHT_UBO));
}

GN_CALL void AppDisplay3D(unsigned int nTime){}
GN_CALL void AppRun(unsigned int nTime){ (void)nTime; }
}
#else
extern "C"{
GN_CALL void AppInit(){}
GN_CALL void AppUnInit(){}
GN_CALL void AppDisplay2D(unsigned int nTime){}
GN_CALL void AppDisplay3D(unsigned int nTime){}
GN_CALL void AppRun(unsigned int nTime){}
}
#endif
