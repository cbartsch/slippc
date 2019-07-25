#ifndef ANALYZER_H_
#define ANALYZER_H_

#include <iostream>
#include <fstream>
#include <unistd.h>  //usleep
#include <math.h>    //sqrt

#include "enums.h"
#include "util.h"
#include "replay.h"

//Size of combo buffer
#define CB_SIZE 255

namespace slip {

class Analyzer {
private:
  std::ostream* _dout; //Debug output stream

  void analyzeInteractions(SlippiReplay &s, uint8_t (&ports)[2], unsigned *all_dynamics);

  void summarizeInteractions(SlippiReplay &s, uint8_t (&ports)[2], unsigned *all_dynamics) const;
  bool is1v1(SlippiReplay &s, uint8_t (&ports)[2]) const;
  void computeAirtime(SlippiReplay &s, uint8_t port) const;
  void computeMaxCombo(SlippiReplay &s, uint8_t p) const;
  void findAllCombos(SlippiReplay &s, uint8_t (&ports)[2], uint8_t i) const;
  void showGameHeader(SlippiReplay &s, uint8_t (&ports)[2]) const;
  void countLCancels(SlippiReplay &s, uint8_t port) const;
  void countTechs(SlippiReplay &s, uint8_t port) const;
  void countLedgegrabs(SlippiReplay &s, uint8_t port) const;
  void countDodges(SlippiReplay &s, uint8_t port) const;
  void countDashdances(SlippiReplay &s, uint8_t port) const;
  void countAirdodgesAndWavelands(SlippiReplay &s, uint8_t port) const;

  void printCombo(unsigned cur_combo, uint8_t (&combo_moves)[CB_SIZE], unsigned (&combo_frames)[CB_SIZE]) const;
  inline std::string stateName(SlippiFrame &f) {
    return Action::name[f.action_pre];
  }

  inline float playerDistance(SlippiFrame &pf, SlippiFrame &of) {
    float xd = pf.pos_x_pre - of.pos_x_pre;
    float yd = pf.pos_y_pre - of.pos_y_pre;
    return sqrt(xd*xd+yd*yd);
  }

  //NOTE: the next few functions do not check for valid frame indices
  //  This is technically unsafe, but boolean shortcut logic should ensure the unsafe
  //    portions never get called.
  inline bool maybeWavelanding(SlippiPlayer &p, unsigned f) const {
    //Code credit to Fizzi
    return p.frame[f].action_pre == 0x002B && (
      p.frame[f-1].action_pre == 0x00EC ||
      (p.frame[f-1].action_pre >= 0x0018 && p.frame[f-1].action_pre <= 0x0022)
      );
  }
  inline bool isDashdancing(SlippiPlayer &p, unsigned f) const {
    //Code credit to Fizzi
    //This should never thrown an exception, since we should never be in turn animation
    //  before frame 2
    return (p.frame[f].action_pre == 0x0014)
        && (p.frame[f-1].action_pre == 0x0012)
        && (p.frame[f-2].action_pre == 0x0014);
  }
  inline bool isInJumpsquat(SlippiFrame &f) const {
    return f.action_pre == 0x0018;
  }
  inline bool isSpotdodging(SlippiFrame &f) const {
    return f.action_pre == 0x00EB;
  }
  inline bool isAirdodging(SlippiFrame &f) const {
    return f.action_pre == 0x00EC;
  }
  inline bool isDodging(SlippiFrame &f) const {
    return (f.action_pre >= 0x00E9) && (f.action_pre <= 0x00EB);
  }
  inline bool inTumble(SlippiFrame &f) const {
    return f.action_pre == 0x0026;
  }
  inline bool inDamagedState(SlippiFrame &f) const {
    return (f.action_pre >= 0x004B) && (f.action_pre <= 0x005B);
  }
  inline bool inMissedTechState(SlippiFrame &f) const {
    return (f.action_pre >= 0x00B7) && (f.action_pre <= 0x00C6);
  }
  inline bool inFloorTechState(SlippiFrame &f) const {  //Excluiding walltechs, walljumps, and ceiling techs
    return (f.action_pre >= 0x00B7) && (f.action_pre <= 0x00C9);
  }
  inline bool inTechState(SlippiFrame &f) const {  //Including walltechs, walljumps, and ceiling techs
    return (f.action_pre >= 0x00B7) && (f.action_pre <= 0x00CC);
  }
  inline bool isShielding(SlippiFrame &f) const {
    return f.flags_3 & 0x80;
  }
  inline bool isInShieldstun(SlippiFrame &f) const {
    return f.action_pre == 0x00B5;
  }
  inline bool isGrabbed(SlippiFrame &f) const {
    return (f.action_pre >= 0x00DF) && (f.action_pre <= 0x00E8);
    // return f.action_pre == 0x00E3;
  }
  inline bool isThrown(SlippiFrame &f) const {
    return (f.action_pre >= 0x00EF) && (f.action_pre <= 0x00F3);
  }
  inline bool isAirborne(SlippiFrame &f) const {
    return f.airborne;
  }
  inline bool isInHitstun(SlippiFrame &f) const {
    return f.flags_4 & 0x02;
  }
  inline bool isInHitlag(SlippiFrame &f) const {
    return f.flags_2 & 0x20;
  }
  inline bool isDead(SlippiFrame &f) const {
    return f.flags_5 & 0x10;
  }
  inline bool isOnLedge(SlippiFrame &f) const {
    return f.action_pre == 0x00FD;
  }
  inline bool isOffStage(SlippiReplay &s, SlippiFrame &f) const {
    return
      f.pos_x_pre >  Stage::ledge[s.stage] ||
      f.pos_x_pre < -Stage::ledge[s.stage] ||
      f.pos_y_pre <  0
      ;
  }

  //TODO: can be fairly easily optimized by changing elapsed frames at the beginning to frames left
  inline std::string frameAsTimer(unsigned fnum) const {
    int elapsed  = fnum-START_FRAMES;
    elapsed      = (elapsed < 0) ? 0 : elapsed;
    int secs     = elapsed/60;
    int frames   = elapsed-(60*secs);
    int mins     = secs/60;
    secs        -= (60*mins);

    //Convert from elapsed to left
    int lmins = 8-mins;
    if (secs > 0 || frames > 0) {
      lmins -= 1;
    }
    int lsecs = 60-secs;
    if (frames > 0) {
      lsecs -= 1;
    }
    int lframes = 60-frames;

    return "0" + std::to_string(lmins) + ":"
      + (lsecs   < 10 ? "0" : "") + std::to_string(lsecs) + ":"
      + (lframes < 6  ? "0" : "") + std::to_string(int(100*(float)lframes/60.0f));
  }
public:
  Analyzer(std::ostream* dout);
  ~Analyzer();
  void analyze(SlippiReplay &s);
};

}

#endif /* ANALYZER_H_ */
