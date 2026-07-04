// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 模型系统 实现                              ║
// ║  OBJ 加载器 + 几何体生成器                                ║
// ╚══════════════════════════════════════════════════════════╝
#include<GN.h>
#include"GN_3Dmodel.h"
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<cmath>
#include<vector>
#include<string>

#ifdef GN_USING_3D
#include"_Vulkan/GN_Vk.h"
#endif

namespace GN3d {

using namespace GNg;

// ═══════════════════════════════════════════════════════════
//  sMESH 实现
// ═══════════════════════════════════════════════════════════
sMESH::sMESH() : aVertices(0), aIndices(0), aVertexCount(0), aIndexCount(0)
#ifdef GN_USING_3D
    , aVertexBuffer(VK_NULL_HANDLE), aVertexMemory(VK_NULL_HANDLE)
    , aIndexBuffer(VK_NULL_HANDLE), aIndexMemory(VK_NULL_HANDLE)
#endif
{
    aName[0]=0;
    aDiffuse[0]=0.8f; aDiffuse[1]=0.8f; aDiffuse[2]=0.8f;
    aSpecular[0]=1.0f; aSpecular[1]=1.0f; aSpecular[2]=1.0f;
    aShininess=32.0f;
    aMaterial=0;
    aAlbedoTex=nullptr;
    aNormalTex=nullptr;
}

sMESH::~sMESH(){ Release(); ReleaseGPU(); }

void sMESH::Release(){
    if(aVertices){ delete[] aVertices; aVertices=0; }
    if(aIndices) { delete[] aIndices;  aIndices=0;  }
    aVertexCount=0; aIndexCount=0;
}

void sMESH::ReleaseGPU(){
#ifdef GN_USING_3D
    GNg::GN_VkRelease3DMesh(aVertexBuffer,aVertexMemory,aIndexBuffer,aIndexMemory);
    aVertexBuffer=VK_NULL_HANDLE; aVertexMemory=VK_NULL_HANDLE;
    aIndexBuffer=VK_NULL_HANDLE;  aIndexMemory=VK_NULL_HANDLE;
#endif
}

void sMESH::UploadToGPU(){
#ifdef GN_USING_3D
    GNg::GN_VkUpload3DMesh(&aVertexBuffer,&aVertexMemory,
                           &aIndexBuffer,&aIndexMemory,
                           aVertices, aVertexCount, sizeof(VkVertex3D),
                           aIndices, aIndexCount);
#endif
}

void sMESH::Draw(const sMAT44 *modelMatrix) const {
#ifdef GN_USING_3D
    float sr=1.0f, sg=1.0f, sb=1.0f, sh=32.0f;
    const GNg::sIMAGE *tex = aAlbedoTex;  // 优先 mesh 纹理
    if(aMaterial){
        sr=aMaterial->aSpecular.x; sg=aMaterial->aSpecular.y; sb=aMaterial->aSpecular.z;
        sh=aMaterial->aShininess;
        if(!tex) tex = aMaterial->aAlbedoTex;  // 从材质获取纹理
    } else {
        sr=aSpecular[0]; sg=aSpecular[1]; sb=aSpecular[2]; sh=aShininess;
    }
    GNg::GN_VkDraw3D(aVertices, aVertexCount,
                     aIndices, aIndexCount,
                     modelMatrix,
                     aDiffuse[0], aDiffuse[1], aDiffuse[2],
                     sr, sg, sb, sh, tex);
#else
    (void)modelMatrix;
#endif
}

void sMESH::DrawPBR(const sMAT44 *modelMatrix) const {
#ifdef GN_USING_3D
    float dr=0.8f, dg=0.8f, db=0.8f, rough=0.5f, metal=0.0f, ao=1.0f;
    const GNg::sIMAGE *albTex = aAlbedoTex;
    const GNg::sIMAGE *nrmTex = aNormalTex;
    if(aMaterial){
        dr=aMaterial->aDiffuse.x; dg=aMaterial->aDiffuse.y; db=aMaterial->aDiffuse.z;
        rough=aMaterial->aRoughness;
        metal=aMaterial->aMetallic;
        if(!albTex) albTex = aMaterial->aAlbedoTex;
        if(!nrmTex) nrmTex = aMaterial->aNormalTex;
    } else {
        dr=aDiffuse[0]; dg=aDiffuse[1]; db=aDiffuse[2];
    }
    GNg::GN_VkDraw3DPBR(aVertices, aVertexCount,
                        aIndices, aIndexCount,
                        modelMatrix,
                        dr, dg, db,
                        rough, metal, ao,
                        albTex, nrmTex);
#else
    (void)modelMatrix;
#endif
}

// ═══════════════════════════════════════════════════════════
//  cMODEL 实现
// ═══════════════════════════════════════════════════════════
cMODEL::cMODEL() : aMeshes(0), aMeshCount(0), aVisible(true){
    aName[0]=0;
    Mat44Identity(&aTransform);
    aPosition.x=aPosition.y=aPosition.z=0.0f;
    QuatIdentity(&aRotation);
    aScale.x=aScale.y=aScale.z=1.0f;
}

cMODEL::~cMODEL(){ Release(); ReleaseGPU(); }

void cMODEL::Release(){
    if(aMeshes){
        for(uint32_t i=0;i<aMeshCount;++i) aMeshes[i].Release();
        delete[] aMeshes; aMeshes=0;
    }
    aMeshCount=0;
}

void cMODEL::ReleaseGPU(){
    if(aMeshes){
        for(uint32_t i=0;i<aMeshCount;++i) aMeshes[i].ReleaseGPU();
    }
}

void cMODEL::UploadAllToGPU(){
    if(aMeshes){
        for(uint32_t i=0;i<aMeshCount;++i) aMeshes[i].UploadToGPU();
    }
}

void cMODEL::UpdateTransform(){
    Mat44ComposeTRS(&aTransform, &aPosition, &aRotation, &aScale);
}

void cMODEL::Draw() const {
    if(!aVisible || !aMeshes) return;
    for(uint32_t i=0;i<aMeshCount;++i){
        aMeshes[i].Draw(&aTransform);
    }
}

void cMODEL::DrawPBR() const {
    if(!aVisible || !aMeshes) return;
    for(uint32_t i=0;i<aMeshCount;++i){
        aMeshes[i].DrawPBR(&aTransform);
    }
}

// ═══════════════════════════════════════════════════════════
//  OBJ 加载器
// ═══════════════════════════════════════════════════════════

struct ObjIndex {
    int v, vt, vn;
};

struct ObjFace {
    ObjIndex a, b, c;
};

bool cMODEL::LoadOBJ(const char *filename){
    FILE *fp = fopen(filename, "rb");
    if(!fp) return false;

    const char *slash = strrchr(filename, '/');
    const char *bslash = strrchr(filename, '\\');
    const char *fn = slash ? slash+1 : (bslash ? bslash+1 : filename);
    strncpy(aName, fn, sizeof(aName)-1);
    aName[sizeof(aName)-1]=0;

    std::vector<float>    positions;
    std::vector<float>    normals;
    std::vector<float>    texcoords;
    std::vector<ObjFace>  faces;
    std::vector<uint32_t>   faceGroups;
    std::vector<std::string> groupNames;
    uint32_t currentGroup = 0;
    groupNames.push_back("default");

    char line[512];
    while(fgets(line, sizeof(line), fp)){
        if(line[0]=='\n' || line[0]=='\r' || line[0]=='#') continue;

        if(strncmp(line, "v ", 2)==0){
            float x,y,z;
            if(sscanf(line+2, "%f %f %f", &x, &y, &z)==3){
                positions.push_back(x); positions.push_back(y); positions.push_back(z);
            }
        }
        else if(strncmp(line, "vn ", 3)==0){
            float x,y,z;
            if(sscanf(line+3, "%f %f %f", &x, &y, &z)==3){
                normals.push_back(x); normals.push_back(y); normals.push_back(z);
            }
        }
        else if(strncmp(line, "vt ", 3)==0){
            float u,v=0;
            if(sscanf(line+3, "%f %f", &u, &v)>=1){
                texcoords.push_back(u); texcoords.push_back(v<0?-v:v);
            }
        }
        else if(strncmp(line, "f ", 2)==0){
            ObjFace f;
            int n=0;
            char *tok = line+2;
            while(*tok && n<3){
                while(*tok==' ') ++tok;
                int v=0, vt=0, vn=0;
                if(sscanf(tok, "%d/%d/%d", &v, &vt, &vn)>=1){
                    if(n==0){ f.a.v=v; f.a.vt=vt; f.a.vn=vn; }
                    else if(n==1){ f.b.v=v; f.b.vt=vt; f.b.vn=vn; }
                    else { f.c.v=v; f.c.vt=vt; f.c.vn=vn; }
                    ++n;
                }
                while(*tok && *tok!=' ') ++tok;
            }
            if(n>=3){
                faces.push_back(f);
                faceGroups.push_back(currentGroup);
            }
        }
        else if(strncmp(line, "usemtl ", 7)==0 || strncmp(line, "g ", 2)==0){
            const char *name = (line[0]=='u') ? line+7 : line+2;
            char gname[128];
            strncpy(gname, name, sizeof(gname)-1);
            gname[sizeof(gname)-1]=0;
            char *nl = strchr(gname, '\n'); if(nl) *nl=0;
            nl = strchr(gname, '\r'); if(nl) *nl=0;
            char *s = gname; while(*s==' ') ++s;

            bool found=false;
            for(uint32_t gi=0; gi<(uint32_t)groupNames.size(); ++gi){
                if(groupNames[gi] == s){
                    currentGroup=gi; found=true; break;
                }
            }
            if(!found){
                currentGroup = (uint32_t)groupNames.size();
                groupNames.push_back(s);
            }
        }
    }
    fclose(fp);

    if(faces.empty() || positions.empty()){ return false; }

    // 自动计算法线
    if(normals.empty()){
        normals.resize(positions.size(), 0.0f);
        for(size_t fi=0; fi<faces.size(); ++fi){
            int i0=(faces[fi].a.v-1)*3, i1=(faces[fi].b.v-1)*3, i2=(faces[fi].c.v-1)*3;
            if(i0<0||i1<0||i2<0||(size_t)i2+2>=positions.size()) continue;
            float ax=positions[i1+0]-positions[i0+0];
            float ay=positions[i1+1]-positions[i0+1];
            float az=positions[i1+2]-positions[i0+2];
            float bx=positions[i2+0]-positions[i0+0];
            float by=positions[i2+1]-positions[i0+1];
            float bz=positions[i2+2]-positions[i0+2];
            float nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
            for(int j=0;j<3;j++){
                int idx = (j==0?faces[fi].a.v:(j==1?faces[fi].b.v:faces[fi].c.v))-1;
                if((size_t)(idx*3+2) < normals.size()){
                    normals[idx*3+0]+=nx; normals[idx*3+1]+=ny; normals[idx*3+2]+=nz;
                }
            }
        }
        for(size_t i=0; i+2<normals.size(); i+=3){
            float len=sqrtf(normals[i]*normals[i]+normals[i+1]*normals[i+1]+normals[i+2]*normals[i+2]);
            if(len>0.00001f){
                normals[i]/=len; normals[i+1]/=len; normals[i+2]/=len;
            }
        }
    }

    if(texcoords.empty()){
        texcoords.push_back(0.0f); texcoords.push_back(0.0f);
    }

    uint32_t groupCount = (uint32_t)groupNames.size();
    aMeshCount = groupCount;
    aMeshes = new sMESH[groupCount];

    for(uint32_t gi=0; gi<groupCount; ++gi){
        strncpy(aMeshes[gi].aName, groupNames[gi].c_str(), sizeof(aMeshes[gi].aName)-1);

        std::vector<ObjFace> gf;
        for(size_t fi=0; fi<faces.size(); ++fi){
            if(faceGroups[fi]==gi) gf.push_back(faces[fi]);
        }
        if(gf.empty()){
            aMeshes[gi].aVertices=0; aMeshes[gi].aIndices=0;
            aMeshes[gi].aVertexCount=0; aMeshes[gi].aIndexCount=0;
            continue;
        }

        uint32_t triCount = (uint32_t)gf.size();
        aMeshes[gi].aVertexCount = triCount * 3;
        aMeshes[gi].aIndexCount = triCount * 3;
        aMeshes[gi].aVertices = new float[aMeshes[gi].aVertexCount * 11];
        aMeshes[gi].aIndices  = new uint32_t[aMeshes[gi].aIndexCount];

        float *vtx = aMeshes[gi].aVertices;
        for(uint32_t ti=0; ti<triCount; ++ti){
            ObjIndex idxs[3]={gf[ti].a, gf[ti].b, gf[ti].c};
            for(int v=0; v<3; ++v){
                int pi = idxs[v].v - 1;
                int ni = idxs[v].vn - 1;
                int ti_idx = idxs[v].vt - 1;

                if(pi>=0 && (size_t)(pi*3+2)<positions.size()){
                    vtx[0]=positions[pi*3+0]; vtx[1]=positions[pi*3+1]; vtx[2]=positions[pi*3+2];
                } else { vtx[0]=vtx[1]=vtx[2]=0.0f; }

                if(ni>=0 && (size_t)(ni*3+2)<normals.size()){
                    vtx[3]=normals[ni*3+0]; vtx[4]=normals[ni*3+1]; vtx[5]=normals[ni*3+2];
                } else if(pi>=0 && (size_t)(pi*3+2)<normals.size()){
                    vtx[3]=normals[pi*3+0]; vtx[4]=normals[pi*3+1]; vtx[5]=normals[pi*3+2];
                } else { vtx[3]=vtx[4]=0.0f; vtx[5]=1.0f; }

                vtx[6]=1.0f; vtx[7]=0.0f; vtx[8]=0.0f;

                if(ti_idx>=0 && (size_t)(ti_idx*2+1)<texcoords.size()){
                    vtx[9]=texcoords[ti_idx*2+0]; vtx[10]=texcoords[ti_idx*2+1];
                } else { vtx[9]=0.0f; vtx[10]=0.0f; }

                vtx += 11;
                aMeshes[gi].aIndices[ti*3+v] = ti*3+v;
            }
        }
    }

    UpdateTransform();
    return true;
}

// ═══════════════════════════════════════════════════════════
//  几何体生成器
// ═══════════════════════════════════════════════════════════
cMODEL *CreateCubeModel(float size, float r, float g, float b){
    cMODEL *m = new cMODEL();
    strcpy(m->aName, "Cube");
    float h = size * 0.5f;

    m->aMeshCount = 1;
    m->aMeshes = new sMESH[1];
    m->aMeshes[0].aVertexCount = 36;
    m->aMeshes[0].aIndexCount  = 36;
    m->aMeshes[0].aDiffuse[0]=r; m->aMeshes[0].aDiffuse[1]=g; m->aMeshes[0].aDiffuse[2]=b;

    float *v = new float[36 * 11];
    uint32_t *idx = new uint32_t[36];
    for(int i=0;i<36;i++) idx[i]=i;

    auto setFace = [&](int fi, float nx,float ny,float nz,
                       float x0,float y0,float z0, float x1,float y1,float z1,
                       float x2,float y2,float z2, float x3,float y3,float z3){
        int base = fi*44; // 4 vertices * 11 floats
        auto sv = [&](int vi, float px,float py,float pz, float u,float uv){
            int b = base + vi*11;
            v[b+0]=px; v[b+1]=py; v[b+2]=pz;
            v[b+3]=nx; v[b+4]=ny; v[b+5]=nz;
            v[b+6]=1;  v[b+7]=0;  v[b+8]=0;
            v[b+9]=u;  v[b+10]=uv;
        };
        sv(0, x0,y0,z0, 0,0); sv(1, x1,y1,z1, 1,0);
        sv(2, x2,y2,z2, 1,1); sv(3, x3,y3,z3, 0,1);
        int ib = fi*6, vb = fi*4;
        idx[ib+0]=vb+0; idx[ib+1]=vb+1; idx[ib+2]=vb+2;
        idx[ib+3]=vb+0; idx[ib+4]=vb+2; idx[ib+5]=vb+3;
    };

    setFace(0, 1,0,0,  h,-h,-h,  h, h,-h,  h, h, h,  h,-h, h);
    setFace(1,-1,0,0, -h,-h, h, -h, h, h, -h, h,-h, -h,-h,-h);
    setFace(2, 0,1,0, -h, h,-h, -h, h, h,  h, h, h,  h, h,-h);
    setFace(3, 0,-1,0, -h,-h, h, -h,-h,-h,  h,-h,-h,  h,-h, h);
    setFace(4, 0,0,1, -h,-h, h,  h,-h, h,  h, h, h, -h, h, h);
    setFace(5, 0,0,-1, h,-h,-h, -h,-h,-h, -h, h,-h,  h, h,-h);

    m->aMeshes[0].aVertices = v;
    m->aMeshes[0].aIndices  = idx;
    strcpy(m->aMeshes[0].aName, "cube");
    m->UpdateTransform();
    return m;
}

cMODEL *CreateSphereModel(float radius, int segments, float r, float g, float b){
    cMODEL *m = new cMODEL();
    strcpy(m->aName, "Sphere");
    m->aMeshCount = 1;
    m->aMeshes = new sMESH[1];
    m->aMeshes[0].aDiffuse[0]=r; m->aMeshes[0].aDiffuse[1]=g; m->aMeshes[0].aDiffuse[2]=b;

    int stacks = segments, slices = segments * 2;
    int vc = (stacks+1)*(slices+1), ic = stacks*slices*6;

    float *v = new float[vc*11];
    uint32_t *idx = new uint32_t[ic];

    for(int i=0; i<=stacks; i++){
        float phi = 3.14159265f * (float)i / (float)stacks;
        float y = -cosf(phi) * radius;
        float ringR = sinf(phi) * radius;
        for(int j=0; j<=slices; j++){
            float theta = 2.0f*3.14159265f * (float)j/(float)slices;
            float x = cosf(theta)*ringR, z = sinf(theta)*ringR;
            int vi = (i*(slices+1)+j)*11;
            v[vi+0]=x; v[vi+1]=y; v[vi+2]=z;
            float len=sqrtf(x*x+y*y+z*z);
            if(len>0.0001f){ v[vi+3]=x/len; v[vi+4]=y/len; v[vi+5]=z/len; }
            else { v[vi+3]=0; v[vi+4]=1; v[vi+5]=0; }
            v[vi+6]=1; v[vi+7]=0; v[vi+8]=0;
            v[vi+9]=(float)j/(float)slices; v[vi+10]=(float)i/(float)stacks;
        }
    }

    int ii=0;
    for(int i=0; i<stacks; i++){
        for(int j=0; j<slices; j++){
            int a=i*(slices+1)+j, b=a+slices+1;
            idx[ii++]=a; idx[ii++]=b; idx[ii++]=a+1;
            idx[ii++]=a+1; idx[ii++]=b; idx[ii++]=b+1;
        }
    }

    m->aMeshes[0].aVertices=v; m->aMeshes[0].aIndices=idx;
    m->aMeshes[0].aVertexCount=vc; m->aMeshes[0].aIndexCount=ic;
    strcpy(m->aMeshes[0].aName, "sphere");
    m->UpdateTransform();
    return m;
}

cMODEL *CreatePlaneModel(float width, float depth, float r, float g, float b){
    cMODEL *m = new cMODEL();
    strcpy(m->aName, "Plane");
    m->aMeshCount = 1;
    m->aMeshes = new sMESH[1];
    m->aMeshes[0].aDiffuse[0]=r; m->aMeshes[0].aDiffuse[1]=g; m->aMeshes[0].aDiffuse[2]=b;

    float hw=width*0.5f, hd=depth*0.5f;
    float *v = new float[44]; // 4 verts * 11
    uint32_t *idx = new uint32_t[6];

    auto sv = [&](int vi, float x,float y,float z, float u,float uv){
        v[vi+0]=x; v[vi+1]=y; v[vi+2]=z;
        v[vi+3]=0; v[vi+4]=1; v[vi+5]=0;
        v[vi+6]=1; v[vi+7]=0; v[vi+8]=0;
        v[vi+9]=u; v[vi+10]=uv;
    };
    sv(0, -hw,0,-hd, 0,0); sv(11, hw,0,-hd, 1,0);
    sv(22, hw,0, hd, 1,1); sv(33, -hw,0, hd, 0,1);
    idx[0]=0; idx[1]=1; idx[2]=2; idx[3]=0; idx[4]=2; idx[5]=3;

    m->aMeshes[0].aVertices=v; m->aMeshes[0].aIndices=idx;
    m->aMeshes[0].aVertexCount=4; m->aMeshes[0].aIndexCount=6;
    strcpy(m->aMeshes[0].aName, "plane");
    m->UpdateTransform();
    return m;
}

} // namespace GN3d
