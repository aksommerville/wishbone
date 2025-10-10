/* modal.h
 * Generic objects representing a layer of the global UI.
 */
 
#ifndef MODAL_H
#define MODAL_H

struct modal;
struct modal_type;

struct modal {
  const struct modal_type *type;
  int defunct;
};

struct modal_type {
  const char *name;
  int objlen;
  int opaque; // Nonzero if nothing below me needs to render.
  int interactive; // Nonzero if I want update events. Otherwise they go to something below me.
  void (*del)(struct modal *modal);
  int (*init)(struct modal *modal);
  void (*focus)(struct modal *modal,int focus);
  void (*update)(struct modal *modal,double elapsed,int input,int pvinput);
  void (*render)(struct modal *modal);
};

/* del, new, and push should only be called by other modal functions.
 * To create a modal, use 'spawn', and to delete it, set 'defunct'.
 */
void modal_del(struct modal *modal);
struct modal *modal_new(const struct modal_type *type);
int modal_push(struct modal *modal); // HANDOFF

// Checks stack without dereferencing (modal).
int modal_is_resident(const struct modal *modal);

void modal_reap_defunct();

struct modal *modal_spawn(const struct modal_type *type);

extern const struct modal_type modal_type_play;
extern const struct modal_type modal_type_dialogue;
extern const struct modal_type modal_type_gameover;
extern const struct modal_type modal_type_hello;

#endif
