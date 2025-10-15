/* modal_lockpick.c
 * Minigame where you pick a lock with your wishbone.
 */
 
#include "game/wishbone.h"

#define FLDW 8
#define FLDH 9
#define BONE_SPACING 3 /* m */
#define LABEL_LIMIT 2
#define ANGRY_LIMIT 42 /* With an 8x9 field, we can have max 42 blocks (and that would be unplayable). */
#define ANGRY_TIME 0.500
#define WIN_TIME 0.750 /* A wee cooldown after the puzzle completes, before reporting victory. */
#define FIREWORK_LIMIT 16
#define FIREWORK_TIME 1.000
#define FIREWORK_GROWTH 8.0 /* m/s */
#define FIREWORK_ROTATE 3.0 /* rad/s */
#define FIREWORK_SUBROTATE 1.0 /* rad/s */
#define WARNING_TIME 5.0

// Constants for tuning difficulty:
#define DEATHCLOCK_MIN_DIFFICULTY 0x40 /* No timer below this difficulty. */
#define DEATHCLOCK_MIN_TIME 60.0 /* Timer at minimum difficulty. */
#define DEATHCLOCK_MAX_TIME 45.0 /* Timer at maximum difficulty. */
#define PAIRC_MIN 2 /* How many pairs of block a minimum difficulty. */
#define PAIRC_MAX 9 /* '' maximum. */

#define STRIX_ADVICE 3
#define STRIX_WARNING 4

static const uint32_t color_by_blockid[]={
  0x000000ff, // zero is not a real block
  0xff0000ff,
  0x00c000ff,
  0x0000ffff,
  0xffff00ff,
  0xff40f0ff,
  0x00ffffff,
};

struct modal_lockpick {
  struct modal hdr;
  int flagid;
  int difficulty;
  uint8_t fld[FLDW*FLDH]; // blockid; ie index in (color_by_blockid)
  int bonex; // column of left bone
  int boney; // top row of bone
  struct label {
    int texid,x,y,w,h;
    int strix;
  } labelv[LABEL_LIMIT];
  int labelc;
  double deathclock; // Zero until the first significant action, then counts down.
  struct angry {
    int y,x;
    double clock;
  } angryv[ANGRY_LIMIT]; // Sorted by (y,x) and may contain noops (clock<=0).
  int angryc;
  double angryclock;
  double winclock;
  struct firework {
    double x,y; // Center of explosion in fractional grid cells.
    double radius;
    double t; // Orbital positions.
    double subt; // Rotation of each key.
    double clock; // Counts down.
  } fireworkv[FIREWORK_LIMIT];
  int fireworkc;
};

#define MODAL ((struct modal_lockpick*)modal)

/* Delete.
 */
 
static void _lockpick_del(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Add label with a string resource.
 */
 
static struct label *lockpick_labelv_add(struct modal *modal,int strix) {
  if (MODAL->labelc>=LABEL_LIMIT) return 0;
  const char *text=0;
  int textc=res_get_string(&text,1,strix);
  if (textc<1) return 0;
  struct label *label=MODAL->labelv+MODAL->labelc++;
  if ((label->texid=font_render_to_texture(0,g.font,text,textc,FBW>>2,FBH,0xffffffff))<0) return 0;
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->strix=strix;
  return label;
}

static struct label *lockpick_label_by_strix(struct modal *modal,int strix) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) if (label->strix==strix) return label;
  return 0;
}

/* Init.
 */
 
static int _lockpick_init(struct modal *modal) {
  MODAL->bonex=(FLDW>>1)-(BONE_SPACING>>1);
  MODAL->boney=FLDH-1;
  
  struct label *label;
  if (label=lockpick_labelv_add(modal,STRIX_ADVICE)) {
    int fldx=(FBW>>1)-((FLDW*NS_sys_tilesize)>>1);
    label->x=(fldx>>1)-(label->w>>1);
    label->y=(FBH>>1)-(label->h>>1);
  }
  if (label=lockpick_labelv_add(modal,STRIX_WARNING)) {
    int rightx=(FBW>>1)+((FLDW*NS_sys_tilesize)>>1);
    label->x=rightx+((FBW-rightx)>>1)-(label->w>>1);
    label->y=(FBH>>1)-(label->h>>1)-10; // -10 leaves a bit of space for the clock
  }
  return 0;
}

