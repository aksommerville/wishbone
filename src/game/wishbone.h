#ifndef EGG_GAME_MAIN_H
#define EGG_GAME_MAIN_H

#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"
#include "opt/graf/graf.h"
#include "opt/font/font.h"
#include "opt/res/res.h"
#include "egg_res_toc.h"
#include "shared_symbols.h"
#include "modal/modal.h"
#include "sprite/sprite.h"

#define FBW 320
#define FBH 192

// Quantized positions held by some sprite. We only track interesting ones.
#define QPOS_LIMIT 32

#define FORGOTTEN_LIMIT 16

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
  struct font *font;
  int pvinput;
  int framec;
  
  struct modal **modalv;
  int modalc,modala;
  struct modal *modal_focus; // WEAK, do not dereference without checking. For id only.
  
  struct sprite **spritev;
  int spritec,spritea;
  struct sprite *hero; // WEAK, OPTIONAL, modal_play populates at the start of each frame.
  
  struct map *map; // WEAK, owned by mapv.
  uint8_t physics[256];
  int map_dirty; // Set when you change (g.map->v) so modal_play knows to rerender the background.
  int hp,maxhp;
  int item; // Zero for none, or flagid.
  uint8_t flags[(NS_FLAG_COUNT+7)>>3];
  struct qpos { int x,y; } qposv[QPOS_LIMIT];
  int qposc;
  struct forgotten { // A thing left behind, typically a wishbone.
    int mapid;
    int x,y;
    int flagid;
    int forgottenid;
  } forgottenv[FORGOTTEN_LIMIT];
  int forgottenc;
  int forgottenid_next;
} g;

int res_load();
int res_search(int tid,int rid);
int res_get(void *dstpp,int tid,int rid);
struct map *res_get_map(int rid);
int res_get_string(void *dstpp,int rid,int strix);

int load_map(int mapid);

int flag_get(int flagid);
int flag_set(int flagid,int v); // => nonzero if changed

void qpos_press(int x,int y);
void qpos_release(int x,int y);

#define SFX(tag) wishbone_sound(RID_sound_##tag);
void wishbone_sound(int rid);

// Represented spells are some combination of [LRUD]. Also a good format for programmatic analysis.
int spell_eval(const char *src,int srcc);
int spell_repr(char *dst,int dsta,int spellid);
void spell_cast(int spellid);

int forgotten_add(int mapid,int x,int y,int flagid);
void forgotten_remove(int forgottenid);

#endif
