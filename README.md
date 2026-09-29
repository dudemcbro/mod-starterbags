# mod-starterbags

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that makes
sure every character has all bag slots filled and all bank bag slots unlocked.

## What it does

Every time a character logs in, the module:

1. **Unlocks all 7 bank bag slots** for free, if any are still locked.
2. **Fills every empty equipped bag slot** (the 4 slots next to the backpack)
   with a new Green Woolen Bag (item 4241, 8 slots).
3. **Fills every empty bank bag slot** with a new Green Woolen Bag.

If a bag cannot be placed in its slot, the module tries any free bank slot,
then the backpack and bags, and as a last resort mails it to the character
(subject "Starter bag"). Failures are logged under the `module.starterbags`
log filter.

Slots that already hold a bag are never touched, so characters keep their
bigger bags.

### Things to know

- It runs on **every** login, not only the first one. A character that removes
  or sells a bag from one of these slots gets a new Green Woolen Bag the next
  time it logs in. The bag is not soulbound and sells to vendors for 4s 50c.
- It applies to every character that logs in, including existing characters
  and, on servers running mod-playerbots, bots.
- The bag item is set in code (`STARTER_BAG_ENTRY` in
  `src/mod_starterbags.cpp`). To hand out a different bag, change that value and
  rebuild. There is no config file and no SQL.

## Installation

1. Clone the module into the core's `modules` folder:

   ```sh
   cd <azerothcore>/modules
   git clone https://github.com/dudemcbro/mod-starterbags.git
   ```

2. Re-run CMake and rebuild the core. No `CMakeLists.txt` is needed: AzerothCore
   compiles everything under `src/` and calls `Addmod_starterbagsScripts()`.

3. Restart `worldserver`.

To remove it, delete the module folder and rebuild. Bags and bank slots that
were already given stay with the characters.

## License

GNU General Public License v3.0, see [LICENSE](LICENSE).
