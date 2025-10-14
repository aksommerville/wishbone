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
#define CMD_map_lock       0x43 /* u16:position u8:flagid u8:difficulty */
#define CMD_map_neighbors  0x60 /* u16:west, u16:east, u16:north, u16:south */
#define CMD_map_sprite     0x61 /* u16:position, u16:spriteid, u32:arg */
#define CMD_map_spawn      0x62 /* u16:spriteid, u8:count, u8:flagid, u32:arg */

#define CMD_sprite_animate   0x01 /* ; treasure */
#define CMD_sprite_image     0x20 /* u16:imageid */
#define CMD_sprite_tile      0x21 /* u8:tileid, u8:xform */
#define CMD_sprite_type      0x22 /* u16:sprtype */
#define CMD_sprite_layer     0x23 /* u16:layer */
#define CMD_sprite_animonce  0x24 /* u8:tilecount, u8:periodms */
#define CMD_sprite_phymask   0x40 /* b32:physics */

#define NS_tilesheet_physics 1
#define NS_tilesheet_family 0
#define NS_tilesheet_neighbors 0
#define NS_tilesheet_weight 0

#define NS_physics_vacant 0
#define NS_physics_solid 1
#define NS_physics_water 2

#define NS_prize_none      0
#define NS_prize_heart     1
#define NS_prize_container 2
#define NS_prize_wishbone  3

#define NS_spell_invalid 0
#define NS_spell_teleport 1
#define NS_spell_rain 2
#define NS_spell_fire 3

#define NS_npcaction_none 0
#define NS_npcaction_wishbone 1
#define NS_npcaction_heal 2
#define NS_npcaction_sensei 3 /* Provide a string, but he says a placeholder instead if you don't have the wishbone. */

// Editor uses the comment after a 'sprtype' symbol as a prompt in the new-sprite modal.
// Should match everything after 'spriteid' in the CMD_map_sprite args.
#define NS_sprtype_dummy      0 /* (u32)0 */
#define NS_sprtype_hero       1 /* (u32)0 */
#define NS_sprtype_treasure   2 /* (u8:flag)zero (u8:prize)none (u8)0 (u8)0 */
#define NS_sprtype_boomerang  3 /* (u24)0 0x02 */
#define NS_sprtype_bug        4 /* (u32)0 */
#define NS_sprtype_soulballs  5 /* (u24)0 (u8)count */
#define NS_sprtype_firepot    6 /* sprite:fireball (u8:flag)one (u8)0 */
#define NS_sprtype_missile    7 /* (u32)0 */
#define NS_sprtype_npc        8 /* (u8)strix (u8:npcaction)none (u16)0 */
#define NS_sprtype_bonfire    9 /* (u32)0 */
#define NS_sprtype_animonce  10 /* (u32)0 */
#define FOR_EACH_SPRTYPE \
  _(dummy) \
  _(hero) \
  _(treasure) \
  _(boomerang) \
  _(bug) \
  _(soulballs) \
  _(firepot) \
  _(missile) \
  _(npc) \
  _(bonfire) \
  _(animonce)

#define NS_flag_zero 0
#define NS_flag_one 1
#define NS_flag_wishbone 2
#define NS_flag_lock1 3
#define NS_flag_lock2 4
#define NS_flag_castle_exit 5
#define NS_flag_container1 6
#define NS_flag_hclock 7
#define NS_flag_container2 8
#define NS_flag_container3 9
#define NS_flag_training_bridge 10
#define NS_flag_backdoorbugs 11
#define NS_FLAG_COUNT 12

#endif
