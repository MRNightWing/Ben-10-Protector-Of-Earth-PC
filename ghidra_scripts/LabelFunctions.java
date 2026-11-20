// LabelFunctions.java
// Labels 46 reverse-engineered engine functions in the Ben 10 PSP Ghidra project.
// Run via Ghidra Headless: -postScript LabelFunctions.java

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Listing;

public class LabelFunctions extends GhidraScript {

    // -----------------------------------------------------------------------
    // Data: { address (long), name, description }
    // -----------------------------------------------------------------------
    private static final Object[][] FUNCTIONS = {
        // Core Engine
        { 0x0008e298L, "Engine_MainLoop",          "Main game loop: input → update → render → vblank" },
        { 0x0008e4d0L, "Engine_WaitVblank",         "Waits for vertical blank via sceDisplayWaitVblankStart" },
        { 0x0006f824L, "Engine_LevelManager",       "Level management, calls Engine_LoadStartLevel" },
        { 0x000070acL, "Engine_LoadStartLevel",      "Loads start level; refs OFFER_MULTIPLAYER, AUTO_CONNECT strings" },
        { 0x00006e94L, "Engine_GetConfigParam",      "Reads config/boot parameters" },

        // Input / Controller
        { 0x0007141cL, "Input_PollController",      "Polls PSP controller via sceCtrlReadBufferPositive stub" },
        { 0x00071af0L, "Engine_UpdateInput",         "Processes raw input state each frame" },
        { 0x000e6758L, "sceCtrlSetSamplingMode",    "PSP library stub – sets controller sampling mode" },
        { 0x000e6770L, "sceCtrlSetSamplingCycle",   "PSP library stub – sets controller sampling cycle" },

        // WAD Streaming
        { 0x000811ecL, "Engine_StreamStratWad",     "Initiates WAD strategy stream; ref: StreamStratWad %d:\"%s\"" },
        { 0x0007fb14L, "CStreamManager_StartWad",   "Starts loading a WAD archive; ref: Starting Wad %d:\"%s\"" },
        { 0x0007fd20L, "CStreamManager_StopWad",    "Stops an active WAD stream; ref: Stopping Wad" },
        { 0x0007fdc4L, "CStreamManager_SuspendWad", "Suspends WAD streaming (pause state)" },
        { 0x0007ff64L, "CStreamManager_UnsuspendWad","Resumes a suspended WAD stream" },
        { 0x00080d2cL, "CStreamManager_StreamWad",  "Core WAD streaming loop" },
        { 0x00081648L, "CStreamManager_RemoveWad",  "Removes/unloads a WAD from the stream manager" },
        { 0x00081f5cL, "CStreamManager_LoadMapAndStrats","Loads map data and strategy WADs" },
        { 0x00080458L, "CStreamManager_StartLoad",  "Initiates async load sequence for a WAD" },

        // PSP File / UMD
        { 0x00070e08L, "PSPFile_Open",              "Opens a file on UMD/MS via sceIo" },
        { 0x00070efcL, "PSPFile_Close",             "Closes an open PSP file handle" },
        { 0x00070f5cL, "PSPFile_Read",              "Synchronous read from PSP file" },
        { 0x000705acL, "PSPFile_AsyncClose",        "Asynchronous file close" },
        { 0x00070654L, "PSPFile_AsyncRead",         "Asynchronous file read" },
        { 0x00070a90L, "Engine_UMDActivate",        "Activates UMD drive (sceUmdActivate)" },
        { 0x00070af8L, "Engine_UMDCheckState",      "Polls UMD drive ready state" },

        // Graphics / Framebuffer
        { 0x000c78e8L, "Engine_InitFramebuffer",    "Initialises PSP framebuffer; calls Engine_SetupRenderBuffers" },
        { 0x000c76c4L, "Engine_SetupRenderBuffers", "Configures double-buffered render targets" },
        { 0x000e6738L, "sceDisplaySetFrameBuf",     "PSP library stub – sets active framebuffer" },
        { 0x000e6740L, "sceDisplayWaitVblankStart", "PSP library stub – waits for vblank start" },

        // Memory
        { 0x000573c8L, "Engine_MemAlloc",           "Main heap allocator" },
        { 0x000578e4L, "Engine_MemPrintStats",      "Prints heap usage statistics to log" },
        { 0x000924a0L, "Engine_VolatileMemInit",    "Initialises volatile/scratch memory pool" },
        { 0x00092774L, "Engine_VolatileMemShutdown","Shuts down volatile memory pool" },
        { 0x0007f5ecL, "Engine_VolatileMemUnlock",  "Unlocks volatile memory region for write" },

        // Audio / Video
        { 0x0007f01cL, "Engine_Log",               "Central logging function (called by all WAD/stream log strings)" },
        { 0x00089dc4L, "Sound_Play",               "Plays a sound effect or music track" },
        { 0x000af670L, "Sound_LoadControlFile",    "Loads audio control/bank file" },
        { 0x000900c4L, "Video_MpegInit",           "Initialises MPEG/FMV decoder" },
        { 0x00092fb0L, "Video_DecodeAtrac",        "Decodes Atrac3+ audio frame" },
        { 0x000930a8L, "Video_DecodeAvc",          "Decodes AVC/H.264 video frame" },

        // Animation
        { 0x000203d4L, "Anim_SetSpeed",            "Sets animation playback speed multiplier" },
        { 0x00020e44L, "Bone_FindByName",          "Finds a skeleton bone node by name string" },
        { 0x00065174L, "Anim_PlayBlend",           "Blended animation playback" },

        // Debug
        { 0x00077c60L, "Debug_RenderFPS",          "Renders FPS counter on-screen" },
        { 0x000775a4L, "Debug_TakeScreenshotTGA",  "Captures screen to TGA file on MS" },
        { 0x0007774cL, "Debug_CheckStackSize",     "Validates thread stack has not overflowed" },
    };

