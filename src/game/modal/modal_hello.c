#include "game/wishbone.h"

#define LABEL_LIMIT 8

#define STRIX_TITLE 2
#define STRIX_CONTINUE 10
#define STRIX_NEW_GAME 11
#define STRIX_QUIT 12
#define STRIX_BY_AK 13
#define STRIX_WHEN 14
#define STRIX_FOR_JAM 15

struct modal_hello {
  struct modal hdr;
  struct label {
    int strix;
    int texid;
    int x,y,w,h;
    uint32_t rgba;
    int enable;
  } labelv[LABEL_LIMIT];
  int labelc;
  int labelp;
};

#define MODAL ((struct modal_hello*)modal)

/* Delete.
 */
 
static void _hello_del(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Add label.
 */
 
static struct label *hello_add_label(struct modal *modal,int strix,uint32_t rgba) {
  if (MODAL->labelc>=LABEL_LIMIT) return 0;
  struct label *label=MODAL->labelv+MODAL->labelc++;
  label->strix=strix;
  const char *text=0;
  int textc=res_get_string(&text,1,strix);
  label->texid=font_render_to_texture(0,g.font,text,textc,FBW,FBH,rgba);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=(FBW>>1)-(label->w>>1);
  return label;
}

/* Init.
 */
 
static int _hello_init(struct modal *modal) {
  struct label *label;
  
  int y=40;
  if (label=hello_add_label(modal,STRIX_TITLE,0xffffffff)) { label->y=y; y+=12; }
  y=150;
  if (label=hello_add_label(modal,STRIX_BY_AK,0x208060ff)) { label->y=y; y+=12; label->x=10; }
  if (label=hello_add_label(modal,STRIX_WHEN,0x208060ff)) { label->y=y; y+=12; label->x=10; }
  if (label=hello_add_label(modal,STRIX_FOR_JAM,0x208060ff)) { label->y=y; y+=12; label->x=10; }
  
  y=80;
  if (label=hello_add_label(modal,STRIX_CONTINUE,0xffffffff)) { label->y=y; y+=12; label->enable=(g.saved_gamec>0); }
  if (label=hello_add_label(modal,STRIX_NEW_GAME,0xffffffff)) { label->y=y; y+=12; label->enable=1; }
  if (label=hello_add_label(modal,STRIX_QUIT,0xffffffff)) { label->y=y; y+=12; label->enable=1; }
  
  while ((MODAL->labelp<MODAL->labelc)&&!MODAL->labelv[MODAL->labelp].enable) MODAL->labelp++;
  
  return 0;
}

/* Focus.
 */
 
static void _hello_focus(struct modal *modal,int focus) {
  if (focus) {
    egg_play_song(RID_song_behind_each_tapestry,0,1);
  }
}

/* Activate.
 */
 
static void hello_activate(struct modal *modal) {
  if ((MODAL->labelp<0)||(MODAL->labelp>=MODAL->labelc)) return;
  switch (MODAL->labelv[MODAL->labelp].strix) {
    case STRIX_CONTINUE: {
        modal->defunct=1;
        // This is safe, even if the saved game is empty or malformed:
        if (game_load(g.saved_game,g.saved_gamec)>=0) {
          modal_spawn(&modal_type_play);
        }
      } break;
    case STRIX_NEW_GAME: {
        modal->defunct=1;
        g.saved_gamec=0;
        if (game_reset()>=0) {
          modal_spawn(&modal_type_play);
        }
      } return;
    case STRIX_QUIT: egg_terminate(0); break;
  }
}

/* Move cursor.
 */
 
static void hello_move(struct modal *modal,int d) {
  SFX(uimotion)
  int panic=MODAL->labelc;
  while (panic-->0) {
    MODAL->labelp+=d;
    if (MODAL->labelp<0) MODAL->labelp=MODAL->labelc-1;
    else if (MODAL->labelp>=MODAL->labelc) MODAL->labelp=0;
    if (MODAL->labelv[MODAL->labelp].enable) return;
  }
}

/* Update.
 */
 
static void _hello_update(struct modal *modal,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) hello_activate(modal);
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) hello_move(modal,-1);
    if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) hello_move(modal,1);
  }
}

/* Render.
 */
 
static void _hello_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x104030ff);
  struct label *label=MODAL->labelv;
  int i=0;
  for (;i<MODAL->labelc;i++,label++) {
    if (i==MODAL->labelp) graf_fill_rect(&g.graf,label->x-2,label->y-2,label->w+4,label->h+3,0x000000ff);
    int gray_out=(label->strix==STRIX_CONTINUE)&&!label->enable;
    if (gray_out) graf_set_alpha(&g.graf,0x40);
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
    if (gray_out) graf_set_alpha(&g.graf,0xff);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_hello={
  .name="hello",
  .objlen=sizeof(struct modal_hello),
  .opaque=1,
  .interactive=1,
  .del=_hello_del,
  .init=_hello_init,
  .focus=_hello_focus,
  .update=_hello_update,
  .render=_hello_render,
};
