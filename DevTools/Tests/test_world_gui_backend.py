"""Exercise the actual DX11 GUI world-draw scope with recording D3D doubles.
No Windows SDK required; this is not a substitute for a Windows/headset build.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "OpenBarnyard/Source/Platform/DX11/AGUI2Renderer_DX11.cpp").read_text()
method = source[source.index("void AGUI2RendererDX8::RenderWorldSpace("):]
mock = r'''
#include <cassert>
#include <cstddef>
using TFLOAT=float;using TBOOL=bool;using UINT=unsigned;
#define TNULL nullptr
#define TFALSE false
#define FAILED(x) ((x)!=0)
constexpr unsigned D3D11_BIND_DEPTH_STENCIL=1,D3D11_USAGE_DEFAULT=0,D3D11_CLEAR_DEPTH=1;
enum BASISVECTOR{BASISVECTOR_RIGHT,BASISVECTOR_UP,BASISVECTOR_FORWARD};
struct TVector4{float x,y,z,w;};
struct TMatrix44{
    int id=0;
    static const TMatrix44 IDENTITY;
    void SetBasisVector4(BASISVECTOR,const TVector4&){}
};
inline const TMatrix44 TMatrix44::IDENTITY{0};
struct TVector2{float x,y;};
struct T2GUITransform{TVector2 m_aMatrixRows[2],m_vecTranslation;};
struct D3D11_TEXTURE2D_DESC{
    unsigned Width=100,Height=100,Format=1,BindFlags=0,MiscFlags=0,CPUAccessFlags=0,Usage=0;
    struct{unsigned Count=1,Quality=0;}SampleDesc;
};
struct D3D11_DEPTH_STENCIL_VIEW_DESC{unsigned Format=1;};
struct ID3D11Resource{virtual ~ID3D11Resource()=default;};
struct ID3D11Texture2D:ID3D11Resource{
    D3D11_TEXTURE2D_DESC desc;
    void GetDesc(D3D11_TEXTURE2D_DESC* d){*d=desc;}
};
struct ID3D11DepthStencilView{
    ID3D11Texture2D* texture;
    void GetResource(ID3D11Resource** p){*p=texture;}
    void GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* d){d->Format=texture->desc.Format;}
};
struct ID3D11RenderTargetView{};
namespace Microsoft::WRL{
template<class T>struct ComPtr{
    T* p=nullptr;
    T* Get() const{return p;}
    T** GetAddressOf(){return &p;}
    T* operator->()const{return p;}
    explicit operator bool()const{return p!=nullptr;}
    void Reset(){p=nullptr;}
    template<class U>int As(ComPtr<U>* out){out->p=dynamic_cast<U*>(p);return out->p?0:1;}
};
}
auto& WorldGUIMaskTexture(){static Microsoft::WRL::ComPtr<ID3D11Texture2D> t;return t;}
auto& WorldGUIMaskDepth(){static Microsoft::WRL::ComPtr<ID3D11DepthStencilView> d;return d;}
struct Device{
    int allocations=0;bool fail=false;
    int CreateTexture2D(const D3D11_TEXTURE2D_DESC* d,void*,ID3D11Texture2D** p){
        if(fail)return 1;
        *p=new ID3D11Texture2D;(*p)->desc=*d;++allocations;return 0;
    }
    int CreateDepthStencilView(ID3D11Texture2D* t,void*,ID3D11DepthStencilView** p){*p=new ID3D11DepthStencilView{t};return 0;}
};
struct GPU{
    ID3D11RenderTargetView* colour=nullptr;
    ID3D11DepthStencilView* depth=nullptr;
    ID3D11DepthStencilView* sceneDepth=nullptr;
    int clears=0;
    void OMGetRenderTargets(int,ID3D11RenderTargetView** c,ID3D11DepthStencilView** d){*c=colour;*d=depth;}
    void ClearDepthStencilView(ID3D11DepthStencilView* d,unsigned,float,int){
        assert(d && d!=sceneDepth);++clears;
    }
};
struct TRenderContextD3D{
    TMatrix44 projection{42};
    auto& GetProjectionMatrix(){return projection;}
};
struct TRenderD3DInterface{
    enum TRANSFORM{TRANSFORM_WORLD,TRANSFORM_VIEW,TRANSFORM_PROJECTION};
    struct STATEBLOCK{int value;};
    struct Pipeline{
        ID3D11RenderTargetView* pRenderTargetView=nullptr;
        ID3D11DepthStencilView* pDepthStencilView=nullptr;
        void* apPSShaderResources[2]={};
        void* pVertexShader=nullptr;void* pPixelShader=nullptr;void* pInputLayout=nullptr;
    }pipeline;
    Device device;GPU gpu;TRenderContextD3D context;
    TMatrix44 transforms[3]={{1},{2},{3}};
    int state=99,flushes=0;
    bool zEnable=true;
    static auto Interface(){static TRenderD3DInterface r;return &r;}
    auto GetDevice(){return &device;}auto GetDeviceContext(){return &gpu;}
    void FlushShaders(){++flushes;}
    auto& GetTransform(TRANSFORM t){return transforms[t];}
    void SetTransform(TRANSFORM t,const TMatrix44& m){transforms[t]=m;}
    auto GetCurrentContext(){return &context;}
    void CaptureStateBlock(STATEBLOCK& b){b.value=state;}
    void ApplyStateBlock(const STATEBLOCK& b){state=b.value;}
    auto& GetDesiredState(){return pipeline;}
    void SetRenderTargets(ID3D11RenderTargetView* c,ID3D11DepthStencilView* d){pipeline.pRenderTargetView=c;pipeline.pDepthStencilView=d;}
    void ApplyDesiredState(){gpu.colour=pipeline.pRenderTargetView;gpu.depth=pipeline.pDepthStencilView;}
    void SetFogEnable(bool){}void SetZEnable(bool b){zEnable=b;}
    void SetTexture(int i,void* p){pipeline.apPSShaderResources[i]=p;}
    void SetShaders(void* v,void* p,void* l){pipeline.pVertexShader=v;pipeline.pPixelShader=p;pipeline.pInputLayout=l;}
    void UpdateRenderStates(){}
};
struct AGUI2RendererDX8{
    static inline float sm_fZCoordinate=.1f;
    static inline bool sm_bUnknownFlag=true;
    static inline const TMatrix44* s_pWorldFrame=nullptr;
    static inline const TMatrix44* s_pWorldProjection=nullptr;
    T2GUITransform transforms[32]={};
    T2GUITransform* m_pTransforms=transforms;
    int m_iTransformCount=0;
    void* m_pMaterial=reinterpret_cast<void*>(1);
    unsigned m_uiColour=123;
    void BeginScene(){TRenderD3DInterface::Interface()->state=5;m_pMaterial=nullptr;m_uiColour=0;sm_bUnknownFlag=false;}
    void EndScene(){TRenderD3DInterface::Interface()->state=6;}
    void SetTransform(const T2GUITransform& t){transforms[m_iTransformCount]=t;}
    void RenderWorldSpace(const TMatrix44&,void(*)(void*),void*,bool);
};
'''
main = r'''
struct Draw {AGUI2RendererDX8* gui;bool masked;int count=0;};
void draw(void* data){
    auto d=static_cast<Draw*>(data);++d->count;
    auto r=TRenderD3DInterface::Interface();
    assert(AGUI2RendererDX8::s_pWorldFrame);
    assert(AGUI2RendererDX8::s_pWorldProjection->id==42); // eye projection captured
    assert(r->transforms[2].id==0); // clip transform is not projected twice
    assert(r->gpu.depth==r->pipeline.pDepthStencilView);
    assert(d->masked ? (r->gpu.depth && r->gpu.depth!=r->gpu.sceneDepth) : !r->gpu.depth);
    assert(d->gui->transforms[0].m_aMatrixRows[1].y==1); // no screen Y flip/scaling
}
int main(){
    auto r=TRenderD3DInterface::Interface();
    ID3D11Texture2D texture;ID3D11DepthStencilView depth{&texture};ID3D11RenderTargetView colour;
    r->gpu.sceneDepth=&depth;r->SetRenderTargets(&colour,&depth);r->ApplyDesiredState();
    AGUI2RendererDX8 gui;gui.transforms[0].m_vecTranslation={7,8};
    Draw d{&gui,false};
    gui.RenderWorldSpace({9},draw,&d,false);
    assert(d.count==1 && r->device.allocations==0 && r->gpu.clears==0);
    assert(r->gpu.depth==&depth && r->state==99 && r->transforms[2].id==3);
    assert(gui.m_pMaterial==reinterpret_cast<void*>(1)&&gui.m_uiColour==123);
    assert(gui.transforms[0].m_vecTranslation.x==7 && gui.sm_bUnknownFlag);
    assert(!gui.s_pWorldFrame);
    d.masked=true;gui.RenderWorldSpace({9},draw,&d,true);
    gui.RenderWorldSpace({9},draw,&d,true);
    assert(d.count==3 && r->device.allocations==1 && r->gpu.clears==2);
    assert(r->gpu.depth==&depth && r->pipeline.pDepthStencilView==&depth);
    texture.desc.Width=200;gui.RenderWorldSpace({9},draw,&d,true);
    assert(r->device.allocations==2 && d.count==4); // target resize
    texture.desc.Width=300;r->device.fail=true;
    gui.RenderWorldSpace({9},draw,&d,true);
    assert(d.count==4 && r->gpu.depth==&depth && r->state==99); // allocation failure
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(mock + method + main)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    str(path / "test.cpp"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: GUI eye projection, scratch mask depth isolation/reuse/resize/failure, target/state restoration")
