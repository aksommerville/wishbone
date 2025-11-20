/* modal_victory.c
 * Final cutscene, credits, and stats readout.
 */
 
#include "game/wishbone.h"

#define DECAL_LIMIT 16
#define SCENEH 128
#define STRIX_HEY_DRAGON 22
#define STRIX_WISH_YOU_WERE_DEAD 23
#define STRIX_YOU_WIN 24
#define STRIX_TIME 25
#define STRIX_HEARTS 26
#define STRIX_THANKS 27
#define CAPTION_MARGIN 6

// Master timing.
//TODO tweak until it feels right, and write the music to match
#define TIME_HEY_DRAGON           1.0
#define TIME_BETWEEN_SPEECH       3.0
#define TIME_BRANDISH             4.0
#define TIME_WISH_YOU_WERE_DEAD   5.0
#define TIME_BREAK1               7.0
#define TIME_BREAK2               8.0
#define TIME_IDLE                 9.0
#define TIME_LAND                10.0
#define TIME_DRAGON              12.0
#define TIME_REPORT              15.0

struct modal_victory {
  struct modal hdr;
  struct decalsheet_entry decalv[DECAL_LIMIT];
  double clock;
  int rpt_texid,rptw,rpth; // Final report and farewell verbiage, a static image.
  int caption_texid,captionw,captionh,caption_strix; // Below the action scene, can be empty.
};

#define MODAL ((struct modal_victory*)modal)

static void _victory_del(struct modal *modal) {
  egg_texture_del(MODAL->rpt_texid);
  egg_texture_del(MODAL->caption_texid);
}

/* Load decalsheet.
 */
 
static int victory_load_decals(struct modal *modal) {
  const void *serial=0;
  int serialc=res_get(&serial,EGG_TID_decalsheet,RID_decalsheet_victory);
  struct decalsheet_reader reader;
  if (decalsheet_reader_init(&reader,serial,serialc)<0) return -1;
  struct decalsheet_entry entry;
  while (decalsheet_reader_next(&entry,&reader)>0) {
    if (entry.decalid>=DECAL_LIMIT) continue;
    MODAL->decalv[entry.decalid]=entry;
  }
  return 0;
}

/* Compose text for report.
 */
 
static int victory_repr_time(char *dst,int dsta,struct modal *modal,const char *pfx,int pfxc) {
  int dstc=pfxc;
  if (dstc<=dsta) memcpy(dst,pfx,dstc);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  int ms=(int)(g.playtime*1000.0);
  if (ms<0) ms=0;
  int sec=ms/1000; ms%=1000;
  int min=sec/60; sec%=60;
  int hour=min/60; min%=60;
  if (hour>99) { hour=min=sec=99; ms=999; }
  if (hour>=10) {
    if (dstc<dsta) dst[dstc]='0'+hour/10; dstc++;
  }
  if (hour) {
    if (dstc<dsta) dst[dstc]='0'+hour%10; dstc++;
    if (dstc<dsta) dst[dstc]=':'; dstc++;
  }
  if (dstc<dsta) dst[dstc]='0'+min/10; dstc++;
  if (dstc<dsta) dst[dstc]='0'+min%10; dstc++;
  if (dstc<dsta) dst[dstc]=':'; dstc++;
  if (dstc<dsta) dst[dstc]='0'+sec/10; dstc++;
  if (dstc<dsta) dst[dstc]='0'+sec%10; dstc++;
  if (dstc<dsta) dst[dstc]='.'; dstc++;
  if (dstc<dsta) dst[dstc]='0'+ms/100; dstc++;
  if (dstc<dsta) dst[dstc]='0'+(ms/10)%10; dstc++;
  if (dstc<dsta) dst[dstc]='0'+ms%10; dstc++;
  return dstc;
}
 
static int victory_repr_hearts(char *dst,int dsta,struct modal *modal,const char *pfx,int pfxc) {
  int dstc=pfxc;
  if (dstc<=dsta) memcpy(dst,pfx,dstc);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  if ((g.maxhp>=0)&&(g.maxhp<10)) { // Not allowed to reach 10. If it does, don't try anything.
    if (dstc<dsta) dst[dstc]='0'+g.maxhp;
    dstc++;
  }
  return dstc;
}

/* Generate the RGBA report image.
 * It's the size of a full framebuffer. Caller should crop after.
 */
 
