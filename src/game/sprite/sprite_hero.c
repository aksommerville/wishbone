#include "game/wishbone.h"

#define WALK_SPEED 6.000
#define CORRECTION_SPEED 1.000
#define BOOMERANG_DOUBLE_CLICK_TIME 0.250 /* 0.200..0.250 feels about right */
#define WAND_HOLD_TIME 0.500
#define VAULT_WARMUP_TIME 0.250
#define VAULT_TIME 0.250
#define VAULT_SPEED 10.000
#define SWING_MINIMUM_TIME 0.250
#define HERO_EVENT_LIMIT 16

#define ACTION_STATE_NONE 0
#define ACTION_STATE_STAB 1
#define ACTION_STATE_SWING 2
#define ACTION_STATE_VAULT 3
#define ACTION_STATE_BOOMERANG 4
#define ACTION_STATE_WAND 5
#define ACTION_STATE_SLINGSHOT 6
#define ACTION_STATE_LOCKPICK 7

struct sprite_hero {
  struct sprite hdr;
  int input;
  int qx,qy;
  uint8_t facedir; // 0x40,0x10,0x08,0x02
  double animclock;
  int animframe;
  double dumbclock; // Counts up always.
  struct hero_event {
    int press,release;
    double time;
  } eventv[HERO_EVENT_LIMIT];
  int eventp;
  int fresh_event; // Nonzero for one update after a new event is pushed.
  int action_state;
  double actionclock;
  int vaultdx,vaultdy;
};

static struct hero_event empty_event={0};

#define SPRITE ((struct sprite_hero*)sprite)

/* Delete.
 */
 
static void _hero_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  SPRITE->qx=-1;
  SPRITE->qy=-1;
  SPRITE->facedir=0x02;
  return 0;
}

/* Get buttons pressed and released at some point in our recent past.
 * (dp) in (-HERO_EVENT_LIMIT..-1) normally, and -1 is the most recent change.
 * You can also use (0..HERO_EVENT_LIMIT-1) to read from the beginning of history forward.
 * (not sure we'd ever want to do that).
 */

static struct hero_event *hero_get_event(struct sprite *sprite,int dp) {
  if ((dp<-HERO_EVENT_LIMIT)||(dp>=HERO_EVENT_LIMIT)) return &empty_event;
  int p=SPRITE->eventp+dp;
  if (p<0) p+=HERO_EVENT_LIMIT;
  else if (p>=HERO_EVENT_LIMIT) p-=HERO_EVENT_LIMIT;
  return SPRITE->eventv+p;
}

/* If current context permits a lockpick operation, start it and return nonzero.
 */
 
static int hero_check_lockpick(struct sprite *sprite) {
  int x=SPRITE->qx,y=SPRITE->qy;
  switch (SPRITE->facedir) {
    case 0x40: y--; break;
    case 0x10: x--; break;
    case 0x08: x++; break;
    case 0x02: y++; break;
  }
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  //TODO Check poi for an unpicked lock at (x,y). If one exists, enter the lockpick modal and return nonzero.
  return 0;
}

/* If history indicates a boomerang, start it and return nonzero.
 */
 
static int hero_check_boomerang(struct sprite *sprite) {
  // Must be exactly [press,release,press] of SOUTH, within a time threshold.
  const struct hero_event *press1=hero_get_event(sprite,-3);
  const struct hero_event *release=hero_get_event(sprite,-2);
  const struct hero_event *press2=hero_get_event(sprite,-1);
  if ((press1->press!=EGG_BTN_SOUTH)||press1->release) return 0;
  if ((release->release!=EGG_BTN_SOUTH)||release->press) return 0;
  if ((press2->press!=EGG_BTN_SOUTH)||press2->release) return 0;
  double duration=press2->time-press1->time;
  if (duration>BOOMERANG_DOUBLE_CLICK_TIME) return 0;
  // OK, we're doing it.
  fprintf(stderr,"BOOMERANG\n");//TODO
  SPRITE->action_state=ACTION_STATE_BOOMERANG;
  return 1;
}

/* With STAB or SWING ongoing, check whether to enter WAND mode.
 */
 
static int hero_check_wand(struct sprite *sprite) {
  // Must be no intervening events (even inert-feeling stuff like releasing the dpad).
  const struct hero_event *event=hero_get_event(sprite,-1);
  if ((event->press!=EGG_BTN_SOUTH)||event->release) return 0;
  double duration=SPRITE->dumbclock-event->time;
  if (duration<WAND_HOLD_TIME) return 0;
  // OK, we're doing it.
  fprintf(stderr,"WAND\n");
  SPRITE->action_state=ACTION_STATE_WAND;
  return 1;
}

/* If VAULT should begin, do it and return nonzero.
 */
 
