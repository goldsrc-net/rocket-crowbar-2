// ListFunctions.java - Ghidra headless post-script: every function with its rva, size and name.
//
// Args: -postScript ListFunctions.java <out.tsv>
// Writes rva, size (body address count), name (namespaced, as ApplyNames left it).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;

public class ListFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        long base = currentProgram.getImageBase().getOffset();
        int n = 0;
        try (PrintWriter w = new PrintWriter(getScriptArgs()[0])) {
            w.println("rva\tsize\tname");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                w.printf("0x%x\t%d\t%s%n", f.getEntryPoint().getOffset() - base,
                         f.getBody().getNumAddresses(), f.getName(true));
                n++;
            }
        }
        println("ListFunctions: " + n + " functions -> " + getScriptArgs()[0]);
    }
}
