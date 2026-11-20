"""Compile the real sprite renderer against a recording backend, without Windows.
Run: python3 DevTools/Tests/test_world_space_ui.py
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MOCK = r'''
#pragma once
#include <cassert>
#include <cmath>
using TFLOAT=float;
using TINT=int;
using TUINT32=unsigned;
using TBOOL=bool;
using TIndexType=unsigned short;
using TBYTE=unsigned char;
#define TNULL nullptr
#define TTRUE true
#define TFALSE false
#define TOSHI_NAMESPACE_USING using namespace Toshi;
namespace Toshi {
struct TVector2 { float x=0,y=0; void Set(float a,float b){x=a;y=b;} };
struct TVector3 { float x=0,y=0,z=0; void Set(float a,float b,float c){x=a;y=b;z=c;} };
struct TVector4 { float x=0,y=0,z=0,w=1; };
struct TMatrix44 {
    float tx=0,ty=0,tz=0,yaw=0;
    static const TMatrix44 IDENTITY;
    static void TransformVector(TVector4& out,const TMatrix44& m,const TVector4& v) {
        out={std::cos(m.yaw)*v.x-std::sin(m.yaw)*v.z+m.tx,
             v.y+m.ty, std::sin(m.yaw)*v.x+std::cos(m.yaw)*v.z+m.tz,1};
    }
};
inline const TMatrix44 TMatrix44::IDENTITY{};
struct TMath { static void SinCos(float a,float& s,float& c){s=std::sin(a);c=std::cos(a);} };
struct TMaterial { enum {FLAGS_NO_CULL=1}; };
struct T2GUIMaterial { void* GetTextureResource(){return this;} };
struct T2GUITextureSection {
    T2GUIMaterial mat;
    TVector4 uv1{.1f,.2f,.7f,.2f},uv2{.7f,.8f,.1f,.8f};
    auto GetMaterial(){return &mat;}
    auto& GetUVPoint1(){return uv1;} auto& GetUVPoint2(){return uv2;}
};
struct Context {
    TMatrix44 view,model;
    struct Proj {float m_fNearClip=.06f;} proj;
    auto& GetWorldViewMatrix(){return view;}
    auto GetModelViewMatrix(){return model;}
    auto GetProjectionParams(){return proj;}
    void SetModelViewMatrix(const TMatrix44& m){model=m;}
};
struct TRenderInterfaceHAL {
    Context ctx;
    static auto Interface(){static TRenderInterfaceHAL r;return &r;}
    auto GetCurrentContext(){return &ctx;}
    void FlushShaders(){}
};
}
struct AVRManager {
    unsigned eye=~0u;
    static auto& GetSingleton(){static AVRManager r;return r;}
    auto GetCurrentEye(){return eye;} unsigned GetViewCount(){return 2;}
};
struct ASysMaterial {
    enum {BLENDMODE_WORLD_UI=11};
    static inline int alive=0;
    ASysMaterial(){++alive;} ~ASysMaterial(){--alive;}
    bool Create(int mode){assert(mode==BLENDMODE_WORLD_UI);return true;}
    void SetTextureNum(int n){assert(n==1);}
    void SetFlags(int,bool){} void SetTexture(void*){}
    void Destroy(){delete this;}
};
struct ASysMesh {
    enum {FLAGS_VERTICES_DYNAMIC_APPEND=4,FLAGS_INDICES_DYNAMIC_APPEND=32};
    struct Vertex {Toshi::TVector3 vPosition;unsigned uiColor;Toshi::TVector2 vUV;};
    struct LockBuffer {
        // Match TVertexPoolResourceInterface: streams are byte pointers, not void*.
        struct {TBYTE* apStreams[1];} VertexLock;
        struct {TIndexType* pBuffer;} IndexLock;
    };
    static inline int alive=0,draws=0;
    static inline Vertex last[24];
    Vertex vertices[24]; TIndexType indices[36];
    ASysMesh(){++alive;} ~ASysMesh(){--alive;}
    bool CreatePools(unsigned flags,int v,int i){assert(flags==36 && v==24 && i==36);return true;}
    void SetMaterial(ASysMaterial*){}
    bool Lock(LockBuffer& l){l.VertexLock.apStreams[0]=reinterpret_cast<TBYTE*>(vertices);l.IndexLock.pBuffer=indices;return true;}
    void Unlock(int v,int i){assert(v==4 && i==6);for(int k=0;k<i;++k)assert(indices[k]<v);}
    void Validate(){}
    void Render(){
        auto m=Toshi::TRenderInterfaceHAL::Interface()->ctx.model;
        assert(m.tx==0 && m.yaw==0);
        for(int i=0;i<4;++i) { last[i]=vertices[i]; }
        ++draws;
    }
    void DestroyResource(){delete this;}
};
struct ASysShader {
    static auto GetSingleton(){static ASysShader s;return &s;}
    auto CreateMaterial(void*){return new ASysMaterial;}
    auto CreateMesh(void*){return new ASysMesh;}
};
'''
MAIN = r'''
#include "Render/AWorldSpaceUI.h"
bool close(float a,float b){return std::abs(a-b)<.0001f;}
int main(){
    using namespace Toshi;
    auto& vr=AVRManager::GetSingleton();
    auto& ctx=TRenderInterfaceHAL::Interface()->ctx;
    T2GUITextureSection texture;
    AWorldSpaceUI::Sprite sprite{0,0,20,10,0,0xaabbccdd};
    {
        AWorldSpaceUI ui;
        ui.Render({0,0,5,1},&sprite,1,&texture,.02f);
        assert(ASysMesh::alive==0 && ASysMesh::draws==0); // UI/desktop pass
        float centres[2];
        for(unsigned eye=0;eye<2;++eye){
            vr.eye=eye;ctx.view.tx=eye==0?.032f:-.032f;
            ctx.model.tx=123;
            ui.Render({0,0,5,1},&sprite,1,&texture,.02f);
            auto* v=ASysMesh::last;
            centres[eye]=(v[0].vPosition.x+v[3].vPosition.x)*.5f;
            assert(close(centres[eye],ctx.view.tx));
            assert(close(v[3].vPosition.x-v[0].vPosition.x,.4f));
            assert(close(v[3].vPosition.y-v[0].vPosition.y,.2f));
            assert(close(v[0].vUV.x,.1f) && close(v[3].vUV.y,.8f));
            assert(v[0].uiColor==sprite.colour && ctx.model.tx==123);
        }
        assert(centres[0]>centres[1]); // stereo disparity at real target depth
        assert(ASysMesh::alive==1 && ASysMaterial::alive==1); // reused between eyes
        const int draws=ASysMesh::draws;
        ui.Render({0,0,-1,1},&sprite,1,&texture,.02f);
        ui.Render({0,0,.01f,1},&sprite,1,&texture,.02f);
        assert(ASysMesh::draws==draws); // behind head / near plane
        // Head yaw and translation move the anchor exactly like a scene point.
        ctx.view={.3f,-.2f,0,.5f};
        TVector4 world{2,1,8,1},view;
        TMatrix44::TransformVector(view,ctx.view,world);
        ui.Render(world,&sprite,1,&texture,.02f);
        auto* v=ASysMesh::last;
        assert(close((v[0].vPosition.x+v[3].vPosition.x)*.5f,view.x));
        assert(close((v[0].vPosition.y+v[3].vPosition.y)*.5f,view.y));
        assert(close(v[0].vPosition.z,view.z));
        // Distance changes depth, not world dimensions.
        ui.Render({0,0,20,1},&sprite,1,&texture,.02f);
        assert(close(ASysMesh::last[3].vPosition.x-ASysMesh::last[0].vPosition.x,.4f));
    }
    assert(ASysMesh::alive==0 && ASysMaterial::alive==0);
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / "mock.h").write_text(MOCK)
    for name in ["pch.h", "Math/TVector4.h", "GUI/T2GUITextureSection.h",
                 "Render/ASysShader/ASysShader.h", "Render/ASysShader/ASysMesh.h",
                 "Render/ASysShader/ASysMaterial.h", "VR/AVRManager.h",
                 "Render/TBackend.h", "Core/TMemoryDebugOn.h"]:
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "mock.h"\n')
    (tmp / "main.cpp").write_text(MAIN)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-DTRENDERINTERFACE_DX11", "-I" + str(tmp),
                    "-I" + str(ROOT / "OpenBarnyard/Source"),
                    str(ROOT / "OpenBarnyard/Source/Render/AWorldSpaceUI.cpp"),
                    str(tmp / "main.cpp"), "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], check=True)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-fsyntax-only", "-I" + str(tmp),
                    "-I" + str(ROOT / "OpenBarnyard/Source"),
                    str(ROOT / "OpenBarnyard/Source/Render/AWorldSpaceUI.cpp")], check=True)
print("PASS: stereo anchors, head motion, near rejection, atlas UVs, fixed world size, resource reuse/cleanup")
