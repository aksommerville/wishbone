/* sprite_bonfire.c
 * Simple hazard. Animates 2 frames + flop.
 * Rain extinguishes us.
 */
 
#include "game/wishbone.h"

#define DAMAGE_DISTANCE 0.750

struct sprite_bonfire {
  struct sprite hdr;
  uint8_t tileid0;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_bonfire*)sprite)

static int _bonfire_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  if (g.rainclock>0.0) return -1;
  return 0;
}

static void _bonfire_update(struct sprite *sprite,double elapsed) {
  if (g.rainclock>0.0) {
    //TODO a sizzle effect would be nice
    sprite->defunct=1;
    return;
  }
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    sprite->tileid=SPRITE->tileid0+(SPRITE->animframe&1);
    sprite->xform=(SPRITE->animframe&2)?0:EGG_XFORM_XREV;
  }
  if (g.hero) {
    double dx=g.hero->x-sprite->x;
    double dy=g.hero->y-sprite->y;
    double dist2=dx*dx+dy*dy;
    if (dist2<DAMAGE_DISTANCE*DAMAGE_DISTANCE) {
      sprite_hero_injure(g.hero,sprite);
    }
  }
}

const struct sprite_type sprite_type_bonfire={
  .name="bonfire",
  .objlen=sizeof(struct sprite_bonfire),
  .init=_bonfire_init,
  .update=_bonfire_update,
};
