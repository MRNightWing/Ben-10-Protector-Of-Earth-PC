import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class TestGhidraHeadless extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("=== Ghidra Headless Connected Successfully! ===");
        println("Program name: " + currentProgram.getName());
        println("Image base: " + currentProgram.getImageBase());

        Address addr = toAddr(0x0008e298L);
        Function f = getFunctionAt(addr);
        if (f != null) {
            println("Function at 0x0008e298: " + f.getName());
        } else {
            println("No function at 0x0008e298, attempting getFunctionContaining...");
            f = getFunctionContaining(addr);
            if (f != null) {
                println("Function containing 0x0008e298: " + f.getName() + " at " + f.getEntryPoint());
            } else {
                println("Function not found at or containing 0x0008e298.");
            }
        }
    }
}
