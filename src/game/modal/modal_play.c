#include "game/wishbone.h"

/* Transitions are effected at render, so they run even when we're blurred.
 */
#define TRANSITION_TIME_FRAMES 30

static void play_render_bgbits(struct modal *modal);
static void play_render_transbits(struct modal *modal);

struct modal_play {
  struct modal hdr;
  int bgbits;
  int transbits;
  int transclock; // Counts down from TRANSITION_TIME_FRAMES.
  int transdx,transdy; // Transition delta, or (0,0) for fade.
};

#define MODAL ((struct modal_play*)modal)

/* Delete.
 */
 
static void _play_del(struct modal *modal) {
  egg_texture_del(MODAL->bgbits);
  egg_texture_del(MODAL->transbits);
}

/* Init.
 */
 
static int _play_init(struct modal *modal) {
  if (!g.map) return -1;
  if (egg_texture_load_raw(MODAL->bgbits=egg_texture_new(),NS_sys_tilesize*NS_sys_mapw,NS_sys_tilesize*NS_sys_maph,0,0,0)<0) return -1;
  if (egg_texture_load_raw(MODAL->transbits=egg_texture_new(),NS_sys_tilesize*NS_sys_mapw,NS_sys_tilesize*NS_sys_maph,0,0,0)<0) return -1;
  play_render_bgbits(modal);
  return 0;
}

/* Focus.
 */
 
static void _play_focus(struct modal *modal,int focus) {
}

/* Poll sprites for (g.hero), maybe others like it.
 */
 
static void play_find_key_sprites(struct modal *modal) {
  g.hero=0;
  int i=g.spritec;
  struct sprite **p=g.spritev;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if ((sprite->type==&sprite_type_hero)&&!g.hero) {
      g.hero=sprite;
    }
  }
}

/* Update sprites.
 */
 
static void play_update_sprites(struct modal *modal,double elapsed) {
  int i=g.spritec;
  while (i-->0) {
    struct sprite *sprite=g.spritev[i];
    if (sprite->defunct) continue;
    if (sprite->type->update) sprite->type->update(sprite,elapsed);
  }
}

/* Navigate to neighbor map.
 */
 
static void play_nav(struct modal *modal,int dx,int dy) {
  int mapid=0;
       if (dx<0) mapid=g.map->neiw;
  else if (dx>0) mapid=g.map->neie;
  else if (dy<0) mapid=g.map->nein;
  else if (dy>0) mapid=g.map->neis;
  else return;
  if (!mapid) return;
  play_render_transbits(modal);
  if (load_map(mapid)<0) {
    fprintf(stderr,"map:%d failed to load (%+d,%+d from map:%d)\n",mapid,dx,dy,g.map->rid);
    egg_terminate(1);
    return;
  }
  if (g.hero) {
    g.hero->x-=dx*NS_sys_mapw;
    g.hero->y-=dy*NS_sys_maph;
  }
  play_render_bgbits(modal);
  
  // If the opposite transition was already in progress, reverse it.
  // Otherwise, start a fresh transition, even if one was already running.
  if (MODAL->transclock&&(MODAL->transdx==-dx)&&(MODAL->transdy==-dy)) {
    MODAL->transclock=TRANSITION_TIME_FRAMES-MODAL->transclock;
  } else {
    MODAL->transclock=TRANSITION_TIME_FRAMES;
  }
  MODAL->transdx=dx;
  MODAL->transdy=dy;
}

/* Check whether a transition and map load needs to start, and start if so.
 */
 
static void play_check_transitions(struct modal *modal) {
  if (g.hero) {
         if (g.hero->x<0.0) play_nav(modal,-1,0);
    else if (g.hero->y<0.0) play_nav(modal,0,-1);
    else if (g.hero->x>NS_sys_mapw) play_nav(modal,1,0);
    else if (g.hero->y>NS_sys_maph) play_nav(modal,0,1);
    else ; //TODO check for doors. are we doing doors?
  }
}

/* Update.
 */
 
static void _play_update(struct modal *modal,double elapsed,int input,int pvinput) {
  play_find_key_sprites(modal);
  if (g.hero&&(input!=pvinput)) sprite_hero_input(g.hero,input,pvinput);
  play_update_sprites(modal,elapsed);
  sprite_reap_defunct();
  play_check_transitions(modal);
}

/* Sort sprites.
 * Two passes of a bubble sort, in opposite directions.
 * It's allowed to go out of order briefly, but will snap back into order the more we render.
 */
 
static inline int play_rendercmp(const struct sprite *a,const struct sprite *b) {
  if (a->layer<b->layer) return -1;
  if (a->layer>b->layer) return 1;
  if (a->y<b->y) return -1;
  if (a->y>b->y) return 1;
  return 0;
}
 
static void play_sort_sprites(struct modal *modal) {
  if (g.spritec<2) return;
  int done=1,last=g.spritec-1,i=0;
  for (;i!=last;i++) {
    struct sprite *a=g.spritev[i];
    struct sprite *b=g.spritev[i+1];
    if (play_rendercmp(a,b)>0) {
      g.spritev[i]=b;
      g.spritev[i+1]=a;
      done=0;
    }
  }
  if (done) return;
  for (i=g.spritec-1,last=0;i!=last;i--) {
    struct sprite *a=g.spritev[i];
    struct sprite *b=g.spritev[i-1];
    if (play_rendercmp(a,b)<0) {
      g.spritev[i]=b;
      g.spritev[i-1]=a;
    }
  }
}

