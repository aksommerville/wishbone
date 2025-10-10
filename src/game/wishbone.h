#ifndef EGG_GAME_MAIN_H
#define EGG_GAME_MAIN_H

#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"
#include "opt/graf/graf.h"
#include "opt/res/res.h"
#include "egg_res_toc.h"
#include "shared_symbols.h"
#include "modal/modal.h"
#include "sprite/sprite.h"

#define FBW 320
#define FBH 180

extern struct g {
  void *rom;
  int romc;
  struct graf graf;
  struct rom_entry *resv;
  int resc,resa;
  int pvinput;
  
  struct modal **modalv;
  int modalc,modala;
  struct modal *modal_focus; // WEAK, do not dereference without checking. For id only.
  
  struct sprite **spritev;
  int spritec,spritea;
  struct sprite *hero;
} g;

int res_load();
int res_search(int tid,int rid);
int res_get(void *dstpp,int tid,int rid);

#endif
