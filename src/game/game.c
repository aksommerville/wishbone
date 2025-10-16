#include "wishbone.h"

/* Reset entirely.
 */
 
int game_reset() {
  // Let load_map() delete the sprites. It will, and if we clear (g.hero) it will delete hero sprites too.
  g.hero=0;
  g.map=0;
  g.map_dirty=1;
  g.qposc=0;
  g.forgottenc=0;
  g.hp=g.maxhp=3; // 3 hearts is a hallowed tradition.
  g.item=0;
  memset(g.flags,0,sizeof(g.flags));
  g.flags[0]=0x02; // (NS_flag_zero,NS_flag_one) (0,1) must have values (0,1).
  g.playtime=0.0;
  g.victory=0;
  egg_play_song(RID_song_into_the_dirt,0,1);
  if (load_map(RID_map_start)<0) return -1;
  //if (load_map(RID_map_boss)<0) return -1;//XXX while testing end of game
  return 0;
}

/* Load individual fields of saved game.
 * On any error, these functions will ensure a sane global state.
 * (and on successes too, obviously).
 */
 
static int decuint_eval(int *dst,const char *src,int srcc) {
  if (srcc<1) return -1;
  *dst=0;
  for (;srcc-->0;src++) {
    int digit=(*src)-'0';
    if ((digit<0)||(digit>9)) { *dst=0; return -1; }
    if (*dst>INT_MAX/10) { *dst=0; return -1; }
    (*dst)*=10;
    if (*dst>INT_MAX-digit) { *dst=0; return -1; }
    (*dst)+=digit;
  }
  return 0;
}
 
static int game_load_hp(const char *src,int srcc) {
  if (decuint_eval(&g.hp,src,srcc)<0) { g.hp=g.maxhp; return -1; }
  if ((g.hp<3)||(g.hp>10)) { g.hp=g.maxhp; return -1; }
  g.maxhp=g.hp;
  return 0;
}

static int game_load_flags(const char *src,int srcc) {
  int dstp=0,shift=4,srcp=0;
  for (;srcp<srcc;srcp++) {
    int digit;
         if ((src[srcp]>='0')&&(src[srcp]<='9')) digit=src[srcp]-'0';
    else if ((src[srcp]>='a')&&(src[srcp]<='f')) digit=src[srcp]-'a'+10;
    else if ((src[srcp]>='A')&&(src[srcp]<='F')) digit=src[srcp]-'A'+10;
    else return -1;
    if (dstp<sizeof(g.flags)) {
      g.flags[dstp]|=digit<<shift;
    }
    if (shift) shift=0;
    else { shift=4; dstp++; }
  }
  g.flags[0]=(g.flags[0]&~3)|2; // Ensure (zero,one) are (0,1), not allowed to be otherwise.
  return 0;
}

static int game_load_forgotten(const char *src,int srcc) {
  int srcp=0,tokenc;
  const char *token;
  while (srcp<srcc) {
    #define NEXTTOKEN { \
      token=src+srcp; \
      tokenc=0; \
      while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++; \
    }
    int mapid,x,y,prizeid;
    NEXTTOKEN if (decuint_eval(&mapid,token,tokenc)<0) return -1;
    NEXTTOKEN if (decuint_eval(&x,token,tokenc)<0) return -1;
    NEXTTOKEN if (decuint_eval(&y,token,tokenc)<0) return -1;
    NEXTTOKEN if (decuint_eval(&prizeid,token,tokenc)<0) return -1;
    #undef NEXTTOKEN
    if (g.forgottenc<FORGOTTEN_LIMIT) { // Drop them if we're full. (but keep going, for validation purposes)
      struct forgotten *f=g.forgottenv+g.forgottenc++;
      f->mapid=mapid;
      f->x=x;
      f->y=y;
      f->prizeid=prizeid;
      f->forgottenid=g.forgottenc;
    }
  }
  return 0;
}

static int game_load_playtime(const char *src,int srcc) {
  int ms=0;
  if (decuint_eval(&ms,src,srcc)<0) return -1;
  g.playtime=(double)ms/1000.0;
  return 0;
}

