"""Compile real glass UV/fill updates; check atlas filtering and desktop recovery."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "OpenBarnyard/Source/HUD/AMilkFillHudElement.cpp").read_text()
helper = source[source.index("static void UpdateGlassTextureUVs("):source.index("// $Barnyard: FUNCTION 00456450")]
method = source[source.index("void AMilkFillHudElement::SetFill("):]
for rectangle, section in [("m_oMilk", "Glass_Milk"), ("m_oMask", "Glass_Mask"),
                           ("m_oBackPlate", "Glass_Back_Plate"), ("m_oGlint", "Glass_Glint")]:
    assert f'UpdateGlassTextureUVs( {rectangle}, "{section}" );' in source
mock = r'''
#include <cassert>
#include <algorithm>
#include <cmath>
using TFLOAT=float;using TCHAR=char;
struct TVector2 {float x,y;};
struct UV {float x,y,z,w;};
struct TMath {static float Max(float a,float b){return std::max(a,b);}};
struct AWorldSpaceGUI {static inline bool active=false;static bool IsActive(){return active;}};
struct Section {
    float width=64,height=100;
    UV first{.25f,.125f,0,0},last{.5f,.515625f,0,0}; // a region of a 256x256 atlas
    auto& GetUVPoint1(){return first;} auto& GetUVPoint2(){return last;}
    float GetWidth(){return width;} float GetHeight(){return height;}
};
struct T2GUITextureSectionManager {
    static inline Section section;
    static Section* GetTextureSection(const char*){return &section;}
};
struct T2GUIRectangle {
    float width=64,height=100;
    TVector2 uv[4]{};
    float GetWidth(){return width;} float GetHeight(){return height;}
    void SetWidth(float v){width=v;} void SetHeight(float v){height=v;}
    void SetUVs(TVector2 a,TVector2 b,TVector2 c,TVector2 d){uv[0]=a;uv[1]=b;uv[2]=c;uv[3]=d;}
};
struct AMilkFillHudElement {
    T2GUIRectangle m_oMilk,m_oMask,m_oBackPlate;
    void SetFill(float,float);
};
'''
main = r'''
bool close(float a,float b){return std::abs(a-b)<.000001f;}
// In D3D linear filtering, UV*size-0.5 selects the two texels and blend weight.
// Model a bright atlas neighbour immediately to the right of Glass_Milk.
float neighbourContribution(float u){
    float texel=u*256-.5f;int left=int(std::floor(texel));float weight=texel-left;
    return (left>=128?1.f:0.f)*(1-weight)+(left+1>=128?1.f:0.f)*weight;
}
int main(){
    AMilkFillHudElement meter;
    for(bool vr : {false,true,true,false}){
        AWorldSpaceGUI::active=vr;
        for(float fill : {0.f,.5f,1.f}){
            meter.SetFill(fill,1);
            UpdateGlassTextureUVs(meter.m_oMilk,"Glass_Milk");
            assert(meter.m_oMilk.width==64); // no geometry shrink/gap
            assert(std::abs(meter.m_oMilk.height-90.f*fill)<.0001f);
            auto& rect=meter.m_oMilk;
            assert(close(rect.uv[0].x,.25f+(vr?.5f/256:0)));
            assert(close(rect.uv[2].x,.5f-(vr?.5f/256:0)));
            assert(close(rect.uv[0].y,.125f+(vr?.5f/256:0)));
            assert(close(rect.uv[2].y,.515625f-(vr?.5f/256:0)));
            if(vr) assert(neighbourContribution(rect.uv[2].x)==0);
            else assert(neighbourContribution(rect.uv[2].x)==.5f); // reproduces old sampling
        }
    }
    // Mirrored sections retain their orientation and still inset inward.
    auto& section=T2GUITextureSectionManager::section;
    std::swap(section.first,section.last);AWorldSpaceGUI::active=true;
    UpdateGlassTextureUVs(meter.m_oMilk,"Glass_Milk");
    assert(close(meter.m_oMilk.uv[0].x,.5f-.5f/256));
    assert(close(meter.m_oMilk.uv[2].x,.25f+.5f/256));
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(mock + helper + method + main)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    str(path / "test.cpp"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: glass texel-centre UVs prevent atlas-neighbour filtering; no cumulative inset; desktop recovery; original fill geometry")
