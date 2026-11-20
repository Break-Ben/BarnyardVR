"""Compile the real fill update to check VR edge padding and desktop recovery."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "OpenBarnyard/Source/HUD/AMilkFillHudElement.cpp").read_text()
method = source[source.index("void AMilkFillHudElement::SetFill("):]
mock = r'''
#include <cassert>
#include <algorithm>
#include <cmath>
using TFLOAT=float;
struct TMath { static float Max(float a,float b){return std::max(a,b);} };
struct AWorldSpaceGUI { static inline bool active=false; static bool IsActive(){return active;} };
struct Rectangle {
    float width=64,height=100;
    float GetWidth(){return width;} float GetHeight(){return height;}
    void SetWidth(float v){width=v;} void SetHeight(float v){height=v;}
};
struct AMilkFillHudElement {
    Rectangle m_oMilk,m_oMask,m_oBackPlate;
    void SetFill(float,float);
};
'''
main = r'''
int main(){
    AMilkFillHudElement meter;
    for(bool vr : {false,true,false}){
        AWorldSpaceGUI::active=vr;
        for(float fill : {0.f,.5f,1.f}){
            meter.SetFill(fill,1);
            assert(meter.m_oMilk.width==(vr?62.f:64.f));
            assert(std::abs(meter.m_oMilk.height-90.f*fill)<.0001f);
        }
    }
    meter.m_oMask.width=1;AWorldSpaceGUI::active=true;
    meter.SetFill(0,1);assert(meter.m_oMilk.width==0);
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(mock + method + main)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    str(path / "test.cpp"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: symmetric VR milk inset, unchanged fill heights, desktop width recovery, nonnegative width")
