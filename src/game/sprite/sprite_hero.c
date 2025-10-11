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
#define SPELL_LIMIT 16
#define DAMAGE_TIME 0.500
#define SLINGSHOT_ANGVEL 3.000 /* radian/sec */
#define SLINGSHOT_MAG_ADJUST_RATE 40.0 /* pixel/sec */
#define SLINGSHOT_MAG_MAX 30.0
#define SLINGSHOT_VEL_MAX 20.0
#define SLINGSHOT_VEL_MIN 10.0
#define SLINGSHOT_FLIGHT_TIME 0.333

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
  double vaultx,vaulty; // Return here if we drown.
  int stabdir,swingdir; // btnid (dpad only, singles)
  int spell[SPELL_LIMIT]; // btnid (dpad only, singles) or zero
  int spellp;
  double rejectclock; // WAND
  double damageclock;
  double slingt; // radians
  double slingm; // visual displacement in pixels, and related to output force
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

/* Cause an injury.
 * No sound effect, since different hazards might sound different.
 */
 
static void hero_injure(struct sprite *sprite) {
  if (SPRITE->damageclock>0.0) return; // Temporarily invincible while reporting the previous damage.
  g.hp--;
  if (g.hp<=0) {
    g.hp=0;
    sprite->defunct=1;
    //TODO Soulballs.
    //TODO Set a global timer or something, to force reset.
    return;
  }
  SPRITE->damageclock=DAMAGE_TIME;
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
  struct sprite *boomerang=sprite_spawn_res(RID_sprite_boomerang,sprite->x,sprite->y,SPRITE->facedir);
  if (boomerang) {
    SFX(boomerang)
  }
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
  SPRITE->action_state=ACTION_STATE_WAND;
  memset(SPRITE->spell,0,sizeof(SPRITE->spell));
  return 1;
}

/* At the end of a vault or slingshot, we begin tracking collisions against water again.
 * Caller must have arranged for (vaultx,vaulty) to be a legal position, where the jump began.
 * If we're not touching water right now, great, nothing happens.
 * If we have under half a meter's clearance to land, do that. (ie our center is on a legal cell).
 * But if we're right in the water, create a splash, return to the safe position, and get hurt.
 */
 
static void hero_force_onscreen(struct sprite *sprite) {
  if (sprite->x<0.5) sprite->x=0.5;
  else if (sprite->x>NS_sys_mapw-0.5) sprite->x=NS_sys_mapw-0.5;
  if (sprite->y<0.5) sprite->y=0.5;
  else if (sprite->y>NS_sys_maph-0.5) sprite->y=NS_sys_maph-0.5;
}
 
static void hero_return_to_earth(struct sprite *sprite) {

  // First, clamp hard to the screen's boundaries. Don't want any offscreen monkey business.
  // Flight director should have been doing this during the flight, but let's be certain.
  hero_force_onscreen(sprite);
  
  // Get the tile we're focussed on. It's probably not (qx,qy) yet but soon will be.
  int x=(int)sprite->x;
  int y=(int)sprite->y;
  uint8_t tileid=g.map->v[y*NS_sys_mapw+x];
  uint8_t physics=g.physics[tileid];
  
  // If it's water, we're doomed.
  if (physics==NS_physics_water) {
   _splash_:;
    //TODO splash sprite
    SFX(splash)
    sprite->x=SPRITE->vaultx;
    sprite->y=SPRITE->vaulty;
    hero_injure(sprite);
    return;
  }
  
  // If the current position is tenable, we're done.
  if (sprite_test_position(sprite)) return;
  
  /* Consider three possible positions, aligning ourselves exactly to the focus cell on each axis.
   * First to produce a valid position wins.
   * If none does (eg there's a solid sprite here), pretend we're on water.
   * That shouldn't happen, since solid sprites should have stopped us in flight.
   */
  double ox=sprite->x;
  double oy=sprite->y;
  double nx=x+0.5;
  double ny=y+0.5;
  sprite->x=ox; sprite->y=ny; if (sprite_test_position(sprite)) return;
  sprite->x=nx; sprite->y=oy; if (sprite_test_position(sprite)) return;
  sprite->x=nx; sprite->y=oy; if (sprite_test_position(sprite)) return;
  goto _splash_;
}

