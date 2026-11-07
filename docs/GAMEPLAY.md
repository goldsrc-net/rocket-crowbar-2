# How Rocket Crowbar 2 plays

What a player sees in Rocket Crowbar 2 (RC2, beta 1.1) deathmatch, as the rebuild's code has it.
The cvars and commands named here are in [SERVER.md](SERVER.md); every number marked "rc2.ini"
can be changed there ([RC_INI.md](RC_INI.md) lists each key, its default and the release's
value). Where the rebuild differs from RC2 itself,
[reference/DEVIATIONS.md](../reference/DEVIATIONS.md) says so; the per-file detail, with the
original's addresses, is in [reference/dossiers/](../reference/dossiers/).

This page is read from the code. The rebuild has been built and booted, not played, so nothing
here has been seen in game; where the code's effect isn't clear, it says "unverified".

RC2 has its own client library, which draws most of its effects (the shield, the lightning, the
forklift headlights, the nuke's blast) and its menus: the attribute menu, the vote display and an
energy bar on the HUD (heads-up display).

## Joining

- You join in a spectator view. Jump switches between free look over the spawn points and
  following players; secondary attack and `+attack3` step forward and back through them; primary
  attack joins the game.
- The server's MOTD (message of the day) shows, then the attribute menu opens (`attrconfig`
  reopens it). What you pick there applies from your next spawn.
- Every spawn gives you the crowbar plus the weapons your attributes bought, each with the extra
  ammo rc2.ini [Attributes] gives it, unless the map has a `game_player_equip`.
- `spectate` goes back to the spectator view mid-game, and kills you to do it.

## Controls

Every weapon has four attacks:

```
button      command   default key (the release's gfx/shell/kb_def.lst)
primary     +attack   mouse 1, enter
secondary   +attack2  mouse 2
tertiary    +attack3  '
quaternary  +attack4  ;
```

Each attack sets its own wait before each of the four can fire again (rc2.ini
`<Attack>Refire<Attack>` keys, sixteen per weapon, more for zoomed modes). Most charged attacks
burn ammo while they charge, and a charging weapon can't be put away. Fully under water most of
RC2's attacks only click, unless you bought the submersible weapons ability. Frozen players
can't fire or switch weapons.

The other RC2 commands (`jetpack`, `drawenergy`, `createhealth`, `shield`, `transform`,
`rideplayer`, `attrconfig`) have default keys too, all but `shield`.

## Weapons

Every weapon keeps its Half-Life slot and does something else. Ammo, damage, refire
times and limits come from rc2.ini, one heading per weapon. Where a weapon "costs energy", it is
the player's energy (below).

```
weapon         primary                   secondary                 tertiary                  quaternary
crowbar        swing                     a rocket, stronger with   throw it: the crowbarang  hold to charge (costs
                                         the charge; costs energy  flies out and back,       energy); charged swings
                                                                   sticks in walls           shove what they hit, or
                                                                                             you off a wall
glock          a bouncing gib            "the fart": a push along  a sticky gib that gasses  charge a transphasic shot
                                         your view                 what is near it           that passes through walls
                                                                                             and hurts all in line;
                                                                                             costs energy
python         a lightning bolt          charge a chain lightning  charge a ball lightning   zoom
                                         that jumps between        that blasts what it can
                                         visible players           see
mp5            blaster bolts             "the fart" (grenade       bolts that bounce off     charge a phase pulse that
                                         ammo): hurts what is      walls                     blasts on impact
                                         behind you and throws
                                         you forward
shotgun        throw a scientist, feet   launch an HEV suit that   launch a mini forklift    charge a crate (burns
               first                     flies on its own engine   that drives along floors  shells); it grows until it
                                                                   and blows up on walls     bursts
                                                                   and monsters
crossbow       the impailer: a harpoon   charge an exploding bolt  a tether, or recall the   zoom
               that drags and bleeds                               one out
               what it hits
  zoomed       a hitscan shot            a charged hitscan shot    a charged exploding       zoom out
                                                                   hitscan shot
rpg            a scientist mine          a suck mine that ropes    a sphere mine that rolls  a "roman candle" mine
                                         its target and pulls it   to targets and shoots     that fires volleys of
                                         in                        them; costs energy        rockets; costs energy
gauss          (see below)
egon           tractor beam: grab and    push what it holds away;  pull what it holds in;    throw what it holds; with
               carry                     with the beam off, a      with the beam off,        the beam off, the nuke
                                         shrinker shot             charge the imploder
hornet gun     a freeze shot             charge a stronger freeze  a freeze grenade that     zoom
                                         shot; held too long it    sticks to walls and
                                         backfires on you          players
hand grenade   anti-friction grenade     suck grenade              gravity grenade           -
satchel        throw a roach charge;     throw another roach       a conc charge that sticks a remote-control forklift
               with roaches out, arm     charge                    and blasts everything     you drive (below)
               them all                                            away
tripmine       plant a laser mine        plant a bouncing-beam     plant a punch mine whose  plant a target mine that
                                         mine                      beam bores through walls  zaps targets in front of
                                                                                             it
snark          throw a snark             toss a mini grunt         stick a snark cam on a    look through your snark
                                                                   wall                      cam
```

