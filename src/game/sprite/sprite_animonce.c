/* sprite_animonce.c
 * Plays a brief animation then self-destructs.
 * Use CMD_sprite_animonce to configure the animation.
 */
 
#include "game/wishbone.h"

struct sprite_animonce {
  struct sprite hdr;
  double animclock;
  double period;
  int ttl; // How many frames remain.
};

#define SPRITE ((struct sprite_animonce*)sprite)

static int _animonce_init(struct sprite *sprite) {
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->res,sprite->resc)<0) return -1;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_sprite_animonce) {
      SPRITE->ttl=cmd.arg[0]-1;
      SPRITE->period=(double)cmd.arg[1]/1000.0;
      break;
    }
  }
  if (SPRITE->ttl<1) {
    fprintf(stderr,"animonce sprite has no CMD_sprite_animonce, or ttl<1\n");
    return -1;
  }
  SPRITE->animclock=SPRITE->period;
  return 0;
}

static void _animonce_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=SPRITE->period;
    if (--(SPRITE->ttl)<=0) {
      sprite->defunct=1;
    } else {
      sprite->tileid++;
    }
  }
}

const struct sprite_type sprite_type_animonce={
  .name="animonce",
  .objlen=sizeof(struct sprite_animonce),
  .init=_animonce_init,
  .update=_animonce_update,
};
