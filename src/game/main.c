#include "wishbone.h"

struct g g={0};

void egg_client_quit(int status) {
}

int egg_client_init() {

  int fbw=0,fbh=0;
  egg_texture_get_size(&fbw,&fbh,1);
  if ((fbw!=FBW)||(fbh!=FBH)) {
    fprintf(stderr,"Framebuffer size mismatch! metadata=%dx%d header=%dx%d\n",fbw,fbh,FBW,FBH);
    return -1;
  }

  g.romc=egg_rom_get(0,0);
  if (!(g.rom=malloc(g.romc))) return -1;
  egg_rom_get(g.rom,g.romc);
  if (res_load()<0) return -1;
  
  if (!(g.font=font_new())) return -1;
  if (font_add_image(g.font,RID_image_font_0020,0x0020)<0) return -1;
  
  srand_auto();
  
  //TODO Start with modal_type_hello, and have it do these before dismissing:
  //if (game_reset()<0) return -1;
  //if (!modal_spawn(&modal_type_play)) return -1;
  if (!modal_spawn(&modal_type_hello)) return -1;

  return 0;
}

/* Update.
 */

void egg_client_update(double elapsed) {

  // Poll input.
  int input=egg_input_get_one(0);
  int pvinput=g.pvinput;
  if (input!=g.pvinput) {
    // If we ever need global keystrokes, here they are.
    g.pvinput=input;
  }
  
  // Determine which modal should receive the update.
  // Flash focus if needed.
  struct modal *nfocus=0;
  int i=g.modalc;
  while (i-->0) {
    struct modal *q=g.modalv[i];
    if (q->defunct) continue;
    if (!q->type->interactive) continue;
    nfocus=q;
    break;
  }
  if (nfocus!=g.modal_focus) {
    if (modal_is_resident(g.modal_focus)) {
      if (g.modal_focus->type->focus) g.modal_focus->type->focus(g.modal_focus,0);
    }
    g.modal_focus=nfocus;
    if (nfocus) {
      if (nfocus->type->focus) nfocus->type->focus(nfocus,1);
    }
  }
  
  // Update the focussed modal.
  if (nfocus) {
    if (nfocus->type->update) nfocus->type->update(nfocus,elapsed,input,pvinput);
  } else {
    fprintf(stderr,"Modal stack depleted.\n");
    egg_terminate(1);
    return;
  }
  
  modal_reap_defunct();
}

/* Render.
 */

void egg_client_render() {
  g.framec++;
  graf_reset(&g.graf);
  
  // Find the highest opaque modal.
  int opaquep=g.modalc;
  while (opaquep-->0) {
    struct modal *modal=g.modalv[opaquep];
    if (modal->defunct) continue;
    if (!modal->type->opaque) continue;
    break;
  }
  
  // If we don't have an opaque modal, fill black. This shouldn't happen.
  if (opaquep<0) {
    opaquep=0;
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  }
  
  // Render all live modals starting at (opaquep).
  for (;opaquep<g.modalc;opaquep++) {
    struct modal *modal=g.modalv[opaquep];
    if (modal->defunct) continue;
    if (modal->type->render) modal->type->render(modal);
  }
  
  graf_flush(&g.graf);
}

/* Sound effect.
 */
 
void wishbone_sound(int rid) {
  //TODO limiter? pan?
  egg_play_sound(rid,1.0,0.0);
}