/* Reset game from saved state.
 * Saved game is a semicolon-delimited string: HP ; FLAGS ; FORGOTTEN ; PLAYTIME
 *  - HP is a decimal integer (3..6).
 *  - FLAGS is a hex dump.
 *  - FORGOTTEN is comma-delimited decimal integers: MAPID,X,Y,PRIZEID,...
 *  - PLAYTIME is a decimal integer, milliseconds.
 * Whitespace is not permitted anywhere.
 * No errors if input malformed, we just start the game from scratch then.
 */
 
int game_load(const char *src,int srcc) {

  // Start with a regular reset. And if the saved game is empty, that's all.
  if (game_reset()<0) return -1;
  if (!src||(srcc<1)) return 0;
  
  /* Each field decoder will leave its own purview sane, pass or fail.
   * If one step fails, just stop right there.
   * So you might end up with a partial load in error cases, but never an invalid state.
   */
  int srcp=0,tokenc;
  const char *token;
  #define NEXTTOKEN { \
    token=src+srcp; \
    tokenc=0; \
    while ((srcp<srcc)&&(src[srcp++]!=';')) tokenc++; \
  }
  NEXTTOKEN if (game_load_hp(token,tokenc)<0) return 0;
  NEXTTOKEN if (game_load_flags(token,tokenc)<0) return 0;
  NEXTTOKEN if (game_load_forgotten(token,tokenc)<0) return 0;
  NEXTTOKEN if (game_load_playtime(token,tokenc)<0) return 0;
  #undef NEXTTOKEN
  
  // And some odd cleanup...
  if (flag_get(NS_flag_wishbone)) g.item=NS_flag_wishbone;
  
  return 0;
}

/* Save game, text only.
 */
 
static int decuint_repr(char *dst,int dsta,int v) {
  if (v<0) v=0;
  int limit=10,dstc=1;
  while (v>=limit) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
  if (dstc<=dsta) {
    int i=dstc;
    for (;i-->0;v/=10) dst[i]='0'+v%10;
  }
  return dstc;
}
 
int game_encode(char *dst,int dsta) {
  int dstc=0;
  
  dstc+=decuint_repr(dst+dstc,dsta-dstc,g.maxhp);
  if (dstc<dsta) dst[dstc]=';'; dstc++;
  
  int i=sizeof(g.flags);
  const uint8_t *src=g.flags;
  for (;i-->0;src++) {
    if (dstc<dsta) dst[dstc]="0123456789abcdef"[(*src)>>4]; dstc++;
    if (dstc<dsta) dst[dstc]="0123456789abcdef"[(*src)&15]; dstc++;
  }
  if (dstc<dsta) dst[dstc]=';'; dstc++;
  
  const struct forgotten *f=g.forgottenv;
  for (i=g.forgottenc;i-->0;f++) {
    dstc+=decuint_repr(dst+dstc,dsta-dstc,f->mapid);
    if (dstc<dsta) dst[dstc]=','; dstc++;
    dstc+=decuint_repr(dst+dstc,dsta-dstc,f->x);
    if (dstc<dsta) dst[dstc]=','; dstc++;
    dstc+=decuint_repr(dst+dstc,dsta-dstc,f->y);
    if (dstc<dsta) dst[dstc]=','; dstc++;
    dstc+=decuint_repr(dst+dstc,dsta-dstc,f->prizeid);
    if (dstc<dsta) dst[dstc]=','; dstc++;
  }
  if ((dstc<=dsta)&&(dst[dstc-1]==',')) dstc--;
  if (dstc<dsta) dst[dstc]=';'; dstc++;
  
  int ms=(int)(g.playtime*1000.0);
  if (ms<0) ms=0;
  dstc+=decuint_repr(dst+dstc,dsta-dstc,ms);
  
  return dstc;
}

/* Replace (g.physics).
 */
 
static void load_physics(int tilesheetid) {
  memset(g.physics,0,sizeof(g.physics));
  const void *src=0;
  int srcc=res_get(&src,EGG_TID_tilesheet,tilesheetid);
  struct tilesheet_reader reader;
  if (tilesheet_reader_init(&reader,src,srcc)<0) return;
  struct tilesheet_entry entry;
  while (tilesheet_reader_next(&entry,&reader)>0) {
    if (entry.tableid!=NS_tilesheet_physics) continue;
    memcpy(g.physics+entry.tileid,entry.v,entry.c);
  }
}

/* Is any sprite near this cell?
 */
 
