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
#define FBH 192

struct map {
  int rid;
  int imageid;
  int neiw,neie,nein,neis; // Neighbor mapid.
  uint8_t v[NS_sys_mapw*NS_sys_maph]; // Copied each time the map loads.
  const uint8_t *kv; // NS_sys_mapw*NS_sys_maph, from the resource.
  const uint8_t *cmd;
  int cmdc;
};

extern struct g {
  void *rom;
  int romc;
  struct rom_entry *resv;
  int resc,resa;
  struct map *mapv;
  int mapc,mapa;
  
  struct graf graf;
  int pvinput;
  
  struct modal **modalv;
  int modalc,modala;
  struct modal *modal_focus; // WEAK, do not dereference without checking. For id only.
  
  struct sprite **spritev;
  int spritec,spritea;
  struct sprite *hero; // WEAK, OPTIONAL, modal_play populates at the start of each frame.
  
  struct map *map; // WEAK, owned by mapv.
} g;

int res_load();
int res_search(int tid,int rid);
int res_get(void *dstpp,int tid,int rid);
struct map *res_get_map(int rid);

int load_map(int mapid);

#endif
