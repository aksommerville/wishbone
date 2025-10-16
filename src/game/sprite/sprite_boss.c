/* sprite_boss.c
 * This must be instantiated just once, at the end of the game.
 * We take responsibility for some global things like ending the game, not usually a sprite's concern.
 */
 
#include "game/wishbone.h"

#define PHASE_IDLE   0
#define PHASE_WALK   1 /* Try to get in front of the hero, on foot. */
#define PHASE_FLY    2 /* Same as WALK but flying. */
#define PHASE_MENACE 3 /* Brief hold before breathing fire. */
#define PHASE_BURN   4 /* Fire in progress. */

#define LEG_RATE 4.0 /* Hz, ie how many full zero-to-one or one-to-zero changes could be completed per second. */
#define WALK_SPEED 3.0
#define FLY_SPEED 6.0
#define NECK_ANGLE_MIN (M_PI*-0.333)
#define NECK_ANGLE_MAX (M_PI*0.333)
#define NECK_ANGLE_RATE 0.500 /* rad/s */
#define NECK_RANGE 3.0
#define NECK_RATE 1.0 /* m/s */
#define SURVIVAL_TIME 30.0 /* Minimum total duration before we let you win. */
#define MIN_FLIGHT_TIME 0.500 /* We only terminate when the dragon is flying, and must be at least this long. */

struct sprite_boss {
  struct sprite hdr;
  int sealed_room;
  int phase;
  double phaseclock;
  double rleg,fleg; // Rear, front leg motion 0..1 = neutral..up.
  int phasestate; // Usage depends on (phase).
  double targetx,targety; // Target position in WALK or FLY.
  double dx,dy; // Normal of motion in WALK or FLY.
  double neck_angle; // Clockwise radians from horizontal.
  double neck_length; // 0..NECK_RANGE in meters; 0 is fully turtled.
  double neck_angle_target; // Phase controllers set a target, and generic update approaches it.
  double neck_length_target;
  double survival_clock; // Counts down globally. (but only after sealing the room)
  double flight_clock; // Counts up during flight.
  int burnc;
};

#define SPRITE ((struct sprite_boss*)sprite)

/* Init.
 */

static int _boss_init(struct sprite *sprite) {

  // Music goes silent when you enter my room, briefly.
  egg_play_song(0,0,1);
  
  SPRITE->survival_clock=SURVIVAL_TIME;
  
  return 0;
}

/* Floating-point approachment.
 */
 
static inline double approach(double from,double to,double rate) {
  if (from<to) {
    if ((from+=rate)>to) return to;
  } else if (from>to) {
    if ((from-=rate)<to) return to;
  }
  return from;
}

/* Check hero position and seal room if ready.
 */
 
static void boss_seal_cell(struct sprite *sprite,int x,int y) {
  int p=y*NS_sys_mapw+x;
  uint8_t tileid=g.map->v[p];
  uint8_t physics=g.physics[tileid];
  if (physics==NS_physics_solid) return;
  g.map->v[p]=0x33; // Castle wall singleton.
}
 
static void boss_check_seal(struct sprite *sprite) {
  if (g.hero) {
    if (g.hero->x<1.5) return;
    if (g.hero->y<1.5) return;
    if (g.hero->x>NS_sys_mapw-1.5) return;
    if (g.hero->y>NS_sys_maph-1.5) return;
  }
  SPRITE->sealed_room=1;
  int i;
  for (i=0;i<NS_sys_mapw;i++) {
    boss_seal_cell(sprite,i,0);
    boss_seal_cell(sprite,i,NS_sys_maph-1);
  }
  for (i=0;i<NS_sys_maph;i++) {
    boss_seal_cell(sprite,0,i);
    boss_seal_cell(sprite,NS_sys_mapw-1,i);
  }
  g.map_dirty=1;
  egg_play_song(RID_song_bone_fight,0,1);
}

/* End of a motion.
 * If the hero is closeish to our burning range, enter MENACE. Otherwise IDLE.
 */
 
