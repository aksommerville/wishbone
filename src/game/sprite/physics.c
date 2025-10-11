#include "game/wishbone.h"

/* Move sprite.
 */

int sprite_move(struct sprite *sprite,double dx,double dy) {

  if (!sprite->phymask) {
    sprite->x+=dx;
    sprite->y+=dy;
    return 1;
  }
  
  /* I'm going to play fast and cheesy with the collision detection.
   * Sprites are all the same size and we'll have them move each axis independently.
   * We may assume that the first collision we encounter is the only one that matters.
   *
   * ...this is copied verbatim from myscrypt. It might not be adequate for wishbone, since we're a bit more physical.
   */
  
  const double fudge=0.005;
  double l=sprite->x-0.5;
  double r=sprite->x+0.5;
  double t=sprite->y-0.5;
  double b=sprite->y+0.5;
  if ((dx<0.0)||(dx>0.0)) { t+=fudge; b-=fudge; }
  else { l+=fudge; r-=fudge; }
       if (dx<0.0) { l+=dx; r=sprite->x; }
  else if (dx>0.0) { r+=dx; l=sprite->x; }
  else if (dy<0.0) { t+=dy; b=sprite->y; }
  else if (dy>0.0) { b+=dy; t=sprite->y; }
  else return 0;
  
  int cola=(int)l; if (cola<0) cola=0;
  int rowa=(int)t; if (rowa<0) rowa=0;
  int colz=(int)r; if (colz>=NS_sys_mapw) colz=NS_sys_mapw-1;
  int rowz=(int)b; if (rowz>=NS_sys_maph) rowz=NS_sys_maph-1;
  if ((cola<=colz)&&(rowa<=rowz)) {
    const uint8_t *maprow=g.map->v+rowa*NS_sys_mapw+cola;
    int row=rowa;
    for (;row<=rowz;row++,maprow+=NS_sys_mapw) {
      const uint8_t *mapp=maprow;
      int col=cola;
      for (;col<=colz;col++,mapp++) {
        uint8_t physics=g.physics[*mapp];
        if (physics>=32) continue;
        if (sprite->phymask&(1<<physics)) {
               if (dx<0.0) { double n=col+1.5; if (1||n<sprite->x) sprite->x=n; }
          else if (dx>0.0) { double n=col-0.5; if (1||n>sprite->x) sprite->x=n; }
          else if (dy<0.0) { double n=row+1.5; if (1||n<sprite->y) sprite->y=n; }
          else if (dy>0.0) { double n=row-0.5; if (1||n>sprite->y) sprite->y=n; }
          return 0;
        }
      }
    }
  }
  
  int i=g.spritec;
  struct sprite **p=g.spritev;
  for (;i-->0;p++) {
    struct sprite *other=*p;
    if (other==sprite) continue;
    if (other->defunct) continue;
    if (!other->phymask) continue;
    if (other->x+0.5<=l) continue;
    if (other->x-0.5>=r) continue;
    if (other->y+0.5<=t) continue;
    if (other->y-0.5>=b) continue;
         if (dx<0.0) sprite->x=other->x+1.0;
    else if (dx>0.0) sprite->x=other->x-1.0;
    else if (dy<0.0) sprite->y=other->y+1.0;
    else if (dy>0.0) sprite->y=other->y-1.0;
    if ((sprite->type==&sprite_type_hero)&&other->type->bump) {
      const double barrel_radius=0.550;
      if ((dx<0.0)||(dx>0.0)) {
        double q=other->y-sprite->y;
        if ((q>-barrel_radius)&&(q<barrel_radius)) other->type->bump(other,sprite);
      } else {
        double q=other->x-sprite->x;
        if ((q>-barrel_radius)&&(q<barrel_radius)) other->type->bump(other,sprite);
      }
    }
    return 0;
  }

  sprite->x+=dx;
  sprite->y+=dy;
  return 1;
}

/* Test collisions without correcting anything.
 */

int sprite_test_position(const struct sprite *sprite) {

  if (!sprite->phymask) return 1;
  
  const double fudge=0.005;
  double l=sprite->x-0.5+fudge;
  double r=sprite->x+0.5-fudge;
  double t=sprite->y-0.5+fudge;
  double b=sprite->y+0.5-fudge;
  
  int cola=(int)l; if (cola<0) cola=0;
  int rowa=(int)t; if (rowa<0) rowa=0;
  int colz=(int)r; if (colz>=NS_sys_mapw) colz=NS_sys_mapw-1;
  int rowz=(int)b; if (rowz>=NS_sys_maph) rowz=NS_sys_maph-1;
  if ((cola<=colz)&&(rowa<=rowz)) {
    const uint8_t *maprow=g.map->v+rowa*NS_sys_mapw+cola;
    int row=rowa;
    for (;row<=rowz;row++,maprow+=NS_sys_mapw) {
      const uint8_t *mapp=maprow;
      int col=cola;
      for (;col<=colz;col++,mapp++) {
        uint8_t physics=g.physics[*mapp];
        if (physics>=32) continue;
        if (sprite->phymask&(1<<physics)) return 0;
      }
    }
  }
  
  int i=g.spritec;
  struct sprite **p=g.spritev;
  for (;i-->0;p++) {
    struct sprite *other=*p;
    if (other==sprite) continue;
    if (other->defunct) continue;
    if (!other->phymask) continue;
    if (other->x+0.5<=l) continue;
    if (other->x-0.5>=r) continue;
    if (other->y+0.5<=t) continue;
    if (other->y-0.5>=b) continue;
    return 0;
  }

  return 1;
}
