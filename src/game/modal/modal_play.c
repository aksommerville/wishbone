#include "game/wishbone.h"

struct modal_play {
  struct modal hdr;
};

#define MODAL ((struct modal_play*)modal)

/* Delete.
 */
 
static void _play_del(struct modal *modal) {
}

/* Init.
 */
 
static int _play_init(struct modal *modal) {
  return 0;
}

/* Focus.
 */
 
static void _play_focus(struct modal *modal,int focus) {
}

/* Update.
 */
 
static void _play_update(struct modal *modal,double elapsed,int input,int pvinput) {
}

/* Render.
 */
 
static void _play_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
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
