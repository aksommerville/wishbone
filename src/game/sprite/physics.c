#include "game/wishbone.h"

/* Move sprite.
 */

int sprite_move(struct sprite *sprite,double dx,double dy) {
  //TODO physics
  sprite->x+=dx;
  sprite->y+=dy;
  return 1;
}
