#include "game/wishbone.h"

/* Map spellid to a string of gestures.
 */

// Indexed by spellid.
static const char *spell_recipev[]={
  [NS_spell_invalid     ]="",
  [NS_spell_teleport    ]="DDD",
  [NS_spell_rain        ]="ULDURD",
  [NS_spell_fire        ]="DRUDLU",
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
  fprintf(stderr,"TODO %s %d [%s:%d]\n",__func__,spellid,__FILE__,__LINE__);//TODO
}