/* If VAULT should begin, do it and return nonzero.
 */
 
static int hero_check_vault(struct sprite *sprite) {
  // Vault can only be done in cardinal directions, and must be exactly [dpad,south] with a certain maximum interval between.
  const struct hero_event *dpad=hero_get_event(sprite,-2);
  const struct hero_event *south=hero_get_event(sprite,-1);
  if ((south->press!=EGG_BTN_SOUTH)||south->release) return 0;
  if (dpad->release) return 0;
  double duration=south->time-dpad->time;
  if (duration>VAULT_WARMUP_TIME) return 0;
  int dx=0,dy=0;
  switch (dpad->press) {
    case EGG_BTN_LEFT: dx=-1; break;
    case EGG_BTN_RIGHT: dx=1; break;
    case EGG_BTN_UP: dy=-1; break;
    case EGG_BTN_DOWN: dy=1; break;
    default: return 0;
  }
  // OK, we're doing it.
  SFX(vault)
  SPRITE->action_state=ACTION_STATE_VAULT;
  SPRITE->vaultdx=dx;
  SPRITE->vaultdy=dy;
  SPRITE->actionclock=VAULT_TIME;
  SPRITE->vaultx=sprite->x;
  SPRITE->vaulty=sprite->y;
  return 1;
}

/* Update during VAULT.
 */
 
static void hero_update_vault(struct sprite *sprite,double elapsed) {
  uint32_t pvphymask=sprite->phymask;
  sprite->phymask&=~(1<<NS_physics_water);
  if (SPRITE->vaultdx) sprite_move(sprite,SPRITE->vaultdx*VAULT_SPEED*elapsed,0.0);
  else sprite_move(sprite,0.0,SPRITE->vaultdy*VAULT_SPEED*elapsed);
  hero_force_onscreen(sprite);
  sprite->phymask=pvphymask;
  if ((SPRITE->actionclock-=elapsed)<=0.0) {
    SPRITE->action_state=0;
    hero_return_to_earth(sprite);
  }
}

/* Update during WAND.
 */
 
static void hero_append_spell(struct sprite *sprite,int btnid) {
  SPRITE->spell[SPRITE->spellp]=btnid;
  if (++(SPRITE->spellp)>=SPELL_LIMIT) SPRITE->spellp=0;
}
 
