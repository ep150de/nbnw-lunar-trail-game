// Balance harness.
//
//   g++ -std=c++17 -O2 -Isrc -o /tmp/balance tools/balance_harness.cpp
//       src/sim/*.cpp src/core/rng.cpp src/core/json.cpp && /tmp/balance
//   cmake --build build --target balance_harness   (preferred)
//
// This exists because hand-tuning the economy does not work. Every balance value
// in src/sim/balance.h was chosen from a measurement made by this file rather
// than by reasoning, and the loop below is a plain sweep of the two parameters
// that govern whether the game is winnable at all: the payload cap and the
// starting budgets.
//
// The shipped values (cap 2100 kg, budgets 16000 / 8800 / 5800) produce roughly
// 20% / 13% / 4% win rates for the three professions with the scripted policy
// below. That is the intended shape: the game is hard, every tier is
// theoretically winnable, and the hardest tier needs the most correct play.
//
// If you change a number in balance.h, re-run this and check the gradient still
// descends. A patch that makes all three tiers ~50% has broken the design.
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "sim/balance.h"
#include "sim/daystep.h"
#include "sim/depot.h"
#include "sim/hazards.h"
#include "sim/health.h"
#include "sim/trail.h"
using namespace lt;

static bool one(uint64_t seed, Profession prof, const Balance& b, int reservePct,
                int* solOut, double* kmOut){
  RunState s=newRun(seed,prof,b);
  s.energyKwh=b.batteryCapacityKwh; s.progress.landmarkIndex=0; s.pace=Pace::Cruise;
  const double res=reservePct/100.0;
  auto spend=[&](){ return std::max(0.0,s.credits*(1.0-res)); };
  auto purchase=[&](Good g,double q){
    double x=std::min(q,addableAmount(s,g,b));
    while(x>0.01&&buyCost(s,g,x,0,b)>spend()) x*=0.7;
    if(x>0.01) buy(s,g,x,0,b);
  };
  const double c=5.0;
  for(int i=0;i<3;i++){purchase(Good::SpareWheel,1);purchase(Good::SpareSeal,1);}
  for(int i=0;i<2;i++) purchase(Good::SpareBogie,1);
  purchase(Good::Water,b.waterPerCrewKg*c*22); purchase(Good::O2,b.o2PerCrewKg*c*18);
  purchase(Good::Food,b.foodPerCrewKg*c*18); purchase(Good::SuitSets,6);
  purchase(Good::CuttingCharges,20); purchase(Good::Fuel,80); purchase(Good::Oxidiser,480);
  purchase(Good::ColonyCargo,150);
  s.progress.landmarkIndex=1;
  Rng rng; rng.seed(seed);
  bool shop=false; int stall=0; double lastKm=-1;
  auto here=[&](){ return trail()[std::min<size_t>(s.progress.landmarkIndex,trail().size()-1)]; };
  for(int i=0;i<400&&s.outcome==Outcome::Playing;i++){
    if(std::abs(s.progress.kmTravelled-lastKm)<1e-9&&++stall>30) break;
    lastKm=s.progress.kmTravelled; stall=0;
    if(shop){ shop=false;
      size_t ix=std::min<size_t>(s.progress.landmarkIndex,trail().size()-1);
      if(ix>0&&trail()[ix-1].storeIndex>=0){ const Landmark&d=trail()[ix-1];
        jettisonToFit(s,b); rechargeAtDepot(s,d,b,nullptr);
        int st=d.storeIndex;
        auto bt=[&](Good g,double q){
          double x=std::min(q,addableAmount(s,g,b));
          while(x>0.01&&buyCost(s,g,x,st,b)>s.credits) x*=0.7;
          if(x>0.01) buy(s,g,x,st,b);
        };
        bt(Good::Water,b.waterPerCrewKg*c*22); bt(Good::O2,b.o2PerCrewKg*c*18);
        bt(Good::Food,b.foodPerCrewKg*c*18); bt(Good::Oxidiser,480);
        if(s.stock.fuelKg<100) bt(Good::Fuel,120);
        for(int k=0;k<2&&s.hardware.sparesWheel<3;++k){int b0=s.hardware.sparesWheel;bt(Good::SpareWheel,1);if(s.hardware.sparesWheel==b0)break;}
        for(int k=0;k<2&&s.hardware.bogies<3;++k){int b0=s.hardware.bogies;bt(Good::SpareBogie,1);if(s.hardware.bogies==b0)break;}
        bt(Good::ColonyCargo,50);
      }
      continue; }
    const Landmark& L=here();
    if(L.kind==LandmarkKind::Chasm&&s.progress.awaitingChasm){
      auto o=chasmOptions(s,L,b);int bi=0;double best=1e9;
      for(size_t k=0;k<o.size();++k) if(o[k].available&&o[k].risk<best){best=o[k].risk;bi=(int)k;}
      ChasmResult r=resolveChasm(s,L,o[bi].method,rng,b);
      s.progress.kmTravelled+=r.kmGained;s.progress.sol+=std::max<int>(1,r.solsLost);
      s.progress.awaitingChasm=false;s.progress.landmarkIndex=L.index+1; continue; }
    double wn=b.waterPerCrewKg*s.livingCrew(),on=b.o2PerCrewKg*s.livingCrew();
    if(zoneIcePerCrew(s.progress.zone,b)>0&&(s.stock.waterL<wn*10||s.stock.o2Kg<on*12)&&s.stock.cuttingCharges>0){
      if(s.overPayloadCap(b)) jettisonToFit(s,b);
      double cap=std::min(zoneIcePerCrew(s.progress.zone,b)*s.livingCrew()*s.geologistBonus(),b.sortieHaulCapKg);
      double drink=std::min(cap*kSortieDrinkFraction,std::max(0.0,addableAmount(s,Good::Water,b)));
      s.stock.waterL+=drink;
      double e=cap-drink;
      s.stock.o2Kg+=std::min(e*0.111,std::max(0.0,addableAmount(s,Good::O2,b)));
      s.stock.fuelKg+=std::min(e*0.889/0.333*0.42,std::max(0.0,addableAmount(s,Good::Fuel,b)));
      s.stock.cuttingCharges--; s.progress.sol++; s.progress.sortiesRun++;
      continue; }
    if(s.maladiedCrew()>=2){ applyRestingRecovery(s,b,4,nullptr); continue; }
    DayReport d=advanceSol(s,rng,b,true);
    if(d.reachedLandmark) shop=true;
    if(d.atChasm&&s.progress.awaitingChasm){
      const Landmark& cl=here();
      auto o=chasmOptions(s,cl,b);int bi=0;double best=1e9;
      for(size_t k=0;k<o.size();++k) if(o[k].available&&o[k].risk<best){best=o[k].risk;bi=(int)k;}
      ChasmResult r=resolveChasm(s,cl,o[bi].method,rng,b);
      s.progress.kmTravelled+=r.kmGained;s.progress.sol+=std::max<int>(1,r.solsLost);
      s.progress.awaitingChasm=false;s.progress.landmarkIndex=cl.index+1; continue; }
    if(s.progress.kmTravelled>=trail().back().km-0.01){ s.outcome=Outcome::Won; break; }
  }
  *solOut=s.progress.sol; *kmOut=s.progress.kmTravelled;
  return s.outcome==Outcome::Won;
}
namespace {

struct Row {
    double cap;
    double fdCr, msCr, ptCr;
    double fdWin, msWin, ptWin;   // 0..1
    double fdSol, msSol, ptSol;   // mean sols survived
    double fdKm,  msKm,  ptKm;    // mean km covered
};

// Runs one configuration and reports per-profession win rate and mean sols.
Row measure(double cap, double fdCr, double msCr, double ptCr, int runs) {
    Balance b = Balance::defaults();
    b.payloadCapKg = cap;
    b.creditsFlightDirector = fdCr;
    b.creditsMissionSpecialist = msCr;
    b.creditsPayloadTechnician = ptCr;

    Row row{cap, fdCr, msCr, ptCr, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    const double credits[3] = {fdCr, msCr, ptCr};
    double* win[3] = {&row.fdWin, &row.msWin, &row.ptWin};
    double* sol[3] = {&row.fdSol, &row.msSol, &row.ptSol};
    double* km[3]  = {&row.fdKm,  &row.msKm,  &row.ptKm};

    for (int p = 0; p < 3; ++p) {
        int wins = 0;
        double sols = 0;
        double kms = 0;
        for (int i = 0; i < runs; ++i) {
            // A per-run seed keeps the three professions on comparable footing:
            // run i uses the same terrain and hazard stream for all three.
            const uint64_t seed = 0xA100ull + static_cast<uint64_t>(i) * 7919ull;
            int solsOut = 0;
            double kmOut = 0;
            if (one(seed, static_cast<Profession>(p), b, 30, &solsOut, &kmOut)) ++wins;
            sols += solsOut;
            kms += kmOut;
        }
        *win[p] = static_cast<double>(wins) / runs;
        *sol[p] = sols / runs;
        *km[p]  = kms / runs;
        (void)credits[p];
    }
    return row;
}

void printHeader() {
    std::printf("%6s %7s %7s %7s | %-24s | %-24s | %-24s\n", "cap", "FDcr", "MScr", "PTcr",
                "FlightDirector", "MissionSpecialist", "PayloadTechnician");
}

void printRow(const Row& r) {
    char cells[3][64];
    const double win[3] = {r.fdWin, r.msWin, r.ptWin};
    const double sol[3] = {r.fdSol, r.msSol, r.ptSol};
    const double km[3]  = {r.fdKm,  r.msKm,  r.ptKm};
    for (int p = 0; p < 3; ++p) {
        std::snprintf(cells[p], sizeof(cells[p]), "%3.0f%% sol %3.0f km %3.0f", 100.0 * win[p],
                      sol[p], km[p]);
    }
    std::printf("%6.0f %7.0f %7.0f %7.0f | %-24s | %-24s | %-24s\n", r.cap, r.fdCr, r.msCr,
                r.ptCr, cells[0], cells[1], cells[2]);
}

}  // namespace

int main() {
    const int runs = 120;
    const Balance def = Balance::defaults();

    std::printf("Lunar Trail balance harness -- %d runs per cell\n\n", runs);

    std::printf("Shipped configuration:\n");
    printHeader();
    printRow(measure(def.payloadCapKg, def.creditsFlightDirector, def.creditsMissionSpecialist,
                      def.creditsPayloadTechnician, runs));

    std::printf("\nPayload cap sweep at shipped budgets:\n");
    printHeader();
    for (const double cap : {1700.0, 1900.0, 2100.0, 2300.0, 2500.0}) {
        printRow(measure(cap, def.creditsFlightDirector, def.creditsMissionSpecialist,
                         def.creditsPayloadTechnician, runs));
    }

    std::printf("\nFlight Director budget sweep at the shipped cap:\n");
    printHeader();
    for (const double cr : {8000.0, 12000.0, 16000.0, 20000.0, 28000.0}) {
        printRow(measure(def.payloadCapKg, cr, def.creditsMissionSpecialist,
                         def.creditsPayloadTechnician, runs));
    }

    std::printf("\nA patch that lifts all three tiers toward 50%% has broken the design;\n"
                "the gradient must stay steep and every tier must remain winnable.\n");
    return 0;
}
