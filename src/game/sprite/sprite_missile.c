/* sprite_missile.c
 * Targets the hero and travels in a straight line until offscreen.
 */
 
#include "game/wishbone.h"

#define SPEED 5.0
#define DAMAGE_DISTANCE 0.500

struct sprite_missile {
  struct sprite hdr;
  uint8_t tileid0;
  double animclock;
  int animframe;
  double dx,dy;
};

#define SPRITE ((struct sprite_missile*)sprite)

static int _missile_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  
  if (!g.hero) return -1;
  double dx=g.hero->x-sprite->x;
  double dy=g.hero->y-sprite->y;
  if ((dx>-0.5)&&(dx<0.5)&&(dy>-0.5)&&(dy<0.5)) return -1;
  double distance=sqrt(dx*dx+dy*dy);
  SPRITE->dx=(SPEED*dx)/distance;
  SPRITE->dy=(SPEED*dy)/distance;

  return 0;
}

static void _missile_update(struct sprite *sprite,double elapsed) {

  sprite->x+=SPRITE->dx*elapsed;
  sprite->y+=SPRITE->dy*elapsed;
  if ((sprite->x<-0.5)||(sprite->y<-0.5)||(sprite->x>NS_sys_mapw+0.5)||(sprite->y>NS_sys_maph+0.5)) {
    sprite->defunct=1;
    return;
  }
  
  if (g.hero) {
    double dx=g.hero->x-sprite->x;
    double dy=g.hero->y-sprite->y;
    double dist2=dx*dx+dy*dy;
    if (dist2<DAMAGE_DISTANCE*DAMAGE_DISTANCE) {
      sprite_hero_injure(g.hero,sprite);
      sprite->defunct=1; // Missiles are only good for one whack. (player likely bounces in our direction of travel, It'd be unfair to hit her again)
    }
  }

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
}

const struct sprite_type sprite_type_missile={
  .name="missile",
  .objlen=sizeof(struct sprite_missile),
  .init=_missile_init,
  .update=_missile_update,
};
