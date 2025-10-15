/* sprite_treasure.c
 * Step on me to pick up (by setting a flag).
 */
 
#include "game/wishbone.h"

// TTL only applies to hearts.
#define TTL 5.0
#define BLACKOUT_TIME 0.500 /* Brief initial interval when we can't be boomeranged. Otherwise you can kill a monster and fetch its heart in the same stroke. */

struct sprite_treasure {
  struct sprite hdr;
  uint8_t flagid;
  uint8_t prizeid;
  uint8_t forgottenid;
  uint8_t animate;
  uint8_t tileid0;
  double animclock;
  int animframe;
  double ttl;
};

#define SPRITE ((struct sprite_treasure*)sprite)

static int _treasure_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->flagid=sprite->arg>>24;
  SPRITE->prizeid=sprite->arg>>16;
  SPRITE->forgottenid=sprite->arg>>8;
  if (SPRITE->prizeid==NS_prize_heart) SPRITE->ttl=TTL;
  else SPRITE->ttl=999.0;
  if (flag_get(SPRITE->flagid)) return -1;
  
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->res,sprite->resc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_animate: SPRITE->animate=1; break;
      }
    }
  }
  
  return 0;
}

static void treasure_get_got(struct sprite *sprite) {
  if (SPRITE->forgottenid) forgotten_remove(SPRITE->forgottenid);
  SFX(prize)
  sprite->defunct=1;
  flag_set(SPRITE->flagid,1);
  switch (SPRITE->prizeid) {
    case NS_prize_heart: if (++(g.hp)>=g.maxhp) g.hp=g.maxhp; break;
    case NS_prize_container: g.maxhp++; g.hp++; g.saved_game_dirty=1; break;
    case NS_prize_wishbone: g.item=NS_flag_wishbone; flag_set(NS_flag_wishbone,1); break;
  }
}

static void _treasure_update(struct sprite *sprite,double elapsed) {
  switch (SPRITE->animate) {
    case 1: {
        if ((SPRITE->animclock-=elapsed)<=0.0) {
          SPRITE->animclock+=0.250;
          if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
          sprite->tileid=SPRITE->tileid0+SPRITE->animframe;
        }
      } break;
  }
  if (g.hero) {
    const double radius=0.5;
    double dx=g.hero->x-sprite->x;
    if ((dx>-radius)&&(dx<radius)) {
      double dy=g.hero->y-sprite->y;
      if ((dy>-radius)&&(dy<radius)) {
        treasure_get_got(sprite);
      }
    }
  }
  if ((SPRITE->ttl-=elapsed)<=0.0) {
    sprite->defunct=1;
  }
}

const struct sprite_type sprite_type_treasure={
  .name="treasure",
  .objlen=sizeof(struct sprite_treasure),
  .init=_treasure_init,
  .update=_treasure_update,
};

int sprite_treasure_is_pickable(struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_treasure)) return 0;
  if (SPRITE->ttl>TTL) return 1;
  if (SPRITE->ttl>TTL-BLACKOUT_TIME) return 0;
  SPRITE->ttl=999.0;
  return 1;
}