static void boss_burn_or_chill(struct sprite *sprite) {
  if (g.hero) {
    double dy=g.hero->y-sprite->y;
    if ((dy>-2.0)&&(dy<2.0)) {
      SPRITE->phase=PHASE_MENACE;
      SPRITE->phaseclock=0.5;
      return;
    }
  }
  SPRITE->phase=PHASE_IDLE;
  SPRITE->phaseclock=2.0;
}

/* WALK and FLY phase.
 */
 
static void boss_motion(struct sprite *sprite,double elapsed,double speed) {
  // No sense clamping to the target, it's fuzzy in any case.
  sprite->x+=SPRITE->dx*elapsed*speed;
  sprite->y+=SPRITE->dy*elapsed*speed;
}
 
static void boss_update_WALK(struct sprite *sprite,double elapsed) {
  boss_motion(sprite,elapsed,WALK_SPEED);
  
  // Animate legs.
  int legs_ready=1;
  if (SPRITE->phasestate) {
    if ((SPRITE->fleg-=elapsed*LEG_RATE)<=0.0) SPRITE->fleg=0.0; else legs_ready=0;
    if ((SPRITE->rleg+=elapsed*LEG_RATE)>=1.0) SPRITE->rleg=1.0; else legs_ready=0;
  } else {
    if ((SPRITE->fleg+=elapsed*LEG_RATE)>=1.0) SPRITE->fleg=1.0; else legs_ready=0;
    if ((SPRITE->rleg-=elapsed*LEG_RATE)<=0.0) SPRITE->rleg=0.0; else legs_ready=0;
  }
  if (legs_ready) SPRITE->phasestate^=1;
  
  SPRITE->neck_angle_target=0.0;
  SPRITE->neck_length_target=0.0;
}

static void boss_update_FLY(struct sprite *sprite,double elapsed) {
  boss_motion(sprite,elapsed,FLY_SPEED);
  
  // Retract landing gear.
  if ((SPRITE->fleg+=elapsed*LEG_RATE)>=1.0) SPRITE->fleg=1.0;
  if ((SPRITE->rleg+=elapsed*LEG_RATE)>=1.0) SPRITE->rleg=1.0;
  
  SPRITE->neck_angle_target=0.0;
  SPRITE->neck_length_target=0.0;
  
  SPRITE->flight_clock+=elapsed;
}

static void boss_end_WALK(struct sprite *sprite) {
  boss_burn_or_chill(sprite);
}

static void boss_end_FLY(struct sprite *sprite) {
  boss_burn_or_chill(sprite);
  SPRITE->flight_clock=0.0;
}

static void boss_begin_motion(struct sprite *sprite) {

  /* Select a target position, same for WALK and FLY.
   * Aim for roughly the hero's latitude and a fixed distance away from her in the longer horizontal direction.
   * In the middle fifth of longitude, go to whichever side we're not currently on.
   * This should cause us to select a path that goes thru or very near the hero, which I think is desirable.
   * Keep latitude at least 2 meters from the edges; we are very fat.
   */
  double herox,heroy;
  if (g.hero) {
    herox=g.hero->x;
    heroy=g.hero->y;
  } else {
    herox=NS_sys_mapw*0.5;
    heroy=NS_sys_maph*0.5;
  }
  SPRITE->targety=heroy;
  if (SPRITE->targety<2.0) SPRITE->targety=2.0;
  else if (SPRITE->targety>NS_sys_maph-2.5) SPRITE->targety=NS_sys_maph-2.5;
  double heroside; // -1,1
  if (herox<NS_sys_mapw*0.4) heroside=1.0;
  else if (herox>NS_sys_mapw*0.6) heroside=-1.0;
  else if (sprite->x>herox) heroside=-1.0;
  else heroside=1.0;
  SPRITE->targetx=herox+heroside*4.0;
  
  /* If we wind up with too small a delta, replace longitude with one of two fixed positions, whichever is further.
   * This happens if the hero remains stationary. We should not.
   */
  SPRITE->dx=SPRITE->targetx-sprite->x;
  SPRITE->dy=SPRITE->targety-sprite->y;
  const double too_short=2.0;
  if ((SPRITE->dx>-too_short)&&(SPRITE->dx<too_short)&&(SPRITE->dy>-too_short)&&(SPRITE->dy<too_short)) {
    if (sprite->x>=NS_sys_mapw*0.5) SPRITE->targetx=NS_sys_mapw*0.250;
    else SPRITE->targetx=NS_sys_mapw*0.750;
    SPRITE->dx=SPRITE->targetx-sprite->x;
  }
  
  // OK, normalize delta.
  double distance=sqrt(SPRITE->dx*SPRITE->dx+SPRITE->dy*SPRITE->dy);
  SPRITE->dx/=distance;
  SPRITE->dy/=distance;
  
  // Walking or flying?
  double speed;
  if (rand()&1) {
    SPRITE->phase=PHASE_WALK;
    speed=WALK_SPEED;
    SPRITE->phasestate=0; // 0=front leg upward, 1=rear leg upward. Always start by moving the front leg.
  } else {
    SPRITE->phase=PHASE_FLY;
    speed=FLY_SPEED;
  }
  
  // Set phaseclock to put us at the target according to speed. It doesn't have to be exact.
  SPRITE->phaseclock=distance/speed;
  SPRITE->flight_clock=0.0;
}

