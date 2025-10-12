#include "game/wishbone.h"

/* Delete.
 */
 
void sprite_del(struct sprite *sprite) {
  if (!sprite) return;
  if (sprite->type->del) sprite->type->del(sprite);
  free(sprite);
}

/* New.
 */
 
struct sprite *sprite_new(const struct sprite_type *type,double x,double y,uint32_t arg,const void *res,int resc,int spriteid) {
  if (!type) return 0;
  struct sprite *sprite=calloc(1,type->objlen);
  if (!sprite) return 0;
  sprite->type=type;
  sprite->x=x;
  sprite->y=y;
  sprite->arg=arg;
  sprite->res=res;
  sprite->resc=resc;
  sprite->spriteid=spriteid;
  sprite->layer=100;
  
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,res,resc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_image: sprite->imageid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
        case CMD_sprite_tile: sprite->tileid=cmd.arg[0]; sprite->xform=cmd.arg[1]; break;
        case CMD_sprite_type: break; // Already managed by our caller, hopefully.
        case CMD_sprite_layer: sprite->layer=(cmd.arg[0]<<8)|cmd.arg[1]; break;
        case CMD_sprite_phymask: sprite->phymask=(cmd.arg[0]<<24)|(cmd.arg[1]<<16)|(cmd.arg[2]<<8)|cmd.arg[3]; break;
      }
    }
  }
  
  if (type->init&&((type->init(sprite)<0)||sprite->defunct)) {
    sprite_del(sprite);
    return 0;
  }
  return sprite;
}

/* Handoff to global registry and return self as a convenience.
 * Deletes (sprite) in case of failure.
 */
 
static struct sprite *sprite_list(struct sprite *sprite) {
  if (!sprite) return 0;
  if (g.spritec>=g.spritea) {
    int na=g.spritea+32;
    if (na>INT_MAX/sizeof(void*)) { sprite_del(sprite); return 0; }
    void *nv=realloc(g.spritev,sizeof(void*)*na);
    if (!nv) { sprite_del(sprite); return 0; }
    g.spritev=nv;
    g.spritea=na;
  }
  g.spritev[g.spritec++]=sprite;
  return sprite;
}

void sprite_relist(struct sprite *sprite) {
  sprite_list(sprite);
}

/* Spawn.
 */

struct sprite *sprite_spawn_type(const struct sprite_type *type,double x,double y,uint32_t arg) {
  struct sprite *sprite=sprite_new(type,x,y,arg,0,0,0);
  return sprite_list(sprite);
}

struct sprite *sprite_spawn_res(int spriteid,double x,double y,uint32_t arg) {
  const struct sprite_type *type=0;
  const void *res=0;
  int resc=res_get(&res,EGG_TID_sprite,spriteid);
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,res,resc)<0) return 0;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_sprite_type: type=sprite_type_by_sprtype((cmd.arg[0]<<8)|cmd.arg[1]); goto _done_resource_;
    }
  }
 _done_resource_:;
  if (!type) return 0;
  struct sprite *sprite=sprite_new(type,x,y,arg,res,resc,spriteid);
  return sprite_list(sprite);
}

/* Validate live sprite.
 */

int sprite_is_resident(const struct sprite *sprite) {
  if (!sprite) return 0;
  struct sprite **p=g.spritev;
  int i=g.spritec;
  for (;i-->0;p++) if (*p==sprite) return 1;
  return 0;
}

/* Reap defunct sprites.
 */
 
void sprite_reap_defunct() {
  int i=g.spritec;
  while (i-->0) {
    struct sprite *sprite=g.spritev[i];
    if (!sprite->defunct) continue;
    if (sprite==g.hero) g.hero=0;
    g.spritec--;
    memmove(g.spritev+i,g.spritev+i+1,sizeof(void*)*(g.spritec-i));
    sprite_del(sprite);
  }
}

/* Type by id.
 */
 
const struct sprite_type *sprite_type_by_sprtype(int sprtype) {
  switch (sprtype) {
    #define _(tag) case NS_sprtype_##tag: return &sprite_type_##tag;
    FOR_EACH_SPRTYPE
    #undef _
  }
  return 0;
}
