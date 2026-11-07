# Server reference

Everything Rocket Crowbar 2 (RC2, beta 1.1) adds to a Half-Life server's console and config:
installing and running it, its cvars, its commands and admin system, its impulses, the client
userinfo keys it reads, votes, and the files it reads and writes. What each feature does in play
is in [GAMEPLAY.md](GAMEPLAY.md); the 1113 weapon, transform and attribute values in `rc2.ini`
are in [RC_INI.md](RC_INI.md). Where the rebuild differs from RC2 itself,
[reference/DEVIATIONS.md](../reference/DEVIATIONS.md) says so.

Everything here is read from the rebuild's code. The rebuild has been built and booted (it loads
`rc_sandcrab` on the rig with a clean console), not played; where the code shows something whose
effect in game hasn't been seen, this page says "unverified".

## Installing and running

### The gamedir

The mod runs as gamedir `rc2`, next to the stock `valve/` directory, which it falls back to for
anything it doesn't override. The manifest is [game/rc2/liblist.gam](../game/rc2/liblist.gam):

```
game "Rocket Crowbar 2"
gamedir "rc2"
fallback_dir "valve"
gamedll "dlls\hl.dll"
gamedll_linux "dlls/hl.so"
cldll "1"
```

`cldll "1"` matters: unlike RC 1.9, RC2 has its own client library, so clients need it too. The
content (maps, models, sounds, sprites, events, `rc2.ini`, `maps/lcs/`) is the original beta 1.1
release's; the rebuild replaces only the two libraries. A server's `rc2/` is the original
content plus:

```
rc2/liblist.gam          this repo's game/rc2/liblist.gam (the original's points at dlls\rc2.dll)
rc2/dlls/hl.so           the server library
rc2/cl_dlls/client.so    the client library, on Linux clients (a server doesn't need it; each
                         player installs the mod, client library included, himself)
```

### Building

The Linux libraries build in the goldsrc-net buildchain's Docker image (glibc 2.28 floor, so
they load on any glibc 2.28 or newer):

```sh
scripts/build.sh i386 all      # build-i386/dlls/hl.so and build-i386/cl_dll/client.so
scripts/build.sh amd64         # build-amd64/dlls/hl.so (the server only)
scripts/build.sh aarch64       # build-aarch64/dlls/hl.so
```

The server library builds and boots on i386 and amd64. The client library is built and used
for i386; a 64-bit client isn't done yet.

Windows: the original shipped only Windows libraries (`dlls\rc2.dll`, `cl_dlls\client.dll`).
This repo's CI (continuous integration) workflow, inherited from goldsrc-net/halflife-updated,
has a Win32 job that builds `projects/vs2019/projects.sln` and a Win64 job that builds the `hl`
target with CMake. The Visual Studio projects don't list RC2's own sources (`dlls/rc2/`,
`dlls/rc2prefs*`, `cl_dll/rc2/`), so the Win32 job can't build the mod as it stands; the Win64
job globs them as the Linux build does, but has never run, since the repo has no remote yet. So
there is no Windows build of the rebuild yet. A Windows server can still run the original
`rc2.dll` with the original `liblist.gam`.

### Running

A Linux dedicated server (HLDS, the Half-Life Dedicated Server; ReHLDS works the same way):

```sh
./hlds_linux -game rc2 +deathmatch 1 +maxplayers 16 +map rc_sandcrab
```

The release's multiplayer maps are `rc_freefall`, `rc_homestead`, `rc_inanis`,
`rc_marblegarden`, `rc_margaritaville`, `rc_moonside`, `rc_oneking`, `rc_race` (forklift
racing), `rc_sandcrab` and `rc_vertigoquad`, plus `forfunstuff`, `forkliftfun`, `killbox`,
`RCRP_Town` and `rcrp_west_final`. `forkliftfun` doesn't load: its `alex2.wad` isn't in the
release. With an empty `mapcycle.txt` the map cycle falls back to `bootcamp`.

[tools/rig/README.md](../tools/rig/README.md) sets up the original (under Wine) and the rebuild
side by side on one machine, and has a smoke test that compares their consoles.

