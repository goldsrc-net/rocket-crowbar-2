#!/usr/bin/env python3
"""rc2_layout.py - a copy of SDK 2.3 laid out like RC2's classes, for a closer reference build.

RC2 added virtuals and data members to SDK 2.3's base classes (reference/BASE_VIRTUALS.md,
and the class sizes and save-table offsets compared in 2026-11-07's session). Every stock
function that calls one of the moved virtuals or touches a moved member compiles to different
bytes, so against plain SDK 2.3 it doesn't match even when RC2 never changed it. This copies
the pristine singleplayer tree and adds placeholders of RC2's sizes at RC2's places:

  CBaseEntity        one virtual before Respawn (RC2 slot 47), 21 after FVisible (58-78),
                     28 bytes of members (where: --entity-pad, see below)
  CBaseMonster       one virtual after StopFollowing (slot 147), 4 bytes after m_pCine
  CTalkMonster       4 bytes before m_useTime
  CBasePlayerItem    four virtuals after AttachToPlayer (slots 93-96)
  CBasePlayerWeapon  8 bytes before m_flTimeWeaponIdle
  CBasePlayer        2576 bytes after its last member

The placeholders' bodies don't matter (stock code never calls them); their slots and sizes do.
--entity-pad a puts CBaseEntity's 28 bytes right after m_pfnBlocked, b at the end of the class,
since the save tables only bound them between m_pfnBlocked and CBaseDelay.

Usage: scripts/vc6/rc2_layout.py <pristine singleplayer dir> <out dir> --entity-pad a|b
"""
import os, shutil, sys

EDITS = [
    ("dlls/cbase.h", "\tvirtual CBaseEntity *Respawn( void ) { return NULL; }",
     "\tvirtual CBaseEntity *RC2V47( void ) { return this; }\n", "before"),
    ("dlls/cbase.h", "\tvirtual\tBOOL FVisible ( const Vector &vecOrigin );",
     "".join("\n\tvirtual int RC2V%d( void ) { return 0; }" % k for k in range(58, 79)), "after"),
    ("dlls/basemonster.h", "\tvirtual void StopFollowing( BOOL clearSchedule ) {}",
     "\n\tvirtual int RC2V147( void ) { return 0; }", "after"),
    ("dlls/basemonster.h", "\tCCineMonster\t\t*m_pCine;", "\n\tint m_rc2MonsterPad;", "after"),
    ("dlls/talkmonster.h", "\tfloat\t\tm_useTime;", "\tint m_rc2TalkPad;\n", "before"),
    ("dlls/weapons.h", "\tvirtual void AttachToPlayer ( CBasePlayer *pPlayer );",
     "".join("\n\tvirtual int RC2V%d( void ) { return 0; }" % k for k in range(93, 97)), "after"),
    ("dlls/weapons.h", "\tfloat\tm_flTimeWeaponIdle;", "\tint m_rc2WeaponPad[2];\n", "before"),
    ("dlls/player.h", "\tfloat m_flNextChatTime;\n", "\tchar m_rc2PlayerPad[2576];\n", "after"),
]
ENTITY_PAD = {
    "a": ("dlls/cbase.h", "\tvoid (CBaseEntity ::*m_pfnBlocked)( CBaseEntity *pOther );",
          "\n\tint m_rc2EntityPad[7];", "after"),
    "b": ("dlls/cbase.h", "\tint m_fireState;\n", "\tint m_rc2EntityPad[7];\n", "after"),
}


def apply(root, path, anchor, text, where):
    p = os.path.join(root, path)
    s = open(p, encoding="latin-1", newline="").read()
    crlf = "\r\n" in s
    s = s.replace("\r\n", "\n")
    n = s.count(anchor)
    if n != 1:
        sys.exit("%s: anchor found %d times: %r" % (path, n, anchor))
    s = s.replace(anchor, text + anchor if where == "before" else anchor + text)
    open(p, "w", encoding="latin-1", newline="").write(s.replace("\n", "\r\n") if crlf else s)


def main():
    if len(sys.argv) != 5 or sys.argv[3] != "--entity-pad" or sys.argv[4] not in ENTITY_PAD:
        sys.exit(__doc__)
    src, out, pad = sys.argv[1], sys.argv[2], sys.argv[4]
    if os.path.exists(out):
        sys.exit("%s exists; remove it first" % out)
    shutil.copytree(src, out)
    for e in EDITS + [ENTITY_PAD[pad]]:
        apply(out, *e)
    print("RC2 layout (entity pad %s) -> %s" % (pad, out))


if __name__ == "__main__":
    main()
