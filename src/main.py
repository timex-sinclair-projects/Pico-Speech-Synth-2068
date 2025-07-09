"""
SP0256 Emulator for RP2040 Pico - MicroPython Version
Emulates the General Instruments SP0256-AL2 speech synthesizer

v44 - COMPLETE FIXED VERSION

DEBUGGING FEATURES:
- Comprehensive logging to Thonny console with timestamps
- Categorized debug output (SYSTEM, GPIO, AUDIO, TIMING, INTERFACE)
- Memory usage monitoring
- GPIO state tracking  
- Audio playback progress and timing analysis
- Interface monitoring with allophone name lookup

INTERACTIVE COMMANDS (type in Thonny console):
- SPEAK <allophones>    Play sequence: SPEAK HH EH LL OW
- SPEAK <numbers>       Play by ID: SPEAK 27 7 45 53  
- LIST                  Show all available allophones
- TEST                  Run hardware pin test
- STATUS                Show system status and statistics
- MEMORY                Show memory usage
- GPIO                  Show current GPIO pin states  
- DEBUG <category>      Toggle debug category on/off
- HELP                  Show this command list
- Examples:
  SPEAK PA1 HH EH LL OW PA1    # "Hello" with pauses
  SPEAK 0 27 7 45 53 0         # Same as above using numbers

DEBUG CONFIGURATION:
Set the DEBUG_* flags below to control logging verbosity:
- DEBUG_ENABLED: Master switch for all debugging
- DEBUG_GPIO: Pin state changes and address decoding
- DEBUG_AUDIO: Allophone playback progress  
- DEBUG_TIMING: Sample timing accuracy
- DEBUG_INTERFACE: ALD edge detection and command processing
- DEBUG_SYSTEM: Initialization, memory usage, system status
- DEBUG_VERBOSE: Extra detailed output (sample-by-sample)

HARDWARE CONNECTIONS (per schematic):
- GP0-GP5: Address inputs A1-A6 (from 74LVC245)
- GP8: ALD input (Address Load from 74HC138)  
- GP11: PWM audio output (REQUIRES external low-pass filter ~5kHz)
- GP13: LRQ output (Load Request, active low)
- GP14: SBY output (Standby)

IMPORTANT: Audio output on GP11 is RAW PWM and MUST have external filtering!
Suggested filter: 1kΩ resistor + 33nF capacitor to ground, then to amplifier.

USAGE:
1. Load allophone data using the C-to-Python converter
2. Copy generated allophones.py to Pico
3. Run this program in Thonny
4. Monitor debug output in console
5. Send commands via Z80 OUT 17,value / IN 37 OR type commands in Thonny

DEBUG OUTPUT FORMAT:
[timestamp] CATEGORY: message
Example: [00012847] INTERFACE: *** ALD FALLING EDGE DETECTED #1 ***

Common debug messages to watch for:
- "ALD FALLING EDGE DETECTED": Z80 sent a command
- "Address loaded: X (ALLOPHONE_NAME)": Which allophone was requested  
- "Setting BUSY/READY state": LRQ pin state changes
- "Timing issues: X errors": Audio timing problems
- "Memory: X free, Y allocated": Memory usage tracking

"""

import machine
import time
import _thread
from machine import Pin, PWM
import gc

# Debug configuration - Set these to True/False to control logging
DEBUG_ENABLED = True      # Master debug switch
DEBUG_GPIO = False        # GPIO pin state changes (disabled to save memory)
DEBUG_AUDIO = True        # Audio playback progress
DEBUG_TIMING = False      # Sample timing information (disabled to save memory)
DEBUG_INTERFACE = True    # Interface monitoring and ALD edges
DEBUG_SYSTEM = True       # System initialization and memory
DEBUG_VERBOSE = False     # Extra verbose output (sample-by-sample)

# Memory conservation mode - reduces debug output during initialization
MEMORY_CONSERVATIVE = True

# SP0256 Pin definitions (matching your schematic)
PIN_SP_A1 = 0
PIN_SP_A2 = 1
PIN_SP_A3 = 2
PIN_SP_A4 = 3
PIN_SP_A5 = 4
PIN_SP_A6 = 5
PIN_SP_A7 = 6
PIN_SP_A8 = 7
PIN_SP_ALD = 8
PIN_SP_SE = 9
PIN_SP_SBY_RES_N = 10
PIN_SP_DIGITAL_OUT = 11
PIN_SP_RESET_N = 12
PIN_SP_LRQ_N = 13
PIN_SP_SBY = 14

# Audio output pin
SOUND_IO = 11

# Allophone definitions
PA1, PA2, PA3, PA4, PA5 = 0, 1, 2, 3, 4
OY, AY, EH, KK3, PP = 5, 6, 7, 8, 9
JH, NN1, IH, TT2, RR1 = 10, 11, 12, 13, 14
AX, MM, TT1, DH1, IY = 15, 16, 17, 18, 19
EY, DD1, UW1, AO, AA = 20, 21, 22, 23, 24
YY2, AE, HH1, BB1, TH = 25, 26, 27, 28, 29
UH, UW2, AW, DD2, GG3 = 30, 31, 32, 33, 34
VV, GG1, SH, ZH, RR2 = 35, 36, 37, 38, 39
FF, KK2, KK1, ZZ, NG = 40, 41, 42, 43, 44
LL, WW, XR, WH, YY1 = 45, 46, 47, 48, 49
CH, ER1, ER2, OW, DH2 = 50, 51, 52, 53, 54
SS, NN2, HH2, OR, AR = 55, 56, 57, 58, 59
YR, GG2, EL, BB2 = 60, 61, 62, 63

# Aliases for compatibility
NN = NN1
RR = RR1
TT = TT1
DH = DH1
DD = DD1
UW = UW1
GG = GG1
HH = HH1
KK = KK1
YY = YY1
ER = ER1

def debug_print(category, message):
    """Print debug message with timestamp if debugging is enabled"""
    if not DEBUG_ENABLED:
        return
        
    timestamp = time.ticks_ms()
    
    # Check category-specific debug flags
    if category == "GPIO" and not DEBUG_GPIO:
        return
    elif category == "AUDIO" and not DEBUG_AUDIO:
        return
    elif category == "TIMING" and not DEBUG_TIMING:
        return
    elif category == "INTERFACE" and not DEBUG_INTERFACE:
        return
    elif category == "SYSTEM" and not DEBUG_SYSTEM:
        return
        
    print(f"[{timestamp:08d}] {category}: {message}")

