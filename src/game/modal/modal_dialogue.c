#include "game/wishbone.h"

struct modal_dialogue {
  struct modal hdr;
};

#define MODAL ((struct modal_dialogue*)modal)

/* Delete.
 */
 
static void _dialogue_del(struct modal *modal) {
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
  graf_fill_rect(&g.graf,10,FBH>>1,FBW-20,(FBH>>1)-10,0x000000ff);
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
