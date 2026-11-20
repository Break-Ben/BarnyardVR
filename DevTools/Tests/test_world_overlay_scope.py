"""Compile the real overlay scope against a recording backend (no Windows SDK needed).

Run: python3 DevTools/Tests/test_world_overlay_scope.py
This checks pass lifecycle/state isolation, not real DX11/OpenXR rendering.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STUB = r'''
#pragma once
#include <cassert>
using TFLOAT = float;
using TBOOL = bool;
constexpr bool TTRUE = true, TFALSE = false;
#define TOSHI_NAMESPACE_USING using namespace Toshi;
namespace Toshi {
struct TCameraObject;
struct TRenderContext {
    struct VIEWPORTPARAMS { float fX=0, fY=0, fWidth=800, fHeight=600, fMinZ=.2f, fMaxZ=1; } vp;
    struct PROJECTIONPARAMS { float value=7; } proj;
    TCameraObject* camera=nullptr;
    float view=0, cachedProjection=7;
    auto GetViewportParameters() { return vp; }
    auto GetProjectionParams() { return proj; }
    void SetViewportParameters(VIEWPORTPARAMS p) { vp=p; }
    void SetProjectionParams(PROJECTIONPARAMS p) { proj=p; }
    float GetX() { return vp.fX; } float GetY() { return vp.fY; }
    float GetWidth() { return vp.fWidth; } float GetHeight() { return vp.fHeight; }
    auto GetCameraObject() { return camera; }
    void SetCameraObject(TCameraObject* c) { camera=c; }
    float GetWorldViewMatrix() { return view; }
    void Update() { cachedProjection=proj.value; }
};
inline TRenderContext* current=nullptr;
inline TRenderContext* gpuContext=nullptr;
inline TRenderContext* queuedContext=nullptr;
inline float queuedProjection=0;
inline int draws=0;
struct Stack {
    int depth=0;
    void Push(float view) { assert(view==current->view); ++depth; }
    void Pop() { assert(depth>0); --depth; }
};
struct TRenderInterfaceHAL {
    Stack stack;
    static auto Interface() { static TRenderInterfaceHAL r; return &r; }
    auto GetCurrentContext() { return current; }
    void SetCurrentRenderContext(TRenderContext* c) { current=c; }
    auto& GetTransforms() { return stack; }
    void FlushShaders() {
        if (queuedContext) {
            assert(current==queuedContext);
            assert(current->cachedProjection==queuedProjection);
            ++draws; queuedContext=nullptr;
        }
    }
};
struct TViewport {
    TRenderContext ctx;
    bool background=false, depth=true, begun=false;
    int depthClears=0, backgroundClears=0;
    auto GetRenderContext() { return &ctx; }
    void SetX(float v) { ctx.vp.fX=v; } void SetY(float v) { ctx.vp.fY=v; }
    void SetWidth(float v) { ctx.vp.fWidth=v; } void SetHeight(float v) { ctx.vp.fHeight=v; }
    bool AllowBackgroundClear(bool b) { bool old=background; background=b; return old; }
    bool AllowDepthClear(bool b) { bool old=depth; depth=b; return old; }
    void BeginSKU() { gpuContext=&ctx; depthClears+=depth; backgroundClears+=background; }
    void Begin() { assert(!begun); begun=true; BeginSKU(); }
    void End() { assert(begun); TRenderInterfaceHAL::Interface()->FlushShaders(); begun=false; }
};
struct TCameraObject {
    float pose=0;
    void Render() { current->view=pose; current->proj.value=90; current->Update(); }
};
}
struct ARenderer {
    Toshi::TViewport parent;
    Toshi::TCameraObject camera;
    static auto GetSingleton() { static ARenderer r; return &r; }
    auto GetMainViewport() { return &parent; }
    auto GetCameraObject() { return &camera; }
};
struct AVRManager {
    unsigned eye=~0u;
    static auto& GetSingleton() { static AVRManager r; return r; }
    auto GetCurrentEye() { return eye; }
    unsigned GetViewCount() { return 2; }
    void ApplyEyeProjection(Toshi::TRenderContext* c) {
        if (eye<2) { c->proj.value=100+eye; c->Update(); }
    }
};
'''
HARNESS = r'''
#include "Render/AWorldOverlayRenderScope.h"
int main() {
    using namespace Toshi;
    auto r=ARenderer::GetSingleton();
    auto& vr=AVRManager::GetSingleton();
    TViewport overlay;
    TCameraObject oldCamera;
    overlay.ctx.camera=&oldCamera;
    // Reuse the same viewport across both eyes, desktop fallback, then VR again.
    for (unsigned eye : {0u, 1u, ~0u, 0u}) {
        vr.eye=eye;
        const bool vrActive=eye<2;
        r->parent.ctx.vp.fWidth=1800+eye%2*200;
        r->parent.ctx.vp.fHeight=1900;
        current=&r->parent.ctx;
        r->camera.pose=20+eye%2;
        queuedContext=current; queuedProjection=current->cachedProjection;
        int clears=r->parent.depthClears;
        {
            AWorldOverlayRenderScope scope(overlay);
            assert(current==&overlay.ctx && gpuContext==current);
            assert(current->GetWidth()==(vrActive?r->parent.ctx.GetWidth():800));
            assert(current->GetHeight()==(vrActive?1900:600));
            assert(current->view==r->camera.pose);
            assert(current->cachedProjection==(vrActive?100+eye:90));
            assert(current->camera==&r->camera);
            queuedContext=current; queuedProjection=current->cachedProjection;
        }
        assert(current==&r->parent.ctx && gpuContext==current);
        assert(r->parent.depthClears==clears && !r->parent.backgroundClears);
        assert(r->parent.depth && !r->parent.background);
        assert(overlay.ctx.camera==&oldCamera);
        assert(overlay.ctx.GetWidth()==800 && overlay.ctx.GetHeight()==600);
        assert(overlay.ctx.cachedProjection==7);
        assert(TRenderInterfaceHAL::Interface()->stack.depth==0);
        assert(!overlay.begun);
    }
    assert(draws==8 && overlay.depthClears==4 && !overlay.backgroundClears);
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / "mock.h").write_text(STUB)
    for name in ["pch.h", "Render/TViewport.h", "Render/TBackend.h",
                 "Render/TCameraObject.h", "Render/ARenderer.h", "VR/AVRManager.h",
                 "Core/TMemoryDebugOn.h"]:
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "mock.h"\n')
    (tmp / "main.cpp").write_text('#include <initializer_list>\n' + HARNESS)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(tmp), "-I" + str(ROOT / "OpenBarnyard/Source"),
                    str(ROOT / "OpenBarnyard/Source/Render/AWorldOverlayRenderScope.cpp"),
                    str(tmp / "main.cpp"), "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], check=True)
print("PASS: both eyes, desktop fallback, viewport restoration, projection cache and flush order")