def debug_verbose(category, message):
    """Print verbose debug message only if verbose debugging is enabled"""
    if DEBUG_VERBOSE:
        debug_print(category, message)

def debug_gpio_state(pins_dict):
    """Debug print GPIO pin states"""
    if DEBUG_GPIO:
        pin_states = []
        for name, pin in pins_dict.items():
            try:
                state = pin.value()
                pin_states.append(f"{name}={state}")
            except:
                pin_states.append(f"{name}=ERR")
        debug_print("GPIO", f"States: {', '.join(pin_states)}")

def debug_memory():
    """Print memory usage information"""
    if DEBUG_SYSTEM:
        gc.collect()
        free = gc.mem_free()
        alloc = gc.mem_alloc()
        debug_print("SYSTEM", f"Memory: {free} free, {alloc} allocated")

def debug_allophone_info(allophone_id, sample_length):
    """Debug information about allophone being played"""
    duration_ms = (sample_length * 1000) // 11025
    debug_print("AUDIO", f"Allophone {allophone_id}: {sample_length} samples, ~{duration_ms}ms duration")

def debug_hex_dump(data, max_bytes=32):
    """Create a hex dump of binary data for debugging"""
    if not DEBUG_VERBOSE:
        return
        
    hex_str = " ".join([f"{b:02x}" for b in data[:max_bytes]])
    if len(data) > max_bytes:
        hex_str += "..."
    return hex_str

def debug_performance_stats():
    """Print performance statistics"""
    freq = machine.freq()
    debug_print("SYSTEM", f"CPU frequency: {freq} Hz")
    
def debug_pin_test():
    """Test all pins by reading their current states"""
    debug_print("GPIO", "=== PIN TEST ===")
    for i in range(PIN_SP_A1, PIN_SP_A6 + 1):
        try:
            pin = Pin(i, Pin.IN)
            state = pin.value()
            debug_print("GPIO", f"GP{i} (A{i-PIN_SP_A1+1}): {state}")
        except Exception as e:
            debug_print("GPIO", f"GP{i}: ERROR - {e}")
    
    for pin_name, pin_num in [("ALD", PIN_SP_ALD), ("LRQ", PIN_SP_LRQ_N), ("SBY", PIN_SP_SBY)]:
        try:
            pin = Pin(pin_num, Pin.IN)
            state = pin.value()
            debug_print("GPIO", f"GP{pin_num} ({pin_name}): {state}")
        except Exception as e:
            debug_print("GPIO", f"GP{pin_num} ({pin_name}): ERROR - {e}")
    debug_print("GPIO", "=== END PIN TEST ===")

def debug_config_summary():
    """Print current debug configuration"""
    debug_print("SYSTEM", "=== DEBUG CONFIGURATION ===")
    debug_print("SYSTEM", f"Master Debug: {DEBUG_ENABLED}")
    debug_print("SYSTEM", f"GPIO Debug: {DEBUG_GPIO}")
    debug_print("SYSTEM", f"Audio Debug: {DEBUG_AUDIO}")
    debug_print("SYSTEM", f"Timing Debug: {DEBUG_TIMING}")
    debug_print("SYSTEM", f"Interface Debug: {DEBUG_INTERFACE}")
    debug_print("SYSTEM", f"System Debug: {DEBUG_SYSTEM}")
    debug_print("SYSTEM", f"Verbose Debug: {DEBUG_VERBOSE}")
    debug_print("SYSTEM", "=== END CONFIGURATION ===")

# Allophone name lookup for debugging
ALLOPHONE_NAMES = {
    0: "PA1", 1: "PA2", 2: "PA3", 3: "PA4", 4: "PA5",
    5: "OY", 6: "AY", 7: "EH", 8: "KK3", 9: "PP",
    10: "JH", 11: "NN1", 12: "IH", 13: "TT2", 14: "RR1",
    15: "AX", 16: "MM", 17: "TT1", 18: "DH1", 19: "IY",
    20: "EY", 21: "DD1", 22: "UW1", 23: "AO", 24: "AA",
    25: "YY2", 26: "AE", 27: "HH1", 28: "BB1", 29: "TH",
    30: "UH", 31: "UW2", 32: "AW", 33: "DD2", 34: "GG3",
    35: "VV", 36: "GG1", 37: "SH", 38: "ZH", 39: "RR2",
    40: "FF", 41: "KK2", 42: "KK1", 43: "ZZ", 44: "NG",
    45: "LL", 46: "WW", 47: "XR", 48: "WH", 49: "YY1",
    50: "CH", 51: "ER1", 52: "ER2", 53: "OW", 54: "DH2",
    55: "SS", 56: "NN2", 57: "HH2", 58: "OR", 59: "AR",
    60: "YR", 61: "GG2", 62: "EL", 63: "BB2"
}

def debug_allophone_name(allophone_id):
    """Get human-readable name for allophone"""
    return ALLOPHONE_NAMES.get(allophone_id, f"UNK{allophone_id}")

# Global variables for command interface
sp0256_instance = None
command_enabled = True

# Reverse lookup for allophone names to IDs
ALLOPHONE_IDS = {name: id for id, name in ALLOPHONE_NAMES.items()}

def parse_allophone(token):
    """Parse allophone token - can be name (HH) or number (27)"""
    token = token.upper().strip()
    
    # Try parsing as number first
    try:
        allophone_id = int(token)
        if 0 <= allophone_id <= 63:
            return allophone_id
        else:
            return None
    except ValueError:
        pass
    
    # Try parsing as allophone name
    return ALLOPHONE_IDS.get(token)

