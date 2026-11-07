#!/usr/bin/env bash
# normalize GetRCCaps/GetRCCaps2 to (CBaseEntity* pOther, CBaseEntity* pAsker) after a package merge
cd "$1"
files=$(grep -rlE 'GetRCCaps2?\(' dlls pm_shared cl_dll 2>/dev/null)
[ -n "$files" ] || exit 0
sed -i -E 's/GetRCCaps(2?)\(int [A-Za-z_0-9]*, CBaseEntity\* ([A-Za-z_0-9]*)\)/GetRCCaps\1(CBaseEntity* pOther, CBaseEntity* \2)/g; s/GetRCCaps(2?)\(\(int\)\(intptr_t\)([A-Za-z_0-9>-]*), /GetRCCaps\1(\2, /g; s/GetRCCaps(2?)\(\(int\)([A-Za-z_0-9>-]*), /GetRCCaps\1(\2, /g' $files
grep -nE "GetRCCaps2?\(int|first argument is declared int" $files || true