static int hero_check_vault(struct sprite *sprite) {
  // Vault can only be done in cardinal directions, and must be exactly [dpad,south] with a certain minimum interval between.
  const struct hero_event *dpad=hero_get_event(sprite,-2);
  const struct hero_event *south=hero_get_event(sprite,-1);
  if ((south->press!=EGG_BTN_SOUTH)||south->release) return 0;
  if (dpad->release) return 0;
  double duration=south->time-dpad->time;
  if (duration<VAULT_WARMUP_TIME) return 0;
  int dx=0,dy=0;
  switch (dpad->press) {
    case EGG_BTN_LEFT: dx=-1; break;
    case EGG_BTN_RIGHT: dx=1; break;
    case EGG_BTN_UP: dy=-1; break;
    case EGG_BTN_DOWN: dy=1; break;
    default: return 0;
  }
  // OK, we're doing it.
  fprintf(stderr,"VAULT %+d,%+d\n",dx,dy);
  SPRITE->action_state=ACTION_STATE_VAULT;
  SPRITE->vaultdx=dx;
  SPRITE->vaultdy=dy;
  SPRITE->actionclock=VAULT_TIME;
  return 1;
}

/* Update during VAULT.
 */
 
static void hero_update_vault(struct sprite *sprite,double elapsed) {
  uint32_t pvphymask=sprite->phymask;
  sprite->phymask&=~(1<<NS_physics_water);
  if (SPRITE->vaultdx) sprite_move(sprite,SPRITE->vaultdx*VAULT_SPEED*elapsed,0.0);
  else sprite_move(sprite,0.0,SPRITE->vaultdy*VAULT_SPEED*elapsed);
  sprite->phymask=pvphymask;
  if ((SPRITE->actionclock-=elapsed)<=0.0) {
    SPRITE->action_state=0;
    //TODO check whether we're over water and either correct position or drown
  }
}

/* Update during WAND.
 */
 
static void hero_append_spell(struct sprite *sprite,int btnid) {
  fprintf(stderr,"%s 0x%04x\n",__func__,btnid);//TODO
}
 
static void hero_update_wand(struct sprite *sprite,double elapsed) {
  if (SPRITE->fresh_event) {
    const struct hero_event *event=hero_get_event(sprite,-1);
    if (event->release&EGG_BTN_SOUTH) {
      fprintf(stderr,"%s:%d: Commit spell or enter SLINGSHOT\n",__FILE__,__LINE__);//TODO
      SPRITE->action_state=0;
      return;
    }
    if (event->press&EGG_BTN_LEFT) hero_append_spell(sprite,EGG_BTN_LEFT);
    if (event->press&EGG_BTN_RIGHT) hero_append_spell(sprite,EGG_BTN_RIGHT);
    if (event->press&EGG_BTN_UP) hero_append_spell(sprite,EGG_BTN_UP);
    if (event->press&EGG_BTN_DOWN) hero_append_spell(sprite,EGG_BTN_DOWN);
  }
}

/* Update during SLINGSHOT.
 */
 
static void hero_update_slingshot(struct sprite *sprite,double elapsed) {
  //TODO slingshot
  SPRITE->action_state=0;
}

/* Check SWING, begin if warranted and return nonzero.
 */
 
static int is_single_dpad(int input) {
  switch (input) {
    case EGG_BTN_LEFT:
    case EGG_BTN_RIGHT:
    case EGG_BTN_UP:
    case EGG_BTN_DOWN:
      return 1;
  }
  return 0;
}
 
static int hero_check_swing(struct sprite *sprite) {
  // Most recent pressed must be [axisA,axisB,south], within some minimum interval. Intervening releases are ok.
  int dp=-1,got_swing=0,axisb=0,axisa=0;
  double starttime;
  for (;dp>=-HERO_EVENT_LIMIT;dp--) {
    const struct hero_event *event=hero_get_event(sprite,dp);
    if (!event->press) continue;
    if (!got_swing) {
      if (event->press!=EGG_BTN_SOUTH) return 0;
      got_swing=1;
    } else if (!axisb) {
      if (!is_single_dpad(axisb=(event->press&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)))) return 0;
    } else {
      if (!is_single_dpad(axisa=(event->press&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)))) return 0;
      starttime=event->time;
      break;
    }
  }
  if (!axisa) return 0;
  if (axisa&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    if (axisb&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) return 0; // Nope, both horizontal.
  } else {
    if (axisb&(EGG_BTN_UP|EGG_BTN_DOWN)) return 0; // Nope, both vertical.
  }
  double duration=SPRITE->dumbclock-starttime;
  if (duration>SWING_MINIMUM_TIME) return 0;
  // OK, we're doing it.
  fprintf(stderr,"SWING from 0x%04x to 0x%04x\n",axisa,axisb);
  //TODO Whack things.
  SPRITE->action_state=ACTION_STATE_SWING;
  return 1;
}

