"""Exercise the actual reticle opt-in/lifecycle code with the recording backend."""
from pathlib import Path
import runpy
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
base = runpy.run_path(str(Path(__file__).with_name("test_world_space_ui.py")))
mock = base["MOCK"].replace(
    "struct TVector4 { float x=0,y=0,z=0,w=1; };",
    "struct TVector4 { float x=0,y=0,z=0,w=1; static const TVector4 VEC_ZERO; };\n"
    "inline const TVector4 TVector4::VEC_ZERO{};")
mock = mock.replace("struct TMath {", """struct TMath {
    static constexpr float PI=3.14159265359f, TWO_PI=2*PI, HALF_PI=PI/2;
    template<class T> static void Clip(T& v,T lo,T hi){if(v<lo)v=lo;if(v>hi)v=hi;}
""")
mock = mock.replace("auto GetCurrentEye(){return eye;}",
                    "bool active=true; bool IsActive(){return active;} auto GetCurrentEye(){return eye;}")
mock = mock.replace("assert(v==4 && i==6);", "assert(v%4==0 && i==v/4*6);")
mock += r'''
#define TSTATICASSERT(...)
namespace Toshi {
enum { T2GUIATTACHMENT_TOPLEFT, T2GUIATTACHMENT_MIDDLECENTER };
struct T2GUIElement {
    bool visible=true;
    T2GUIElement* parent=nullptr;
    float width=640,height=480;
    auto GetParent(){return parent;}
    bool IsVisible(){return visible;}
    void Show(){visible=true;} void Hide(){visible=false;}
    float GetWidth(){return width;} float GetHeight(){return height;}
    void AddChildTail(T2GUIElement& c){c.parent=this;}
};
struct T2GUIRectangle: T2GUIElement {
    static inline int visibleCount=0;
    void SetDimensions(float w,float h){width=w;height=h;}
    void SetAttachment(int,int){}
    void Show(){if(!visible)++visibleCount;visible=true;}
    void Hide(){if(visible)--visibleCount;visible=false;}
    void RemoveSelf(){Hide();parent=nullptr;}
    void SetTransform(float,float,float){}
    void SetColour(unsigned){}
    void SetTextureSection(T2GUITextureSection*){}
    T2GUIRectangle(){visible=false;}
};
struct T2GUITextureSectionManager {
    static auto GetTextureSection(const char*){static T2GUITextureSection t;return &t;}
};
}
struct AGUI2 {
    static auto GetRootElement(){static Toshi::T2GUIElement e;return &e;}
};
'''
main = r'''
#include "GUI/ATargetReticle.h"
int main(){
    using namespace Toshi;
    auto& vr=AVRManager::GetSingleton();
    vr.active=false;
    {
        ATargetReticle r;
        r.Create(nullptr);r.SetActive(true,true,20);
        assert(T2GUIRectangle::visibleCount==4);
        r.SetWorldSpace(true);
        assert(!r.IsWorldSpaceActive() && T2GUIRectangle::visibleCount==4);
        vr.active=true;vr.eye=0;
        ATargetReticle::RefreshWorldSpaceVisibility();
        assert(r.IsWorldSpaceActive() && T2GUIRectangle::visibleCount==0);
        ATargetReticle::RenderWorldSpace();
        assert(ASysMesh::draws==0); // no supplied anchor yet
        r.SetWorldPosition({0,0,5,1});
        r.Update(.1f);
        ATargetReticle::RenderWorldSpace();
        assert(ASysMesh::draws==1);
        AGUI2::GetRootElement()->Hide();
        ATargetReticle::RenderWorldSpace();
        assert(ASysMesh::draws==1);
        AGUI2::GetRootElement()->Show();
        r.SetActive(false,false,0);
        ATargetReticle::RenderWorldSpace();
        assert(ASysMesh::draws==1);
        r.SetActive(true,false,0);
        vr.active=false;vr.eye=~0u;
        ATargetReticle::RefreshWorldSpaceVisibility();
        assert(T2GUIRectangle::visibleCount==4); // desktop recovery
        ATargetReticle::RenderWorldSpace();
        assert(ASysMesh::draws==1);
        r.SetWorldSpace(false);
        assert(ASysMesh::alive==0 && ASysMaterial::alive==0);
        vr.active=true;vr.eye=1;
        ATargetReticle::RefreshWorldSpaceVisibility();
        assert(T2GUIRectangle::visibleCount==4); // opt-out remains ordinary UI
        r.SetWorldSpace(true);r.SetWorldPosition({0,0,5,1});
        ATargetReticle::RenderWorldSpace();
        assert(ASysMesh::alive==1);
    }
    assert(ASysMesh::alive==0 && ASysMaterial::alive==0);
    assert(T2GUIRectangle::visibleCount==0);
    int draws=ASysMesh::draws;
    ATargetReticle::RenderWorldSpace(); // destroyed entry is gone
    assert(ASysMesh::draws==draws);
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / "mock.h").write_text(mock)
    for name in ["pch.h", "Math/TVector4.h", "Math/TVector2.h",
                 "GUI/T2GUITextureSection.h", "GUI/T2GUIRectangle.h", "GUI/AGUI2.h",
                 "GUI/T2GUITextureSectionManager.h", "Render/ASysShader/ASysShader.h",
                 "Render/ASysShader/ASysMesh.h", "Render/ASysShader/ASysMaterial.h",
                 "VR/AVRManager.h", "Render/TBackend.h", "Core/TMemoryDebugOn.h"]:
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "mock.h"\n')
    (tmp / "main.cpp").write_text(main)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-DTRENDERINTERFACE_DX11", "-I" + str(tmp),
                    "-I" + str(ROOT / "OpenBarnyard/Source"),
                    str(ROOT / "OpenBarnyard/Source/Render/AWorldSpaceUI.cpp"),
                    str(ROOT / "OpenBarnyard/Source/GUI/ATargetReticle.cpp"),
                    str(tmp / "main.cpp"), "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], check=True)
print("PASS: opt-in, no duplicate UI, parent visibility, inactive targets, desktop recovery, teardown")
