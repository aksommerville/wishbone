/* sprite_npc.
 * Faces the hero horizontally and when touched, triggers dialogue and an optional "npcaction".
 */
 
#include "game/wishbone.h"

struct sprite_npc {
  struct sprite hdr;
  uint8_t strix;
  uint8_t npcaction;
  double blackout;
};

#define SPRITE ((struct sprite_npc*)sprite)

static int _npc_init(struct sprite *sprite) {
  SPRITE->strix=sprite->arg>>24;
  SPRITE->npcaction=sprite->arg>>16;
  return 0;
}

static void _npc_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->blackout>0.0) {
    SPRITE->blackout-=elapsed;
  }
  if (g.hero) {
    if (g.hero->x>sprite->x+0.4) sprite->xform=EGG_XFORM_XREV;
    else if (g.hero->x<sprite->x-0.4) sprite->xform=0;
  }
}

static void _npc_bump(struct sprite *sprite,struct sprite *bumper) {
  if (SPRITE->blackout>0.0) return;
  SPRITE->blackout=0.500;
  switch (SPRITE->npcaction) {
  
    case NS_npcaction_wishbone: {
        if (flag_get(NS_flag_wishbone)) {
          modal_dialogue_begin(1,7); // Come back if you need to.
        } else {
          SFX(treasure)
          flag_set(NS_flag_wishbone,1);
          g.item=NS_flag_wishbone;
          modal_dialogue_begin(1,6); // Take this.
        }
      } break;
      
    case NS_npcaction_heal: {
        if (g.hp>=g.maxhp) {
          modal_dialogue_begin(1,9); // Come back if you need to.
        } else {
          SFX(prize)
          g.hp=g.maxhp;
          modal_dialogue_begin(1,8); // You're healed!
        }
      } break;
      
    case NS_npcaction_sensei: {
        if (g.item==NS_flag_wishbone) {
          modal_dialogue_begin(1,SPRITE->strix);
        } else {
          modal_dialogue_begin(1,21);
        }
      } break;
    
    // This is not how victory will be triggered in real life, I'm just using it during development.
    case NS_npcaction_victory: {
        g.victory=1;
      } break;
      
    default: {
        if (SPRITE->strix) {
          modal_dialogue_begin(1,SPRITE->strix);
        }
      }
  }
}

const struct sprite_type sprite_type_npc={
  .name="npc",
  .objlen=sizeof(struct sprite_npc),
  .init=_npc_init,
  .update=_npc_update,
  .bump=_npc_bump,
};
