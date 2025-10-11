/* shared_symbols.h
 * This file is consumed by eggdev and editor, in addition to compiling in with the game.
 */

#ifndef SHARED_SYMBOLS_H
#define SHARED_SYMBOLS_H

#define NS_sys_tilesize 16
#define NS_sys_mapw 20
#define NS_sys_maph 11
#define NS_sys_bgcolor 0x000000

#define CMD_map_image      0x20 /* u16:imageid */
#define CMD_map_switchable 0x40 /* u16:position u8:flagid u8:invert */
#define CMD_map_treadle    0x41 /* u16:position u8:flagid u8:invert */
#define CMD_map_stompbox   0x42 /* u16:position u8:flagid u8:invert */
#define CMD_map_lock       0x43 /* u16:position u8:flagid u8:reserved */
#define CMD_map_neighbors  0x60 /* u16:west, u16:east, u16:north, u16:south */
#define CMD_map_sprite     0x61 /* u16:position, u16:spriteid, u32:arg */
#define CMD_map_door       0x62 /* u16:position, u16:mapid, u16:dstposition, u16:arg */

#define CMD_sprite_animate   0x01 /* ; treasure */
#define CMD_sprite_image     0x20 /* u16:imageid */
#define CMD_sprite_tile      0x21 /* u8:tileid, u8:xform */
#define CMD_sprite_type      0x22 /* u16:sprtype */
#define CMD_sprite_layer     0x23 /* u16:layer */
#define CMD_sprite_phymask   0x40 /* b32:physics */

#define NS_tilesheet_physics 1
#define NS_tilesheet_family 0
#define NS_tilesheet_neighbors 0
#define NS_tilesheet_weight 0

#define NS_physics_vacant 0
#define NS_physics_solid 1
#define NS_physics_water 2

#define NS_prize_none  0
#define NS_prize_heart 1

#define NS_spell_invalid 0
#define NS_spell_teleport 1
#define NS_spell_rain 2
#define NS_spell_fire 3
//TODO dream up some more spells, 5..10 is probly good

// Editor uses the comment after a 'sprtype' symbol as a prompt in the new-sprite modal.
// Should match everything after 'spriteid' in the CMD_map_sprite args.
#define NS_sprtype_dummy      0 /* (u32)0 */
#define NS_sprtype_hero       1 /* (u32)0 */
#define NS_sprtype_treasure   2 /* (u8:flag)zero (u8:prize)none (u8)0 (u8)0 */
#define NS_sprtype_boomerang  3 /* (u24)0 0x02 */
#define FOR_EACH_SPRTYPE \
  _(dummy) \
  _(hero) \
  _(treasure) \
  _(boomerang)

#define NS_flag_zero 0
#define NS_flag_one 1
#define NS_flag_wishbone 2
#define NS_flag_lock1 3
#define NS_flag_lock2 4
#define NS_FLAG_COUNT 5

#endif
