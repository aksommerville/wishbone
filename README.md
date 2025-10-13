# When You Wish Upon A Bone

Requires [Egg v2](https://github.com/aksommerville/egg2) to build.

For [One Week, One World Jam](https://itch.io/jam/one-week-one-world), theme "ONE TOOL, MANY USES".

Zelda-style adventure where Dot has just one item: A wishbone.

She'll use it many ways:
 - Stab. (natural A)
 - Swing. (rotate dpad, A). Bats items away.
 - Pole Vault. (hold dpad, A). Skip 1m or so aerially.
 - Boomerang. (double-tap dpad, A). Like Zelda.
 - Wand. (hold A, dpad to encode). Like Full Moon.
 - Slingshot. (hold A, release with nothing encoded). Pick a direction and magnitude continuously, then fly, and the wishbone stays where it was.
 - Lockpick. (natural A facing context). Minigame where you press plungers from opposite sides. Align to open the lock.
 - Maybe other context-sensitive uses?

Track usage of each technique, and send educators at appropriate moments if it hasn't been used much.
Or maybe just keep the educators static, that's less to think about.

## Agenda

 - 2025-10-10 FRI Jam begins. Maps, sprites, and modals. Basic graphics.
 - 2025-10-11 SAT All bone usages. Monsters. Hazards.
 - 2025-10-12 SUN Aim to have all gameplay logic complete, and strong progress on maps and ancillaries.
 - 2025-10-13 MON Maps, music, decorative touches.
 - 2025-10-14 TUE Overflow. Aim to be finished by EOD.
 - 2025-10-15 WED Overflow.
 - 2025-10-16 THU Itch page. Finish and submit before EOD.
 - 2025-10-17 FRI 17:00 Jam ends.

## TODO

- [x] Transitions. modal_play.c
- [x] Status bar. modal_play.c
- [x] Flags.
- [x] Proper hero motion and physics.
- [x] Hero actions.
- - [x] stab
- - [x] swing
- - [x] polevault
- - [x] boomerang
- - [x] wand
- - [x] slingshot
- - [x] lockpick
- [x] POI-only treadles and stompboxes.
- [x] It's hard to perform SWING, and I keep hitting VAULT instead.
- [x] Hero death.
- [x] Boomerang: Collect treasures and hurt foes.
- [x] Heart containers.
- [x] Educators.
- [ ] Music. Still need something for gameover, and maybe a different one for the boss fight, if there's a boss fight.
- [x] Sound effects.
- [x] Hello modal.
- [ ] Gameover modal and fancy cutscenes.
- [x] Dialogue.
- [x] Combat.
- [ ] Consequences for killing a foe: Prize, stats, flags.
- [ ] More monsters.
- [ ] Proper maps.
- [ ] Indicate the teleport spell, where you start.
- [ ] Splash effect. (eg vault or slingshot into the sea)
- [ ] Lockpick: Unmistakable warning flash at say 5 seconds? It's easy to miss the countdown.
- [ ] Still missing a lot of sounds, and the existing ones could stand some review.
- [ ] Can we detect soft-locking when you're on an island without a wishbone?
- [ ] Dot faces for slingshot flight. Using the generic idle face now.
- [ ] Lots can be prettied up, if there's time.

## Notes

- We're going big. So assume we'll need multiple tilesheets. (don't hard-code against one like I did for Mysteries of the Crypt).
- Spooky Halloween aesthetics wherever possible. As if I'd ever do otherwise :)
- Mutable maps. Use simple POI like we did in Spelling Bee for switches and stuff.
- Maybe a gag where you can trade the wishbone for a swiss army knife, which doesn't do anything useful.
- Work into the narrative that you can break the bone to make a wish. Use that right at the end: "Dragon, I wish you were dead!"
- The wishbone's technical name is Furcula. That sounds useful. "Count Furcula's Castle"...
- Flat list of sprites. Don't use groups, too complicated.
- No "session" container. The globals have gameplay stuff spread across.

- Boomerang can reach a prize 8 meters away, not 9.