static void hero_update_wand(struct sprite *sprite,double elapsed) {

  // After a misspelling, Dot shakes her head for a sec.
  if (SPRITE->rejectclock>0.0) {
    if ((SPRITE->rejectclock-=elapsed)<=0.0) {
      SPRITE->action_state=0;
      SPRITE->facedir=0x02;
    }
    return;
  }

  if (SPRITE->fresh_event) {
    const struct hero_event *event=hero_get_event(sprite,-1);
    
    // On releasing SOUTH, the magic happens.
    if (event->release&EGG_BTN_SOUTH) {
      int spellp=SPRITE->spellp,spellc=0;
      while (spellc<SPELL_LIMIT) {
        int nextp=spellp-1;
        if (nextp<0) nextp+=SPELL_LIMIT;
        if (!SPRITE->spell[nextp]) break;
        spellp=nextp;
        spellc++;
      }
      if (!spellc) {
        SPRITE->action_state=ACTION_STATE_SLINGSHOT;
        SPRITE->slingt=-M_PI/2.0;
        SPRITE->slingm=0.0;
        SPRITE->vaultx=sprite->x;
        SPRITE->vaulty=sprite->y;
        SPRITE->actionclock=0.0;
      } else {
        char spell[SPELL_LIMIT];
        int i=0; for (;i<spellc;i++,spellp++) {
          if (spellp>=SPELL_LIMIT) spellp=0;
          switch (SPRITE->spell[spellp]) {
            case EGG_BTN_LEFT: spell[i]='L'; break;
            case EGG_BTN_RIGHT: spell[i]='R'; break;
            case EGG_BTN_UP: spell[i]='U'; break;
            case EGG_BTN_DOWN: spell[i]='D'; break;
            default: spell[i]='?';
          }
        }
        int spellid=spell_eval(spell,spellc);
        if (spellid) {
          spell_cast(spellid);
        } else {
          SPRITE->rejectclock=1.000;
          return;
        }
        SPRITE->action_state=0;
        SPRITE->facedir=0x02; // Will have been changing during the cast. She faces south while casting, so makes sense to stay there after.
      }
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
  
  // In flight?
  if (SPRITE->actionclock>0.0) {
    if ((SPRITE->actionclock-=elapsed)<=0.0) {
      SPRITE->action_state=0;
      hero_return_to_earth(sprite);
    } else {
      uint32_t pvphymask=sprite->phymask;
      sprite->phymask&=~(1<<NS_physics_water);
      sprite_move(sprite,SPRITE->vaultdx*elapsed,0.0);
      sprite_move(sprite,0.0,SPRITE->vaultdy*elapsed);
      sprite->phymask=pvphymask;
      hero_force_onscreen(sprite);
    }
    return;
  }
  
  if (SPRITE->fresh_event) {
    const struct hero_event *event=hero_get_event(sprite,-1);
    if (event->press&EGG_BTN_SOUTH) {
      if (SPRITE->slingm<=0.0) {
        SPRITE->action_state=0;
      } else {
        SPRITE->actionclock=SLINGSHOT_FLIGHT_TIME;
        double velocity=SLINGSHOT_VEL_MIN+(SPRITE->slingm*(SLINGSHOT_VEL_MAX-SLINGSHOT_VEL_MIN))/SLINGSHOT_MAG_MAX;
        SPRITE->vaultdx=+cos(-SPRITE->slingt)*velocity;
        SPRITE->vaultdy=-sin(-SPRITE->slingt)*velocity;
      }
    }
  }
  switch (SPRITE->input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    // LEFT is clockwise and RIGHT deasil, which is against convention, but feels right because we start with the bullet at the bottom.
    case EGG_BTN_LEFT: SPRITE->slingt+=SLINGSHOT_ANGVEL*elapsed; break;
    case EGG_BTN_RIGHT: SPRITE->slingt-=SLINGSHOT_ANGVEL*elapsed; break;
  }
  switch (SPRITE->input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: {
        if ((SPRITE->slingm-=SLINGSHOT_MAG_ADJUST_RATE*elapsed)<=0.0) {
          SPRITE->slingm=0.0;
        }
      } break;
    case EGG_BTN_DOWN: {
        if ((SPRITE->slingm+=SLINGSHOT_MAG_ADJUST_RATE*elapsed)>SLINGSHOT_MAG_MAX) {
          SPRITE->slingm=SLINGSHOT_MAG_MAX;
        }
      } break;
  }
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
  SFX(swing)
  SPRITE->stabdir=axisb;
  SPRITE->swingdir=axisa;
  //TODO Whack things.
  SPRITE->action_state=ACTION_STATE_SWING;
  SPRITE->actionclock=0.0;
  return 1;
}

/* Begin STAB.
 */
 
static void hero_begin_stab(struct sprite *sprite) {
  fprintf(stderr,"STAB facedir=0x%02x\n",SPRITE->facedir);
  switch (SPRITE->facedir) {
    case 0x40: SPRITE->stabdir=EGG_BTN_UP; break;
    case 0x10: SPRITE->stabdir=EGG_BTN_LEFT; break;
    case 0x08: SPRITE->stabdir=EGG_BTN_RIGHT; break;
    default: SPRITE->stabdir=EGG_BTN_DOWN; break;
  }
  SFX(stab)
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
    // Also tick (actionclock); SWING uses it for the swash animation.
    case ACTION_STATE_STAB:
    case ACTION_STATE_SWING: {
        SPRITE->actionclock+=elapsed;
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
  
  // Can't start any new wishbone action while a boomerang is in flight. Diegetically speaking, we don't really "have" a boomerang at that point.
  if (sprite_boomerang_exists()) return;
  
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
    case ACTION_STATE_STAB:
    case ACTION_STATE_SWING:
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
  
  // During certain operations, we're effectively not touching ground at all. That's expressible.
  switch (SPRITE->action_state) {
    case ACTION_STATE_VAULT:
    case ACTION_STATE_SLINGSHOT:
      x=y=-1;
      break;
  }
  
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
  if (SPRITE->damageclock>0.0) {
    SPRITE->damageclock-=elapsed;
  }
  if (g.item==NS_flag_wishbone) {
    hero_update_wishbone(sprite,elapsed);
  }
  hero_update_walk(sprite,elapsed);
  hero_update_animation(sprite,elapsed);
  hero_check_quantized_position(sprite);
  SPRITE->fresh_event=0;
}

/* Render the spell in a floating scroll centered on the given point.
 */
 
static void hero_render_spell(struct sprite *sprite,int midx,int midy) {
  uint8_t spell[SPELL_LIMIT]; // Tileid, packed and ordered.
  int spellc=1;
  // Back up until we hit a zero or fill the buffer. We're taking one more than we'll display.
  int startp=SPRITE->spellp-1;
  if (startp<0) startp+=SPELL_LIMIT;
  while (spellc<SPELL_LIMIT) {
    if (!SPRITE->spell[startp]) break;
    startp--;
    if (startp<0) startp+=SPELL_LIMIT;
    spellc++;
  }
  // Fill in (spell).
  int i=0,rp=startp;
  if (!SPRITE->spell[rp]) { rp++; spellc--; }
  // Don't draw anything if it's empty. At this point, she might be going for a spell or maybe for slingshot, we don't know.
  if (!spellc) return;
  for (;i<spellc;i++,rp++) {
    if (rp>=SPELL_LIMIT) rp=0;
    switch (SPRITE->spell[rp]) {
      case EGG_BTN_LEFT: spell[i]=0x3b; break;
      case EGG_BTN_RIGHT: spell[i]=0x3c; break;
      case EGG_BTN_UP: spell[i]=0x3d; break;
      case EGG_BTN_DOWN: spell[i]=0x3e; break;
      default: return;
    }
  }
  // If we reached the limit, replace the first direction with an ellipsis.
  // So visually, user thinks the limit is 15 when it's actually 16, but it doesn't matter because no actual spell is that long.
  if (spellc>=SPELL_LIMIT) spell[0]=0x3f;
  // Tiles are 8x8 and we add one fore and aft. Clamp so the whole thing stays onscreen.
  const int tilew=8;
  int tilec=spellc+2;
  int w=tilec*tilew;
  int x=midx-(w>>1);
  if (x<0) x=0;
  else if (x>FBW-w) x=FBW-w;
  // Normally we render at (midy) exactly, but if that's going to clip against the top, drop down by two meters.
  // Status bar is 16 pixels, and another 4 ensures the scroll is always fully on screen.
  if (midy<20) midy+=NS_sys_tilesize<<1;
  // OK draw it!
  x+=tilew>>1;
  graf_tile(&g.graf,x,midy,0x39,0); x+=tilew;
  for (i=0;i<spellc;i++,x+=tilew) graf_tile(&g.graf,x,midy,spell[i],0);
  graf_tile(&g.graf,x,midy,0x3a,0);
}

/* Render, dispatch for state-specific logic.
 * Caller manages the damage tint.
 */
 
static void hero_render_inner(struct sprite *sprite,int dstx,int dsty) {
  
  // SLINGSHOT is something completely different.
  if (SPRITE->action_state==ACTION_STATE_SLINGSHOT) {
    // Beware, the tile is oriented an eighth-turn off, to make it fit in the tile space.
    int polex=(int)(SPRITE->vaultx*NS_sys_tilesize);
    int poley=(int)(SPRITE->vaulty*NS_sys_tilesize)+NS_sys_tilesize; // Unwisely assuming the status bar is always 1 tile.
    int rotation=((SPRITE->slingt+M_PI/4.0)*255.0)/(M_PI*2.0);
    graf_fancy(&g.graf,polex,poley,0x16,0,rotation,NS_sys_tilesize,0,0x808080ff);
    int ax=polex+(int)(cos(M_PI-SPRITE->slingt-M_PI*0.5)*8.0);
    int ay=poley-(int)(sin(M_PI-SPRITE->slingt-M_PI*0.5)*8.0);
    int bx=polex+(int)(cos(M_PI-SPRITE->slingt+M_PI*0.5)*8.0);
    int by=poley-(int)(sin(M_PI-SPRITE->slingt+M_PI*0.5)*8.0);
    if (SPRITE->actionclock>0.0) { // Launched. Show a sproinging line on the bone, and draw Dot flying.
      double sproing=sin((SPRITE->actionclock*M_PI*4.0)/SLINGSHOT_FLIGHT_TIME)*3.0;
      int spx=(int)(cos(M_PI-SPRITE->slingt)*sproing);
      int spy=(int)(sin(M_PI-SPRITE->slingt)*-sproing);
      ax+=spx; ay+=spy; bx+=spx; by+=spy;
      graf_set_input(&g.graf,0);
      graf_line(&g.graf,ax,ay,0xe0c0a0ff,bx,by,0xe0c0a0ff);
      graf_set_image(&g.graf,sprite->imageid);
      graf_tile(&g.graf,dstx,dsty,0x00,0);//TODO dot in flight tile
    } else { // Preparing.
      double focusx=polex+cos(M_PI-SPRITE->slingt)*SPRITE->slingm;
      double focusy=poley-sin(M_PI-SPRITE->slingt)*SPRITE->slingm;
      graf_set_input(&g.graf,0);
      graf_line(&g.graf,focusx,focusy,0xffffffff,ax,ay,0xe0c0a0ff);
      graf_line(&g.graf,focusx,focusy,0xffffffff,bx,by,0xe0c0a0ff);
      graf_set_image(&g.graf,sprite->imageid);
      graf_tile(&g.graf,(int)focusx,(int)focusy,0x17,0);
    }
    return;
  }
  
  // WAND, it's one of five tiles and three of them also need a bone. (the UP wielding tile, it's built into Dot).
  // Also, a readout of the spell so far.
  if (SPRITE->action_state==ACTION_STATE_WAND) {
    if (SPRITE->rejectclock>0.0) {
      int frame=((int)(SPRITE->rejectclock*6.0))&1;
      graf_tile(&g.graf,dstx,dsty,0x0b+frame,0);
      return;
    }
    uint8_t tileid=sprite->tileid+6;
    int bonex=dstx,boney=dsty;
    uint8_t bonexform=0xff;
    const struct hero_event *event=hero_get_event(sprite,-1);
    switch (event->press) {
      case EGG_BTN_LEFT: tileid+=3; bonex-=NS_sys_tilesize; boney+=3; bonexform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
      case EGG_BTN_RIGHT: tileid+=4; bonex+=NS_sys_tilesize; boney+=3; bonexform=EGG_XFORM_SWAP; break;
      case EGG_BTN_UP: tileid+=2; boney-=NS_sys_tilesize-4; bonexform=EGG_XFORM_YREV; break;
      case EGG_BTN_DOWN: tileid+=1; boney+=NS_sys_tilesize; bonexform=0; break;
    }
    graf_tile(&g.graf,dstx,dsty,tileid,0);
    if (bonexform!=0xff) {
      graf_tile(&g.graf,bonex,boney,0x30,bonexform);
    }
    hero_render_spell(sprite,dstx,dsty-NS_sys_tilesize);
    return;
  }
  
  // VAULT, we take a fixed tile based on direction, and also a shadow below.
  if (SPRITE->action_state==ACTION_STATE_VAULT) {
    //graf_tile(&g.graf,dstx,dsty+1,0x0d,0);
    uint8_t tileid=sprite->tileid+0x13;
    uint8_t xform=0;
    if (SPRITE->vaultdx<0) tileid+=2;
    else if (SPRITE->vaultdx>0) { tileid+=2; xform+=EGG_XFORM_XREV; }
    else if (SPRITE->vaultdy<0) tileid+=1;
    if (SPRITE->actionclock<VAULT_TIME*0.5) tileid+=0x10;
    graf_tile(&g.graf,dstx,dsty-1,tileid,xform);
    return;
  }
  
  // STAB or SWING, hold the initial direction, draw the bone, and for SWING draw the swash.
  if ((SPRITE->action_state==ACTION_STATE_STAB)||(SPRITE->action_state==ACTION_STATE_SWING)) {
    uint8_t tileid=sprite->tileid+3;
    uint8_t xform=0,bonexform=0;
    int bonex=dstx,boney=dsty;
    switch (SPRITE->stabdir) {
      case EGG_BTN_DOWN: boney+=NS_sys_tilesize; break; // Down, the natural direction.
      case EGG_BTN_UP: boney-=NS_sys_tilesize; bonexform=EGG_XFORM_YREV; tileid+=1; break; // Up.
      case EGG_BTN_LEFT: bonex-=NS_sys_tilesize; boney+=2; bonexform=EGG_XFORM_SWAP|EGG_XFORM_YREV; tileid+=2; break; // Left, natural orientation of the tile.
      case EGG_BTN_RIGHT: bonex+=NS_sys_tilesize; boney+=2; bonexform=EGG_XFORM_SWAP; tileid+=2; xform=EGG_XFORM_XREV; break; // Right. Left but flopped.
    }
    graf_tile(&g.graf,dstx,dsty,tileid,xform);
    graf_tile(&g.graf,bonex,boney,0x30,bonexform);
    const double swashtime=0.200;
    const int swashframec=8;
    if ((SPRITE->action_state==ACTION_STATE_SWING)&&(SPRITE->actionclock<swashtime)) {
      int swashframe=(int)((SPRITE->actionclock*swashframec)/swashtime);
      if ((swashframe>=0)&&(swashframe<swashframec)) {
        int swx=bonex;
        int swy=boney;
        uint8_t swxform=0;
        switch (SPRITE->stabdir) {
          case EGG_BTN_DOWN: switch (SPRITE->swingdir) {
              case EGG_BTN_LEFT: swx-=NS_sys_tilesize-2; break;
              case EGG_BTN_RIGHT: swx+=NS_sys_tilesize-2; swxform=EGG_XFORM_XREV; break;
            } break;
          case EGG_BTN_UP: switch (SPRITE->swingdir) {
              case EGG_BTN_LEFT: swx-=NS_sys_tilesize-2; swxform=EGG_XFORM_YREV; break;
              case EGG_BTN_RIGHT: swx+=NS_sys_tilesize-2; swxform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
            } break;
          case EGG_BTN_LEFT: switch (SPRITE->swingdir) {
              case EGG_BTN_UP: swy-=NS_sys_tilesize-2; swxform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
              case EGG_BTN_DOWN: swy+=NS_sys_tilesize-2; swxform=EGG_XFORM_SWAP|EGG_XFORM_XREV|EGG_XFORM_YREV; break;
            } break;
          case EGG_BTN_RIGHT: switch (SPRITE->swingdir) {
              case EGG_BTN_UP: swy-=NS_sys_tilesize-2; swxform=EGG_XFORM_SWAP; break;
              case EGG_BTN_DOWN: swy+=NS_sys_tilesize-2; swxform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
            } break;
        }
        graf_tile(&g.graf,swx,swy,0x31+swashframe,swxform);
      }
    }
    return;
  }
  
  // Idle or walking.
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

/* Render.
 */
 
static void _hero_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  if (SPRITE->damageclock>0.0) {
    int alpha=(int)((SPRITE->damageclock*255.0)/DAMAGE_TIME);
    if (alpha>0xff) alpha=0xff;
    if (alpha>0) graf_set_tint(&g.graf,0xff000000|alpha);
  }
  hero_render_inner(sprite,dstx,dsty);
  if (SPRITE->damageclock>0.0) graf_set_tint(&g.graf,0);
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
