/* sprite_soulballs.c
 * Sic plural, one sprite draws multiple balls.
 */
 
#include "game/wishbone.h"

#define TTL 3.0

struct sprite_soulballs {
  struct sprite hdr;
  int ballc;
  uint8_t tileid0;
  double radius;
  double t;
  double animclock;
  int animframe;
  double ttl;
};

#define SPRITE ((struct sprite_soulballs*)sprite)

static int _soulballs_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->ballc=sprite->arg&0xff;
  if (SPRITE->ballc<3) SPRITE->ballc=3;
  SPRITE->ttl=TTL;
  SPRITE->t=((rand()&0x7fff)*(M_PI*2.0))/32768.0;
  SPRITE->animframe=rand()%6;
  return 0;
}

static void _soulballs_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->ttl-=elapsed)<=0.0) {
    sprite->defunct=1;
    return;
  }
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.125;
    if (++(SPRITE->animframe)>=6) SPRITE->animframe=0;
    switch (SPRITE->animframe) {
      case 0: sprite->tileid=SPRITE->tileid0+0; break;
      case 1: sprite->tileid=SPRITE->tileid0+1; break;
      case 2: sprite->tileid=SPRITE->tileid0+2; break;
      case 3: sprite->tileid=SPRITE->tileid0+3; break;
      case 4: sprite->tileid=SPRITE->tileid0+2; break;
      case 5: sprite->tileid=SPRITE->tileid0+1; break;
    }
  }
  SPRITE->radius+=elapsed*10.0;
  SPRITE->t+=elapsed*4.0;
}

static void _soulballs_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  double t=SPRITE->t;
  double dt=(M_PI*2.0)/(double)SPRITE->ballc;
  int i=SPRITE->ballc;
  for (;i-->0;t+=dt) {
    int x=dstx+(int)(SPRITE->radius*NS_sys_tilesize*cos(t));
    int y=dsty-(int)(SPRITE->radius*NS_sys_tilesize*sin(t));
    graf_tile(&g.graf,x,y,sprite->tileid,0);
  }
}

const struct sprite_type sprite_type_soulballs={
  .name="soulballs",
  .objlen=sizeof(struct sprite_soulballs),
  .init=_soulballs_init,
  .update=_soulballs_update,
  .render=_soulballs_render,
};
