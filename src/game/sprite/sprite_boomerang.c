/* sprite_boomerang.c
 */
 
#include "game/wishbone.h"

#define SPEED 10.0
#define RETURN_TIME 0.750

struct sprite_boomerang {
  struct sprite hdr;
  double dx,dy;
  double return_clock;
  uint8_t rotation;
};

#define SPRITE ((struct sprite_boomerang*)sprite)

static int boomerang_global_count=0;

static void _boomerang_del(struct sprite *sprite) {
  if (boomerang_global_count>0) boomerang_global_count--;
}

static int _boomerang_init(struct sprite *sprite) {
  boomerang_global_count++;
  switch (sprite->arg&0xff) {
    case 0x40: SPRITE->dy=-SPEED; break;
    case 0x10: SPRITE->dx=-SPEED; break;
    case 0x08: SPRITE->dx=SPEED; break;
    case 0x02: SPRITE->dy=SPEED; break;
    default: return -1;
  }
  SPRITE->return_clock=RETURN_TIME;
  return 0;
}

static void _boomerang_update(struct sprite *sprite,double elapsed) {
  SPRITE->rotation+=10;
  if (SPRITE->return_clock>0.0) {
    SPRITE->return_clock-=elapsed;
    sprite->x+=SPRITE->dx*elapsed;
    sprite->y+=SPRITE->dy*elapsed;
  } else if (g.hero) {
    const double CLOSE_ENOUGH=0.250; // Must be larger than our quantum of motion, which is kind of fuzzy.
    double dx=g.hero->x-sprite->x;
    double dy=g.hero->y-sprite->y;
    if ((dx>-CLOSE_ENOUGH)&&(dy>=-CLOSE_ENOUGH)&&(dx<CLOSE_ENOUGH)&&(dy<CLOSE_ENOUGH)) {
      SFX(boomerang_catch)
      sprite->defunct=1;
      return;
    }
    double distance=sqrt(dx*dx+dy*dy);
    dx/=distance;
    dy/=distance;
    sprite->x+=dx*SPEED*elapsed;
    sprite->y+=dy*SPEED*elapsed;
  }
  //TODO damage monsters, pick up items, all the things boomerangs really do
}

static void _boomerang_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_fancy(&g.graf,dstx,dsty,sprite->tileid,sprite->xform,SPRITE->rotation,NS_sys_tilesize,0,0x808080ff);
}

const struct sprite_type sprite_type_boomerang={
  .name="boomerang",
  .objlen=sizeof(struct sprite_boomerang),
  .del=_boomerang_del,
  .init=_boomerang_init,
  .update=_boomerang_update,
  .render=_boomerang_render,
};

int sprite_boomerang_exists() {
  return boomerang_global_count;
}
