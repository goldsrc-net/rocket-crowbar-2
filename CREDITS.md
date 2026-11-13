# Credits

This is a fan-made reconstruction of Rocket Crowbar 2 beta 1.1. It is not affiliated with or endorsed by
its authors. Everything below comes from the files RC2 itself ships (the beta installers'
`rc2/credits.txt` and the spec sheets in `rc2/maps/`); each entry names its source. RC2 ships no readme
of its own, and none of its files states terms for the mod as a whole.

## Rocket Crowbar 2

From `credits.txt`, as it lists them:

- **Programmers:** NinjaX9, "original programmer for Rocket Crowbar"; Laser, "2nd project manager,
  developed most of the RC2 code"; AK, "current maintainer of the code".
- **Maps:** Firewater Fischer (firewater_fischer@yahoo.com).
- **Web development:** Tweak (tweak@planethalflife.com), Arch, tkdan, EgEr.
- **Hosting:** Tomh (tomh@rocketcrowbar.com); cooplabs.com, "bandwidth provider, colocation services".
- **More credits:** MF-XG, "Loyal tester/generally evil"; Mr. Hankey (mrhanky7@hotmail.com), "For the
  jetpack model"; Raptor, "For suggesting what became the base for the mini-me turrets".

The other models, sounds and sprites ship without a credit.

## The maps

Each of the release's eight map spec sheets (`maps/RC_*.txt`) gives Andrew Fischer as the author:
rc_homestead, rc_inanis, rc_marblegarden, rc_margaritaville, rc_moonside, rc_oneking, rc_sandcrab and
rc_vertigoquad. `RC_Moonside.txt` gives his address as firewater_fischer@yahoo.com, the one
`credits.txt` gives for Firewater Fischer. Their own credits:

- `RC_Homestead.txt`: "Valve ERC for denoobing me"; "HLRally team for a couple of textures used for the
  terrain".
- `RC_Inanis.txt`: "Valve ERC for helping me learn important stuff"; its edited sounds come "from MDK2,
  Alice, Morrowind, and Quake 3".
- `RC_Marble Garden.txt`: "Valve ERC for helping me learn important stuff and providing a fast review;
  Some textures came from free online archives".
- `RC_Oneking.txt`: a remix of Oneking, crediting "Jeremiah L. Schwab for making the original Oneking".
- `RC_Vertigoquad.txt`: a remix of Vertigo.
- `RC_Sandcrab.txt`: "GlobalAssault.com and Juim of Hotdeathdiner.com for giving me fair and prompt
  reviews".

`killbox.bsp` ships with no spec sheet and no credit.

## Terms stated in the shipped map spec sheets

Every one of the eight spec sheets has the same "Copyright-Permissions" section, recorded as it states
it, for anyone redistributing the maps:

- "Authors MAY NOT use this level as a base to build additional levels."
- "You MUST NOT distribute this level UNLESS you INCLUDE THIS FILE WITH NO MODIFICATIONS!!!. If you
  don't co-operate, then DON'T DISTRIBUTE IT IN ANY FORM!!."
- "This BSP may be distributed ONLY over the Internet and/or BBS systems. You are NOT authorized to put
  this BSP on any CD or distribute it in any way without my permission."

This repository ships none of RC2's maps or other content; `reference/` keeps the original libraries,
for reconstruction only.

## The rebuild

- Steven "StevenlAFl" Linn: reconstruction.
- Built on the Half-Life SDK by Valve. Half-Life is a trademark of Valve Corporation.

### Half-Life Updated

The rebuild is built on [Half-Life Updated](https://github.com/twhl-community/halflife-updated). Its
contributors, as its README listed them at the fork point: Sam Vanheer, JoelTroch, malortie, dtugend,
Revenant100, fel1x-developer, LogicAndTrick, FreeSlave, zpl-zak, edgarbarney, Toodles2You, Jengerer,
thefoofighter, Maxxiii, johndrinkwater, anchurcn, DanielOaks, MegaBrutal, suXinjke, IntriguingTiles,
Oxofemple, YaLTeR, Ronin4862, the man, vasiavasiavasia95, NongBenz, Hezus, Anton, ArroganceJustified,
a1batross, zaklaus, Uncle Mike, Bacontsu, L453rh4wk, P38TaKjYzY, hammermaps, LuckNukeHunter99,
Veinhelm, jay!, BryanHaley, λλλλλλ, Streit, rbar1um43, LambdaLuke87, almix, sabian.

Its special thanks: Valve Software, Gearbox Software, Alfred Reynolds, mikela-valve, the TWHL Community,
Knockout, Gamebanana, ModDB.

### The 64-bit clients' VGUI

The 64-bit clients (for Xash3D) link two libraries in `third_party/`, as mod-ts's do:

- **openvgui** ([FWGS/openvgui](https://github.com/FWGS/openvgui)), an open VGUI 1 by Flying With
  Gauss, BSD 3-clause (`third_party/openvgui/LICENSE`), with its `miniutl` ("Copyright (C) 2018,
  Valve Software; Copyright (c) 2019, Flying With Gauss", BSD 3-clause,
  `third_party/openvgui/miniutl/LICENSE`).
- **vgui_support**, Xash3D's VGUI bridge ("Copyright (C) 2011 Uncle Mike", GPL version 3 or later,
  with an exception for linking Valve's VGUI library), in goldsrc-net's public fork
  ([goldsrc-net/vgui_support](https://github.com/goldsrc-net/vgui_support), branch `64bit`), which
  is the source the 64-bit clients are built from.
