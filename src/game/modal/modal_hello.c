#include "game/wishbone.h"

struct modal_hello {
  struct modal hdr;
};

#define MODAL ((struct modal_hello*)modal)

/* Delete.
 */
 
static void _hello_del(struct modal *modal) {
}

/* Init.
 */
 
static int _hello_init(struct modal *modal) {
  return 0;
}

/* Focus.
 */
 
static void _hello_focus(struct modal *modal,int focus) {
}

/* Update.
 */
 
static void _hello_update(struct modal *modal,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) modal->defunct=1;
  }
}

/* Render.
 */
 
static void _hello_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x008000ff);
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
