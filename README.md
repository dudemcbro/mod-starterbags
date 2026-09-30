# mod-starterbags

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that gives
every new character a full set of bags on its first login.

## What it does

The first time a character enters the world, the module:

1. **Unlocks all 7 bank bag slots** for free, but only if
   `StarterBags.UnlockBankBagSlots = 1` (off by default).
2. **Fills every empty equipped bag slot** (the 4 slots next to the backpack)
   with a new bag, by default a Green Woolen Bag (item 4241, 8 slots).
3. **Fills every empty, unlocked bank bag slot** with the same bag. A new
   character has no unlocked bank bag slots, so without step 1 this step does
   nothing.

If a bag cannot be placed in its slot, the module tries any free bank slot,
then the backpack and bags, and as a last resort mails it to the character
(subject "Starter bag"). Failures are logged under the `module.starterbags`
log filter.

Slots that already hold a bag are never touched.

### Once per character

Bags are handed out **once per character**, from the core's first login hook
(`OnPlayerFirstLogin`). The core marks a character for its first login when it
is created and clears the mark when the character enters the world. The mark
is saved together with the character's items, so no extra table or setting is
needed. As a result:

- A character that sells, destroys or moves a bag does not get a new one. The
  bag sells to vendors for 4s 50c, so this matters.
- Mailed bags are also sent only once: the first login mark is cleared in the
  same database transaction as the mail.
- Characters that had already logged in before this module (or this version
  of it) was installed get nothing. Earlier versions ran on every login; after
  updating, characters that already received bags from them get no more.
- Characters created while `StarterBags.Enable = 0` never get bags from the
  module, even if it is enabled later.

### Playerbots

On servers running [mod-playerbots](https://github.com/mod-playerbots/mod-playerbots),
random bots are created without the first login mark, so they never get bags.
Characters that belong to a real account are treated like any other
character, whether their first login is by the player or as a bot. The module
has no dependency on mod-playerbots.

## Requirements

- AzerothCore WotLK (developed against the
  [mod-playerbots AzerothCore fork](https://github.com/mod-playerbots/azerothcore-wotlk)).
  The module only uses standard core APIs.
- No SQL and no core patches.

## Installation

1. Clone the module into the core's `modules` folder:

   ```sh
   cd <azerothcore>/modules
   git clone https://github.com/dudemcbro/mod-starterbags.git
   ```

2. Re-run CMake and rebuild the core, for example with
   `./acore.sh compiler build`, or with `cmake` followed by `make install` in
   your build folder. No `CMakeLists.txt` is needed: AzerothCore compiles
   everything under `src/` and calls `Addmod_starterbagsScripts()`. CMake has
   to run again so it picks up the module and its config file.

3. Set up the config file (see below).

4. Restart `worldserver`.

To remove the module, delete its folder, re-run CMake and rebuild. Bags and
bank slots that were already given stay with the characters.

## Configuration

The build installs `mod_starterbags.conf.dist` into the `modules` folder next
to `worldserver.conf` (`env/dist/etc/modules/` for `acore.sh` builds).
`./acore.sh compiler build` copies it to `mod_starterbags.conf` if that file
does not exist yet. Otherwise copy it yourself:

```sh
cd <config folder>/modules
cp mod_starterbags.conf.dist mod_starterbags.conf
```

Without `mod_starterbags.conf` the defaults below are used, and the
worldserver may log a "Missing property" message for each option.

| Option | Default | Description |
| --- | --- | --- |
| `StarterBags.Enable` | `1` | Give bags on a character's first login. |
| `StarterBags.BagEntry` | `4241` | `item_template` entry of the bag to give. It must be a bag (class 1, InventoryType 18), otherwise nothing is given and an error is logged. |
| `StarterBags.UnlockBankBagSlots` | `0` | Unlock all 7 bank bag slots for free on the first login and fill them with bags. |

Options are read when a character logs in for the first time, so a changed
value applies after `.reload config` or a restart.

## Updating

```sh
cd <azerothcore>/modules/mod-starterbags
git pull
```

Then re-run CMake, rebuild and restart `worldserver`. Compare your
`mod_starterbags.conf` with the new `mod_starterbags.conf.dist` and add any
new options.

Upgrading from a version without a config file: the old version unlocked all
bank bag slots for free on every login. The new default is `0`. Set
`StarterBags.UnlockBankBagSlots = 1` to keep giving new characters free bank
bag slots. Existing characters keep the slots they already have.

## Contributing

Bug reports and pull requests are welcome on
[GitHub](https://github.com/dudemcbro/mod-starterbags). For larger
changes, please open an issue first, and describe how you tested your change.

## License

GNU General Public License v3.0, see [LICENSE](LICENSE).