def cmd_speak(args):
    """SPEAK command - play sequence of allophones"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
        
    if not args:
        print("USAGE: SPEAK <allophone1> <allophone2> ...")
        print("EXAMPLES:")
        print("  SPEAK HH EH LL OW")
        print("  SPEAK 27 7 45 53")
        print("  SPEAK PA1 HH EH LL OW PA1")
        return
    
    # Parse allophone sequence
    sequence = []
    for arg in args:
        allophone_id = parse_allophone(arg)
        if allophone_id is None:
            print(f"ERROR: Invalid allophone '{arg}'")
            print("Use LIST command to see valid allophones")
            return
        sequence.append(allophone_id)
    
    # Convert to allophone names for display
    names = [debug_allophone_name(id) for id in sequence]
    print(f"SPEAKING: {' '.join(names)} ({len(sequence)} allophones)")
    
    # Play the sequence
    try:
        success = sp0256_instance.play_allophones(sequence)
        if success:
            print("PLAYBACK COMPLETE")
        else:
            print("PLAYBACK FAILED - check debug output")
    except Exception as e:
        print(f"PLAYBACK ERROR: {e}")

def cmd_list(args):
    """LIST command - show available allophones"""
    print("AVAILABLE ALLOPHONES:")
    print("ID   NAME  DESCRIPTION")
    print("-" * 40)
    
    descriptions = {
        0: "PA1   10ms pause", 1: "PA2   30ms pause", 2: "PA3   50ms pause", 
        3: "PA4   100ms pause", 4: "PA5   200ms pause", 5: "OY    Boy",
        6: "AY    Sky", 7: "EH    End", 8: "KK3   Comb", 9: "PP    Pow",
        10: "JH    Dodge", 11: "NN1   Thin", 12: "IH    Sit", 13: "TT2   To",
        14: "RR1   Rural", 15: "AX    Succeed", 16: "MM    Milk", 17: "TT1   Part",
        18: "DH1   They", 19: "IY    See", 20: "EY    Beige", 21: "DD1   Could",
        22: "UW1   To", 23: "AO    Aught", 24: "AA    Hot", 25: "YY2   Yes",
        26: "AE    Hat", 27: "HH1   He", 28: "BB1   Business", 29: "TH    Thin",
        30: "UH    Book", 31: "UW2   Food", 32: "AW    Out", 33: "DD2   Do",
        34: "GG3   Wig", 35: "VV    Vest", 36: "GG1   Got", 37: "SH    Ship",
        38: "ZH    Azure", 39: "RR2   Brain", 40: "FF    Food", 41: "KK2   Sky",
        42: "KK1   Can't", 43: "ZZ    Zoo", 44: "NG    Anchor", 45: "LL    Lake",
        46: "WW    Wool", 47: "XR    Repair", 48: "WH    Whig", 49: "YY1   Yes",
        50: "CH    Church", 51: "ER1   Fir", 52: "ER2   Fir", 53: "OW    Beau",
        54: "DH2   They", 55: "SS    Vest", 56: "NN2   No", 57: "HH2   Hoe",
        58: "OR    Store", 59: "AR    Alarm", 60: "YR    Clear", 61: "GG2   Guest",
        62: "EL    Saddle", 63: "BB2   Business"
    }
    
    for i in range(64):
        desc = descriptions.get(i, f"{ALLOPHONE_NAMES[i]}   Unknown")
        print(f"{i:2d}   {desc}")

def cmd_test(args):
    """TEST command - run hardware tests"""
    print("RUNNING HARDWARE TEST...")
    debug_pin_test()
    if sp0256_instance:
        debug_gpio_state({
            "ALD": sp0256_instance.ald_pin,
            "LRQ": sp0256_instance.lrq_pin,
            "SBY": sp0256_instance.sby_pin
        })
    print("HARDWARE TEST COMPLETE")

def cmd_status(args):
    """STATUS command - show system status"""
    print("SP0256 EMULATOR STATUS:")
    print("-" * 30)
    if sp0256_instance:
        # Ensure attributes exist
        sp0256_instance.add_missing_status_attributes()
        
        print(f"Initialized: YES")
        print(f"Busy: {sp0256_instance.busy}")
        
        last_allophone = sp0256_instance.last_allophone
        if last_allophone >= 0:
            last_name = debug_allophone_name(last_allophone)
            print(f"Last allophone: {last_allophone} ({last_name})")
        else:
            print(f"Last allophone: None")
            
        print(f"Total played: {sp0256_instance.total_allophones_played}")
        print(f"ALD edges detected: {getattr(sp0256_instance, 'ald_edge_count', 0)}")
        print(f"Cached allophones: {len(sp0256_instance.allophones)}")
        
        # Show data sources
        if sp0256_instance.compressed_loader:
            print(f"Data source: Compressed binary")
        elif sp0256_instance.allophone_loader:
            print(f"Data source: Uncompressed Python")
        else:
            print(f"Data source: Placeholder/cached only")
        
        # Memory status
        gc.collect()
        free_mem = gc.mem_free()
        alloc_mem = gc.mem_alloc()
        total_mem = free_mem + alloc_mem
        print(f"Memory: {free_mem} free / {total_mem} total ({(free_mem/total_mem)*100:.1f}% free)")
    else:
        print("Initialized: NO")

def cmd_compress_info(args):
    """COMPRESS command - show compression information"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
        
    print("COMPRESSION STATUS:")
    print("-" * 25)
    
    if sp0256_instance.compressed_loader:
        try:
            # Get compression method if available
            if hasattr(sp0256_instance.compressed_loader, '_loader'):
                method = sp0256_instance.compressed_loader._loader.method
                count = sp0256_instance.compressed_loader.get_allophone_count()
                print(f"Compression: {method.upper()}")
                print(f"Available allophones: {count}")
                print(f"Memory usage: On-demand loading (minimal)")
                print(f"Storage: Binary file format")
            else:
                print("Compressed loader active but details unavailable")
        except Exception as e:
            print(f"Error getting compression info: {e}")
    elif sp0256_instance.allophone_loader:
        print("Compression: NONE")
        print("Storage: Python source format")
        print("Memory usage: Lazy loading with caching")
    else:
        print("Compression: N/A")
        print("Storage: Placeholder data only")
        
    # Show current memory usage by allophones
    cached_count = len(sp0256_instance.allophones)
    if cached_count > 0:
        total_size = sum(len(data) for data in sp0256_instance.allophones.values())
        avg_size = total_size // cached_count
        print(f"Currently cached: {cached_count} allophones, {total_size} bytes (avg {avg_size} bytes/allophone)")
    else:
        print("Currently cached: None")

