/* sprite.h
 */
 
#ifndef SPRITE_H
#define SPRITE_H

struct sprite;
struct sprite_type;

struct sprite {
  const struct sprite_type *type;
  int defunct;
  double x,y;
  int layer;
  int imageid;
  uint8_t tileid,xform;
  uint32_t arg;
  int spriteid; // Resource, if we were spawned that way.
  const void *res; // resource (includes signature; use sprite_reader_init, not cmdlist_reader_init)
  int resc;
  int solid;
};

struct sprite_type {
  const char *name;
  int objlen;
  void (*del)(struct sprite *sprite);
  int (*init)(struct sprite *sprite); // Return <0 or set (defunct) to cleanly decline construction. Does not count as an error.
  void (*update)(struct sprite *sprite,double elapsed);
  
  // Implement only if you need more than a single tile.
  void (*render)(struct sprite *sprite,int dstx,int dsty);
  
  void (*focus)(struct sprite *sprite,int focus);
  
  // You don't subscribe to flags individually, but a sprite can receive *all* flag changes.
  void (*flag)(struct sprite *sprite,int flagid,int v);
};

/* Only the sprite stack itself should call del or new.
 * Others should use 'spawn' to create, and set 'defunct' to delete.
 */
void sprite_del(struct sprite *sprite);
struct sprite *sprite_new(const struct sprite_type *type,double x,double y,uint32_t arg,const void *cmd,int cmdc,int spriteid);
void sprite_reap_defunct();
void sprite_relist(struct sprite *sprite); // only game.c should use this, to reinsert the hero during a transition

struct sprite *sprite_spawn_type(const struct sprite_type *type,double x,double y,uint32_t arg);
struct sprite *sprite_spawn_res(int spriteid,double x,double y,uint32_t arg);

const struct sprite_type *sprite_type_by_sprtype(int sprtype);

#define _(tag) extern const struct sprite_type sprite_type_##tag;
FOR_EACH_SPRTYPE
#undef _

// Physics.
int sprite_move(struct sprite *sprite,double dx,double dy);

void sprite_hero_input(struct sprite *sprite,int input,int pvinput);

#endif