/* Focus.
 */
 
static void _lockpick_focus(struct modal *modal,int focus) {
}

/* Add record of an angry cell.
 */
 
static int lockpick_angryv_search(const struct modal *modal,int x,int y) {
  int lo=0,hi=MODAL->angryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct angry *q=MODAL->angryv+ck;
         if (y<q->y) hi=ck;
    else if (y>q->y) lo=ck+1;
    else if (x<q->x) hi=ck;
    else if (x>q->x) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}
 
static void lockpick_angryv_add(struct modal *modal,int x,int y) {
  int p=lockpick_angryv_search(modal,x,y);
  if (p>=0) {
    // We already have it. This is not unlikely. Just update its clock.
    MODAL->angryv[p].clock=ANGRY_TIME;
  } else {
    if (MODAL->angryc>=ANGRY_LIMIT) return; // Not possible.
    p=-p-1;
    struct angry *angry=MODAL->angryv+p;
    memmove(angry+1,angry,sizeof(struct angry)*(MODAL->angryc-p));
    MODAL->angryc++;
    angry->x=x;
    angry->y=y;
    angry->clock=ANGRY_TIME;
  }
}

/* Add fireworks.
 */
 
static void lockpick_add_fireworks(struct modal *modal,int x,int y) {
  if (MODAL->fireworkc>=FIREWORK_LIMIT) return;
  struct firework *fw=MODAL->fireworkv+MODAL->fireworkc++;
  fw->x=x+0.5;
  fw->y=y+0.5;
  fw->clock=FIREWORK_TIME;
  fw->radius=0.0;
  fw->t=((rand()&0x7fff)*M_PI*2.0)/32768.0;
}

/* Intruder detected! Call when a block moves or deletes.
 * If we haven't tripped the alarm yet, it happens now.
 */
 
static void lockpick_detect_intruder(struct modal *modal) {
  if (MODAL->deathclock>0.0) return; // Already tripped.
  if (MODAL->difficulty<DEATHCLOCK_MIN_DIFFICULTY) return; // No alarm.
  double t=(MODAL->difficulty-DEATHCLOCK_MIN_DIFFICULTY)/(256.0-DEATHCLOCK_MIN_DIFFICULTY);
  MODAL->deathclock=DEATHCLOCK_MIN_TIME+t*(DEATHCLOCK_MAX_TIME-DEATHCLOCK_MIN_TIME);
  SFX(lockpick_alarm)
}

/* Generalization to assess a move of the bone.
 * Call lockpick_assess_motion() with a zeroed (struct assessment).
 * If it comes out (valid), then (killv,movev) indicate the affected cells and (angryv) will be empty.
 * If not (valid), (killv,movev) will be empty and (angryv) may or may not.
 * Cells in (movev) are guaranteed to have neighbors at both delta and negative delta.
 */
 
struct assessment {
  int ox,oy;
  int dx,dy;
  int nx,ny; // (o+d)
  int rx; // (nx+BONE_SPACING)
  int valid;
  int finished;
  uint16_t killv[ANGRY_LIMIT];
  uint16_t angryv[ANGRY_LIMIT];
  uint16_t movev[ANGRY_LIMIT];
  int killc,angryc,movec;
  const struct modal *modal;
};

static int assessment_has_thing(const uint16_t *v,int c,int x,int y) {
  uint16_t n=(y<<8)|x;
  int lo=0,hi=c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint16_t q=v[ck];
         if (n<q) hi=ck;
    else if (n>q) lo=ck+1;
    else return 1;
  }
  return 0;
}

static void assessment_add_thing(uint16_t *v,int *c,int x,int y) {
  if (*c>=ANGRY_LIMIT) return; // oops?
  uint16_t n=(y<<8)|x;
  int lo=0,hi=*c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint16_t q=v[ck];
         if (n<q) hi=ck;
    else if (n>q) lo=ck+1;
    else return;
  }
  memmove(v+lo+1,v+lo,sizeof(uint16_t)*((*c)-lo));
  (*c)++;
  v[lo]=n;
}
static void assessment_add_kill(struct assessment *rpt,int x,int y) { assessment_add_thing(rpt->killv,&rpt->killc,x,y); }
static void assessment_add_angry(struct assessment *rpt,int x,int y) { assessment_add_thing(rpt->angryv,&rpt->angryc,x,y); }
static void assessment_add_move(struct assessment *rpt,int x,int y) { assessment_add_thing(rpt->movev,&rpt->movec,x,y); }

