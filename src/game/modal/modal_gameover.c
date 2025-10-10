#include "game/wishbone.h"

struct modal_gameover {
  struct modal hdr;
};

#define MODAL ((struct modal_gameover*)modal)

/* Delete.
 */
 
static void _gameover_del(struct modal *modal) {
}

/* Init.
 */
 
static int _gameover_init(struct modal *modal) {
  return 0;
}

/* Focus.
 */
 
static void _gameover_focus(struct modal *modal,int focus) {
}

/* Update.
 */
 
static void _gameover_update(struct modal *modal,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) modal->defunct=1;
  }
}

/* Render.
 */
 
static void _gameover_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x800000ff);
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