    @Override
    public void run() throws Exception {
        println("=== LabelFunctions: Starting ===");
        println("Total functions to label: " + FUNCTIONS.length);

        FunctionManager fm  = currentProgram.getFunctionManager();
        SymbolTable    st   = currentProgram.getSymbolTable();
        Listing        lst  = currentProgram.getListing();

        int labeled   = 0;
        int created   = 0;
        int commented = 0;
        int skipped   = 0;

        int txId = currentProgram.startTransaction("LabelFunctions");
        try {
            for (Object[] entry : FUNCTIONS) {
                long   addrLong = (Long)   entry[0];
                String name     = (String) entry[1];
                String desc     = (String) entry[2];

                Address addr = toAddr(addrLong);

                // --- 1. Ensure a Function object exists at this address ---
                Function func = fm.getFunctionAt(addr);
                if (func == null) {
                    // Try to create one
                    try {
                        func = createFunction(addr, name);
                        if (func != null) {
                            created++;
                            println("CREATED function: " + name + " @ " + addr);
                        }
                    } catch (Exception ex) {
                        println("WARN: Could not create function at " + addr + ": " + ex.getMessage());
                    }
                }

                // --- 2. Set / update the function name ---
                if (func != null) {
                    String oldName = func.getName();
                    if (!oldName.equals(name)) {
                        func.setName(name, SourceType.USER_DEFINED);
                        labeled++;
                        println("RENAMED: " + oldName + " → " + name + " @ " + addr);
                    } else {
                        println("OK (already named): " + name + " @ " + addr);
                        skipped++;
                    }

                    // --- 3. Add plate comment (description) ---
                    CodeUnit cu = lst.getCodeUnitAt(addr);
                    if (cu != null) {
                        String existing = cu.getComment(CodeUnit.PLATE_COMMENT);
                        if (existing == null || existing.isEmpty()) {
                            cu.setComment(CodeUnit.PLATE_COMMENT, desc);
                            commented++;
                        }
                    }
                } else {
                    // No function and couldn't create — just set a label
                    try {
                        st.createLabel(addr, name, SourceType.USER_DEFINED);
                        labeled++;
                        println("LABELED (no func): " + name + " @ " + addr);
                    } catch (Exception ex) {
                        println("WARN: Could not create label at " + addr + ": " + ex.getMessage());
                        skipped++;
                    }
                }
            }
            currentProgram.endTransaction(txId, true); // commit
        } catch (Exception e) {
            currentProgram.endTransaction(txId, false); // rollback on error
            throw e;
        }

        println("=== LabelFunctions: Done ===");
        println("  Renamed  : " + labeled);
        println("  Created  : " + created);
        println("  Commented: " + commented);
        println("  Skipped  : " + skipped);
    }
}