// Recursively consider one cell in light of the move described by (rpt).
// Returns nonzero if the cell is available in the new field, possibly after a move.
static int lockpick_assess_cell(struct assessment *rpt,int x,int y) {
  if ((x<0)||(y<0)||(x>=FLDW)||(y>=FLDH)) return 0; // OOB. We shouldn't have been called but whatever.
  const struct modal *modal=rpt->modal;
  const uint8_t *fldp=MODAL->fld+y*FLDW+x;
  if (!fldp) return 1; // Empty cell. Yep, it's good.
  
  // The cell is occupied. Can it move by (dx,dy)?
  int nx=x+rpt->dx;
  int ny=y+rpt->dy;
  if ((nx<0)||(ny<0)||(nx>=FLDW)||(ny>=FLDH)) { // Nope, it's at the edge.
    assessment_add_angry(rpt,x,y);
    return 0;
  }
  const uint8_t *np=MODAL->fld+ny*FLDW+nx;
  if (!*np) { // Yep, next cell is empty.
    assessment_add_move(rpt,x,y);
    return 1;
  }
  if (*np==*fldp) { // Yep, and we can kill both of them.
    assessment_add_kill(rpt,x,y);
    assessment_add_kill(rpt,nx,ny);
    return 1;
  }
  if (lockpick_assess_cell(rpt,nx,ny)) { // Yep, after recursion.
    assessment_add_move(rpt,x,y);
    return 1;
  }
  // Nope. Blocked after recursion.
  assessment_add_angry(rpt,x,y);
  return 0;
}

static void lockpick_assess_motion(struct assessment *rpt,const struct modal *modal,int dx,int dy) {
  if (!dx&&!dy) return;
  rpt->ox=MODAL->bonex;
  rpt->oy=MODAL->boney;
  rpt->dx=dx;
  rpt->dy=dy;
  rpt->nx=rpt->ox+dx;
  rpt->ny=rpt->oy+dy;
  rpt->rx=rpt->nx+BONE_SPACING;
  rpt->modal=modal;
  
  // If the new bone position is OOB, it's invalid and no cell impact.
  // Player is allowed to move down from the bottom row, but that's handled elsewhere.
  if ((rpt->nx<0)||(rpt->ny<0)||(rpt->rx>=FLDW)||(rpt->ny>=FLDH)) return;
  
  // Visit each cell covered by the new bone.
  int y=rpt->ny;
  const uint8_t *lp=MODAL->fld+y*FLDW+rpt->nx;
  const uint8_t *rp=MODAL->fld+y*FLDW+rpt->rx;
  for (;y<FLDH;y++,lp+=FLDW,rp+=FLDW) {
    if (*lp) lockpick_assess_cell(rpt,rpt->nx,y);
    if (*rp) lockpick_assess_cell(rpt,rpt->rx,y);
  }
  
  // If anything is angry, nix the kills and moves. Otherwise call it valid.
  if (rpt->angryc) {
    rpt->killc=0;
    rpt->movec=0;
  } else {
    rpt->valid=1;
  }
  
  // If we're killing something, check whether it's the final kill.
  if (rpt->killc) {
    int blockc=0,i=FLDW*FLDH;
    const uint8_t *p=MODAL->fld;
    for (;i-->0;p++) if (*p) blockc++;
    if (rpt->killc>=blockc) rpt->finished=1;
  }
}

/* Move bone. This is the whole game, right here.
 */
 
