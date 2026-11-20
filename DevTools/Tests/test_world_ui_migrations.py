"""Source-integration guard for the gameplay world-UI audit.

Runtime geometry/state behavior is exercised by test_world_space_ui.py,
test_world_space_gui.py and test_world_gui_backend.py. These checks ensure
actual game consumers select those paths (the original barnyard regression).
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "OpenBarnyard/Source"


def read(path):
    return (SRC / path).read_text()


managers = {
    path.stem for path in SRC.rglob("*.h")
    if path.name != "AMilkReticleManager.h" and
    re.search(r"AMilkReticleManager\s+m_", path.read_text())
}
assert managers == {
    "ASimState", "APrecisionSquirt", "AChasingChicksMiniGame", "AChickenCoopDefender",
    "AHoneyCollectingGame", "AVeggiePatchDefenderGame", "ACowTippingMiniGameState",
}, f"Review any added/removed world target manager consumer: {managers}"
manager = read("GUI/AMilkReticleManager.cpp")
assert "m_aTargets[ i ].oReticle.SetWorldSpace( TTRUE );" in manager
assert manager.count("GetCurrentEye() == 1") == 3
assert "!rTarget.oReticle.IsWorldSpaceActive()" in manager
for name in managers - {"ASimState", "APrecisionSquirt", "ACowTippingMiniGameState"}:
    code = read(f"MiniGame/{name}.cpp")
    removal = code[code.index(f"void {name}::OnRemoval()") :]
    assert "m_oReticles.SetWorldSpace( TFALSE );" in removal
launcher = read("Units/Projectiles/AAimedSquirtProjectileLauncher.cpp")
activate = launcher[launcher.index("void AAimedSquirtProjectileLauncher::Activate()") :]
assert "m_oReticle.SetWorldSpace( TTRUE );" in activate.split("// $Barnyard: FUNCTION")[0]
assert launcher.count("SetWorldPosition( m_vAimPoint )") == 2
assert "rReticle.SetWorldSpace( TTRUE );" in read("MiniGame/ASuperSquirtMicroGame.cpp")
assert "m_oTargetReticle.SetWorldSpace( TTRUE );" in read("MiniGame/ACowTippingMiniGameState.cpp")
assert "pReticle->SetWorldPosition( vReticle );" in read("MiniGame/ACowTippingMiniGameState_Player.cpp")

for path, element in [
    ("MiniGame/ATapperMiniGameState.cpp", "m_oMilkFill"),
    ("GUI/AGUI2TargetIndicator.cpp", "m_oRoot"),
    ("HUD/ADartFireHudElement.cpp", "m_oTargetLock"),
    ("HUD/AMiniGamePlayerFloat.cpp", "*this"),
]:
    assert f"AWorldSpaceGUI::SetAnchor( {element}," in read(path), path
assert "AWorldSpaceGUI::Remove( &m_oRingPoint );" in read("MiniGame/AGolfReticle.cpp")
assert "AWorldSpaceGUI::SetAnchor( m_oRingPoint," not in read("MiniGame/AGolfReticle.cpp")
assert "AWorldSpaceGUI::SetFrame( m_oCanvas, frame );" in read("HUD/ADartFireHudElement.cpp")
floats = read("GUI/AGUI2FloatHudElement.cpp")
assert "AWorldSpaceGUI::Register( *this," in floats
assert "GetCurrentEye() == 0" in floats
assert "AWorldSpaceGUI::Draw( position," in floats
assert "m_iNumVisibleTimed = m_oVisibleTimed.iCount;" in floats
assert "m_iNumVisibleUnit = m_oVisibleUnit.iCount;" in floats
assert read("Platform/Windows/ARenderer_Win.cpp").count("AWorldSpaceGUI::RenderAll();") == 1
# Filtering is render-only: raw IsVisible must not suppress Tick or gameplay.
engine = (ROOT / "Toshi/Source/GUI/T2GUIElement.h").read_text()
visible = engine[engine.index("TBOOL IsVisible() const") : engine.index("TBOOL ShouldRender() const")]
assert "s_cbRenderFilter" not in visible
assert "it->ShouldRender()" in (ROOT / "Toshi/Source/GUI/T2GUIElement.cpp").read_text()
print("PASS: all seven target-manager consumers, launcher/slingshot anchors, golf, Tapper, Darts, labels, float text, render-only filtering")
