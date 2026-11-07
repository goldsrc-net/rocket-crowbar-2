#!/usr/bin/env bash
# dump-signatures.sh - extract recoverable C++ signatures from the original
# Rocket Crowbar 2 (beta 1.1) libraries, to seed reconstruction of the source.
# Same method as mod-ts/reference/dump-signatures.sh, minus the Linux half:
# RC2 never shipped a Linux build, so the Windows exports are all we have.
#
#   rc2.dll       Windows MSVC server dll, no PDB, no RTTI. Exports are the SDK's
#                 save/restore callbacks (think/touch/use, fully typed once
#                 demangled) and the LINK_ENTITY_TO_CLASS factories.
#   client.dll    Windows MSVC client dll. Exports are mostly VGUI templates.
#   beta1/        the same two libraries from beta 1 (2004-01-03), for diffing.
#
# Output (written next to this script), per dll <base>:
#   exports-<base>.raw      raw mangled exports
#   signatures-<base>.txt   demangled signatures
#   entities-<base>.txt     unmangled exports (entity classnames + engine API)
#   fields-rc2.txt          m_* save/restore field names found in rc2.dll strings
#   classes.txt             distinct class names with method counts
set -euo pipefail
cd "$(dirname "$0")"

PE_DLLS="rc2.dll client.dll"

UNDNAME=""
for c in llvm-undname llvm-undname-21 llvm-undname-20 llvm-undname-19 llvm-undname-18; do
  if command -v "$c" >/dev/null 2>&1; then UNDNAME="$c"; break; fi
done
[ -n "$UNDNAME" ] || { echo "ERROR: no llvm-undname found (apt install llvm)"; exit 1; }

export LC_ALL=C
: > classes.tmp
for DLL in $PE_DLLS; do
  base="${DLL%.dll}"
  echo ">> $DLL exports"
  objdump -p "$DLL" | sed -n '/\[Ordinal\/Name Pointer\] Table/,/^$/p' \
    | awk 'NR>1 && NF {print $NF}' > exports-all.tmp
  grep '^?' exports-all.tmp | sort -u > "exports-$base.raw"
  grep -v '^?' exports-all.tmp | sort -u > "entities-$base.txt"
  "$UNDNAME" < "exports-$base.raw" > "signatures-$base.txt" 2>/dev/null || true
  echo "   $(wc -l < "exports-$base.raw") mangled, $(wc -l < "entities-$base.txt") unmangled"
  cat "signatures-$base.txt" >> classes.tmp
done
rm -f exports-all.tmp

strings -n 4 rc2.dll | grep -E '^m_[A-Za-z0-9_]+$' | sort -u > fields-rc2.txt
echo ">> $(wc -l < fields-rc2.txt) field names"

grep -oE '[A-Za-z_][A-Za-z0-9_]*::' classes.tmp | sed 's/:://' \
  | sort | uniq -c | sort -rn > classes.txt
rm -f classes.tmp
echo ">> $(wc -l < classes.txt) distinct classes"