def cmd_diagnose(args):
    """DIAGNOSE command - diagnose allophone loading issues"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
        
    print("SP0256 DIAGNOSTIC TEST:")
    print("-" * 30)
    
    # Test basic allophone loading
    test_ids = [0, 1, 27, 7, 45, 53]  # PA1, PA2, HH, EH, LL, OW (HELLO)
    
    for allophone_id in test_ids:
        name = debug_allophone_name(allophone_id)
        print(f"\nTesting allophone {allophone_id} ({name}):")
        
        try:
            # Force reload by removing from cache
            if allophone_id in sp0256_instance.allophones:
                del sp0256_instance.allophones[allophone_id]
            
            # Try to load
            data = sp0256_instance.get_allophone(allophone_id)
            
            if data is None:
                print(f"  FAILED: Returned None")
            elif not isinstance(data, (bytes, bytearray)):
                print(f"  FAILED: Wrong type {type(data)}")
            elif len(data) == 0:
                print(f"  FAILED: Empty data")
            else:
                print(f"  OK: {len(data)} bytes, type {type(data)}")
                
                # Check first few bytes
                preview = data[:8]
                hex_preview = " ".join(f"{b:02x}" for b in preview)
                print(f"      Preview: {hex_preview}")
                
        except Exception as e:
            print(f"  ERROR: {e}")
            import sys
            sys.print_exception(e)
    
    # Test data sources
    print(f"\nDATA SOURCES:")
    print(f"Compressed loader: {'Available' if sp0256_instance.compressed_loader else 'None'}")
    print(f"Uncompressed loader: {'Available' if sp0256_instance.allophone_loader else 'None'}")
    
    # Memory status
    gc.collect()
    free_mem = gc.mem_free()
    print(f"\nMEMORY: {free_mem} bytes free")
    
    print("\nDIAGNOSTIC COMPLETE")

def cmd_memory(args):
    """MEMORY command - show detailed memory usage"""
    gc.collect()
    free = gc.mem_free()
    alloc = gc.mem_alloc()
    total = free + alloc
    
    print("MEMORY USAGE:")
    print("-" * 20)
    print(f"Free:      {free:6d} bytes ({(free/total)*100:.1f}%)")
    print(f"Allocated: {alloc:6d} bytes ({(alloc/total)*100:.1f}%)")
    print(f"Total:     {total:6d} bytes")
    
    if sp0256_instance:
        loaded_count = len(sp0256_instance.allophones)
        total_size = sum(len(data) for data in sp0256_instance.allophones.values())
        print(f"Allophones loaded: {loaded_count}")
        print(f"Allophone data: {total_size} bytes")
        
        if free < 20000:
            print("WARNING: Low memory! Consider freeing unused allophones.")
            if loaded_count > 10:
                print("Recommendation: Use CLEANUP command or restart.")

def cmd_cleanup(args):
    """CLEANUP command - free memory by clearing unused allophones"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
        
    print("CLEANING UP MEMORY...")
    
    # Show before state
    gc.collect()
    before_free = gc.mem_free()
    before_count = len(sp0256_instance.allophones)
    
    # Free non-essential allophones
    freed_count = sp0256_instance._free_non_essential_allophones()
    
    # Force garbage collection
    gc.collect()
    
    # Show after state
    after_free = gc.mem_free()
    after_count = len(sp0256_instance.allophones)
    freed_memory = after_free - before_free
    
    print(f"CLEANUP COMPLETE:")
    print(f"  Freed {freed_count} allophones")
    print(f"  Recovered {freed_memory} bytes")
    print(f"  Free memory: {after_free} bytes")

def cmd_reset(args):
    """RESET command - reset allophone cache and free all memory"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
        
    print("RESETTING ALLOPHONE CACHE...")
    
    gc.collect()
    before_free = gc.mem_free()
    before_count = len(sp0256_instance.allophones)
    
    # Clear all but essential allophones
    sp0256_instance.allophones.clear()
    sp0256_instance._load_essential_allophones()
    
    gc.collect()
    after_free = gc.mem_free()
    after_count = len(sp0256_instance.allophones)
    
    freed_memory = after_free - before_free
    
    print(f"RESET COMPLETE:")
    print(f"  Cleared {before_count} allophones, kept {after_count}")
    print(f"  Recovered {freed_memory} bytes")
    print(f"  Free memory: {after_free} bytes")

def cmd_gpio(args):
    """GPIO command - show GPIO states"""
    if sp0256_instance:
        debug_gpio_state({
            "ALD": sp0256_instance.ald_pin,
            "LRQ": sp0256_instance.lrq_pin,
            "SBY": sp0256_instance.sby_pin
        })
        
        # Show address pins
        address = sp0256_instance.read_address()
        print(f"Current address: {address} ({debug_allophone_name(address)})")
    else:
        print("SP0256 not initialized")

def cmd_debug(args):
    """DEBUG command - toggle debug categories"""
    global DEBUG_GPIO, DEBUG_AUDIO, DEBUG_TIMING, DEBUG_INTERFACE, DEBUG_SYSTEM, DEBUG_VERBOSE
    
    if not args:
        print("DEBUG CATEGORIES:")
        print(f"  GPIO: {DEBUG_GPIO}")
        print(f"  AUDIO: {DEBUG_AUDIO}")
        print(f"  TIMING: {DEBUG_TIMING}")
        print(f"  INTERFACE: {DEBUG_INTERFACE}")
        print(f"  SYSTEM: {DEBUG_SYSTEM}")
        print(f"  VERBOSE: {DEBUG_VERBOSE}")
        print("USAGE: DEBUG <category> to toggle")
        return
    
    category = args[0].upper()
    if category == "GPIO":
        DEBUG_GPIO = not DEBUG_GPIO
        print(f"GPIO debug: {DEBUG_GPIO}")
    elif category == "AUDIO":
        DEBUG_AUDIO = not DEBUG_AUDIO
        print(f"AUDIO debug: {DEBUG_AUDIO}")
    elif category == "TIMING":
        DEBUG_TIMING = not DEBUG_TIMING
        print(f"TIMING debug: {DEBUG_TIMING}")
    elif category == "INTERFACE":
        DEBUG_INTERFACE = not DEBUG_INTERFACE
        print(f"INTERFACE debug: {DEBUG_INTERFACE}")
    elif category == "SYSTEM":
        DEBUG_SYSTEM = not DEBUG_SYSTEM
        print(f"SYSTEM debug: {DEBUG_SYSTEM}")
    elif category == "VERBOSE":
        DEBUG_VERBOSE = not DEBUG_VERBOSE
        print(f"VERBOSE debug: {DEBUG_VERBOSE}")
    else:
        print(f"Unknown debug category: {category}")

def cmd_help(args):
    """HELP command - show available commands"""
    print("SP0256 EMULATOR COMMANDS:")
    print("-" * 40)
    print("SPEAK <allophones>  Play sequence of allophones")
    print("                    Examples: SPEAK HH EH LL OW")
    print("                             SPEAK 27 7 45 53")
    print("LIST                List all available allophones")
    print("TEST                Run hardware pin test")
    print("STATUS              Show system status")
    print("MEMORY              Show memory usage")
    print("GPIO                Show GPIO pin states")
    print("DEBUG <category>    Toggle debug category")
    print("DIAGNOSE            Run diagnostic tests")
    print("CLEANUP             Free unused allophone memory")
    print("RESET               Reset allophone cache")
    print("HELLO               Quick test - say 'Hello'")
    print("WORLD               Quick test - say 'World'")
    print("QUIT/EXIT/BYE       Exit command interface")
    print("HELP                Show this help")
    print("")
    print("EXAMPLE WORDS & PHRASES:")
    print("HELLO:    SPEAK HH EH LL OW")
    print("WORLD:    SPEAK WW OR LL DD1")
    print("TEST:     SPEAK TT2 EH SS TT2")
    print("COMPUTER: SPEAK KK1 AX MM PP YY1 UW1 TT2 ER")
    print("SPEECH:   SPEAK SS PA3 PP IY CH")
    print("READY:    SPEAK RR1 EH DD1 IY")
    print("")
    print("Use allophone names (HH, EH, LL) or numbers (27, 7, 45)")
    print("PA1-PA5 are pauses (10ms, 30ms, 50ms, 100ms, 200ms)")
    print("")
    print("HARDWARE NOTE: GP11 audio output requires external filter!")

def cmd_hello(args):
    """HELLO command - quick test"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
        
    print("SAYING: Hello")
    try:
        # Use allophone IDs directly: HH, EH, LL, OW
        hello_sequence = [27, 7, 45, 53]  
        success = sp0256_instance.play_allophones(hello_sequence)
        if success:
            print("HELLO COMPLETE")
        else:
            print("HELLO FAILED - check debug output")
    except Exception as e:
        print(f"ERROR: {e}")
        debug_print("SYSTEM", f"Hello command failed: {e}")
        