static int sprite_near_cell(int col,int row) {
  double l=col+0.250;
  double r=col+0.750;
  double t=row+0.250;
  double b=row+0.750;
  struct sprite **p=g.spritev;
  int i=g.spritec;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->x<l) continue;
    if (sprite->x>r) continue;
    if (sprite->y<t) continue;
    if (sprite->y>b) continue;
    return 1;
  }
  return 0;
}

/* Select a position for a randomly-spawning monster.
 * Returns nonzero if we picked something valid, zero to cancel.
 */
 
static int choose_spawn_point(double *x,double *y) {
  const int margin=2; // Don't spawn near the edges.
  const int x0=margin,y0=margin,w=NS_sys_mapw-(margin<<1),h=NS_sys_maph-(margin<<1);
  
  /* We're going to do something smarter than the obvious drunk-stab.
   * Catalogue all the valid cells, being mindful of sprites in addition to the map.
   * Then select randomly from that set.
   */
  uint16_t candidatev[NS_sys_mapw*NS_sys_maph]; // (y<<8)|x
  int candidatec=0;
  const uint8_t *rowstart=g.map->v+y0*NS_sys_mapw+x0;
  int row=y0,yi=h;
  for (;yi-->0;row++,rowstart+=NS_sys_mapw) {
    const uint8_t *p=rowstart;
    int col=x0,xi=w;
    for (;xi-->0;col++,p++) {
      if (!g.physics[*p]) {
        if (!sprite_near_cell(col,row)) {
          candidatev[candidatec++]=(row<<8)|col;
        }
      }
    }
  }
  if (candidatec<1) return 0;
  int candidatep=rand()%candidatec;
  *x=(candidatev[candidatep]&0xff)+0.5;
  *y=(candidatev[candidatep]>>8)+0.5;
  return 1;
}

/* Load map.
 */
 
int load_map(int mapid) {
  struct map *map=res_get_map(mapid);
  if (!map) return -1;
  
  if (g.map) {
    // Drop qpos.
    while (g.qposc>0) qpos_release(g.qposv[g.qposc-1].x,g.qposv[g.qposc-1].y);
  }
  
  /* Delete all sprites.
   * We're allowed to do this because we can only be called in between sprite-list updates.
   * Capture the hero, don't delete her.
   */
  struct sprite *hero=0;
  while (g.spritec>0) {
    g.spritec--;
    struct sprite *sprite=g.spritev[g.spritec];
    if (sprite==g.hero) hero=sprite;
    else sprite_del(sprite);
  }
  g.hero=hero; // Just in case (g.hero) was pointing to something nonresident before (shouldn't be possible).
  
  /* Restore all cells to their default.
   */
  memcpy(map->v,map->kv,NS_sys_mapw*NS_sys_maph);
  
  /* If the maps have different imageid, replace (g.physics).
   */
  if (!g.map||(g.map->imageid!=map->imageid)) {
    load_physics(map->imageid);
  }
  
  g.has_spawn=0;
  g.map_dirty=0; // I guess technically it is dirty, but modal_play already knows it's changing.
  g.map=map;
  if (hero) sprite_relist(hero);
  
  /* Run initial commands.
   */
  struct cmdlist_reader reader;
  if (cmdlist_reader_init(&reader,map->cmd,map->cmdc)<0) return -1;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
    
      case CMD_map_song: egg_play_song((cmd.arg[0]<<8)|cmd.arg[1],0,1); break;

      case CMD_map_lock: {
          int x=cmd.arg[0];
          int y=cmd.arg[1];
          if ((x<NS_sys_mapw)&&(y<NS_sys_maph)) {
            if (flag_get(cmd.arg[2])) map->v[y*NS_sys_mapw+x]++;
          }
        } break;
        
      case CMD_map_switchable:
      case CMD_map_stompbox: {
          int x=cmd.arg[0];
          int y=cmd.arg[1];
          if ((x<NS_sys_mapw)&&(y<NS_sys_maph)) {
            if (flag_get(cmd.arg[2])!=cmd.arg[3]) map->v[y*NS_sys_mapw+x]++;
          }
        } break;
        
      // treadles reset when they become visible.
      case CMD_map_treadle: flag_set(cmd.arg[2],cmd.arg[3]); break;

      case CMD_map_sprite: {
          double x=cmd.arg[0]+0.5;
          double y=cmd.arg[1]+0.5;
          int spriteid=(cmd.arg[2]<<8)|cmd.arg[3];
          uint32_t arg=(cmd.arg[4]<<24)|(cmd.arg[5]<<16)|(cmd.arg[6]<<8)|cmd.arg[7];
          if ((spriteid==RID_sprite_hero)&&hero) {
            // We already have a hero; don't create a new one.
          } else {
            sprite_spawn_res(spriteid,x,y,arg);
          }
        } break;
        
      case CMD_map_spawn: {
          uint8_t flagid=cmd.arg[3];
          if (!flagid||!flag_get(flagid)) {
            int spriteid=(cmd.arg[0]<<8)|cmd.arg[1];
            uint32_t arg=(cmd.arg[4]<<24)|(cmd.arg[5]<<16)|(cmd.arg[6]<<8)|cmd.arg[7];
            int c=cmd.arg[2];
            while (c-->0) {
              double x,y;
              if (!choose_spawn_point(&x,&y)) break;
              sprite_spawn_res(spriteid,x,y,arg);
              if (flagid) g.has_spawn=1;
            }
          }
        } break;
      
    }
  }
  
  // Place forgotten things.
  {
    struct forgotten *f=g.forgottenv;
    int i=g.forgottenc;
    for (;i-->0;f++) {
      if (f->mapid!=g.map->rid) continue;
      struct sprite *treasure=sprite_spawn_res(RID_sprite_wishbone,f->x+0.5,f->y+0.5,(f->prizeid<<16)|(f->forgottenid<<8));
    }
  }
  
  //TODO song?
  //TODO generic inbound triggers
  
  return 0;
}

