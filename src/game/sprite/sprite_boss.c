/* sprite_boss.c
 * This must be instantiated just once, at the end of the game.
 * We take responsibility for some global things like ending the game, not usually a sprite's concern.
 */
 
#include "game/wishbone.h"

struct sprite_boss {
  struct sprite hdr;
  int sealed_room;
};

#define SPRITE ((struct sprite_boss*)sprite)

static int _boss_init(struct sprite *sprite) {

  // Music goes silent when you enter my room, briefly.
  egg_play_song(0,0,1);
  
  return 0;
}

/* Check hero position and seal room if ready.
 */
 
static void boss_seal_cell(struct sprite *sprite,int x,int y) {
  int p=y*NS_sys_mapw+x;
  uint8_t tileid=g.map->v[p];
  uint8_t physics=g.physics[tileid];
  if (physics==NS_physics_solid) return;
  g.map->v[p]=0x33; // Castle wall singleton.
}
 
static void boss_check_seal(struct sprite *sprite) {
  if (g.hero) {
    if (g.hero->x<1.5) return;
    if (g.hero->y<1.5) return;
    if (g.hero->x>NS_sys_mapw-1.5) return;
    if (g.hero->y>NS_sys_maph-1.5) return;
  }
  SPRITE->sealed_room=1;
  int i;
  for (i=0;i<NS_sys_mapw;i++) {
    boss_seal_cell(sprite,i,0);
    boss_seal_cell(sprite,i,NS_sys_maph-1);
  }
  for (i=0;i<NS_sys_maph;i++) {
    boss_seal_cell(sprite,0,i);
    boss_seal_cell(sprite,NS_sys_mapw-1,i);
  }
  g.map_dirty=1;
  egg_play_song(RID_song_bone_fight,0,1);
}

/* Update.
 */

static void _boss_update(struct sprite *sprite,double elapsed) {

  /* Wait for the hero to enter the room, then seal the door.
   * Until this happens, we don't do much.
   */
  if (!SPRITE->sealed_room) {
    boss_check_seal(sprite);
    return;
  }
  
  // Face the hero.
  if (g.hero) {
    if (g.hero->x<sprite->x) sprite->xform=0;
    else sprite->xform=EGG_XFORM_XREV;
  }
}

/* Get whacked.
 */

static int _boss_whack(struct sprite *sprite,struct sprite *whacker,double dx,double dy) {
  //TODO
  sprite->defunct=1;
  g.victory=1;
  return 0;
}

/* Render.
 */
 
static void _boss_render(struct sprite *sprite,int dstx,int dsty) {
  const int ht=NS_sys_tilesize>>1;
  graf_set_image(&g.graf,sprite->imageid);
  
  // Take a few measurements.
  int neckx,necky,headx,heady;
  int rlegx,rlegy,flegx,flegy;
  if (sprite->xform&EGG_XFORM_XREV) {
    neckx=dstx+NS_sys_tilesize;
    necky=dsty-ht;
    headx=dstx+NS_sys_tilesize*2; // TODO head should move around
    heady=dsty;
    rlegx=dstx+ht;
    rlegy=dsty+NS_sys_tilesize; // TODO legs move up and down when walking.
    flegx=dstx-ht;
    flegy=dsty+NS_sys_tilesize;
  } else {
    neckx=dstx-NS_sys_tilesize;
    necky=dsty-ht;
    headx=dstx-NS_sys_tilesize*2;
    heady=dsty;
    rlegx=dstx-ht;
    rlegy=dsty+NS_sys_tilesize;
    flegx=dstx+ht;
    flegy=dsty+NS_sys_tilesize;
  }
  
  // Body and legs.
  graf_tile(&g.graf,rlegx,rlegy,0x86,sprite->xform);
  graf_tile(&g.graf,dstx-ht,dsty-ht,0x73,0);
  graf_tile(&g.graf,dstx+ht,dsty-ht,0x74,0);
  graf_tile(&g.graf,dstx-ht,dsty+ht,0x83,0);
  graf_tile(&g.graf,dstx+ht,dsty+ht,0x84,0);
  graf_tile(&g.graf,flegx,flegy,0x86,sprite->xform);
  
  // Neck.
  int segmentc=4;
  int i=0; for (;i<segmentc;i++) {
    int x=((neckx*(segmentc-i))+(headx*i))/segmentc;
    int y=((necky*(segmentc-i))+(heady*i))/segmentc;
    graf_tile(&g.graf,x,y,0x76,0);
  }
  
  // Head.
  graf_tile(&g.graf,headx,heady,0x75,sprite->xform);
  
  // Wing.
  graf_tile(&g.graf,dstx,dsty,0x77,EGG_XFORM_YREV); // TODO flap when flying.
}

/* Type definition.
 */

const struct sprite_type sprite_type_boss={
  .name="boss",
  .objlen=sizeof(struct sprite_boss),
  .init=_boss_init,
  .update=_boss_update,
  .whack=_boss_whack,
  .render=_boss_render,
};
