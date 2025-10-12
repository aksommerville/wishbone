/* sprite_firepot.c
 * Doesn't need to be a firepot specifically... Anything with ping-pong animation that generates sprites periodically.
 */
 
#include "game/wishbone.h"

#define MISSILE_TIME_MIN 1.500
#define MISSILE_TIME_MAX 3.000

struct sprite_firepot {
  struct sprite hdr;
  uint8_t tileid0;
  uint16_t missile_spriteid;
  uint8_t flagid;
  double animclock;
  int animframe;
  int enable;
  double missileclock;
};

#define SPRITE ((struct sprite_firepot*)sprite)

static void firepot_reset_missileclock(struct sprite *sprite) {
  SPRITE->missileclock=MISSILE_TIME_MIN+((rand()&0x7fff)*(MISSILE_TIME_MAX-MISSILE_TIME_MIN))/32768.0;
}

static void firepot_spawn_missile(struct sprite *sprite) {
  struct sprite *missile=sprite_spawn_res(SPRITE->missile_spriteid,sprite->x,sprite->y,0);
}

static int _firepot_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->missile_spriteid=sprite->arg>>16;
  SPRITE->flagid=sprite->arg>>8;
  SPRITE->enable=flag_get(SPRITE->flagid);
  firepot_reset_missileclock(sprite);
  return 0;
}

static void _firepot_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->enable) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
      switch (SPRITE->animframe) {
        case 0: sprite->tileid=SPRITE->tileid0+0; break;
        case 1: sprite->tileid=SPRITE->tileid0+1; break;
        case 2: sprite->tileid=SPRITE->tileid0+2; break;
        case 3: sprite->tileid=SPRITE->tileid0+1; break;
      }
    }
    if ((SPRITE->missileclock-=elapsed)<=0.0) {
      firepot_spawn_missile(sprite);
      firepot_reset_missileclock(sprite);
    }
  } else {
    sprite->tileid=SPRITE->tileid0+3;
  }
}

static void _firepot_flag(struct sprite *sprite,int flagid,int v) {
  if (flagid==SPRITE->flagid) {
    SPRITE->enable=v;
    SPRITE->animclock=0.0;
    if (SPRITE->enable) firepot_reset_missileclock(sprite);
  }
}

const struct sprite_type sprite_type_firepot={
  .name="firepot",
  .objlen=sizeof(struct sprite_firepot),
  .init=_firepot_init,
  .update=_firepot_update,
  .flag=_firepot_flag,
};
