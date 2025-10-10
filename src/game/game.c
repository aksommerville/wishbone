#include "wishbone.h"

/* Load map.
 */
 
int load_map(int mapid) {
  struct map *map=res_get_map(mapid);
  if (!map) return -1;
  
  if (g.map) {
    //TODO Outbound triggers for old map?
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
  
  g.map=map;
  if (hero) sprite_relist(hero);
  
  /* Run initial commands.
   */
  struct cmdlist_reader reader;
  if (cmdlist_reader_init(&reader,map->cmd,map->cmdc)<0) return -1;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {

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
      
      //TODO switches and similar poi effect
    }
  }
  
  //TODO song?
  //TODO generic inbound triggers
  
  return 0;
}
