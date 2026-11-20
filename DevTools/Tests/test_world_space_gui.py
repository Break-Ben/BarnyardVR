"""Compile the real world-GUI registry/frame code with a recording GUI backend.
Run: python3 DevTools/Tests/test_world_space_gui.py
"""
from pathlib import Path
import runpy
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
base = runpy.run_path(str(Path(__file__).with_name("test_world_space_ui.py")))
mock = base["MOCK"]
mock = mock.replace("struct TVector4 { float x=0,y=0,z=0,w=1; };", """struct TVector4 {
    float x=0,y=0,z=0,w=1;
    TVector3 AsVector3() const{return {x,y,z};}
};
enum BASISVECTOR {BASISVECTOR_RIGHT,BASISVECTOR_UP,BASISVECTOR_FORWARD,BASISVECTOR_TRANSLATION};""")
mock = mock.replace("    TVector4 translation{};", """    TVector4 translation{};
    TVector4 basis[3]={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
    void SetBasisVector4(BASISVECTOR i,const TVector4& v){basis[i]=v;}
    void SetTranslation(const TVector4& v){translation=v;}
    void Multiply(const TMatrix44& a,const TMatrix44& b){
        for(int k=0;k<3;++k) RotateVector(basis[k],a,b.basis[k]);
        TransformVector(translation,a,b.translation);
    }""")
