#include "wishbone.h"

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
        
      case CMD_map_door: break;//TODO? u16:position, u16:mapid, u16:dstposition, u16:arg
      
    }
  }
  
  // Place forgotten things.
  {
    struct forgotten *f=g.forgottenv;
    int i=g.forgottenc;
    for (;i-->0;f++) {
      if (f->mapid!=g.map->rid) continue;
      struct sprite *treasure=sprite_spawn_res(RID_sprite_wishbone,f->x+0.5,f->y+0.5,(f->flagid<<24)|(f->forgottenid<<8));
      fprintf(stderr,"created forgottenid=%d sprite=%p\n",f->forgottenid,treasure);
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
    
      case CMD_map_treadle: {
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
            flag_set(cmd.arg[2],flag_get(cmd.arg[2])?0:1);
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
 
int forgotten_add(int mapid,int x,int y,int flagid) {
  if (g.forgottenc>=FORGOTTEN_LIMIT) return -1;
  if (g.forgottenid_next<1) g.forgottenid_next=1;
  struct forgotten *f=g.forgottenv+g.forgottenc++;
  f->mapid=mapid;
  f->x=x;
  f->y=y;
  f->flagid=flagid;
  f->forgottenid=g.forgottenid_next++;
  return f->forgottenid;
}

void forgotten_remove(int forgottenid) {
  int i=g.forgottenc;
  while (i-->0) {
    struct forgotten *f=g.forgottenv+i;
    if (f->forgottenid!=forgottenid) continue;
    g.forgottenc--;
    memmove(f,f+1,sizeof(struct forgotten)*(g.forgottenc-i));
    return;
  }
}
