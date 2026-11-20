"""Numerical regressions for world GUI mask depth and Darts aim alignment.
Compiles the real clip-transform helper and the actual Darts frame-building code.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
darts = (ROOT / "OpenBarnyard/Source/HUD/ADartFireHudElement.cpp").read_text()
start = darts.index("constexpr TFLOAT size = 256.0f;")
end = darts.index("AWorldSpaceGUI::SetFrame( m_oCanvas, frame );", start)
frame_body = darts[start:end]
mock = r'''
#pragma once
#include <cmath>
#include <cassert>
#include <initializer_list>
using TFLOAT=float;
namespace Toshi {
enum BASISVECTOR{BASISVECTOR_RIGHT,BASISVECTOR_UP,BASISVECTOR_FORWARD};
struct TVector4{float x=0,y=0,z=0,w=1;};
struct TVector3{
    float x=0,y=0,z=0;
    void CrossProduct(const TVector3& a,const TVector3& b){x=a.y*b.z-a.z*b.y;y=a.z*b.x-a.x*b.z;z=a.x*b.y-a.y*b.x;}
    float MagnitudeSq(){return x*x+y*y+z*z;}
    void Normalize(){float n=std::sqrt(MagnitudeSq());x/=n;y/=n;z/=n;}
};
struct TMatrix44{
    float m_f11=1,m_f12=0,m_f13=0,m_f14=0;
    float m_f21=0,m_f22=1,m_f23=0,m_f24=0;
    float m_f31=0,m_f32=0,m_f33=1,m_f34=0;
    float m_f41=0,m_f42=0,m_f43=0,m_f44=1;
    static const TMatrix44 IDENTITY;
    void SetBasisVector4(BASISVECTOR i,const TVector4& v){
        float* p=&m_f11+4*i;p[0]=v.x;p[1]=v.y;p[2]=v.z;p[3]=v.w;
    }
    void SetTranslation(const TVector4& v){m_f41=v.x;m_f42=v.y;m_f43=v.z;m_f44=v.w;}
    void Multiply(const TMatrix44& a,const TMatrix44& b){
        const float* l=&a.m_f11;const float* r=&b.m_f11;float* o=&m_f11;
        for(int i=0;i<4;++i)for(int k=0;k<4;++k){o[4*i+k]=0;for(int j=0;j<4;++j)o[4*i+k]+=l[4*j+k]*r[4*i+j];}
    }
};
inline const TMatrix44 TMatrix44::IDENTITY{};
}
'''
main = r'''
#include "Render/AWorldSpaceGUITransform.h"
using namespace Toshi;
TVector4 transform(const TMatrix44& m,const TVector4& p){
    return {m.m_f11*p.x+m.m_f21*p.y+m.m_f31*p.z+m.m_f41*p.w,
            m.m_f12*p.x+m.m_f22*p.y+m.m_f32*p.z+m.m_f42*p.w,
            m.m_f13*p.x+m.m_f23*p.y+m.m_f33*p.z+m.m_f43*p.w,
            m.m_f14*p.x+m.m_f24*p.y+m.m_f34*p.z+m.m_f44*p.w};
}
bool close(float a,float b){return std::abs(a-b)<.0001f;}
TMatrix44 dartFrame(const TVector4& a_rcTop,const TVector4& a_rcRight,const TVector4& a_rcLeft,const TVector4& a_rcBottom){
FRAME_BODY
return frame;
}
int main(){
    TMatrix44 projection;
    projection.m_f11=1.2f;projection.m_f22=1.3f;
    projection.m_f31=.1f;projection.m_f32=-.07f;
    projection.m_f33=1.0001f;projection.m_f34=1;
    projection.m_f43=-.1f;projection.m_f44=0;
    for(float distance : {.2f,1.f,10.f,100.f,1000.f}){
        TMatrix44 frame,local;
        frame.m_f11=.02f;frame.m_f22=.02f;frame.m_f23=.003f;
        frame.m_f41=.032f;frame.m_f42=.5f;frame.m_f43=distance;
        local.m_f41=7;local.m_f42=-3;
        auto clip=BuildWorldGUIClipTransform(frame,local,projection);
        auto mask=transform(clip,{2,3,.05f,1});
        auto milk=transform(clip,{2,3,.1f,1});
        assert(mask.x==milk.x && mask.y==milk.y && mask.w==milk.w);
        TMatrix44 view,reference;view.Multiply(frame,local);reference.Multiply(projection,view);
        auto real=transform(reference,{2,3,0,1});
        assert(close(real.x,mask.x)&&close(real.y,mask.y)&&close(real.w,mask.w));
        // Check the actual 24-bit quantization that previously collapsed tiny
        // world-space mask offsets. The fill must fail inside the opaque mask
        // and pass outside it (cleared depth=1), even far from the viewer.
        auto quantize=[](float depth){return std::lround(depth*16777215.f);};
        auto maskZ=quantize(mask.z/mask.w),milkZ=quantize(milk.z/milk.w);
        assert(maskZ<milkZ && milkZ<quantize(1));
    }
    // Exact board plane/scale from ADartBoard::GetCorners: normal=-X,
    // side=+Z, half-size=5. Callers pass -aimX/-aimY to the GUI.
    auto board=dartFrame({0,-5,0,1},{0,0,5,1},{0,0,-5,1},{0,5,0,1});
    for(float x : {-1.f,-.3f,0.f,.7f,1.f})for(float y : {-1.f,0.f,1.f}){
        auto intersection=transform(board,{-x*128,-y*128,0,1});
        assert(close(intersection.x,0)); // not offset towards/away from camera
        assert(close(intersection.y,-y*5) && close(intersection.z,x*5));
    }
}
'''.replace("FRAME_BODY", frame_body.replace("return;", "return TMatrix44::IDENTITY;"))
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "Math").mkdir()
    (path / "Math/TMatrix44.h").write_text(mock)
    (path / "test.cpp").write_text(main)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", f"-I{path}",
                    f"-I{ROOT / 'OpenBarnyard/Source'}", str(path / "test.cpp"),
                    "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: identical mask/fill XYW, stable 24-bit depth at 0.2–1000 units, Darts intersection matches throw plane/aim")