mock = mock.replace("    unsigned eye=~0u;", "    bool active=true;\n    bool IsActive(){return active;}\n    unsigned eye=~0u;")
mock = mock.replace("struct TVector2 {", "struct TVector2 { static const TVector2 VEC_ZERO;")
mock += "\ninline const Toshi::TVector2 Toshi::TVector2::VEC_ZERO{};\n"
mock += r'''
#include <vector>
namespace Toshi {
struct T2GUITransform {};
struct T2GUIElement {
    inline static bool (*s_cbRenderFilter)(const T2GUIElement*)=nullptr;
    inline static void (*s_cbDestroyed)(const T2GUIElement*)=nullptr;
    bool visible=true;
    T2GUIElement* parent=nullptr;
    std::vector<T2GUIElement*> children;
    int draws=0;
    virtual ~T2GUIElement(){if(s_cbDestroyed)s_cbDestroyed(this);}
    void GetDimensions(float& w,float& h){w=h=20;}
    void PivotPos(float& x,float& y,float,float){x=y=0;}
    bool IsVisible() const{return visible;}
    bool ShouldRender() const{return visible && (!s_cbRenderFilter||s_cbRenderFilter(this));}
    auto GetParent() const{return parent;}
    virtual void Render(){++draws;for(auto child:children)if(child->ShouldRender())child->Render();}
    void Add(T2GUIElement& child){child.parent=this;children.push_back(&child);}
};
}
struct AGUI2RendererDX8 {
    void PushTransform(const Toshi::T2GUITransform&,const Toshi::TVector2&,const Toshi::TVector2&){}
    void PopTransform(){}
    static inline std::vector<Toshi::TMatrix44> frames;
    static inline std::vector<bool> masks;
    void RenderWorldSpace(const Toshi::TMatrix44& frame,void(*draw)(void*),void* data,bool masked=false){
        frames.push_back(frame);masks.push_back(masked);draw(data);
    }
};
struct AGUI2 {
    static auto GetRenderer(){static AGUI2RendererDX8 r;return &r;}
    static auto GetRootElement(){static Toshi::T2GUIElement root;return &root;}
};
'''
main = r'''
#include "Render/AWorldSpaceGUI.h"
bool close(float a,float b){return std::abs(a-b)<.0001f;}
int main(){
    using namespace Toshi;
    auto root=AGUI2::GetRootElement();
    auto& vr=AVRManager::GetSingleton();
    auto& ctx=TRenderInterfaceHAL::Interface()->ctx;
    T2GUIElement panel,child;
    root->Add(panel);panel.Add(child);
    AWorldSpaceGUI::SetAnchor(panel,{0,0,5,1},.02f,true);
    assert(panel.IsVisible() && !panel.ShouldRender()); // ticking/gameplay unaffected
    assert(child.ShouldRender());
    vr.eye=0;AWorldSpaceGUI::RenderAll();
    assert(panel.draws==1 && child.draws==1);
    assert(AGUI2RendererDX8::masks.back());
    auto first=AGUI2RendererDX8::frames.back();
    assert(close(first.translation.z,5) && close(first.basis[0].x,.02f));
    vr.eye=1;ctx.view.tx=-.032f;AWorldSpaceGUI::RenderAll();
    assert(panel.draws==2 && close(AGUI2RendererDX8::frames.back().translation.x,-.032f));
    ctx.view={0,0,0,0,.7f};AWorldSpaceGUI::RenderAll();
    auto rolled=AGUI2RendererDX8::frames.back();
    assert(close(rolled.basis[0].x,.02f*std::cos(.7f)));
    assert(close(rolled.basis[0].y,.02f*std::sin(.7f)));
    // Separately anchored children (Darts' dart-follow marker) draw once, not
    // once at the board and again at the dart.
    AWorldSpaceGUI::SetAnchor(child,{0,0,8,1});
    AWorldSpaceGUI::RenderAll();
    assert(panel.draws==4 && child.draws==4);
    panel.visible=false;AWorldSpaceGUI::RenderAll();
    assert(panel.draws==4 && child.draws==4);
    panel.visible=true;panel.parent=nullptr;AWorldSpaceGUI::RenderAll();
    assert(panel.draws==4 && child.draws==4); // detached scene
    panel.parent=root;
    AWorldSpaceGUI::Remove(&child);
    vr.active=false;
    assert(panel.ShouldRender());
    root->Render();assert(panel.draws==5 && child.draws==5);
    vr.active=true;vr.eye=~0u;AWorldSpaceGUI::RenderAll();assert(panel.draws==5);
    vr.eye=0;AWorldSpaceGUI::SetAnchor(panel,{0,0,-1,1});
    AWorldSpaceGUI::RenderAll();assert(panel.draws==5); // near/behind
    TMatrix44 board;
    board.SetTranslation({1,2,9,1});
    board.SetBasisVector4(BASISVECTOR_UP,{0,0,.1f,0});
    AWorldSpaceGUI::SetFrame(panel,board);ctx.view={};
    AWorldSpaceGUI::RenderAll();
    auto fixed=AGUI2RendererDX8::frames.back();
    assert(close(fixed.translation.z,9)&&close(fixed.basis[1].z,.1f));
    AWorldSpaceGUI::Remove(&panel);
    {
        T2GUIElement temporary;root->Add(temporary);
        AWorldSpaceGUI::SetAnchor(temporary,{0,0,5,1});
    }
    int before=AGUI2RendererDX8::frames.size();AWorldSpaceGUI::RenderAll();
    assert(int(AGUI2RendererDX8::frames.size())==before); // destructor unregisters
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / "mock.h").write_text(mock)
    for name in ["pch.h", "Math/TVector4.h", "Math/TMatrix44.h", "GUI/T2GUIElement.h",
                 "GUI/T2GUITextureSection.h", "GUI/AGUI2.h", "VR/AVRManager.h",
                 "Render/ASysShader/ASysShader.h", "Render/ASysShader/ASysMesh.h",
                 "Render/ASysShader/ASysMaterial.h", "Render/TBackend.h",
                 "Platform/DX11/AGUI2Renderer_DX11.h", "Core/TMemoryDebugOn.h"]:
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "mock.h"\n')
    (tmp / "main.cpp").write_text(main)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-DTRENDERINTERFACE_DX11",
                    f"-I{tmp}", f"-I{ROOT / 'OpenBarnyard/Source'}",
                    str(ROOT / "OpenBarnyard/Source/Render/AWorldSpaceUI.cpp"),
                    str(ROOT / "OpenBarnyard/Source/Render/AWorldSpaceGUI.cpp"),
                    str(tmp / "main.cpp"), "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], check=True)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
                    f"-I{tmp}", f"-I{ROOT / 'OpenBarnyard/Source'}",
                    str(ROOT / "OpenBarnyard/Source/Render/AWorldSpaceGUI.cpp")], check=True)
print("PASS: world GUI stereo frames, roll, no duplicate children, masks, parent visibility, detach, desktop, teardown")