/* Render sprites.
 */
 
static void play_render_sprites(struct modal *modal,int x0,int y0) {
  int i=0,imageid=0;
  for (;i<g.spritec;i++) {
    struct sprite *sprite=g.spritev[i];
    if (sprite->defunct) continue;
    int dstx=x0+(int)(sprite->x*NS_sys_tilesize);
    int dsty=y0+(int)(sprite->y*NS_sys_tilesize);
    if (sprite->type->render) {
      sprite->type->render(sprite,dstx,dsty);
    } else {
      if (sprite->imageid!=imageid) {
        graf_set_image(&g.graf,imageid=sprite->imageid);
      }
      graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
    }
  }
}

/* Render status bar.
 */
 
static void play_render_status_bar(struct modal *modal,int x,int y,int w,int h) {
  graf_fill_rect(&g.graf,x,y,w,h,0x000000ff);
  //TODO status bar. for now just dumping some things to see what it looks like
  graf_set_image(&g.graf,RID_image_sprites);
  int dstx=x+NS_sys_tilesize;
  int dsty=y+(h>>1);
  graf_tile(&g.graf,dstx,dsty,0x41,0); dstx+=12;
  graf_tile(&g.graf,dstx,dsty,0x41,0); dstx+=12;
  graf_tile(&g.graf,dstx,dsty,0x41,0); dstx+=12;
  graf_tile(&g.graf,dstx,dsty,0x40,0); dstx+=12;
  graf_tile(&g.graf,dstx,dsty,0x40,0); dstx+=12;
  graf_tile(&g.graf,FBW-NS_sys_tilesize,dsty,0x43,0);
}

/* Render.
 */
 
static void _play_render(struct modal *modal) {
  const int worldw=NS_sys_tilesize*NS_sys_mapw;
  const int worldh=NS_sys_tilesize*NS_sys_maph;
  const int statush=FBH-worldh;
  play_sort_sprites(modal);
  if (MODAL->transclock>0) MODAL->transclock--; // Transition clock is unusual to tick against render frames instead of update time.
  
  if (MODAL->transclock) {
    int x0=(MODAL->transdx*worldw*MODAL->transclock)/TRANSITION_TIME_FRAMES;
    int y0=(MODAL->transdy*worldh*MODAL->transclock)/TRANSITION_TIME_FRAMES+statush;
    int tx=x0-worldw*MODAL->transdx;
    int ty=y0-worldh*MODAL->transdy;
    graf_set_input(&g.graf,MODAL->transbits);
    graf_decal(&g.graf,tx,ty,0,0,worldw,worldh);
    graf_set_input(&g.graf,MODAL->bgbits);
    graf_decal(&g.graf,x0,y0,0,0,worldw,worldh);
    play_render_sprites(modal,x0,y0);

  } else {
    graf_set_input(&g.graf,MODAL->bgbits);
    graf_decal(&g.graf,0,statush,0,0,worldw,worldh);
    play_render_sprites(modal,0,statush);
  }
  
  play_render_status_bar(modal,0,0,FBW,statush);
}

/* Render bgbits.
 */
 
static void play_render_bgbits(struct modal *modal) {
  graf_flush(&g.graf);
  const int x0=NS_sys_tilesize>>1;
  int y=NS_sys_tilesize>>1;
  const uint8_t *src=g.map->v;
  struct egg_render_tile vtxv[NS_sys_mapw*NS_sys_maph];
  struct egg_render_tile *vtx=vtxv;
  int yi=NS_sys_maph;
  for (;yi-->0;y+=NS_sys_tilesize) {
    int x=x0;
    int xi=NS_sys_mapw;
    for (;xi-->0;x+=NS_sys_tilesize,vtx++,src++) {
      vtx->x=x;
      vtx->y=y;
      vtx->tileid=*src;
      vtx->xform=0;
    }
  }
  struct egg_render_uniform un={
    .dsttexid=MODAL->bgbits,
    .srctexid=graf_tex(&g.graf,g.map->imageid),
    .mode=EGG_RENDER_TILE,
    .alpha=0xff,
  };
  egg_render(&un,vtxv,sizeof(vtxv));
}

/* Render transbits.
 */
 
static void play_render_transbits(struct modal *modal) {
  const int worldw=NS_sys_tilesize*NS_sys_mapw;
  const int worldh=NS_sys_tilesize*NS_sys_maph;
  graf_set_input(&g.graf,MODAL->bgbits);
  graf_set_output(&g.graf,MODAL->transbits);
  graf_decal(&g.graf,0,0,0,0,worldw,worldh);
  if (g.hero) g.hero->defunct=1;
  play_render_sprites(modal,0,0);
  if (g.hero) g.hero->defunct=0;
  graf_set_output(&g.graf,1);
}

/* Type definition.
 */
 
const struct modal_type modal_type_play={
  .name="play",
  .objlen=sizeof(struct modal_play),
  .opaque=1,
  .interactive=1,
  .del=_play_del,
  .init=_play_init,
  .focus=_play_focus,
  .update=_play_update,
  .render=_play_render,
};
