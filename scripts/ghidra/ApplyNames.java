// ApplyNames.java - Ghidra headless post-script for RC2's Windows libraries.
//
// Names functions from reference/names-<lib>.tsv (scripts/vc6/match.py: rva, size, mangled
// name, demangled name, reference object). A function is created where analysis didn't make
// one, the MSVC name is set and demangled, which puts it in its class namespace and applies
// the prototype. Functions analysis already named (exports, Function ID) are only renamed
// when they still have a default FUN_ name.
//
// Args: -postScript ApplyNames.java <reference dir>
import ghidra.app.script.GhidraScript;
import ghidra.app.util.demangler.DemangledObject;
import ghidra.app.util.demangler.DemanglerUtil;
import ghidra.app.util.demangler.DemanglerOptions;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

public class ApplyNames extends GhidraScript {

    @Override
    public void run() throws Exception {
        String refDir = getScriptArgs()[0];
        String lib = currentProgram.getName().replaceAll("\\.dll$", "");
        File tsv = new File(refDir, "names-" + lib + ".tsv");
        Address base = currentProgram.getImageBase();
        int created = 0, renamed = 0, kept = 0, demangled = 0;
        try (BufferedReader r = new BufferedReader(new FileReader(tsv))) {
            String line = r.readLine(); // header
            while ((line = r.readLine()) != null) {
                String[] f = line.split("\t");
                Address a = base.add(Long.decode(f[0]));
                String name = f[2];
                Function fn = getFunctionAt(a);
                if (fn == null) {
                    disassemble(a);
                    fn = createFunction(a, null);
                    if (fn == null) {
                        printerr("ApplyNames: no function at " + a + " for " + name);
                        continue;
                    }
                    created++;
                }
                if (fn.getSymbol().getSource() != SourceType.DEFAULT &&
                    !fn.getName().startsWith("FUN_")) {
                    kept++;
                    continue;
                }
                fn.setName(name, SourceType.IMPORTED);
                renamed++;
                if (name.startsWith("?")) {
                    java.util.List<DemangledObject> ds = DemanglerUtil.demangle(currentProgram, name, a);
                    if (ds != null && !ds.isEmpty() &&
                        ds.get(0).applyTo(currentProgram, a, new DemanglerOptions(), monitor))
                        demangled++;
                }
            }
        }
        println("ApplyNames: " + lib + ": " + renamed + " named (" + created + " functions created, " +
                demangled + " demangled), " + kept + " already named by analysis");
        int total = 0, named = 0;
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            total++;
            if (!fn.getName().startsWith("FUN_")) named++;
        }
        println("ApplyNames: " + lib + ": " + named + " of " + total + " functions have a name");
    }
}
