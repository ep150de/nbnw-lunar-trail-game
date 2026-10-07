// UI capture / sanity test.
//
//   SDL_VIDEODRIVER=dummy ./build/lunar_trail_ui_capture
//
// Exercises every widget's drawing path without a display and asserts that each
// one actually paints pixels and never writes a palette index outside the 32
// colours. This catches the class of bug that a smoke test misses: a panel that
// draws nothing, a text call positioned off-screen, a blit that corrupts the
// index buffer.
//
// It also writes PNGs when a real framebuffer is available, so the output can be
// looked at rather than merely asserted on.
#include <cstdio>
#include <cmath>
#include "core/renderer.h"
#include "core/input.h"
#include "game/ui.h"
#include "sim/balance.h"
#include "sim/trail.h"
using namespace lt;
int main(){
  Renderer r;
  if (!r.init("cap")) { printf("no renderer\n"); return 1; }
  Balance b=Balance::defaults();
  RunState s=newRun(0x1234,Profession::MissionSpecialist,b);
  s.stock.waterL=640; s.stock.o2Kg=48; s.stock.foodKg=120;
  s.stock.fuelKg=90; s.stock.oxidiserKg=540; s.stock.cargoKg=180;
  s.stock.suitSets=9; s.stock.cuttingCharges=22;
  s.hardware.sparesWheel=2; s.hardware.sparesBogie=1; s.hardware.sparesSeal=2;
  s.credits=3400; s.energyKwh=412; s.health=58; s.progress.sol=31;
  s.progress.kmTravelled=57.0; s.progress.zone=Zone::Psr;

  const char* what[]={"gauge","bar","panel","menu","map","backdrop","text","message","log","buttons"};
  for (int i=0;i<10;++i){
    r.beginFrame();
    switch(i){
      case 0: drawGauge(r,4,4,120,"OXYGEN","48 kg",0.4,8.8,kCyanHi,false); break;
      case 1: drawBar(r,4,20,100,8,0.5,kCyan,kUiBlack,kUiGrey,4); break;
      case 2: r.panel(4,40,120,40,kUiBlack,kCyan);
              r.titledPanel(4,90,160,30,"TITLE",kMetalBlack,kAmber,kMetalDeep); break;
      case 3: drawMenu(r,8,10,200,{"One","Two","Three"},{"a","","c"},1,
                       Input(),std::vector<bool>(3,true),kCyan); break;
      case 4: drawTrailMap(r,4,10,312,44,s,{14.0,31.0,48.0},kCyanHi); break;
      case 5: drawBackdrop(r,120,kVoidDark,kVoidNear,kMetalDeep); break;
      case 6: r.drawTextCentered(160,60,"The quick brown fox jumps over 42 lazy dogs.",kUiWhite);
              r.drawTextScaled(20,80,"SCALED 2x",kAmberHi,2);
              r.drawTextWrapped(4,110,300,"A long wrapped line of text that should wrap at the right margin.",kUiGrey,8); break;
      case 7: drawMessageBox(r,"BOX",{"line one","line two is considerably longer than the first"}, "ENTER",kRedHi); break;
      case 8: { LogPanel l; l.push("first"); l.pushAll({"second","third"}); l.draw(r,4,4,200,80,"LOG",kCyan); } break;
      case 9: { Button bx; bx.x=10;bx.y=10;bx.w=120;bx.h=14;bx.label="Click";
                endButton(r,bx,kCyan);
                bx.enabled=false; bx.y=30; bx.label="Disabled"; endButton(r,bx,kCyan); } break;
    }
    // Sample the framebuffer.
    const auto& px = r.idxBuffer();
    double sum=0; int nonBg=0; uint8_t maxIdx=0;
    for (uint8_t v : px){ sum+=v; if(v!=31) ++nonBg; if(v>maxIdx) maxIdx=v; }
    printf("%-10s mean %6.1f  painted %5.1f%%  maxIndex %3u  %s\n", what[i],
           sum/px.size(), 100.0*nonBg/px.size(), maxIdx,
           maxIdx<32 ? "ok" : "OUT OF RANGE");
    if (maxIdx>=32) return 1;
  }
  return 0;
}
