#include "game/wishbone.h"

/* Delete.
 */
 
void modal_del(struct modal *modal) {
  if (!modal) return;
  if (modal->type->del) modal->type->del(modal);
  free(modal);
}

/* New.
 */
 
struct modal *modal_new(const struct modal_type *type) {
  if (!type) return 0;
  struct modal *modal=calloc(1,type->objlen);
  if (!modal) return 0;
  modal->type=type;
  if (type->init&&(type->init(modal)<0)) {
    modal_del(modal);
    return 0;
  }
  return modal;
}

/* Push.
 */
 
int modal_push(struct modal *modal) {
  if (!modal) return -1;
  if (g.modalc>=g.modala) {
    int na=g.modala+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(g.modalv,sizeof(void*)*na);
    if (!nv) return -1;
    g.modalv=nv;
    g.modala=na;
  }
  g.modalv[g.modalc++]=modal; // HANDOFF
  return 0;
}

/* Check residence.
 */

int modal_is_resident(const struct modal *modal) {
  if (!modal) return 0;
  int i=g.modalc;
  struct modal **p=g.modalv;
  for (;i-->0;p++) if (*p==modal) return 1;
  return 0;
}

/* Reap defunct.
 */
 
void modal_reap_defunct() {
  int i=g.modalc;
  while (i-->0) {
    struct modal *modal=g.modalv[i];
    if (!modal->defunct) continue;
    g.modalc--;
    memmove(g.modalv+i,g.modalv+i+1,sizeof(void*)*(g.modalc-i));
    modal_del(modal);
  }
}

/* Spawn.
 */

struct modal *modal_spawn(const struct modal_type *type) {
  struct modal *modal=modal_new(type);
  if (!modal) return 0;
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