Notes:

- **Gauss.** It works around a beacon you throw with the quaternary attack (it sticks to walls
  and rides doors and trains; it dies after 30 s in flight). With the beacon out, primary
  teleports what you hit to the beacon, secondary teleports you to it, tertiary teleports
  everything near your aim point, and quaternary takes the beacon back (free within 56 units).
  With no beacon, primary swaps places with what you hit, secondary sends you to a random spawn
  point, and tertiary charges a blink to where you aim; aiming at the sky kills you ("in_space").
  A teammate where you land telefrags the one who moved.
- **Crowbarang.** While one is out you can't pick the crowbar up. With the crowbar control
  ability it steers back toward you while you can see it; the code reads the ability bit, not a
  held button (unverified what the player controls).
- **Egon.** The nuke costs 200 cells and 100 energy (rc2.ini [Egon]). You see through it and
  steer it with your view; after its first second, attack again sets it off, and it also goes off
  on touch or when you die, freeze or leave. Its blast reaches through 256 units of wall. Pressing
  secondary while the imploder charges shrinks you instead, with no end time (unverified how you
  grow back).
- **Snark cam.** Looking through one: secondary and `+attack3` cycle your cams, primary fires
  the cam's laser for 1 energy, releasing `+attack4` looks back.
- **Mines.** Tripmine lasers hurt what crosses them instead of setting the mine off; a mine
  blows when shot, when its 120 s are up, or when the door or train it sits on moves. The
  planter sees through his own mine until he walks 64 units away. Mines, roaches, snarks, snark
  cams and the gravity and suck grenades each have a maximum out at once per player (rc2.ini
  `...MaxDeployed`, `...MaxOut`).
- **Riding.** A weapon or item you can't take (you already have it) goes to a player riding
  your shoulders.
- **Dropping.** `drop` makes a weapon box that lasts 120 s; with more than 20 weapon boxes in the
  map, nothing drops.

## Energy

Every player has energy, 0 to 100, shown on the HUD. It regenerates on its own (rc2.ini
[Attributes] EnergyBaseRegenerationRate, about 0.83 a second, times the energy regeneration
attribute; a quarter as fast with the shield up), pausing briefly after each use. Each kill of
a player gives 10.

Energy pays for:

```
use                      cost (rc2.ini [Attributes] unless noted)
jetpack                  5 to start, then 5 a second idle, about 6.7 a second moving
jetboost                 10 a boost
transform                60, each way
shoulder riding          40 to climb on, 20 to get off (fixed in the code)
shield                   5 to raise ([Shield]); hits on it drain energy instead of health
damage reflection        a third of the damage reflected
crowbar rocket, glock    each weapon's own rc2.ini key
  transphasic, RPG mines,
  nuke, snark cam laser
```

Drawing energy (`drawenergy`, on the ground) takes 3 s and gives 80, then can't be used again
for 45 s. Creating health (`createhealth`) turns 30 energy into up to 150 health over 4.5 s, then
waits 45 s.

## Attributes

