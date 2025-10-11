#include "game/wishbone.h"

#define WALK_SPEED 6.000

struct sprite_hero {
  struct sprite hdr;
  int input;
  int qx,qy;
  uint8_t facedir; // 0x40,0x10,0x08,0x02
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_hero*)sprite)

/* Delete.
 */
 
static void _hero_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  SPRITE->qx=-1;
  SPRITE->qy=-1;
  SPRITE->facedir=0x02;
  return 0;
}

/* Walking and such.
 */
 
static void hero_update_walk(struct sprite *sprite,double elapsed) {
  switch (SPRITE->input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: sprite_move(sprite,-WALK_SPEED*elapsed,0.0); break;
    case EGG_BTN_RIGHT: sprite_move(sprite,WALK_SPEED*elapsed,0.0); break;
  }
  switch (SPRITE->input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: sprite_move(sprite,0.0,-WALK_SPEED*elapsed); break;
    case EGG_BTN_DOWN: sprite_move(sprite,0.0,WALK_SPEED*elapsed); break;
  }
}

/* Animation.
 */
 
static void hero_update_animation(struct sprite *sprite,double elapsed) {
  if (SPRITE->input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
}

/* Quantize my position and if it's changed, trigger POI.
 */
 
static void hero_check_quantized_position(struct sprite *sprite) {
  int x=(int)sprite->x; if (sprite->x<0.0) x=-1;
  int y=(int)sprite->y; if (sprite->y<0.0) y=-1;
  if (x>=NS_sys_mapw) x=-1;
  if (y>=NS_sys_maph) y=-1;
  if ((x==SPRITE->qx)&&(y==SPRITE->qy)) return;
  qpos_release(SPRITE->qx,SPRITE->qy);
  SPRITE->qx=x;
  SPRITE->qy=y;
  qpos_press(x,y);
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  hero_update_walk(sprite,elapsed);
  hero_update_animation(sprite,elapsed);
  hero_check_quantized_position(sprite);
}

/* Render.
 */
 
static void _hero_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  uint8_t tileid=sprite->tileid;
  uint8_t xform=0;
  switch (SPRITE->facedir) {
    case 0x02: break; // Down, the natural direction.
    case 0x40: tileid+=1; break; // Up.
    case 0x10: tileid+=2; break; // Left, natural orientation of the tile.
    case 0x08: tileid+=2; xform=EGG_XFORM_XREV; break; // Right. Left but flopped.
  }
  switch (SPRITE->animframe) {
    case 1: tileid+=0x10; break;
    case 3: tileid+=0x20; break;
  }
  graf_tile(&g.graf,dstx,dsty,tileid,xform);
}

/* Modal gains or loses focus.
 */
 
static void _hero_focus(struct sprite *sprite,int focus) {
  if (!focus) {
    SPRITE->input=0;
    SPRITE->animclock=0;
    SPRITE->animframe=0;
  }
}

/* Type.
 */
 
const struct sprite_type sprite_type_hero={
  .name="hero",
  .objlen=sizeof(struct sprite_hero),
  .del=_hero_del,
  .init=_hero_init,
  .update=_hero_update,
  .render=_hero_render,
  .focus=_hero_focus,
};

/* Input changed.
 */
 
void sprite_hero_input(struct sprite *sprite,int input,int pvinput) {
  SPRITE->input=input;
  
  // When a dpad button goes ON, facedir changes immediately.
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) SPRITE->facedir=0x10;
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) SPRITE->facedir=0x08;
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) SPRITE->facedir=0x40;
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) SPRITE->facedir=0x02;
  
  // When a dpad button goes OFF, we're facing that way, and one other dpad button is held, face the one still held.
  if (
    (!(input&EGG_BTN_LEFT)&&(pvinput&EGG_BTN_LEFT)&&(SPRITE->facedir==0x10))||
    (!(input&EGG_BTN_RIGHT)&&(pvinput&EGG_BTN_RIGHT)&&(SPRITE->facedir==0x08))||
    (!(input&EGG_BTN_UP)&&(pvinput&EGG_BTN_UP)&&(SPRITE->facedir==0x40))||
    (!(input&EGG_BTN_DOWN)&&(pvinput&EGG_BTN_DOWN)&&(SPRITE->facedir==0x02))
  ) {
    if (input&EGG_BTN_LEFT) SPRITE->facedir=0x10;
    else if (input&EGG_BTN_RIGHT) SPRITE->facedir=0x08;
    else if (input&EGG_BTN_UP) SPRITE->facedir=0x40;
    else if (input&EGG_BTN_DOWN) SPRITE->facedir=0x02;
  }
}