/* Begin STAB.
 */
 
static void hero_begin_stab(struct sprite *sprite) {
  fprintf(stderr,"STAB\n");
  //TODO Whack things.
  SPRITE->action_state=ACTION_STATE_STAB;
}

/* Check for all usage of the wishbone.
 */
 
static void hero_update_wishbone(struct sprite *sprite,double elapsed) {
  
  // If an action is in progress, that takes precedence over everything else.
  switch (SPRITE->action_state) {
    // VAULT,WAND,SLINGSHOT: Complex, call out.
    case ACTION_STATE_VAULT: hero_update_vault(sprite,elapsed); return;
    case ACTION_STATE_WAND: hero_update_wand(sprite,elapsed); return;
    case ACTION_STATE_SLINGSHOT: hero_update_slingshot(sprite,elapsed); return;
    
    // STAB,SWING: End action on release, or begin WAND if held long enough.
    // A held stroke is noop, it doesn't continue hurting foes or anything. Only reason to hold is to enter the WAND state.
    case ACTION_STATE_STAB:
    case ACTION_STATE_SWING: {
        if (!(SPRITE->input&EGG_BTN_SOUTH)) {
          SPRITE->action_state=0;
        } else {
          hero_check_wand(sprite);
        }
      } return;
      
    // BOOMERANG,LOCKPICK: No activity, just wait for the hold to release.
    case ACTION_STATE_BOOMERANG:
    case ACTION_STATE_LOCKPICK: {
        if (!(SPRITE->input&EGG_BTN_SOUTH)) {
          SPRITE->action_state=0;
        }
      } return;
  }
  
  // Nothing in progress. Has something started just now?
  if (SPRITE->fresh_event) {
    const struct hero_event *event=hero_get_event(sprite,-1);
    if (event->press&EGG_BTN_SOUTH) {
      // The start of a stroke is one of: stab,swing,vault,boomerang,lockpick
      if (hero_check_lockpick(sprite)) return;
      if (hero_check_boomerang(sprite)) return;
      if (hero_check_vault(sprite)) return;
      if (hero_check_swing(sprite)) return;
      hero_begin_stab(sprite);
    }
  }
}

/* Is walking suppressed, in the given state?
 */
 
static int action_state_prevents_walking(int state) {
  switch (state) {
    case ACTION_STATE_VAULT:
    case ACTION_STATE_WAND:
    case ACTION_STATE_SLINGSHOT:
      return 1;
  }
  return 0;
}

/* Walking and such.
 */
 
static void hero_update_walk(struct sprite *sprite,double elapsed) {

  if (action_state_prevents_walking(SPRITE->action_state)) return;

  int walkdx=0,walkdy=0,xok=0,yok=0;
  switch (SPRITE->input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: walkdx=-1; xok=sprite_move(sprite,-WALK_SPEED*elapsed,0.0); break;
    case EGG_BTN_RIGHT: walkdx=1; xok=sprite_move(sprite,WALK_SPEED*elapsed,0.0); break;
  }
  switch (SPRITE->input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: walkdy=-1; yok=sprite_move(sprite,0.0,-WALK_SPEED*elapsed); break;
    case EGG_BTN_DOWN: walkdy=1; yok=sprite_move(sprite,0.0,WALK_SPEED*elapsed); break;
  }
  
  // If movement was rejected and was on just one axis, cheat the other axis toward a half-meter interval.
  // Prefer one-meter intervals over halves.
  // To make things a bit more confusing, remember that the whole-meter quantization has our center at 0.5.
  const double half_meter_radius=0.100; // 0.250 would balance halves and wholes exactly; 0.000 would use wholes only.
  if (walkdx&&!walkdy&&!xok) {
    double slop=sprite->y-(int)sprite->y;
    double targety=sprite->y;
    if (slop<half_meter_radius) targety=(int)sprite->y;
    else if (slop>1.0-half_meter_radius) targety=(int)sprite->y+1.0;
    else targety=(int)sprite->y+0.5;
    if (sprite->y<targety) {
      sprite_move(sprite,0.0,CORRECTION_SPEED*elapsed);
      if (sprite->y>targety) sprite->y=targety;
    } else if (sprite->y>targety) {
      sprite_move(sprite,0.0,-CORRECTION_SPEED*elapsed);
      if (sprite->y<targety) sprite->y=targety;
    }
  } else if (walkdy&&!walkdx&&!yok) {
    double slop=sprite->x-(int)sprite->x;
    double targetx=sprite->x;
    if (slop<half_meter_radius) targetx=(int)sprite->x;
    else if (slop>1.0-half_meter_radius) targetx=(int)sprite->x+1.0;
    else targetx=(int)sprite->x+0.5;
    if (sprite->x<targetx) {
      sprite_move(sprite,CORRECTION_SPEED*elapsed,0.0);
      if (sprite->x>targetx) sprite->x=targetx;
    } else if (sprite->x>targetx) {
      sprite_move(sprite,-CORRECTION_SPEED*elapsed,0.0);
      if (sprite->x<targetx) sprite->x=targetx;
    }
  }
}