/* Recheck POI after a flag change.
 */
 
static void recheck_poi(int flagid,int v) {
  if (!g.map) return;
  struct cmdlist_reader reader;
  if (cmdlist_reader_init(&reader,g.map->cmd,g.map->cmdc)<0) return;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {

      // Stompboxes change when something else actuates the same flag. A little weird but we have to.
      // Treadles do not.
      case CMD_map_switchable:
      case CMD_map_stompbox: {
          if (cmd.arg[2]!=flagid) break;
          int x=cmd.arg[0];
          int y=cmd.arg[1];
          if ((x<NS_sys_mapw)&&(y<NS_sys_maph)) {
            int p=y*NS_sys_mapw+x;
            if (flag_get(cmd.arg[2])!=cmd.arg[3]) g.map->v[p]=g.map->kv[p]+1;
            else g.map->v[p]=g.map->kv[p];
            g.map_dirty=1;
          }
        } break;
      case CMD_map_lock: {
          if (cmd.arg[2]!=flagid) break;
          int x=cmd.arg[0];
          int y=cmd.arg[1];
          if ((x<NS_sys_mapw)&&(y<NS_sys_maph)) {
            int p=y*NS_sys_mapw+x;
            if (flag_get(cmd.arg[2])) g.map->v[p]=g.map->kv[p]+1;
            else g.map->v[p]=g.map->kv[p];
            g.map_dirty=1;
          }
        } break;
    }
  }
}

/* Flags.
 */
 
int flag_get(int flagid) {
  if ((flagid<0)||(flagid>=NS_FLAG_COUNT)) return 0;
  return (g.flags[flagid>>3]&(1<<(flagid&7)))?1:0;
}

int flag_set(int flagid,int v) {
  if ((flagid<2)||(flagid>=NS_FLAG_COUNT)) return 0; // sic 2, flags 0 and 1 are immutable
  uint8_t *p=g.flags+(flagid>>3);
  uint8_t mask=1<<(flagid&7);
  if (v) {
    if ((*p)&mask) return 0;
    (*p)|=mask;
  } else {
    if (!((*p)&mask)) return 0;
    (*p)&=~mask;
  }
  
  // Notify sprites.
  struct sprite **spritep=g.spritev;
  int i=g.spritec;
  for (;i-->0;spritep++) {
    struct sprite *sprite=*spritep;
    if (sprite->defunct) continue;
    if (!sprite->type->flag) continue;
    sprite->type->flag(sprite,flagid,v);
  }
  
  recheck_poi(flagid,v?1:0);
  g.saved_game_dirty=1;
  return 1;
}