The attribute menu (`attrconfig`) spends a budget of points, rc2.ini [Attributes] TotalPoints
(150 by default; the release's rc2.ini sets 9999, which makes it effectively unlimited), on ten
values, on abilities, on starting weapons and on one transform. A choice that goes over the
budget is replaced with the defaults ("Too many attribute points used"). In multiplayer it
applies from your next spawn; in single player once.

The values, as the code computes them from the default rc2.ini prices (derived from the
formulas in `dlls/client.cpp`, not seen in game):

```
attribute          what it does                          default (its points)   most (points)
jump               scales jump height                    1x (10)                5x (50)
speed              scales movement speed                 1x (20)                3x (60)
attack             scales damage you deal                1x (30)                2x (60)
defend             scales damage you take                1x (30)                0.33x (60; 40 in the
                                                                                release, 0.69x)
health             spawn health                          100 (20)               300 (100)
extra health       room above spawn health               0                      +500 (100)
armor              spawn armor (and the battery cap      0                      500 (100; 80 in the
                   when over 100)                                               release, 400)
extra armor        added to the battery cap              0                      1000 (100; 80, 800)
energy regen       scales energy regeneration            1x (10)                6x (60)
damage reflection  share of damage taken sent back to    0                      0.33 (50)
                   the attacker, for energy
```

Zero points in jump gives no jump, and health starts at 50 (rc2.ini MinHealth); speed and
energy regeneration need at least one point. Health above the maximum decays back to it.

Abilities, each for its rc2.ini price:

```
ability              what it gives
wallhop              in the air, jump off a wall you touch
aqualung             no drowning
jetboost             in the air, jump boosts you where you look (energy, 2 s apart)
jetpack              the jetpack command
energy draw          the drawenergy command
create health        the createhealth command
crowbar control      steer the crowbarang (see Weapons)
shield               the shield command; it starts raised when you spawn
long jump            the long jump module
submersible weapons  RC2's weapons fire under water
```

Weapons: the glock, Python, MP5, shotgun, crossbow, RPG, egon, gauss and hornet gun can each be
bought as a spawn weapon. The crowbar always comes.

## Shield

An ellipsoid round the player (20 units across and 40 high standing, 26 and 30 ducking). A hit
on it costs energy instead of health and flashes where it struck; projectiles bounce off it.
rc2.ini [Shield] sets fractions per kind of damage (blast 0.6, bullets 0.75, discharges 0.5);
unverified exactly how each applies. It drops when energy falls under 5. Raising it isn't
allowed while shrunk, transformed or riding.

## Transforms

One of five monsters, picked in the attribute menu; `transform` changes into it and back, for 60
energy each way, when the server allows it (`mp_allowtransform`). You can't transform while
dead, shrunk, frozen, riding, being ridden or racing. You're held still for a second or so
while it happens; your health keeps its fraction of the maximum. The assassin and controller
need room for their size ("Not enough room to transform"); the houndeye, bat and headcrab don't
check (RC2's bug, kept). Your weapon is put away until you change back, and gives and batteries
don't work on you meanwhile.

```
transform    primary                  secondary               +attack3             +attack4
assassin     pistol, spread tightens  kick: a swipe, then a   cloak (on energy)    zoom
             while you fire           back kick
houndeye     charge and let off the   charge a beam that      lie down to sleep:   locator (on energy)
             sonic blast              hits through walls,     faster health and
                                      with recoil             energy
controller   throw a ball that        hold to charge a ball   -                    -
             blasts on impact         that leeches health
                                      and gives it back
stukabat     bite                     -                       -                    land on the floor or
                                                                                   hang from the ceiling
                                                                                   (faster regeneration)
headcrab     leap: damage by speed;   -                       -                    -
             a slow hit heals
```

rc2.ini has a heading per transform ([TransformAssassin] and the rest). The assassin also
wallhops by nature.

## Riding, forklifts and racing

- **Shoulder riding.** `rideplayer` climbs onto the shoulder of the player in front of you
  (within 64 units), left or right; up to two can ride one player. Riders take no damage but the
  world's and die with the player under them; what he can't carry goes to them. Being ridden is
  up to the ridden player: his userinfo `allowriding` (the menu's "Allow others to ride you",
  off by default) and the server's `mp_riding`; in teamplay only teammates. A rider's shots
  belong to the player he rides.
- **Mini forklift** (shotgun tertiary): driverless, it runs along the floor and blows up on
  walls and monsters.
- **Remote-control forklift** (satchel quaternary): you watch through it and drive it. It
  sticks to and climbs any world surface, steers by your view, speeds up and slows on your
  movement buttons, jumps off the surface on secondary attack, and blows up on primary, on
  hitting something that can be hurt, or when you die. Clients with `cl_forkliftlights 1` see
  its headlights.
- **Racing** (`rc_race`): using a map's race controller turns you into a mini forklift on its
  track, which the movement code steers along the track's points. You need the transform's 60
  energy to start, though the race doesn't spend it. `+use` gives up the race at a spawn point;
  `kill` doesn't work while racing. The server sends the client the track; how a race is timed
  or won is unverified (no lap or finish code turned up on the server side).

## Scoring

- Frags as in Half-Life: one per kill, minus one for a suicide or a death by the world.
- With `mp_scoremode 1` there is also a damage-based score, shown on the scoreboard and used
  for the frag limit: damage you deal to players (0.8 per maximum health's worth) and to
  monsters (0.1), plus 0.2 a kill; minus for damage taken (0.01 by players, 0.05 by the world,
  0.1 by yourself) and for deaths (0.05 to a player, 0.25 otherwise, 0.5 a suicide). All
  rc2.ini [Score].
- Death messages are picked at random, weighted, from a table of 383 lines by weapon or cause,
  with the victim's pronouns from his userinfo `gender`.

## Players

- **No fall damage** in multiplayer.
- **Head shots** do 2.5 times the damage; other hit groups normal.
- **Freezing** (hornet gun) holds the victim, and the players riding with him, still and
  without weapons until it thaws.
- **Shrinking** (egon shrinker) makes a player a fifth of his size for 10 s: slower, unable to
  duck, and crushed by full-size players.
- **Votes** for a map or a kick, on screen (SERVER.md).
- **Map exits.** In multiplayer a level's `trigger_changelevel` kills you instead
  (`sv_noexit`).
- **Random items** with `mp_random` (SERVER.md).

## Monsters

- They spawn in multiplayer, drop nothing there, and their corpses fade.
- Frozen monsters stop thinking but are still pushed; a frozen ichthyosaur doesn't swim.
  Shrunk ones talk higher and fear full-size things.
- Admins can make monsters allied or hostile (`rcadmin mkally`, `mkhostile`).

## Entities RC2 adds

Placeable in maps or LCS (level customization script) files:

```
race_controller  target = the race_path points' name; exit_target; track_radius
race_path        a track point; radius
info_player_coop a plain point entity
maintainer_ent   made by LCS "maintain" lines
lcs_spawner      made by LCS "create" lines
```

Items take a `norandomize` keyvalue that keeps `mp_random` off them.

The rest are made by the game: the projectiles (`blaster_bolt`, `bouncy_bolt`, `phase_pulse`,
`glock_gib`, `glock_stickygib`, `transphasic_gib`, `impailer`, `xbow_teather`, `explode_bolt`,
`crowbar_rocket`, `crowbarang`, `shrinker_project`, `nuclear_missile`, `ball_lightning`,
`chain_hurter`, `shotgun_sci`, `shotgun_hev`, `shotgun_forklift`, `shotgun_crate`,
`control_forklift`, `snark_project`, `snark_cam`, `mini_grunt`, `sm_project`, `rm_rocket`,
`gauss_becon`), the mines and grenades (`scientist_mine`, `suck_mine`, `sphere_mine`,
`rocket_mine`, `bouncy_tripmine`, `punch_tripmine`, `target_tripmine`, `roach_charge`,
`conc_charge`, `antifriction_grenade`, `suck_grenade`, `gravity_grenade`, `freeze_grenade`,
`ultima_grenade`), and helpers (`freeze_timer`, `shrink_timer`, `antifric_timer`,
`implode_timer`, `virtual_hull`, `tcontroller_tball`, `tcontroller_cball`, `timedevent`).
`ultima_grenade` has no weapon attack; it can only be made by name (an LCS create, or `give`
once something has precached it). `reference/entities-rc2.txt`
lists every class the original exports.

## Half-Life entities RC2 doesn't have

- `crossbow_bolt`: the crossbow fires `impailer`, `explode_bolt` and `xbow_teather`.
- `func_vehicle` and `func_vehiclecontrols` come from Half-Life Updated. The rebuild keeps them;
  RC2 has neither (not yet listed in reference/DEVIATIONS.md).