static void victory_generate_report_image(uint32_t *rgba,struct modal *modal) {
  int y=0,tmpc,srcc,w;
  char tmp[64];
  const char *src;
  
  srcc=res_get_string(&src,1,STRIX_YOU_WIN);
  if ((w=font_measure_string(g.font,src,srcc))>0) {
    int x=(FBW>>1)-(w>>1);
    font_render(rgba+y*FBW+x,FBW-x,FBH-y,FBW<<2,g.font,src,srcc,0xffffffff);
  }
  y+=20;
  
  srcc=res_get_string(&src,1,STRIX_TIME);
  tmpc=victory_repr_time(tmp,sizeof(tmp),modal,src,srcc);
  if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
    if ((w=font_measure_string(g.font,tmp,tmpc))>0) {
      int x=(FBW>>1)-(w>>1);
      font_render(rgba+y*FBW+x,FBW-x,FBH-y,FBW<<2,g.font,tmp,tmpc,0xffffffff);
    }
  }
  y+=12;
  
  srcc=res_get_string(&src,1,STRIX_HEARTS);
  tmpc=victory_repr_hearts(tmp,sizeof(tmp),modal,src,srcc);
  if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
    if ((w=font_measure_string(g.font,tmp,tmpc))>0) {
      int x=(FBW>>1)-(w>>1);
      font_render(rgba+y*FBW+x,FBW-x,FBH-y,FBW<<2,g.font,tmp,tmpc,0xffffffff);
    }
  }
  y+=20;
  
  srcc=res_get_string(&src,1,STRIX_THANKS);
  if ((w=font_measure_string(g.font,src,srcc))>0) {
    int x=(FBW>>1)-(w>>1);
    font_render(rgba+y*FBW+x,FBW-x,FBH-y,FBW<<2,g.font,src,srcc,0xffffffff);
  }
}

/* Nonzero if there's any nonzero pixel in this rect.
 * Stride is presumed to be a full framebuffer.
 */
 
static int image_something(const uint32_t *v,int w,int h) {
  for (;h-->0;v+=FBW) {
    const uint32_t *p=v;
    int xi=w;
    for (;xi-->0;p++) if (*p) return 1;
  }
  return 0;
}

/* Generate the static final report.
 */
 
static int victory_generate_report(struct modal *modal) {
  uint32_t *rgba=calloc(FBW*4,FBH);
  if (!rgba) return -1;
  victory_generate_report_image(rgba,modal);
  int x=0,y=0,w=FBW,h=FBH;
  while (w&&!image_something(rgba+x,1,h)) { x++; w--; }
  while (w&&!image_something(rgba+x+w-1,1,h)) w--;
  while (h&&!image_something(rgba+y*FBW+x,w,1)) { y++; h--; }
  while (h&&!image_something(rgba+(y+h-1)*FBW+x,w,1)) h--;
  MODAL->rpt_texid=egg_texture_new();
  egg_texture_load_raw(MODAL->rpt_texid,w,h,FBW<<2,rgba+y*FBW+x,FBW*h*4);
  MODAL->rptw=w;
  MODAL->rpth=h;
  free(rgba);
  return 0;
}

/* Generate the caption texture if we don't have it already.
 */
 
static int victory_set_caption(struct modal *modal,int strix) {
  if (strix==MODAL->caption_strix) return 0;
  MODAL->caption_strix=strix;
  const char *text=0;
  int textc=res_get_string(&text,1,strix);
  MODAL->caption_texid=font_render_to_texture(MODAL->caption_texid,g.font,text,textc,FBW-(CAPTION_MARGIN<<1),FBH-SCENEH-CAPTION_MARGIN,0xffffffff);
  egg_texture_get_size(&MODAL->captionw,&MODAL->captionh,MODAL->caption_texid);
  return 0;
}

/* Init.
 */

static int _victory_init(struct modal *modal) {
  g.saved_game_dirty=1;
  if (victory_load_decals(modal)<0) return -1;
  if (victory_generate_report(modal)<0) return -1;
  return 0;
}

static void _victory_focus(struct modal *modal,int focus) {
}

/* Update.
 */

static void _victory_update(struct modal *modal,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
      if (MODAL->clock<TIME_REPORT) {
        // Make them wait.
      } else {
        modal->defunct=1;
        modal_spawn(&modal_type_hello);
      }
    }
  }
  
  double pvtime=MODAL->clock;
  MODAL->clock+=elapsed;
  if ((pvtime<TIME_BREAK2)&&(MODAL->clock>=TIME_BREAK2)) wishbone_song(0,0);
  else if ((pvtime<TIME_DRAGON)&&(MODAL->clock>=TIME_DRAGON)) wishbone_song(RID_song_bonefare,0);
  
  // Load captions if the time is right.
  if ((MODAL->clock>=TIME_HEY_DRAGON)&&(MODAL->clock<TIME_BETWEEN_SPEECH)) victory_set_caption(modal,STRIX_HEY_DRAGON);
  else if ((MODAL->clock>=TIME_WISH_YOU_WERE_DEAD)&&(MODAL->clock<TIME_BREAK1)) victory_set_caption(modal,STRIX_WISH_YOU_WERE_DEAD);
  else MODAL->caption_strix=0;
}

/* Render a decal by decalid.
 * Caller sets graf's image first.
 */
 
