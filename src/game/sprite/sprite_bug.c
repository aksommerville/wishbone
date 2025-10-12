/* sprite_bug.c
 * The basic monster.
 * 3-frame animation, random motion.
 */
 
#include "game/wishbone.h"

// Strictly speaking, 1.0 is correct, but the graphics don't always fill out their tiles, and also give the user some grace.
#define DAMAGE_DISTANCE 0.750

#define WALK_SPEED 4.0
#define WAIT_TIME_MIN 0.250
#define WAIT_TIME_MAX 1.000

struct sprite_bug {
  struct sprite hdr;
  uint8_t tileid0;
  double animclock;
  int animframe;
  double targetx,targety;
  double waitclock;
};

#define SPRITE ((struct sprite_bug*)sprite)

static inline int bug_likes_cell(int x,int y) {
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  uint8_t tileid=g.map->v[y*NS_sys_mapw+x];
  uint8_t physics=g.physics[tileid];
  return (physics==NS_physics_vacant);
}

// Set a nonzero waitclock, and choose a new target.
static void bug_wait(struct sprite *sprite) {
  SPRITE->waitclock=WAIT_TIME_MIN+((rand()&0x7fff)*(WAIT_TIME_MAX-WAIT_TIME_MIN))/32768.0;
  
  int col=(int)sprite->x; if (col<0) col=0; else if (col>=NS_sys_mapw) col=NS_sys_mapw-1;
  int row=(int)sprite->y; if (row<0) row=0; else if (row>=NS_sys_maph) row=NS_sys_maph-1;
  struct candidate {
    int col,row;
  } candidatev[4];
  int candidatec=0;
  if (bug_likes_cell(col-1,row)) candidatev[candidatec++]=(struct candidate){col-1,row};
  if (bug_likes_cell(col+1,row)) candidatev[candidatec++]=(struct candidate){col+1,row};
  if (bug_likes_cell(col,row-1)) candidatev[candidatec++]=(struct candidate){col,row-1};
  if (bug_likes_cell(col,row+1)) candidatev[candidatec++]=(struct candidate){col,row+1};
  if (candidatec<1) { // Nowhere to go? Bump the wait clock up by a couple seconds, and we'll check again then.
    SPRITE->waitclock+=2.000;
    return;
  }
  int p=rand()%candidatec;
  SPRITE->targetx=candidatev[p].col+0.5;
  SPRITE->targety=candidatev[p].row+0.5;
}

static int _bug_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->targetx=sprite->x;
  SPRITE->targety=sprite->y;
  bug_wait(sprite);
  return 0;
}

static void _bug_update(struct sprite *sprite,double elapsed) {

  // If (waitclock) is set, tick it down.
  if (SPRITE->waitclock>0.0) {
    SPRITE->waitclock-=elapsed;
    
  // Not waiting? Move.
  } else {
    int xok=1,yok=1;
    if (sprite->x<SPRITE->targetx) {
      if ((sprite->x+=WALK_SPEED*elapsed)>=SPRITE->targetx) sprite->x=SPRITE->targetx;
      else xok=0;
    } else if (sprite->x>SPRITE->targetx) {
      if ((sprite->x-=WALK_SPEED*elapsed)<=SPRITE->targetx) sprite->x=SPRITE->targetx;
      else xok=0;
    }
    if (sprite->y<SPRITE->targety) {
      if ((sprite->y+=WALK_SPEED*elapsed)>=SPRITE->targety) sprite->y=SPRITE->targety;
      else yok=0;
    } else if (sprite->y>SPRITE->targety) {
      if ((sprite->y-=WALK_SPEED*elapsed)<=SPRITE->targety) sprite->y=SPRITE->targety;
      else yok=0;
    }
    if (xok&&yok) bug_wait(sprite);
  }
  
  // Deal damage to the hero as warranted. Circular hitbox.
  if (g.hero) {
    double dx=g.hero->x-sprite->x;
    double dy=g.hero->y-sprite->y;
    double dist2=dx*dx+dy*dy;
    if (dist2<DAMAGE_DISTANCE*DAMAGE_DISTANCE) {
      sprite_hero_injure(g.hero,sprite);
    }
  }
  
  // We animation nonstop, even when standing still. Ants in the pants.
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    switch (SPRITE->animframe) {
      case 0: case 2: sprite->tileid=SPRITE->tileid0; break;
      case 1: sprite->tileid=SPRITE->tileid0+1; break;
      case 3: sprite->tileid=SPRITE->tileid0+2; break;
    }
  }
}

static int _bug_whack(struct sprite *sprite,struct sprite *whacker,double nx,double ny) {
  sprite->defunct=1;
  sprite_spawn_res(RID_sprite_soulballs,sprite->x,sprite->y,4);
  //TODO prizes? stats? flags?
  return 1;
}

const struct sprite_type sprite_type_bug={
  .name="bug",
  .objlen=sizeof(struct sprite_bug),
  .init=_bug_init,
  .update=_bug_update,
  .whack=_bug_whack,
};