### At each map start

In this order, as the code runs them:

1. `sv_maxvelocity` is set to 4000 (the world's precache), whatever the config says.
2. The game rules are installed (`game.cfg` runs first, as in Half-Life).
3. Every `rc2.ini` key goes back to its built-in default, then the file `rccfgfile` names is
   read over them (rc2.ini, below).
4. The map's level customization script (LCS), `maps/lcs/<map>.lcs`, has its first pass: the
   classnames it creates are precached.
5. `mp_scoremode` is read; a change takes effect at the next map.
6. Three seconds in, the LCS file's second pass runs its commands.

## Cvars

RC2's own cvars are server cvars (`FCVAR_SERVER`), so they show in server browsers. The
original's full set, with defaults and flags, is `reference/cvars-rc2.tsv`.

### Rocket Crowbar 2's own

```
cvar               default  what it does
mp_teamannoy       0        teamplay only. 0: a player's weapon effects don't act on his
                            teammates (still on himself): the gauss teleports, the egon tractor
                            and shrinker, freezing, anti-friction, the impailer, the crowbar's
                            shove and crowbarang, the suck mine, the assassin's kick. 1: they act
                            on anyone. Outside teamplay they always act on anyone
mp_riding          1        0: nobody can ride another player's shoulder. 1: a player can be
                            ridden if his userinfo allowriding is nonzero (see Client userinfo
                            keys); in teamplay only by a teammate
mp_random          0        swaps items for random ones, in multiplayer: 1 once at map start, of
                            the same kind (a gun for a gun, an explosive for an explosive, ammo
                            for ammo); 2 once, for anything; 3 as 1 and again on every respawn;
                            4 as 2 and again on every respawn. An item with "norandomize" set in
                            the map is left alone, and so are spawn weapons, gives and impulse 101
mp_scoremode       0        1: a damage-based score as well as frags (GAMEPLAY.md); the frag limit
                            applies to it, and mp_fragsleft goes out with two decimals. Read at
                            map start. Any other nonzero value turns the frag limit over to the
                            damage score without scoring anything (unverified in play)
mp_allowtransform  1        0: the transform command answers "Server has disallowed transforming"
sv_allowgive       1        0: give and givefor are refused for everyone (Commands, Gives)
sv_noexit          1        in multiplayer (not coop), a trigger_changelevel kills (50000 damage,
                            always gibs) whoever touches or triggers it instead of changing level
sv_chglevel        0        with sv_noexit 0, in deathmatch: 1 lets a trigger_changelevel change
                            level. The original reads the next map's name before it is filled in,
                            so it goes to whatever map an earlier changelevel stored, or to ""
                            on the first one (kept)
sv_pcstuff         1        not 0: precache seven monsters at map start (human grunt, scientist,
                            Barney, alien slave, ichthyosaur, headcrab, Gargantua), so give and
                            LCS lines can make them mid-game
sv_tempadmins      0        WON (World Opponent Network) ids that get admin level 1, separated by
                            space, tab, newline, comma or semicolon (see Admin rights)
rccfgfile          rc2.ini  the prefs file, in the game directory (rc2.ini, below)
```

### Stock Half-Life cvars RC2 treats differently

```
cvar              how
mp_chattime       held to 10..120 seconds (the SDK's floor is 1)
mp_fraglimit      with mp_scoremode 1, compared with the damage-based score
mp_weaponstay     a weapon the player already has goes to a player riding his shoulders
sv_cheats         read every frame; needed for give, givefor, fov and impulse 101, even by
                  admins
sv_maxvelocity    set to 4000 at every map start
mp_falldamage     not registered: there is no fall damage in multiplayer
mp_allowmonsters  not registered: monsters are always allowed
```

The other stock cvars (`mp_teamplay`, `mp_timelimit`, `mp_friendlyfire`, `mp_forcerespawn`,
`mp_flashlight`, `mp_autocrosshair`, `decalfrequency`, `mp_teamlist`, `mp_teamoverride`,
`mp_defaultteam`, `displaysoundlist`, `sv_alltalk`, `voice_serverdebug`, the `sk_*` skill cvars)
keep their Half-Life defaults and meaning. Most weapon numbers come from `rc2.ini`, not the
`sk_*` cvars.

### Added by Half-Life Updated, not RC2

RC2 doesn't have these; the rebuild keeps them from its base (reference/DEVIATIONS.md). At
their defaults none changes anything.

```
cvar                          default  what it does
allow_spectators              0        nothing reads it: RC2's own spectator view is used
sk_plr_hornet_dmg1-3          0        read, then overwritten with 10 in multiplayer. RC2's
                                       skill.cfg doesn't set them, so every map load prints
                                       "** GetSkillCVar Got a zero for sk_plr_hornet_dmg1 **"
sv_allowbunnyhopping          0        no effect: the bunny-hop speed cap always applies, as in
                                       RC2
sv_busters                    0        1 installs Half-Life Updated's "busters" game rules (the
                                       egon game) in place of RC2's; untried with RC2's egon
sv_pushable_fixed_tick_fudge  15       nothing reads it
```

## Commands

Typed in the client console, or bound. RC2's own are in the release's key setup menu
(`gfx/shell/kb_act.lst`): `jetpack`, `drawenergy`, `createhealth`, `shield`, `transform`,
`rideplayer`, `attrconfig`, `+attack3` and `+attack4`; all but `shield` have a default key
(`gfx/shell/kb_def.lst`).

### Everyone

```
command                            what it does
say, say_team                      chat. No flood limit, as in RC2
say_private, sayprivate            say_private <name or index> <text>: a message to one player,
                                   echoed to you and logged
kill                               suicide; at most every 5 s, not while suicide-banned or racing
spectate, observe                  go to the spectator view; kills you (it counts as a death by
                                   the world). Refused while suicide-banned or give-banned
drop [weapon]                      drop the current or named weapon
use <weapon>, weapon_<name>        select a weapon (weapon_<name> not while frozen)
lastinv                            the last weapon (not while frozen)
fov [value]                        with sv_cheats, set your field of view; otherwise print it
attrconfig                         open the attribute menu (GAMEPLAY.md, Attributes)
setattr <13 numbers>               what the attribute menu sends; typed by hand it answers
                                   'Please use the "attrconfig" command'
transform                          into the transform your attributes chose, or back
                                   (mp_allowtransform)
rideplayer [right]                 climb onto the shoulder of the player in front of you, or get
                                   off (mp_riding). Any argument but "right" asks for the left
                                   shoulder; "right" or none picks one at random
jetpack                            toggle the jetpack (an ability)
energydraw, drawenergy             stand and draw energy (an ability)
createhealth, makehealth,          turn energy into health (an ability)
  energy2health
shield                             toggle the shield (an ability)
vote yes|no                        vote in the running vote (anything but "yes" is no)
votemap <map>                      start a map vote
votekick <index or name>           start a kick vote; no argument lists the player indices
give, givefor <classname>          see Gives
rcadmin [command]                  admin commands (see Admin); no argument lists them
rccfg [command]                    rc2.ini in game (see rc2.ini); no argument lists them
menuselect <n>                     teamplay's menu, as in Half-Life
vban, VModEnable                   the voice manager's, as in Half-Life
```

Half-Life Updated's own client commands (`fullupdate`, `specmode`, `closemenus`, `follownext`)
aren't in RC2's set, so they answer "Unknown command".

### Gives

`give <classname>` spawns the entity at your feet; a `weapon_` or `ammo_` is picked up (when
the rules let you have it and you aren't transformed) or removed, an `item_` is touched and
removed, anything else stays where it spawned. `givefor <classname> [distance]` spawns it in
front of you (128 units by default), facing your way. Both need, at once:

- `sv_cheats` not 0, for everyone, admins included;
- `sv_allowgive` not 0;
- a live, joined player, or admin level 2 when spectating;
- not give-banned (`rcadmin giveban`) and not frozen;
- a classname that is already precached (otherwise "Entity not precached! (create aborted)");
  `sv_pcstuff 1` and the map's LCS creates add to what is precached;
- 0.1 s since your last give or impulse.

Gives are never swapped by `mp_random`.

### Admin rights

There are two admin levels, worked out when a player connects:

- **2 (super admin):** the listen server's own player; or a WON id whose highest level in
  `adminprim.cfg` is 2.
- **1 (admin):** level 1 in `adminprim.cfg`; a WON id listed in `sv_tempadmins` (checked at each
  command); or promoted by a super admin.

`adminprim.cfg` is lines of `<wonid> <level>`, `#` starting a comment line. It is opened by
plain name, so from the server process's working directory (normally the Half-Life directory,
not `rc2/`), and is read at connect, so an edit applies to players who join after it.

WON ids belong to the 2001 authentication network; current engines no longer issue real ones.
What the engine returns for them on the rig is unverified (ReHLDS is understood to return the
same value, 4294967295, for every player). If so, `adminprim.cfg` and `sv_tempadmins` can't
tell players apart, and the listen server's host is the only admin that works reliably.

### Admin

`rcadmin <command> [args]`. Commands on "the aimed entity" use what you look at within 8192
units (never the world), or else your selection (`selectent`). Commands with a classname
argument act on every entity of that class instead ("all" or a one-character name: every
entity); none of them act on players. A command on another player needs a level above his,
unless you are a super admin.

```
command                               level  what it does
listclients                           1      print each player's index and name
giveban <player> 0|1                  1      bar a player from give and givefor; the
                                             spectator view checks it too (unverified that
                                             RC2 meant that)
suicideban <player> 0|1               1      bar a player from kill and spectate
forcespawn <player>                   1      spawn a spectating player
camera set|on|off                     1      place your camera at your eyes; look through it;
                                             back to your own view
camera player <player>                1      look through another player's eyes
selectent [index]                     1      select the aimed entity or entity <index> (not a
                                             player); -1 clears
punch <x> [y] [z]                     1      add to the aimed entity's punch angle
setangle <x> [y] [z]                  1      set the aimed entity's angles (not brush entities)
move <x> <y> <z> [0]                  1      move the aimed entity by that offset; a fifth
                                             argument makes it absolute coordinates. Not players
movecenterforward [distance]          1      put the aimed entity's center in front of you (128
                                             units by default). Not players
dumpentinfo                           1      print the aimed entity's classname, index, origin and
                                             angles
kill [classname]                      1      kill the aimed entity, or every one of a class
                                             (monsters only, not brush entities)
use [classname]                       1      use (toggle) it, the world as the activator
useset on|off|set|toggle [classname]  1      use with that use type
mkally, mkallied, makeallied          1      a monster forgets players as enemies (until a
  [classname]                                player hits it while facing it)
mkhostile, makehostile [classname]    1      a monster is provoked again
fixtrain, fixtrains                   1      let players drive every func_tracktrain again
                                             (clears no-control and forward-only) and re-enable
                                             path_tracks that disable trains
nukeclte                              1      tell every client to drop its client-side temporary
                                             effects. The help lists it as level 2; level 1 can
                                             use it
remove [classname]                    2      remove the aimed entity, or every one of a class
lcs <command> ...                     2      write a line to the map's LCS file (below)
stopvote                              2      end the running vote (not in the help listing)
mkspawn                               2      add a deathmatch spawn point where you stand (for
                                             this map only; lcs create writes one to the file)
promote <player>                      2      give a non-admin level 1
demote <player>                       2      take level 1 away (not from a super admin)
```

`<player>` is a player index, or else the first player whose name contains the text.

### Level customization commands

`rcadmin lcs <command>` appends a line to the current map's `maps/lcs/<map>.lcs` (LCS files,
below). It doesn't run it: the line takes effect from the next time the map loads.

