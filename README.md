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