static void lockpick_move(struct modal *modal,int dx,int dy) {

  // First, if she's moving down and at the bottom row, end the game.
  // Except if it's already won, let the terminal cooldown play out.
  if ((dy>0)&&(MODAL->boney>=FLDH-1)) {
    if (MODAL->winclock>0.0) return;
    SFX(exit)
    modal->defunct=1;
    return;
  }

  // Assess the move.
  struct assessment rpt={0};
  lockpick_assess_motion(&rpt,modal,dx,dy);
  
  // If no cells are impacted, we either move the bone or reject due to edge collision.
  // Either way, we can finish right now without setting the deathclock or touching the field.
  if (!rpt.killc&&!rpt.angryc&&!rpt.movec) {
    if (rpt.valid) {
      SFX(uimotion)
      MODAL->bonex=rpt.nx;
      MODAL->boney=rpt.ny;
    } else {
      SFX(reject)
    }
    return;
  }
  
  // If invalid, play a sound, note the angry cells, and don't move the bone.
  if (!rpt.valid) {
    SFX(reject)
    int i=rpt.angryc;
    while (i-->0) {
      int x=rpt.angryv[i]&0xff;
      int y=rpt.angryv[i]>>8;
      lockpick_angryv_add(modal,x,y);
    }
    return;
  }
  
  // It's valid. Move the bone.
  MODAL->bonex=rpt.nx;
  MODAL->boney=rpt.ny;
  
  // If anything moves or kills, engage the lock's built-in defences.
  // Moving the bone by itself does not trip any alarms.
  if (rpt.killc||rpt.movec) lockpick_detect_intruder(modal);
  
  // Kill the cells that want it.
  int i=rpt.killc;
  while (i-->0) {
    int x=rpt.killv[i]&0xff;
    int y=rpt.killv[i]>>8;
    MODAL->fld[y*FLDW+x]=0;
    int ox=x+dx,oy=y+dy;
    if ((ox>=0)&&(oy>=0)&&(ox<FLDW)&&(oy<FLDH)&&assessment_has_thing(rpt.killv,rpt.killc,ox,oy)) {
      // There's a kill in our direction of travel. That's the other half of this pair. So no fireworks here.
    } else {
      lockpick_add_fireworks(modal,x,y);
    }
  }
  
  /* With killing done, move the cells that want it.
   * Need to make a copy of the field and apply changes there, then copy it all back.
   * During the operation, we write to (tmp) but read from (MODAL->fld).
   */
  if (rpt.movec) {
    uint8_t tmp[FLDW*FLDH];
    memcpy(tmp,MODAL->fld,FLDW*FLDH);
    for (i=rpt.movec;i-->0;) {
      int fx=rpt.movev[i]&0xff;
      int fy=rpt.movev[i]>>8;
      int tx=fx+dx;
      int ty=fy+dy;
      tmp[ty*FLDW+tx]=MODAL->fld[fy*FLDW+fx];
      tmp[fy*FLDW+fx]=MODAL->fld[(fy-dy)*FLDW+(fx-dx)];
    }
    memcpy(MODAL->fld,tmp,FLDW*FLDH);
  }
  
  // Final considerations: Sound effect and completion.
  if (rpt.finished) {
    SFX(lockpick_success)
    MODAL->winclock=WIN_TIME;
  } else if (rpt.killc) {
    SFX(lockpick_kill)
  } else if (rpt.movec) {
    SFX(lockpick_move)
  } else {
    SFX(uimotion)
  }
}

/* Update.
 */
 
static void _lockpick_update(struct modal *modal,double elapsed,int input,int pvinput) {

  // If the terminal cooldown has started, only that and fireworks matter.
  // In particular, the deathclock stops after success.
  if (MODAL->winclock>0.0) {
    MODAL->deathclock=0.0;
    if ((MODAL->winclock-=elapsed)<=0.0) {
      modal->defunct=1;
      flag_set(MODAL->flagid,1);
      return;
    }
  }

  // Poll input.
  if (input!=pvinput) {
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) lockpick_move(modal,-1,0);
    if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) lockpick_move(modal,1,0);
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) lockpick_move(modal,0,-1);
    if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) lockpick_move(modal,0,1);
    if (modal->defunct) return; // In case exit and death tie on the same frame, don't let them both happen.
  }
  
  // Update fireworks.
  int i=MODAL->fireworkc;
  struct firework *fw=MODAL->fireworkv+i-1;
  for (;i-->0;fw--) {
    if ((fw->clock-=elapsed)<=0.0) {
      MODAL->fireworkc--;
      memmove(fw,fw+1,sizeof(struct firework)*(MODAL->fireworkc-i));
    } else {
      fw->radius+=FIREWORK_GROWTH*elapsed;
      fw->t+=FIREWORK_ROTATE*elapsed;
      fw->subt+=FIREWORK_SUBROTATE*elapsed;
    }
  }
  
  // Tick angry clocks, and drop angries from the tail. OK to leave noops in the middle, we'll get them eventually.
  struct angry *angry=MODAL->angryv;
  for (i=MODAL->angryc;i-->0;angry++) {
    angry->clock-=elapsed;
  }
  while (MODAL->angryc&&(MODAL->angryv[MODAL->angryc-1].clock<=0.0)) MODAL->angryc--;
  
  // Tick the deathclock if running.
  if (MODAL->deathclock>0.0) {
    if ((MODAL->deathclock-=elapsed)<=0.0) {
      modal->defunct=1;
      SFX(lockpick_defense)
      sprite_hero_injure(g.hero,0);
    }
  }
}