/* IDLE phase.
 */
 
static void boss_update_IDLE(struct sprite *sprite,double elapsed) {
  if ((SPRITE->rleg-=LEG_RATE*elapsed)<=0.0) SPRITE->rleg=0.0;
  if ((SPRITE->fleg-=LEG_RATE*elapsed)<=0.0) SPRITE->fleg=0.0;
  
  SPRITE->neck_angle_target=0.0;
  SPRITE->neck_length_target=0.0;
}

static void boss_end_IDLE(struct sprite *sprite) {
  boss_begin_motion(sprite);
}

/* MENACE and BURN phase.
 */
 
static void boss_head_toward_hero(struct sprite *sprite) {
  if (!g.hero) return;
  double ox=sprite->x;
  if (sprite->xform&EGG_XFORM_XREV) ox+=1.0; else ox-=1.0;
  double oy=sprite->y-0.5;
  double dx=g.hero->x-ox;
  double dy=g.hero->y-oy;
  if (dx<0.0) dx=-dx; // Our angle is flop-invariant.
  SPRITE->neck_angle_target=atan2(-dy,dx);
  if (SPRITE->neck_angle_target<NECK_ANGLE_MIN) SPRITE->neck_angle_target=NECK_ANGLE_MIN;
  else if (SPRITE->neck_angle_target>NECK_ANGLE_MAX) SPRITE->neck_angle_target=NECK_ANGLE_MAX;
  if ((dx>0.500)||(dy<-0.500)||(dy>0.500)) {
    double distance=sqrt(dx*dx+dy*dy);
    SPRITE->neck_length_target=distance*0.500;
    if (SPRITE->neck_length_target>NECK_RANGE) SPRITE->neck_length_target=NECK_RANGE;
  }
}
 
static void boss_update_MENACE(struct sprite *sprite,double elapsed) {
  
  // Legs to neutral.
  if ((SPRITE->rleg-=LEG_RATE*elapsed)<=0.0) SPRITE->rleg=0.0;
  if ((SPRITE->fleg-=LEG_RATE*elapsed)<=0.0) SPRITE->fleg=0.0;
  
  boss_head_toward_hero(sprite);
}

static void boss_update_BURN(struct sprite *sprite,double elapsed) {

  // Burninate?
  if (g.hero) {
    double l,r,t,b,headx;
    if (sprite->xform&EGG_XFORM_XREV) {
      double neckx=sprite->x+1.0;
      headx=neckx+SPRITE->neck_length*cos(SPRITE->neck_angle);
      l=headx;
      r=headx+3.0;
    } else {
      double neckx=sprite->x-1.0;
      headx=neckx-SPRITE->neck_length*cos(SPRITE->neck_angle);
      l=headx-3.0;
      r=headx;
    }
    if ((g.hero->x>l)&&(g.hero->x<r)) {
      double necky=sprite->y-0.5;
      double heady=necky-SPRITE->neck_length*sin(SPRITE->neck_angle);
      t=heady-0.5;
      b=heady+0.5;
      if ((g.hero->y>t)&&(g.hero->y<b)) {
        // Lie about my position, call it where my head is, so she reacts appealingly.
        double pvx=sprite->x,pvy=sprite->y;
        sprite->x=headx;
        sprite->y=heady;
        sprite_hero_injure(g.hero,sprite);
        sprite->x=pvx;
        sprite->y=pvy;
      }
    }
  }
  
  // Legs to neutral.
  if ((SPRITE->rleg-=LEG_RATE*elapsed)<=0.0) SPRITE->rleg=0.0;
  if ((SPRITE->fleg-=LEG_RATE*elapsed)<=0.0) SPRITE->fleg=0.0;
  
  boss_head_toward_hero(sprite);
}

