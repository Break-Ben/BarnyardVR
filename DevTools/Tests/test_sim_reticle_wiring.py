"""Check the normal barnyard's actual render hook, not just renderer primitives.

Compiles ASimState's helper and OnRender body against minimal scene doubles.
Full scene construction/removal is checked for explicit opt-in/cleanup calls.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "OpenBarnyard/Source/GameInterface/ASimState.cpp").read_text()


def function(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


# These checks would fail with the previous implementation, which enabled only
# Precision Squirt's manager while the barnyard continued using shared 2D UI.
assert "m_oMilkReticles.SetWorldSpace( TTRUE );" in function("void ASimState::OnInsertion()")
removal = function("void ASimState::OnRemoval()")
assert "m_oMilkReticles.SetWorldSpace( TFALSE );" in removal
assert "SetSimSquirtWorldSpace( TFALSE );" in removal
assert "SetSimSquirtWorldSpace( TFALSE );" in function("void ASimState::OnDeactivate()")

mock = r'''
#include <cassert>
using TFLOAT=float;
using TBOOL=bool;
#define TTRUE true
#define TFALSE false
#define TSTATICCAST(T,p) static_cast<T*>(p)
#define TGetClass(T) dummyClass
inline int dummyClass;
struct ATargetReticle {
    bool world=false,active=true;
    void SetActive(bool b,bool,float){active=b;}
    void SetWorldSpace(bool b){world=b;}
};
struct AProjectileLauncher {bool IsA(int*){return true;}};
struct AAimedSquirtProjectileLauncher: AProjectileLauncher {
    ATargetReticle reticle;
    auto& GetReticle(){return reticle;}
};
struct AUnit {
    bool IsA(int*){return true;}
    void* FindComponent(int*){return nullptr;}
};
struct AUnitPlayer: AUnit {
    AAimedSquirtProjectileLauncher launcher;
    auto GetActiveLauncher(){return &launcher;}
};
struct APlayerManager {
    AUnitPlayer player;
    bool present=true;
    static bool IsSingletonCreated(){return true;}
    static auto GetSingleton(){static APlayerManager p;return &p;}
    AUnit* GetPlayerUnit(int){return present?&player:nullptr;}
};
struct AGameLoader {
    static inline bool barn=false;
    static bool IsBarnExploreMiniGame(){return barn;}
};
struct AVRManager {
    unsigned eye=~0u;
    static auto& GetSingleton(){static AVRManager r;return r;}
    auto GetCurrentEye(){return eye;}
};
struct AVehicleSkidManager {
    static bool IsSingletonCreated(){return false;}
    static auto GetSingleton(){static AVehicleSkidManager r;return &r;}
    void Render(){}
};
using AMilkManager=AVehicleSkidManager;
struct APlayerControlComponent {bool CanUseHand(){return false;}};
struct Renderable {void Render(){}};
struct Manager {
    int updates=0;
    void UpdateSim(float){++updates;}
};
struct ASimState {
    bool m_bIsActive=true;
    Manager m_oMilkReticles;
    Renderable m_oMilkBottles;
    Renderable* m_pLadderBeansGame=nullptr;
    void OnRender(float);
};
'''
main = r'''
int main(){
    ASimState state;
    auto& vr=AVRManager::GetSingleton();
    auto& r=APlayerManager::GetSingleton()->player.launcher.reticle;
    vr.eye=0;state.OnRender(.016f);
    assert(r.world && state.m_oMilkReticles.updates==1);
    vr.eye=1;state.OnRender(0);
    assert(state.m_oMilkReticles.updates==1);
    SetSimSquirtWorldSpace(false);
    assert(!r.world && !r.active);
    state.m_bIsActive=false;vr.eye=0;state.OnRender(.016f);
    assert(!r.world && state.m_oMilkReticles.updates==1);
    state.m_bIsActive=true;AGameLoader::barn=true;state.OnRender(.016f);
    assert(!r.world && state.m_oMilkReticles.updates==1);
    AGameLoader::barn=false;vr.eye=~0u;state.OnRender(.016f);
    assert(r.world && state.m_oMilkReticles.updates==2); // desktop still updates
    APlayerManager::GetSingleton()->present=false;
    SetSimSquirtWorldSpace(false); // teardown tolerates missing player
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    cpp = path / "test.cpp"
    cpp.write_text(mock + function("static void SetSimSquirtWorldSpace(") + "\n" +
                   function("void ASimState::OnRender(") + main)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    str(cpp), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: barnyard opt-in, actual render hook, once-per-frame updates, suspend/teardown wiring")