/* Render.
 */
 
static void _lockpick_render(struct modal *modal) {
  const int fldw=FLDW*NS_sys_tilesize;
  const int fldh=FLDH*NS_sys_tilesize;
  const int fldx=(FBW>>1)-(fldw>>1);
  const int fldy=(FBH>>1)-(fldh>>1);
  
  { // Background.
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x6a0b1cff); // The dark red of our lock's tile.
    if ((MODAL->deathclock>0.0)&&(MODAL->deathclock<WARNING_TIME)) { // Flash background when close to the end. "Hurry up!"
      double whole,fract;
      fract=modf(MODAL->deathclock,&whole);
      if (fract>0.5) {
        int alpha=(int)((fract-0.5)*256.0);
        graf_fill_rect(&g.graf,0,0,FBW,FBH,0xffff0000|alpha);
      }
    }
    graf_fill_rect(&g.graf,fldx-1,fldy-1,fldw+2,fldh+2,0x000000ff);
  }
  
  // Tiles and bone use fancies from image:sprites.
  graf_set_image(&g.graf,RID_image_sprites);
  
  { // Field of tiles, quantized positions.
    // We've taken pains to keep (angryv) in render order, so it doesn't need to be searched at each cell.
    const struct angry *angry=MODAL->angryv;
    int angryp=0;
    int y=fldy+(NS_sys_tilesize>>1);
    const uint8_t *p=MODAL->fld;
    int yi=FLDH,row=0;
    for (;yi-->0;y+=NS_sys_tilesize,row++) {
      int x=fldx+(NS_sys_tilesize>>1);
      int xi=FLDW,col=0;
      for (;xi-->0;x+=NS_sys_tilesize,col++,p++) {
        if (!*p) continue;
        
        // Flash white if angry.
        uint32_t tint=0;
        while (
          (angryp<MODAL->angryc)&&(
            (angry->y<row)||
            ((angry->y==row)&&(angry->x<col))
          )
        ) { angryp++; angry++; }
        if ((angryp<MODAL->angryc)&&(angry->y==row)&&(angry->x==col)&&(angry->clock>0.0)) {
          int alpha=(int)((angry->clock*255.0)/ANGRY_TIME);
          if (alpha<0) alpha=0; else if (alpha>0xff) alpha=0xff;
          tint=0xffffff00|alpha;
        }
        
        graf_fancy(&g.graf,x,y,0x70,0,0,NS_sys_tilesize,tint,color_by_blockid[*p]);
      }
    }
  }
  
  { // Bone.
    int lx=fldx+MODAL->bonex*NS_sys_tilesize+(NS_sys_tilesize>>1);
    int rx=lx+BONE_SPACING*NS_sys_tilesize;
    int y=fldy+MODAL->boney*NS_sys_tilesize+(NS_sys_tilesize>>1);
    uint8_t tileid=0x61;
    for (;y<FBH+NS_sys_tilesize;y+=NS_sys_tilesize) {
      graf_fancy(&g.graf,lx,y,tileid,0,0,NS_sys_tilesize,0,0);
      graf_fancy(&g.graf,rx,y,tileid,EGG_XFORM_XREV,0,NS_sys_tilesize,0,0);
      tileid=0x71; // Everything below the top row is the same tile.
    }
  }
  
  { // Fireworks.
    const struct firework *fw=MODAL->fireworkv;
    int i=MODAL->fireworkc;
    for (;i-->0;fw++) {
      int alpha=(int)((fw->clock*255.0)/FIREWORK_TIME);
      if (alpha<1) alpha=1; else if (alpha>0xff) alpha=0xff;
      uint32_t primary=0x80808000|alpha;
      double t=fw->t;
      int elemi=6; // Arbitrary; how many keys per firework.
      double dt=(M_PI*2.0)/(double)elemi;
      int rotation=(int)((fw->subt*256.0)/(M_PI*2.0));
      for (;elemi-->0;t+=dt) {
        int x=fldx+(int)((fw->x+fw->radius*cos(t))*NS_sys_tilesize);
        int y=fldy+(int)((fw->y-fw->radius*sin(t))*NS_sys_tilesize);
        graf_fancy(&g.graf,x,y,0x62,0,rotation,NS_sys_tilesize,0,primary);
      }
    }
  }
  
  { // Death clock.
    if (MODAL->deathclock>0.0) {
      struct label *label=lockpick_label_by_strix(modal,STRIX_WARNING);
      if (label) {
        int sec=(int)(MODAL->deathclock+1.0); // ceiling; we won't show zero
        if (sec>0) {
          if (sec>99) sec=99;
          int right=fldx+fldw;
          int x=right+((FBW-right)>>1); // center of construction, regardless of digit count
          int y=label->y+label->h+10;
          double fract=MODAL->deathclock+1.0-sec;
          int alpha=(int)(fract*255.0);
          if (alpha<1) alpha=1; else if (alpha>0xff) alpha=0xff;
          uint32_t primary=0x80808000|alpha;
          if (sec>=10) {
            graf_fancy(&g.graf,x-4,y,0x46+sec/10,0,0,NS_sys_tilesize,0,primary);
            graf_fancy(&g.graf,x+4,y,0x46+sec%10,0,0,NS_sys_tilesize,0,primary);
          } else {
            graf_fancy(&g.graf,x,y,0x46+sec,0,0,NS_sys_tilesize,0,primary);
          }
        }
      }
    }
  }
  
  { // Labels.
    struct label *label=MODAL->labelv;
    int i=MODAL->labelc;
    for (;i-->0;label++) {
      if (label->strix==STRIX_WARNING) {
        if (MODAL->deathclock<=0.0) continue; // hide until the lock detects an intruder
      }
      graf_set_input(&g.graf,label->texid);
      graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
    }
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_lockpick={
  .name="lockpick",
  .objlen=sizeof(struct modal_lockpick),
  .opaque=1,
  .interactive=1,
  .del=_lockpick_del,
  .init=_lockpick_init,
  .focus=_lockpick_focus,
  .update=_lockpick_update,
  .render=_lockpick_render,
};

/* Randomly pick a vacant cell not on the edge.
 * Caller must arrange to ask for no more than the available space ((FLDW-2)*(FLDH-2)==42, ie 21 pairs).
 */
 
static void lockpick_unused_cell(int *x,int *y,const struct modal *modal) {
  int panic=500;
  while (panic-->0) {
    *x=1+rand()%(FLDW-2);
    *y=1+rand()%(FLDH-2);
    if (!MODAL->fld[(*y)*FLDW+(*x)]) return;
  }
}

/* Public accessors.
 */
 
int modal_lockpick_setup(struct modal *modal,int flagid,int difficulty) {
  int blockidc=sizeof(color_by_blockid)/sizeof(color_by_blockid[0]);
  if (blockidc<2) { modal->defunct=1; return -1; }
  if (!modal||(modal->type!=&modal_type_lockpick)) return -1;
  MODAL->flagid=flagid;
  MODAL->difficulty=difficulty;
  
  /* Prepare the field.
   */
  int pairc=PAIRC_MIN+((difficulty*(PAIRC_MAX-PAIRC_MIN+1))>>8);
  if (pairc<PAIRC_MIN) pairc=PAIRC_MIN; else if (pairc>PAIRC_MAX) pairc=PAIRC_MAX;
  int i=pairc,blockid=1;
  while (i-->0) {
    int x,y;
    lockpick_unused_cell(&x,&y,modal);
    MODAL->fld[y*FLDW+x]=blockid;
    lockpick_unused_cell(&x,&y,modal);
    MODAL->fld[y*FLDW+x]=blockid;
    if (++blockid>=blockidc) blockid=1; // zero is not a real block
  }
  
  return 0;
}