static void boss_end_MENACE(struct sprite *sprite) {
  SPRITE->phase=PHASE_BURN;
  SPRITE->phaseclock=2.0;
}

static void boss_end_BURN(struct sprite *sprite) {
  boss_begin_motion(sprite);
  SPRITE->burnc++;
}

/* Update.
 */

static void _boss_update(struct sprite *sprite,double elapsed) {

  /* Wait for the hero to enter the room, then seal the door.
   * Until this happens, we don't do much.
   */
  if (!SPRITE->sealed_room) {
    boss_check_seal(sprite);
    return;
  }
  
  // Face the hero, all phases.
  if (g.hero) {
    if (g.hero->x<sprite->x) sprite->xform=0;
    else sprite->xform=EGG_XFORM_XREV;
    
    // Regardless of phase, if she touches my body, deal an injury.
    double dx=g.hero->x-sprite->x;
    double dy=g.hero->y-sprite->y;
    const double radius=1.25;
    double d2=dx*dx+dy*dy;
    if (d2<radius*radius) {
      sprite_hero_injure(g.hero,sprite);
    }
  }
  
  // Tick the phaseclock and enter a new phase on expiry.
  if ((SPRITE->phaseclock-=elapsed)<=0.0) {
    switch (SPRITE->phase) {
      case PHASE_IDLE: boss_end_IDLE(sprite); break;
      case PHASE_WALK: boss_end_WALK(sprite); break;
      case PHASE_FLY: boss_end_FLY(sprite); break;
      case PHASE_MENACE: boss_end_MENACE(sprite); break;
      case PHASE_BURN: boss_end_BURN(sprite); break;
      default: boss_begin_motion(sprite); break;
    }
  }
  
  // Further action depends on phase.
  switch (SPRITE->phase) {
    case PHASE_IDLE: boss_update_IDLE(sprite,elapsed); break;
    case PHASE_WALK: boss_update_WALK(sprite,elapsed); break;
    case PHASE_FLY: boss_update_FLY(sprite,elapsed); break;
    case PHASE_MENACE: boss_update_MENACE(sprite,elapsed); break;
    case PHASE_BURN: boss_update_BURN(sprite,elapsed); break;
  }
  
  // Move head toward its target.
  SPRITE->neck_length=approach(SPRITE->neck_length,SPRITE->neck_length_target,NECK_RATE*elapsed);
  SPRITE->neck_angle=approach(SPRITE->neck_angle,SPRITE->neck_angle_target,NECK_ANGLE_RATE*elapsed);
  
  /* Check termination.
   * Dot wins when all of:
   *  - Has wishbone.
   *  - Dot is alive.
   *  - Survived SURVIVAL_TIME seconds.
   *  - Dragon has been flying for MIN_FLIGHT_TIME seconds.
   *  - Dragon burninated at least 3 times. (it looks cool, we wouldn't want to deprive the player of seeing it).
   * If you enter the boss room without a wishbone, of course that is possible, but the only thing you can do is die.
   */
  SPRITE->survival_clock-=elapsed;
  if (g.hero&&(g.item==NS_flag_wishbone)&&(SPRITE->survival_clock<=0.0)) {
    if (SPRITE->flight_clock>=MIN_FLIGHT_TIME) {
      if (SPRITE->burnc>=3) {
        g.victory=1;
      }
    }
  }
}

