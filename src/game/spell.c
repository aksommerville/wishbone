#include "game/wishbone.h"

/* Map spellid to a string of gestures.
 */

// Indexed by spellid.
static const char *spell_recipev[]={
  [NS_spell_invalid     ]="",
  [NS_spell_teleport    ]="DDD",
  [NS_spell_rain        ]="ULDURD",
  [NS_spell_fire        ]="DRUDLU",
  [NS_spell_antechamber ]="LRLRDD",
};
 
int spell_eval(const char *src,int srcc) {
  if (!src) return NS_spell_invalid;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int recipec=sizeof(spell_recipev)/sizeof(void*);
  const char **p=spell_recipev;
  int spellid=0;
  for (;spellid<recipec;spellid++,p++) {
    if (memcmp(*p,src,srcc)) continue;
    if ((*p)[srcc]) continue;
    return spellid;
  }
  return NS_spell_invalid;
}
 
int spell_repr(char *dst,int dsta,int spellid) {
  if (spellid<1) return 0;
  int recipec=sizeof(spell_recipev)/sizeof(void*);
  if (spellid>=recipec) return 0;
  const char *src=spell_recipev[spellid];
  int dstc=0; while (src[dstc]) dstc++;
  if (dstc<=dsta) memcpy(dst,src,dstc);
  return dstc;
}

/* Cast spell.
 */
 
void spell_cast(int spellid) {
  switch (spellid) {
  
    // We're triggered during a sprite update, so it would not be fitting to reload the map right now.
    // Would in fact be devastating.
    case NS_spell_teleport: g.teleport=RID_map_start; break;
      
    case NS_spell_rain: {
        g.rainclock=2.5;
      } break;
      
    case NS_spell_fire: {
        fprintf(stderr,"TODO %s fire\n",__func__);
      } break;
      
    case NS_spell_antechamber: {
        if (flag_get(NS_flag_antechamber_bugs)) g.teleport=RID_map_antechamber;
      } break;
  }
  // Notify sprites.
  struct sprite **p=g.spritev;
  int i=g.spritec;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if (!sprite->type->spell) continue;
    sprite->type->spell(sprite,spellid);
  }
}