```
command                        line written
create <class> [0]             create <class> {x y z} {pitch yaw 0}: your origin and view; a
                               second argument keeps the pitch level
respawncreate <class> [0]      respawncreate ... (comes back whenever it dies)
advcreate <class> <alters>     advcreate <class> <alterations> (alterations: see LCS files)
setspawncmd <alters>           setspawncmd <alterations>, meant to run on every player at spawn.
                               The loader only reads "setspawncommand", so these lines do
                               nothing (kept from RC2)
remove, kill, use [class]      removeent / killent / useent <index> for the aimed entity, or
                               removeall / killall / useall <class>
edit <alters>                  edit <index> <alterations> for the aimed entity
maintain <alters>              maintain <index> <alterations>: keep re-applying them
```

Entity indices in a file are only right for the map as it loads, before anything spawns or
dies.

### Server console

RC2 has no server console commands of its own: `rcadmin` and `rccfg` are client commands.
Half-Life Updated adds `sv_load_all_maps` and `sv_stop_loading_all_maps` (load every map in turn,
to build node graphs); RC2 doesn't have them.

## Impulses

```
impulse  what it does
100      flashlight, as in Half-Life
101      with sv_cheats: every Half-Life weapon with some ammo, the suit and a battery. The only
         cheat impulse RC2 keeps
201      spray your logo
```