static void victory_decal(struct modal *modal,int dstx,int dsty,int decalid) {
  if ((decalid<0)||(decalid>=DECAL_LIMIT)) return;
  const struct decalsheet_entry *decal=MODAL->decalv+decalid;
  graf_decal(&g.graf,dstx,dsty,decal->x,decal->y,decal->w,decal->h);
}

/* Render one of the wishbones travelling along an arc.
 */
 
static void victory_animate_arc(struct modal *modal,double ax,double ay,double bx,double by,double h,int decalid,double t) {
  int x=(int)(ax*(1.0-t)+bx*t);
  int y;
  // We said "arc" but it's actually just two lines. It's fine, don't worry about it.
  if (t<0.5) {
    t*=2.0;
    y=(int)(ay*(1.0-t)+(ay-h)*t);
  } else {
    t=(t-0.5)*2.0;
    y=(int)((ay-h)*(1.0-t)+by*t);
  }
  victory_decal(modal,x,y,decalid);
}

/* Render.
 */

static void _victory_render(struct modal *modal) {

  // Black background.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  
  // Final disposition: Show the report and some "thanks for playing" message.
  if (MODAL->clock>=TIME_REPORT) {
    graf_set_input(&g.graf,MODAL->rpt_texid);
    graf_decal(&g.graf,(FBW>>1)-(MODAL->rptw>>1),(FBH>>1)-(MODAL->rpth>>1),0,0,MODAL->rptw,MODAL->rpth);
    return;
  }
  
  // Action scene background.
  graf_set_image(&g.graf,RID_image_victory);
  victory_decal(modal,0,0,NS_decal_bg);
  
  // Dot is at a fixed position, but many different decals.
  int decalid;
       if (MODAL->clock>=TIME_IDLE) decalid=NS_decal_dot_idle;
  else if (MODAL->clock>=TIME_BREAK2) decalid=NS_decal_dot_break2;
  else if (MODAL->clock>=TIME_BREAK1) decalid=NS_decal_dot_break1;
  else if (MODAL->clock>=TIME_WISH_YOU_WERE_DEAD) decalid=NS_decal_dot_threaten;
  else if (MODAL->clock>=TIME_BRANDISH) decalid=NS_decal_dot_brandish;
  else if (MODAL->clock>=TIME_BETWEEN_SPEECH) decalid=NS_decal_dot_idle;
  else if (MODAL->clock>=TIME_HEY_DRAGON) decalid=NS_decal_dot_shout;
  else decalid=NS_decal_dot_idle;
  victory_decal(modal,10,SCENEH-80,decalid);
  
  // After Dot enters break2 and before the dragon lands, show the two bone pieces rotating and arcing from Dot's hand to the ground.
  if ((MODAL->clock>=TIME_BREAK2)&&(MODAL->clock<TIME_DRAGON)) {
    if (MODAL->clock>=TIME_LAND) { // They've hit the ground.
      victory_decal(modal,170,SCENEH-30,NS_decal_wishbone1);
      victory_decal(modal,190,SCENEH-30,NS_decal_wishbone2);
    } else {
      double t=(MODAL->clock-TIME_BREAK2)/(TIME_LAND-TIME_BREAK2);
      victory_animate_arc(modal,50,SCENEH-60,170,SCENEH-30,40,NS_decal_wishbone1,t);
      victory_animate_arc(modal,60,SCENEH-65,190,SCENEH-30,20,NS_decal_wishbone2,t);
    }
  }
  
  // Near the end of the scene, show the dead dragon flumped on the ground, hugely.
  if (MODAL->clock>=TIME_DRAGON) {
    const double swashdur=0.200;
    if (MODAL->clock<TIME_DRAGON+swashdur) {
      graf_set_input(&g.graf,0);
      int len=(int)((1.0-(MODAL->clock-TIME_DRAGON)/swashdur)*60.0);
      int x=120;
      int y=SCENEH-50;
      for (;x<300;x+=30) {
        graf_line(&g.graf,x,y,0xffffffff,x,y-len,0xffffffff);
      }
      graf_set_image(&g.graf,RID_image_victory);
    }
    victory_decal(modal,100,SCENEH-75,NS_decal_dragon);
  }
  
  // Captions. Update is responsible for timing.
  if (MODAL->caption_strix) {
    graf_set_input(&g.graf,MODAL->caption_texid);
    graf_decal(&g.graf,CAPTION_MARGIN,SCENEH+CAPTION_MARGIN,0,0,MODAL->captionw,MODAL->captionh);
  }
}

/* Type definition.
 */

const struct modal_type modal_type_victory={
  .name="victory",
  .objlen=sizeof(struct modal_victory),
  .opaque=1,
  .interactive=1,
  .del=_victory_del,
  .init=_victory_init,
  .focus=_victory_focus,
  .update=_victory_update,
  .render=_victory_render,
};