/* Render.
 */
 
static void _boss_render(struct sprite *sprite,int dstx,int dsty) {
  const int ht=NS_sys_tilesize>>1;
  graf_set_image(&g.graf,sprite->imageid);
  
  /* Take a few measurements.
   * "Rear" and "front" leg refers to both the render order and the position relative the head.
   */
  const double LEG_RANGE=8.0; // pixels
  int neckx,necky,headx,heady;
  int rlegx,rlegy,flegx,flegy;
  if (sprite->xform&EGG_XFORM_XREV) {
    neckx=dstx+NS_sys_tilesize;
    necky=dsty-ht;
    headx=neckx+(int)(SPRITE->neck_length*cos(SPRITE->neck_angle)*NS_sys_tilesize);
    heady=necky-(int)(SPRITE->neck_length*sin(SPRITE->neck_angle)*NS_sys_tilesize);
    rlegx=dstx-ht;
    rlegy=dsty+NS_sys_tilesize-(int)(SPRITE->rleg*LEG_RANGE);
    flegx=dstx+ht;
    flegy=dsty+NS_sys_tilesize-(int)(SPRITE->fleg*LEG_RANGE);
  } else {
    neckx=dstx-NS_sys_tilesize;
    necky=dsty-ht;
    headx=neckx-(int)(SPRITE->neck_length*cos(SPRITE->neck_angle)*NS_sys_tilesize);
    heady=necky-(int)(SPRITE->neck_length*sin(SPRITE->neck_angle)*NS_sys_tilesize);
    rlegx=dstx+ht;
    rlegy=dsty+NS_sys_tilesize-(int)(SPRITE->rleg*LEG_RANGE);
    flegx=dstx-ht;
    flegy=dsty+NS_sys_tilesize-(int)(SPRITE->fleg*LEG_RANGE);
  }
  
  // Body and legs.
  graf_tile(&g.graf,rlegx,rlegy,0x86,sprite->xform);
  graf_tile(&g.graf,dstx-ht,dsty-ht,0x73,0);
  graf_tile(&g.graf,dstx+ht,dsty-ht,0x74,0);
  graf_tile(&g.graf,dstx-ht,dsty+ht,0x83,0);
  graf_tile(&g.graf,dstx+ht,dsty+ht,0x84,0);
  graf_tile(&g.graf,flegx,flegy,0x86,sprite->xform);
  
  // Neck.
  int segmentc=4;
  int i=0; for (;i<segmentc;i++) {
    int x=((neckx*(segmentc-i))+(headx*i))/segmentc;
    int y=((necky*(segmentc-i))+(heady*i))/segmentc;
    graf_tile(&g.graf,x,y,0x76,0);
  }
  
  // Head.
  if (SPRITE->phase==PHASE_BURN) {
    int firex=headx,firedx=(sprite->xform&EGG_XFORM_XREV)?NS_sys_tilesize:-NS_sys_tilesize;
    firex+=firedx>>1;
    graf_tile(&g.graf,firex,heady,0x89,sprite->xform); firex+=firedx;
    graf_tile(&g.graf,firex,heady,0x88,sprite->xform); firex+=firedx;
    graf_tile(&g.graf,firex,heady,0x87,sprite->xform);
    graf_tile(&g.graf,headx,heady,0x85,sprite->xform);
  } else {
    graf_tile(&g.graf,headx,heady,0x75,sprite->xform);
  }
  
  // Wing. We flap when flying, and that is purely ornamental, managed right here.
  if ((SPRITE->phase==PHASE_FLY)&&(((int)(SPRITE->phaseclock*4.0))&1)) {
    graf_tile(&g.graf,dstx,dsty-NS_sys_tilesize,0x77,sprite->xform);
  } else { // Normal, wing down.
    graf_tile(&g.graf,dstx,dsty,0x77,EGG_XFORM_YREV|sprite->xform);
  }
}

/* Type definition.
 */

const struct sprite_type sprite_type_boss={
  .name="boss",
  .objlen=sizeof(struct sprite_boss),
  .init=_boss_init,
  .update=_boss_update,
  .render=_boss_render,
};