/* Animation.
 */
 
static void hero_update_animation(struct sprite *sprite,double elapsed) {
  if (SPRITE->input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
}

/* Quantize my position and if it's changed, trigger POI.
 */
 
static void hero_check_quantized_position(struct sprite *sprite) {
  int x=(int)sprite->x; if (sprite->x<0.0) x=-1;
  int y=(int)sprite->y; if (sprite->y<0.0) y=-1;
  if (x>=NS_sys_mapw) x=-1;
  if (y>=NS_sys_maph) y=-1;
  if ((x==SPRITE->qx)&&(y==SPRITE->qy)) return;
  qpos_release(SPRITE->qx,SPRITE->qy);
  SPRITE->qx=x;
  SPRITE->qy=y;
  qpos_press(x,y);
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  SPRITE->dumbclock+=elapsed;
  if (g.item==NS_flag_wishbone) {
    hero_update_wishbone(sprite,elapsed);
  }
  hero_update_walk(sprite,elapsed);
  hero_update_animation(sprite,elapsed);
  hero_check_quantized_position(sprite);
  SPRITE->fresh_event=0;
}

/* Render.
 */
 
static void _hero_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  uint8_t tileid=sprite->tileid;
  uint8_t xform=0;
  switch (SPRITE->facedir) {
    case 0x02: break; // Down, the natural direction.
    case 0x40: tileid+=1; break; // Up.
    case 0x10: tileid+=2; break; // Left, natural orientation of the tile.
    case 0x08: tileid+=2; xform=EGG_XFORM_XREV; break; // Right. Left but flopped.
  }
  switch (SPRITE->animframe) {
    case 1: tileid+=0x10; break;
    case 3: tileid+=0x20; break;
  }
  graf_tile(&g.graf,dstx,dsty,tileid,xform);
}

/* Modal gains or loses focus.
 */
 
static void _hero_focus(struct sprite *sprite,int focus) {
  if (!focus) {
    SPRITE->input=0;
    SPRITE->animclock=0;
    SPRITE->animframe=0;
  }
}

/* Type.
 */
 
const struct sprite_type sprite_type_hero={
  .name="hero",
  .objlen=sizeof(struct sprite_hero),
  .del=_hero_del,
  .init=_hero_init,
  .update=_hero_update,
  .render=_hero_render,
  .focus=_hero_focus,
};

/* Input changed.
 */
 
void sprite_hero_input(struct sprite *sprite,int input,int pvinput) {
  SPRITE->input=input;
  
  // All changes to input state get recorded for examination during update.
  SPRITE->fresh_event=1;
  struct hero_event *event=SPRITE->eventv+SPRITE->eventp++;
  if (SPRITE->eventp>=HERO_EVENT_LIMIT) SPRITE->eventp=0;
  event->press=input&~pvinput;
  event->release=pvinput&~input;
  event->time=SPRITE->dumbclock;
  
  // When a dpad button goes ON, facedir changes immediately.
  if (event->press&EGG_BTN_LEFT) SPRITE->facedir=0x10;
  if (event->press&EGG_BTN_RIGHT) SPRITE->facedir=0x08;
  if (event->press&EGG_BTN_UP) SPRITE->facedir=0x40;
  if (event->press&EGG_BTN_DOWN) SPRITE->facedir=0x02;
  
  // When a dpad button goes OFF, we're facing that way, and one other dpad button is held, face the one still held.
  if (
    ((event->release&EGG_BTN_LEFT)&&(SPRITE->facedir==0x10))||
    ((event->release&EGG_BTN_RIGHT)&&(SPRITE->facedir==0x08))||
    ((event->release&EGG_BTN_UP)&&(SPRITE->facedir==0x40))||
    ((event->release&EGG_BTN_DOWN)&&(SPRITE->facedir==0x02))
  ) {
    if (input&EGG_BTN_LEFT) SPRITE->facedir=0x10;
    else if (input&EGG_BTN_RIGHT) SPRITE->facedir=0x08;
    else if (input&EGG_BTN_UP) SPRITE->facedir=0x40;
    else if (input&EGG_BTN_DOWN) SPRITE->facedir=0x02;
  }
}
