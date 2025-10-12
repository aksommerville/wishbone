#include "game/wishbone.h"

#define LABEL_LIMIT 1

struct modal_gameover {
  struct modal hdr;
  struct label {
    int texid,x,y,w,h;
  } labelv[LABEL_LIMIT];
  int labelc;
};

#define MODAL ((struct modal_gameover*)modal)

/* Delete.
 */
 
static void _gameover_del(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Add label.
 * Initially centered, because there might only be one.
 */
 
static struct label *gameover_add_label(struct modal *modal,int strix) {
  if (MODAL->labelc>=LABEL_LIMIT) return 0;
  struct label *label=MODAL->labelv+MODAL->labelc++;
  const char *text=0;
  int textc=res_get_string(&text,1,strix);
  label->texid=font_render_to_texture(0,g.font,text,textc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=(FBW>>1)-(label->w>>1);
  label->y=(FBH>>1)-(label->h>>1);
  return label;
}

/* Init.
 */
 
static int _gameover_init(struct modal *modal) {
  gameover_add_label(modal,5);
  return 0;
}

/* Focus.
 */
 
static void _gameover_focus(struct modal *modal,int focus) {
  if (focus) {
    egg_play_song(0,0,1);//TODO gameover music
  }
}

/* Update.
 */
 
static void _gameover_update(struct modal *modal,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
      modal->defunct=1;
      modal_spawn(&modal_type_hello);
    }
  }
}

/* Render.
 */
 
static void _gameover_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_gameover={
  .name="gameover",
  .objlen=sizeof(struct modal_gameover),
  .opaque=1,
  .interactive=1,
  .del=_gameover_del,
  .init=_gameover_init,
  .focus=_gameover_focus,
  .update=_gameover_update,
  .render=_gameover_render,
};