def cmd_world(args):
    """WORLD command - quick test"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
    print("SAYING: World")
    try:
        success = sp0256_instance.play_allophones([WW, OR, LL, DD1])
        if success:
            print("WORLD COMPLETE")
        else:
            print("WORLD FAILED - check debug output")
    except Exception as e:
        print(f"ERROR: {e}")

def cmd_quit(args):
    """QUIT command - exit the command interface"""
    global command_enabled
    command_enabled = False
    print("Exiting command interface...")
    return False

# Command lookup table
COMMANDS = {
    'SPEAK': cmd_speak,
    'LIST': cmd_list,
    'TEST': cmd_test,
    'DIAGNOSE': cmd_diagnose,
    'STATUS': cmd_status,
    'MEMORY': cmd_memory,
    'CLEANUP': cmd_cleanup,
    'RESET': cmd_reset,
    'GPIO': cmd_gpio,
    'DEBUG': cmd_debug,
    'HELLO': cmd_hello,
    'WORLD': cmd_world,
    'HELP': cmd_help,
    'QUIT': cmd_quit,
    'EXIT': cmd_quit,
    'BYE': cmd_quit,
    '?': cmd_help
}

def process_command(command_line):
    """Process a command from the console"""
    command_line = command_line.strip()
    if not command_line:
        return True  # Continue
    
    parts = command_line.split()
    command = parts[0].upper()
    args = parts[1:] if len(parts) > 1 else []
    
    # Handle quit/exit commands directly
    if command in ['QUIT', 'EXIT', 'BYE']:
        print("Goodbye!")
        return False  # Exit
    
    if command in COMMANDS:
        try:
            result = COMMANDS[command](args)
            # Some commands may return False to indicate exit
            if result is False:
                return False
        except Exception as e:
            print(f"COMMAND ERROR: {e}")
            debug_print("SYSTEM", f"Command '{command}' failed: {e}")
    else:
        print(f"UNKNOWN COMMAND: {command}")
        print("Type HELP for available commands")
    
    return True  # Continue

def command_interface():
    """Interactive command interface"""
    print("\n" + "="*50)
    print("SP0256 COMMAND INTERFACE READY")
    print("Type HELP for available commands")
    print("Examples:")
    print("  SPEAK HH EH LL OW    # Say 'Hello'")
    print("  HELLO                # Quick hello test")
    print("  LIST                 # Show allophones")
    print("  STATUS               # System status")
    print("  QUIT                 # Exit")
    print("="*50)
    
    try:
        while command_enabled:
            try:
                # Get command from user
                command_line = input("> ")
                if not process_command(command_line):
                    break  # User requested quit
            except EOFError:
                # Handle Ctrl+D
                print("\nGoodbye!")
                break
            except KeyboardInterrupt:
                # Handle Ctrl+C
                print("\nUse QUIT to exit or continue typing commands...")
                continue
    except Exception as e:
        debug_print("SYSTEM", f"Command interface error: {e}")
    
    debug_print("SYSTEM", "Command interface terminated")

class SP0256Emulator:
    def __init__(self):
        # Force garbage collection before starting
        gc.collect()
        
        debug_print("SYSTEM", "Initializing SP0256 Emulator...")
        initial_mem = gc.mem_free()
        debug_print("SYSTEM", f"Initial free memory: {initial_mem} bytes")
        
        # Initialize ALL attributes FIRST to avoid attribute errors
        self.busy = False
        self.last_allophone = -1
        self.total_allophones_played = 0
        self.allophones = {}
        self.allophone_loader = None
        self.compressed_loader = None
        self.ald_edge_count = 0
        
        # Initialize hardware systems
        try:
            debug_print("SYSTEM", "Setting up GPIO pins...")
            self.setup_gpio()
            
            debug_print("SYSTEM", "Setting up PWM audio...")
            self.setup_pwm()
            
            debug_print("SYSTEM", "Setting up allophone system...")
            self.load_allophones()
            
        except Exception as e:
            debug_print("SYSTEM", f"FATAL ERROR during initialization: {e}")
            raise
        
        # Final memory check
        gc.collect()
        final_mem = gc.mem_free()
        used_mem = initial_mem - final_mem
        debug_print("SYSTEM", f"SP0256 Emulator ready - Used {used_mem} bytes, {final_mem} bytes free")

        if final_mem < 30000:  # Less than 30KB free
            debug_print("SYSTEM", "WARNING: Low memory after initialization")
    
    def setup_gpio(self):
        """Initialize all GPIO pins with error handling"""
        try:
            # Address input pins (A1-A6) with pull-down for clean reads
            self.addr_pins = []
            for i in range(PIN_SP_A1, PIN_SP_A6 + 1):
                try:
                    pin = Pin(i, Pin.IN, Pin.PULL_DOWN)
                    self.addr_pins.append(pin)
                    debug_verbose("GPIO", f"Initialized address pin GP{i}")
                except Exception as e:
                    debug_print("GPIO", f"Failed to initialize GP{i}: {e}")
                    raise
                    
            # ALD input pin with pull-up (active low)
            self.ald_pin = Pin(PIN_SP_ALD, Pin.IN, Pin.PULL_UP)
            debug_verbose("GPIO", "Initialized ALD pin")
            
            # LRQ output pin (active low)
            self.lrq_pin = Pin(PIN_SP_LRQ_N, Pin.OUT)
            self.lrq_pin.value(0)  # Ready state (low = ready)
            debug_verbose("GPIO", "Initialized LRQ pin")
            
            # SBY output pin
            self.sby_pin = Pin(PIN_SP_SBY, Pin.OUT)  
            self.sby_pin.value(1)  # Standby state (high = standby)
            debug_verbose("GPIO", "Initialized SBY pin")
            
            debug_print("GPIO", "All GPIO pins initialized successfully")
            
        except Exception as e:
            debug_print("GPIO", f"GPIO initialization failed: {e}")
            raise
        
    def setup_pwm(self):
        """Initialize PWM for audio output with error handling"""
        try:
            self.pwm = PWM(Pin(SOUND_IO))
            self.pwm.freq(125000)  # PWM frequency for good audio reproduction
            self.pwm.duty_u16(32768)  # 50% duty cycle (silence)
            debug_print("AUDIO", f"PWM initialized on GP{SOUND_IO} at 125kHz")
            debug_print("AUDIO", "WARNING: Add external low-pass filter (~5kHz) for proper audio!")
        except Exception as e:
            debug_print("AUDIO", f"PWM initialization failed: {e}")
            raise

    def setup_allophone_loaders(self):
        """Initialize allophone data loaders"""
        # Try to setup compressed loader
        try:
            from allophone_compressed import CompressedAllophoneLoader
            self.compressed_loader = CompressedAllophoneLoader()
            debug_print("SYSTEM", "Compressed allophone loader initialized")
        except ImportError:
            debug_print("SYSTEM", "Compressed allophone loader not available")
            self.compressed_loader = None
        
        # Try to setup uncompressed loader  
        try:
            from allophone_loader import AllophoneLoader
            self.allophone_loader = AllophoneLoader()
            debug_print("SYSTEM", "Uncompressed allophone loader initialized")
        except ImportError:
            debug_print("SYSTEM", "Uncompressed allophone loader not available") 
            self.allophone_loader = None

    def load_allophones(self):
        """Load allophone sample data with proper loader setup"""
        # Setup loaders first
        self.setup_allophone_loaders()
        
        # Initialize cache
        self.allophones = {}
        
        # Try importing legacy allophones.py file
        try:
            from allophones import allophones, get_allophone_count
            # Only load essential ones to save memory
            essential_ids = [0, 1, 2, 3, 4]  # PA1-PA5
            for i in essential_ids:
                if i in allophones:
                    self.allophones[i] = allophones[i]
            debug_print("SYSTEM", f"Loaded {len(self.allophones)} essential allophones from legacy file")
        except ImportError:
            debug_print("SYSTEM", "Legacy allophones.py not found - using lazy loading")
        
        # Load essential allophones (pauses)
        self._load_essential_allophones()
        
        debug_print("SYSTEM", f"Allophone system initialized with {len(self.allophones)} cached allophones")
        
        # Memory check
        gc.collect()
        free_mem = gc.mem_free()
        if free_mem < 30000:
            debug_print("SYSTEM", "WARNING: Low memory after allophone initialization")

    def _generate_placeholder_allophone(self, allophone_id):
        """Generate placeholder allophone data"""
        if allophone_id < 5:  # Pauses PA1-PA5
            pause_durations = [
                int(0.010 * 11025),  # PA1: 10ms
                int(0.030 * 11025),  # PA2: 30ms  
                int(0.050 * 11025),  # PA3: 50ms
                int(0.100 * 11025),  # PA4: 100ms
                int(0.200 * 11025),  # PA5: 200ms
            ]
            data = bytes([0x80] * pause_durations[allophone_id])
        else:
            # Basic tone for speech allophones (~18ms)
            data = bytes([0x80] * 200)
        
        debug_verbose("AUDIO", f"Generated placeholder for allophone {allophone_id}: {len(data)} bytes")
        return data

    def _load_essential_allophones(self):
        """Load essential allophones (pauses) into cache"""
        debug_print("SYSTEM", "Loading essential allophones (pauses)")
        essential_ids = [0, 1, 2, 3, 4]  # PA1-PA5
        
        for allophone_id in essential_ids:
            if allophone_id not in self.allophones:
                data = self.get_allophone(allophone_id)
                if data:
                    debug_verbose("SYSTEM", f"Loaded essential allophone {allophone_id}")

    def _free_non_essential_allophones(self):
        """Free memory by removing non-essential allophones from cache"""
        essential_ids = {0, 1, 2, 3, 4}  # PA1-PA5 pauses
        
        to_remove = []
        for allophone_id in self.allophones:
            if allophone_id not in essential_ids:
                to_remove.append(allophone_id)
        
        freed_count = 0
        for allophone_id in to_remove:
            del self.allophones[allophone_id]
            freed_count += 1
        
        debug_print("SYSTEM", f"Freed {freed_count} non-essential allophones")
        return freed_count

    def add_missing_status_attributes(self):
        """Ensure all status attributes exist for command interface"""
        if not hasattr(self, 'total_allophones_played'):
            self.total_allophones_played = 0
        if not hasattr(self, 'last_allophone'):
            self.last_allophone = -1
        if not hasattr(self, 'ald_edge_count'):
            self.ald_edge_count = 0

    def read_address(self):
        """Read 6-bit address from GPIO pins A1-A6"""
        address = 0
        for i, pin in enumerate(self.addr_pins):
            if pin.value():
                address |= (1 << i)
        
        debug_verbose("GPIO", f"Read address: {address:06b} (decimal: {address})")
        return address

    def get_allophone(self, allophone_id):
        """Get allophone data with lazy loading and error handling"""
        if allophone_id < 0 or allophone_id > 63:
            debug_print("AUDIO", f"Invalid allophone ID: {allophone_id}")
            return None
        
        # Check if already loaded in cache
        if allophone_id in self.allophones:
            debug_verbose("AUDIO", f"Allophone {allophone_id} found in cache")
            return self.allophones[allophone_id]
        
        # Try loading from compressed loader first
        if self.compressed_loader:
            try:
                data = self.compressed_loader.get_allophone(allophone_id)
                if data:
                    self.allophones[allophone_id] = data
                    debug_print("AUDIO", f"Loaded allophone {allophone_id} from compressed data")
                    return data
            except Exception as e:
                debug_print("AUDIO", f"Failed to load from compressed data: {e}")
        
        # Try loading from uncompressed loader
        if self.allophone_loader:
            try:
                data = self.allophone_loader.get_allophone(allophone_id)
                if data:
                    self.allophones[allophone_id] = data
                    debug_print("AUDIO", f"Loaded allophone {allophone_id} from uncompressed data")
                    return data
            except Exception as e:
                debug_print("AUDIO", f"Failed to load from uncompressed data: {e}")
        
        # Generate placeholder if no data source available
        debug_print("AUDIO", f"Generating placeholder for allophone {allophone_id}")
        data = self._generate_placeholder_allophone(allophone_id)
        self.allophones[allophone_id] = data
        return data
        
    def play_allophone(self, allophone_id):
        """Play a single allophone with comprehensive error handling"""
        if allophone_id >= 64:
            debug_print("AUDIO", f"Invalid allophone ID: {allophone_id}")
            return False
            
        sample_data = self.get_allophone(allophone_id)
        if sample_data is None:
            debug_print("AUDIO", f"Failed to get allophone {allophone_id}")
            return False
        
        allophone_name = debug_allophone_name(allophone_id)
        debug_print("AUDIO", f"Playing allophone {allophone_id} ({allophone_name})")
        debug_allophone_info(allophone_id, len(sample_data))
        
        # Set busy state
        self.lrq_pin.value(1)  # Busy (high)
        self.sby_pin.value(0)  # Not in standby
        self.busy = True
        
        # Calculate precise timing for 11025 Hz
        sample_period_us = int(1000000 / 11025)  # ~90.7 microseconds
        
        # Play samples
        start_time = time.ticks_us()
        timing_errors = 0
        
        for i, sample in enumerate(sample_data):
            # Convert 8-bit sample to 16-bit PWM duty cycle
            duty = int((sample / 255.0) * 65535)
            self.pwm.duty_u16(duty)
            
            # Wait for precise timing
            next_sample_time = start_time + (i + 1) * sample_period_us
            current_time = time.ticks_us()
            
            if time.ticks_diff(current_time, next_sample_time) > sample_period_us:
                timing_errors += 1
                
            while time.ticks_diff(time.ticks_us(), next_sample_time) < 0:
                pass
                
        # Return to ready state
        self.pwm.duty_u16(32768)  # Back to silence (50% duty cycle)
        self.lrq_pin.value(0)  # Ready (low)
        self.sby_pin.value(1)  # Standby
        self.busy = False
        
        # Update statistics
        self.last_allophone = allophone_id
        self.total_allophones_played += 1
        
        if timing_errors > 0:
            debug_print("TIMING", f"Timing issues: {timing_errors} errors")
        
        debug_print("AUDIO", f"Completed allophone {allophone_id}")
        return True
            
    def play_allophones(self, allophone_list):
        """Play a sequence of allophones with proper error handling"""
        if not allophone_list:
            debug_print("AUDIO", "Empty allophone list")
            return False
            
        debug_print("AUDIO", f"Playing sequence of {len(allophone_list)} allophones")
        
        success_count = 0
        for i, allophone_id in enumerate(allophone_list):
            allophone_name = debug_allophone_name(allophone_id)
            debug_print("AUDIO", f"Playing {i+1}/{len(allophone_list)}: {allophone_id} ({allophone_name})")
            
            if self.play_allophone(allophone_id):
                success_count += 1
            else:
                debug_print("AUDIO", f"Failed to play allophone {allophone_id} - continuing")
                
        debug_print("AUDIO", f"Sequence complete: {success_count}/{len(allophone_list)} successful")
        return success_count == len(allophone_list)
    
    def monitor_interface(self):
        """Monitor the interface for address load signals with debouncing"""
        last_ald = self.ald_pin.value()
        debounce_time = 1000  # 1ms debounce in microseconds
        last_edge_time = 0
        
        debug_print("INTERFACE", "Starting interface monitoring...")
        debug_print("INTERFACE", "Waiting for ALD falling edge signals...")
        
        try:
            while True:
                current_ald = self.ald_pin.value()
                current_time = time.ticks_us()
                
                # Detect falling edge on ALD (address load) with debouncing
                if (last_ald == 1 and current_ald == 0 and 
                    time.ticks_diff(current_time, last_edge_time) > debounce_time):
                    
                    last_edge_time = current_time
                    self.ald_edge_count += 1
                    debug_print("INTERFACE", f"*** ALD FALLING EDGE DETECTED #{self.ald_edge_count} ***")
                    
                    if not self.busy:
                        # Read address from input pins
                        address = self.read_address()
                        allophone_name = debug_allophone_name(address)
                        debug_print("INTERFACE", f"Address loaded: {address} ({allophone_name})")
                        debug_print("INTERFACE", "Setting BUSY/READY state: BUSY")
                        
                        # Play the requested allophone
                        success = self.play_allophone(address)
                        if success:
                            debug_print("INTERFACE", f"Successfully played allophone {address}")
                        else:
                            debug_print("INTERFACE", f"Failed to play allophone {address}")
                            
                        debug_print("INTERFACE", "Setting BUSY/READY state: READY")
                    else:
                        debug_print("INTERFACE", f"Busy - ignoring address load request")
                        
                last_ald = current_ald
                time.sleep_us(10)  # Small delay to prevent excessive polling
                
        except KeyboardInterrupt:
            debug_print("INTERFACE", "Interface monitoring stopped by user")
        except Exception as e:
            debug_print("INTERFACE", f"Interface monitoring error: {e}")
            raise

    def debug_system_status(self):
        """Comprehensive system status for debugging"""
        debug_print("SYSTEM", "=== SYSTEM STATUS ===")
        debug_print("SYSTEM", f"Busy: {self.busy}")
        debug_print("SYSTEM", f"Last allophone: {self.last_allophone}")
        debug_print("SYSTEM", f"Total played: {self.total_allophones_played}")
        debug_print("SYSTEM", f"ALD edges: {self.ald_edge_count}")
        debug_print("SYSTEM", f"Cached allophones: {len(self.allophones)}")
        
        # GPIO states
        try:
            debug_print("SYSTEM", f"ALD pin: {self.ald_pin.value()}")
            debug_print("SYSTEM", f"LRQ pin: {self.lrq_pin.value()}")
            debug_print("SYSTEM", f"SBY pin: {self.sby_pin.value()}")
        except:
            debug_print("SYSTEM", "GPIO state read error")
            
        # Memory status
        gc.collect()
        free_mem = gc.mem_free()
        alloc_mem = gc.mem_alloc()
        debug_print("SYSTEM", f"Memory: {free_mem} free, {alloc_mem} allocated")
        
        debug_print("SYSTEM", "=== END STATUS ===")

def core1_main():
    """Main function for core 1 (speech synthesis)"""
    global sp0256_instance
    
    debug_print("SYSTEM", "=== CORE1 STARTED ===")
    debug_print("SYSTEM", "Core1: Initializing SP0256 emulator...")
    
    try:
        sp0256_instance = SP0256Emulator()
        debug_print("SYSTEM", "Core1: SP0256 emulator initialized successfully")
    except Exception as e:
        debug_print("SYSTEM", f"Core1: FATAL ERROR during initialization: {e}")
        return
    
    # Test sequence for debugging
    test_sequence = [HH, EH, LL, AX, OW, PA5, WW, OR, LL, DD1]
    debug_print("SYSTEM", f"Core1: Test sequence prepared: {test_sequence}")
    
    # Optional test playback (disabled by default)
    if DEBUG_ENABLED and False:  # Change to True to enable startup test
        debug_print("SYSTEM", "Core1: Running test sequence...")
        try:
            sp0256_instance.play_allophones(test_sequence)
            debug_print("SYSTEM", "Core1: Test sequence completed successfully")
        except Exception as e:
            debug_print("SYSTEM", f"Core1: ERROR during test sequence: {e}")
    
    # Monitor interface
    debug_print("SYSTEM", "Core1: Starting interface monitoring...")
    try:
        sp0256_instance.monitor_interface()
    except KeyboardInterrupt:
        debug_print("SYSTEM", "Core1: Interface monitoring stopped by user")
    except Exception as e:
        debug_print("SYSTEM", f"Core1: FATAL ERROR in interface monitoring: {e}")
        raise

def main():
    """Main program"""
    global command_enabled
    
    debug_print("SYSTEM", "=== SP0256 EMULATOR STARTING ===")
    debug_config_summary()
    
    print("*********************")
    print("*  SP0256 Emulator  *") 
    print("*   COMPLETE v44    *")
    print("*********************")
    
    debug_performance_stats()
    debug_memory()
    
    # Optional pin test for debugging hardware connections
    if DEBUG_GPIO:
        debug_pin_test()
    
    # Launch speech synthesis on second core
    debug_print("SYSTEM", "Starting Core1 thread for speech synthesis...")
    try:
        _thread.start_new_thread(core1_main, ())
        debug_print("SYSTEM", "Core1 thread started successfully")
    except Exception as e:
        debug_print("SYSTEM", f"FATAL ERROR: Could not start Core1 thread: {e}")
        return
    
    # Wait for SP0256 to initialize
    debug_print("SYSTEM", "Waiting for SP0256 initialization...")
    timeout = 50  # 5 seconds
    while sp0256_instance is None and timeout > 0:
        time.sleep(0.1)
        timeout -= 1
    
    if sp0256_instance is None:
        debug_print("SYSTEM", "ERROR: SP0256 failed to initialize")
        return
    
    debug_print("SYSTEM", "SP0256 initialized, starting command interface...")
    
    # Check memory status and give recommendations
    gc.collect()
    free_mem = gc.mem_free()
    if free_mem < 20000:
        print(f"\nWARNING: Low memory ({free_mem} bytes free)")
        print("Recommendations:")
        print("- Use CLEANUP command to free memory if needed")
        print("- Avoid long sequences of allophones")
        print("- Use RESET command if system becomes unstable")
    elif free_mem < 40000:
        print(f"\nNote: Memory available: {free_mem} bytes")
        print("Use MEMORY command to monitor usage")
    else:
        print(f"\nMemory OK: {free_mem} bytes available")
    
    # Hardware setup reminder
    print("\nHARDWARE REMINDER:")
    print("GP11 audio output REQUIRES external low-pass filter (~5kHz)")
    print("Suggested: 1kΩ resistor + 33nF capacitor to ground")
    print("Without filter, audio will be distorted!")
    
    # Optional startup demo (disabled by default to save memory)
    if False and DEBUG_ENABLED:  # Change first False to True to enable demo
        debug_print("SYSTEM", "Running startup demo...")
        print("\nStartup test: 'Hello World'")
        try:
            sp0256_instance.play_allophones([HH, EH, LL, OW, PA2, WW, OR, LL, DD1])
            print("Demo complete!")
        except Exception as e:
            debug_print("SYSTEM", f"Demo failed: {e}")
    
    # Main core runs the command interface
    try:
        command_interface()
    except KeyboardInterrupt:
        debug_print("SYSTEM", "=== SHUTDOWN: Keyboard interrupt received ===")
        print("\nShutdown requested by user")
        command_enabled = False
    except Exception as e:
        debug_print("SYSTEM", f"=== FATAL ERROR in command interface: {e} ===")
        raise
    finally:
        debug_print("SYSTEM", "=== SP0256 EMULATOR TERMINATING ===")

if __name__ == "__main__":
    main()