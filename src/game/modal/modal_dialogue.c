#include "game/wishbone.h"

struct modal_dialogue {
  struct modal hdr;
  int rid,strix;
  int texid,x,y,w,h;
};

#define MODAL ((struct modal_dialogue*)modal)

/* Delete.
 */
 
static void _dialogue_del(struct modal *modal) {
  egg_texture_del(MODAL->texid);
}

/* Init.
 */
 
static int _dialogue_init(struct modal *modal) {
  return 0;
}

/* Focus.
 */
 
static void _dialogue_focus(struct modal *modal,int focus) {
}

/* Update.
 */
 
static void _dialogue_update(struct modal *modal,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) modal->defunct=1;
  }
}

/* Render.
 */
 
static void _dialogue_render(struct modal *modal) {
  graf_fill_rect(&g.graf,MODAL->x-2,MODAL->y-2,MODAL->w+4,MODAL->h+4,0x000000ff);
  graf_set_input(&g.graf,MODAL->texid);
  graf_decal(&g.graf,MODAL->x,MODAL->y,0,0,MODAL->w,MODAL->h);
}

/* Type definition.
 */
 
const struct modal_type modal_type_dialogue={
  .name="dialogue",
  .objlen=sizeof(struct modal_dialogue),
  .opaque=0,
  .interactive=1,
  .del=_dialogue_del,
  .init=_dialogue_init,
  .focus=_dialogue_focus,
  .update=_dialogue_update,
  .render=_dialogue_render,
};

/* Reinitialize my texture, take measurements, etc.
 */
 
static int dialogue_build_texture(struct modal *modal) {
  egg_texture_del(MODAL->texid);
  const char *text=0;
  int textc=res_get_string(&text,MODAL->rid,MODAL->strix);
  MODAL->texid=font_render_to_texture(0,g.font,text,textc,FBW-10,FBH-10,0xffffffff);
  egg_texture_get_size(&MODAL->w,&MODAL->h,MODAL->texid);
  MODAL->x=(FBW>>1)-(MODAL->w>>1);
  MODAL->y=FBH-MODAL->h-10;
  return 0;
}

/* Spawn and prepare a modal.
 */

struct modal *modal_dialogue_begin(int rid,int strix) {
  struct modal *modal=modal_spawn(&modal_type_dialogue);
  if (!modal) return 0;
  MODAL->rid=rid;
  MODAL->strix=strix;
  dialogue_build_texture(modal);
  return modal;
}
