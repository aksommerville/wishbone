#include "wishbone.h"

/* Map registry.
 */
 
static int res_mapv_search(int rid) {
  int lo=0,hi=g.mapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct map *q=g.mapv+ck;
         if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct map *res_mapv_insert(int p,int rid) {
  if ((p<0)||(p>g.mapc)) return 0;
  if (g.mapc>=g.mapa) {
    int na=g.mapa+32;
    if (na>INT_MAX/sizeof(struct map)) return 0;
    void *nv=realloc(g.mapv,sizeof(struct map)*na);
    if (!nv) return 0;
    g.mapv=nv;
    g.mapa=na;
  }
  struct map *map=g.mapv+g.mapc++;
  memset(map,0,sizeof(struct map));
  map->rid=rid;
  return map;
}

struct map *res_get_map(int rid) {
  int p=res_mapv_search(rid);
  if (p<0) return 0;
  return g.mapv+p;
}

/* Validate and register map.
 */
 
static int res_welcome_map(int rid,const void *serial,int serialc) {
  struct map_res mr;
  if (map_res_decode(&mr,serial,serialc)<0) return -1;
  if ((mr.w!=NS_sys_mapw)||(mr.h!=NS_sys_maph)) {
    fprintf(stderr,"map:%d size %d,%d must be %d,%d\n",rid,mr.w,mr.h,NS_sys_mapw,NS_sys_maph);
    return -1;
  }
  int p=res_mapv_search(rid);
  if (p>=0) return -1;
  p=-p-1;
  struct map *map=res_mapv_insert(p,rid);
  if (!map) return -1;
  map->kv=mr.v;
  map->cmd=mr.cmd;
  map->cmdc=mr.cmdc;
  
  struct cmdlist_reader reader;
  if (cmdlist_reader_init(&reader,map->cmd,map->cmdc)<0) return -1;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_image: map->imageid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_neighbors: {
          map->neiw=(cmd.arg[0]<<8)|cmd.arg[1];
          map->neie=(cmd.arg[2]<<8)|cmd.arg[3];
          map->nein=(cmd.arg[4]<<8)|cmd.arg[5];
          map->neis=(cmd.arg[6]<<8)|cmd.arg[7];
        } break;
    }
  }
  return 0;
}

/* Receive resource initially.
 * The TOC is complete, but peer resources have not necessarily been welcomed yet.
 */
 
static int res_welcome(const struct rom_entry *res) {
  switch (res->tid) {
    case EGG_TID_map: return res_welcome_map(res->rid,res->v,res->c);
  }
  return 0;
}

/* Load resources, happens only at app init.
 */
 
int res_load() {
  struct rom_reader reader;
  if (rom_reader_init(&reader,g.rom,g.romc)<0) return -1;
  struct rom_entry entry;
  while (rom_reader_next(&entry,&reader)>0) {
    if (g.resc>=g.resa) {
      int na=g.resa+64;
      if (na>INT_MAX/sizeof(struct rom_entry)) return -1;
      void *nv=realloc(g.resv,sizeof(struct rom_entry)*na);
      if (!nv) return -1;
      g.resv=nv;
      g.resa=na;
    }
    g.resv[g.resc++]=entry;
  }
  struct rom_entry *res=g.resv;
  int i=g.resc;
  for (;i-->0;res++) {
    if (res_welcome(res)<0) return -1;
  }
  return 0;
}

/* Get resource.
 */

int res_search(int tid,int rid) {
  int lo=0,hi=g.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rom_entry *q=g.resv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int res_get(void *dstpp,int tid,int rid) {
  int p=res_search(tid,rid);
  if (p<0) return 0;
  const struct rom_entry *res=g.resv+p;
  *(const void**)dstpp=res->v;
  return res->c;
}

/* Get string.
 */
 
int res_get_string(void *dstpp,int rid,int strix) {
  if (strix<0) return 0;
  if ((rid<1)||(rid>=0x40)) return 0;
  const void *src;
  int srcc=res_get(&src,EGG_TID_strings,(egg_prefs_get(EGG_PREF_LANG)<<6)|rid);
  struct strings_reader reader;
  if (strings_reader_init(&reader,src,srcc)<0) return 0;
  struct strings_entry string;
  while (strings_reader_next(&string,&reader)>0) {
    if (string.index<strix) continue;
    if (string.index>strix) return 0;
    *(const void**)dstpp=string.v;
    return string.c;
  }
  return 0;
}
