#include "game/wishbone.h"

struct sprite_hero {
  struct sprite hdr;
  int input;
};

#define SPRITE ((struct sprite_hero*)sprite)

/* Delete.
 */
 
static void _hero_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  //TODO proper motion
  const double speed=6.0;
  switch (SPRITE->input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: sprite->x-=speed*elapsed; break;
    case EGG_BTN_RIGHT: sprite->x+=speed*elapsed; break;
  }
  switch (SPRITE->input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: sprite->y-=speed*elapsed; break;
    case EGG_BTN_DOWN: sprite->y+=speed*elapsed; break;
  }
}

/* Render.
 */
 
static void _hero_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
}

/* Modal gains or loses focus.
 */
 
static void _hero_focus(struct sprite *sprite,int focus) {
  if (!focus) {
    //TODO Return to neutral animation frame.
    //TODO Drop any user-driven activity. Walking or item.
    SPRITE->input=0;
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
  //TODO Impulse actions?
  //XXX
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    if (g.hp<g.maxhp) g.hp++;
  }
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) {
    if (g.hp>0) g.hp--;
  }
}