/* Quantized cell is newly held or released.
 */
 
static int qpos_set_flag(int x,int y,int v) {
  int result=0;
  struct cmdlist_reader reader;
  if (cmdlist_reader_init(&reader,g.map->cmd,g.map->cmdc)<0) return 0;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
    
      case CMD_map_treadle: { // XXX we don't actually use treadles anywhere
          if (x!=cmd.arg[0]) break;
          if (y!=cmd.arg[1]) break;
          result++;
          flag_set(cmd.arg[2],v!=cmd.arg[3]);
          int p=y*NS_sys_mapw+x;
          if (v==cmd.arg[3]) g.map->v[p]=g.map->kv[p];
          else g.map->v[p]=g.map->kv[p]+1;
          g.map_dirty=1;
        } break;
      
      case CMD_map_stompbox: {
          if (x!=cmd.arg[0]) break;
          if (y!=cmd.arg[1]) break;
          result++;
          if (v==1) { // Stompboxes only change the flag on entry.
            SFX(treadle_on)
            flag_set(cmd.arg[2],flag_get(cmd.arg[2])?0:1);
          } else {
            SFX(treadle_off)
          }
        } break;
    }
  }
  return result;
}

/* Registry of transient quantized positions held by a sprite.
 */
 
void qpos_press(int x,int y) {
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return;
  if (g.qposc>=QPOS_LIMIT) return;
  struct qpos *qpos=g.qposv;
  int i=g.qposc,already=0;
  for (;i-->0;qpos++) if ((qpos->x==x)&&(qpos->y==y)) { already=1; break; }
  qpos->x=x;
  qpos->y=y;
  g.qposc++;
  if (!already) {
    if (!qpos_set_flag(x,y,1)) {
      // Nothing interesting here, don't record it.
      g.qposc--;
    }
  }
}

void qpos_release(int x,int y) {
  int i=g.qposc,rmc=0;
  struct qpos *qpos=g.qposv+i-1;
  for (;i-->0;qpos--) {
    if (qpos->x!=x) continue;
    if (qpos->y!=y) continue;
    if (rmc) return; // We removed one but it's still held.
    rmc++;
    g.qposc--;
    memmove(qpos,qpos+1,sizeof(struct qpos)*(g.qposc-i));
  }
  if (!rmc) return; // No record of it.
  qpos_set_flag(x,y,0);
}

/* Forgotten things.
 */
 
int forgotten_add(int mapid,int x,int y,int prizeid) {
  if (g.forgottenc>=FORGOTTEN_LIMIT) return -1;
  if (g.forgottenid_next<1) g.forgottenid_next=1;
  struct forgotten *f=g.forgottenv+g.forgottenc++;
  f->mapid=mapid;
  f->x=x;
  f->y=y;
  f->prizeid=prizeid;
  f->forgottenid=g.forgottenid_next++;
  g.saved_game_dirty=1;
  return f->forgottenid;
}

void forgotten_remove(int forgottenid) {
  int i=g.forgottenc;
  while (i-->0) {
    struct forgotten *f=g.forgottenv+i;
    if (f->forgottenid!=forgottenid) continue;
    g.forgottenc--;
    memmove(f,f+1,sizeof(struct forgotten)*(g.forgottenc-i));
    g.saved_game_dirty=1;
    return;
  }
}

/* Is there a sprite of the given rid?
 */
 
static int spriteid_in_play(int spriteid) {
  struct sprite **p=g.spritev;
  int i=g.spritec;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue; // Very important to check this. We typically get called when the monster in question is defunct but not yet removed.
    if (sprite->spriteid==spriteid) return 1;
  }
  return 0;
}

/* If all spawned sprites are dead, set the appropriate flag.
 * Anyone killing a monster should call this after, if (g.has_spawn).
 */
 
void check_dead_spawn() {
  struct cmdlist_reader reader;
  if (cmdlist_reader_init(&reader,g.map->cmd,g.map->cmdc)<0) return;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_spawn) {
      uint8_t flagid=cmd.arg[3];
      if (!flagid||flag_get(flagid)) continue;
      int spriteid=(cmd.arg[0]<<8)|cmd.arg[1];
      if (!spriteid_in_play(spriteid)) {
        flag_set(flagid,1);
      }
    }
  }
}