Impulses are taken at most every 0.1 s (204 excepted), and none while frozen; an
impulse that comes too early waits instead of being dropped. RC2 has no transform impulses:
the transform comes from the attribute menu.

## Client userinfo keys

Set with `setinfo`. RC2's client menu sets `allowriding` and `gender`.

```
key                what it does
allowriding        nonzero: others may ride your shoulders (mp_riding 1). The release's menu
                   default is 0
gender             the pronouns in death messages: 1, f or F female; 2, n, N, i, I, u or U
                   neuter; otherwise male
noautoswitch       nonzero: picking up a weapon never switches to it
cl_autowepswitch   Half-Life Updated's; RC2's client never sends it
```

## Votes

Every player's client shows the question; those who have voted see a running tally,
refreshed twice a second.

- `votemap <map>`: the map must exist on the server. It passes with 51% of the other players
  (rounded up); 5 s after it passes, the level changes.
- `votekick <index or name>`: passes with 75% of the other players (rounded up), and then runs
  `banid 10 #<entity index> kick`. `banid` takes a user id, not an entity index, so this bans
  whoever has that user id, usually not the target (RC2's bug, kept).
- The caller can't vote in his own vote; HLTV (Half-Life TV) proxies don't count. The kick vote
  meant to keep its target out of the count too, but marks the caller twice instead, so the
  target can vote (kept).
- A vote passes as soon as enough say yes. It fails as soon as the players who haven't voted
  are fewer than the votes needed, even with yes votes already in (unverified whether that is
  what RC2 meant). After 60 s it is decided on the votes cast: yes must reach the same fraction
  of those.
- One vote at a time, none while the game is over, and none alone. A player may call another
  120 s after his last one.
- `rcadmin stopvote` ends it. A map change ends it too.

There are no vote cvars: the fractions, 60 s and 120 s are fixed in the code.

## rc2.ini

All weapon, transform, attribute, shield, energy, score and ammo numbers live in one file,
`rc2.ini` (or whatever `rccfgfile` names), in the game directory. [RC_INI.md](RC_INI.md) lists
every key with its type, default, the release's value, its clamps and which code reads it.

- The format is `[Heading]` lines and `Key=value` lines; headings and keys are matched ignoring
  case. A line outside a heading, without `=`, or naming no key prints a console message and is
  ignored. Lines of two characters or fewer are skipped.
- At each map start every key goes back to its built-in default and then the file is read over
  them, so a key the file leaves out has its default, and an in-game change lasts until the next
  map.
- Values are clamped as they are parsed, by type and by the key's flags (RC_INI.md).
- Keys marked NETWORK_SYNC are also read by the client library (the attribute menu's prices,
  weapon effects): a joining client gets every one that isn't at its default, and a change in
  game goes out to everyone.
- The release's `rc2.ini` sets every key. It changes some defaults: notably
  `[Attributes] TotalPoints=9999` (default 150), which makes the attribute point-buy
  effectively unlimited.

In game, `rccfg <command>`:

```
command                                  level  what it does
listheadings                             any    the headings
listkeys <heading> <page>                any    a heading's keys and values, 20 a page
findkey <partial key> [partial heading]  any    keys starting with that text, with type and
                                                flags (stops after 22)
setkey <heading> <key> <value>           1      change a value now, and tell everyone. A key
                                                marked REGISTER_ITEM_INFO also re-reads the
                                                weapons' item info
reset                                    1      every key back to its default
save [file]                              2      write every key to the file (rccfgfile by
                                                default), in the game directory
savediff <file>                          2      meant to write only the keys that differ from
                                                their defaults; it tests each heading's first key
                                                for the whole heading, so a heading comes out
                                                whole or empty (RC2's bug, kept)
load [file]                              2      read a file over the current values (without
                                                resetting first), and send the changes
```

Levels are the admin levels above (a `sv_tempadmins` player counts as level 1). Commands are
case-sensitive.

## LCS files

`maps/lcs/<map>.lcs` in the game directory: a level customization script, run in multiplayer
only, which adds, removes and changes a map's entities without recompiling it. RC2's descendant
of RC 1.9's `.lec` files, in a new format. The release has scripts for six Half-Life campaign
maps (`c0a0`, `c1a0`, `c2a4f`, `c2a5`, `c2a5a`, `c3a2a`), mostly adding deathmatch spawn points.

One command per line; `#` starts a comment line. A token in `"..."`, `'...'` or `{...}` keeps
its spaces.

```
line                                         what it does
create <class> {x y z} {pitch yaw roll}      spawn it once, 0.25 s after the script runs
respawncreate <class> {x y z} {p y r}        spawn it, and again 1.5 s after each death
advcreate <class> <alterations>              spawn it with alterations applied before and after
                                             it spawns
removeall / killall / useall <class>         every live entity of that class: remove it, deal it
                                             100000 damage, or use it (USE_SET 1, from the
                                             world). A leading '*' matches a prefix:
                                             "*monster_" is every monster, "monster_" is none
removeent / killent / useent <index>         the same for one entity index
edit <index> <alterations>                   apply alterations to that entity once
maintain <index> <alterations>               apply them every frame while it lives
setspawncommand <alterations>                meant to apply them to every player as he
                                             spawns; the alteration code skips players, so it
                                             changes nothing (unverified in play)
```

The creates are precached in the first pass (world precache); everything runs in the second,
three seconds into the map.

Alterations are `key=value` tokens that set an entity's fields: `origin` (or `org`, `pos`),
`vel`, `avel`, `ang`, `health`, `max_health`, `armorvalue`, `grav`, `friction`, `scale`,
`alpha`, `rcolor`, `trans`/`notrans`, `glowsh`/`noglowsh`, `takedamage`/`notakedamage`, `seq`,
`frate`, `targetname`, `target`, `owner`, `dodmg` (damage it), `keyvalue=<key>,<value>` (a map
keyvalue), `select=<index>` (alter that entity from here on), and more; keys match by prefix,
first match wins. Values are expressions over numbers, vectors, other entities' and players'
fields, time and random numbers; `dlls/rc2/maintainent.cpp` has the keys and
`dlls/rc2/alteration.cpp` the expression syntax. Two RC2 quirks, kept: the expression parser
loses one character from the end for every space it strips, so write alterations without
spaces (the release's `c3a2a.lcs` has spaces in its `edit` lines, so they come out wrong,
unverified how); and players and the world can't be altered.

## Files

In the game directory unless noted:

```
file                    read or written
rc2.ini                 all the tuning (rccfgfile; RC_INI.md). Read at each map start; rccfg
                        save and savediff write it or another file
maps/lcs/<map>.lcs      the map's level customization script. rcadmin lcs appends to it
adminprim.cfg           admin levels by WON id; in the server's working directory, not the game
                        directory. Read at connect
game.cfg                run when the game rules are installed, at each map start (as Half-Life)
skill.cfg               run once at startup (as Half-Life)
mapcycle.txt            the map cycle (as Half-Life); empty means bootcamp
motd.txt                (the motdfile cvar) shown to a player as he joins, then the attribute
                        menu opens
savedattrs.txt          client side: the attribute menu's ten saved setups
```
