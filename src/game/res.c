#include "wishbone.h"

/* Receive resource initially.
 * The TOC is complete, but peer resources have not necessarily been welcomed yet.
 */
 
static int res_welcome(const struct rom_entry *res) {
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
