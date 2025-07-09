"""
SP0256 Emulator for RP2040 Pico - MicroPython Version
Emulates the General Instruments SP0256-AL2 speech synthesizer

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

To reduce console spam, disable categories you don't need:
Example: Set DEBUG_TIMING = False to hide timing messages

HARDWARE CONNECTIONS (per schematic):
- GP0-GP5: Address inputs A1-A6 (from 74LVC245)
- GP8: ALD input (Address Load from 74HC138)  
- GP11: PWM audio output
- GP13: LRQ output (Load Request, active low)
- GP14: SBY output (Standby)

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
DEBUG_GPIO = True         # GPIO pin state changes
DEBUG_AUDIO = True        # Audio playback progress
DEBUG_TIMING = True       # Sample timing information
DEBUG_INTERFACE = True    # Interface monitoring and ALD edges
DEBUG_SYSTEM = True       # System initialization and memory
DEBUG_VERBOSE = False     # Extra verbose output (sample-by-sample)

# To reduce debug output, set specific categories to False:
# DEBUG_TIMING = False    # Disable timing messages
# DEBUG_AUDIO = False     # Disable audio progress messages
# DEBUG_GPIO = False      # Disable GPIO state messages

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
        sp0256_instance.play_allophones(sequence)
        print("PLAYBACK COMPLETE")
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
        print(f"Initialized: YES")
        print(f"Busy: {sp0256_instance.busy}")
        print(f"Last allophone: {sp0256_instance.last_allophone} ({debug_allophone_name(sp0256_instance.last_allophone)})")
        print(f"Total played: {sp0256_instance.total_allophones_played}")
        print(f"Available allophones: {len(sp0256_instance.allophones)}")
    else:
        print("Initialized: NO")
    
    debug_performance_stats()

def cmd_memory(args):
    """MEMORY command - show memory usage"""
    debug_memory()

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

def cmd_hello(args):
    """HELLO command - quick test"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
    print("SAYING: Hello")
    try:
        sp0256_instance.play_allophones([HH, EH, LL, OW])
        print("HELLO COMPLETE")
    except Exception as e:
        print(f"ERROR: {e}")

def cmd_world(args):
    """WORLD command - quick test"""
    if not sp0256_instance:
        print("ERROR: SP0256 not initialized")
        return
    print("SAYING: World")
    try:
        sp0256_instance.play_allophones([WW, OR, LL, DD1])
        print("WORLD COMPLETE")
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
    'STATUS': cmd_status,
    'MEMORY': cmd_memory,
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
        # Initialize GPIO pins
        self.setup_gpio()
        
        # Initialize PWM for audio
        self.setup_pwm()
        
        # Load allophone data
        self.load_allophones()
        
        # Status flags
        self.busy = False
        
        print("SP0256 Emulator initialized")
        
    def setup_gpio(self):
        """Initialize all GPIO pins"""
        # Address input pins (A1-A6, we only need 6 bits for 64 allophones)
        self.addr_pins = []
        for i in range(PIN_SP_A1, PIN_SP_A6 + 1):
            pin = Pin(i, Pin.IN)
            self.addr_pins.append(pin)
            
        # ALD input pin
        self.ald_pin = Pin(PIN_SP_ALD, Pin.IN)
        
        # LRQ output pin (active low)
        self.lrq_pin = Pin(PIN_SP_LRQ_N, Pin.OUT)
        self.lrq_pin.value(0)  # Ready state (low = ready)
        
        # SBY output pin
        self.sby_pin = Pin(PIN_SP_SBY, Pin.OUT)  
        self.sby_pin.value(1)  # Standby state (high = standby)
        
    def setup_pwm(self):
        """Initialize PWM for audio output"""
        self.pwm = PWM(Pin(SOUND_IO))
        self.pwm.freq(125000)  # PWM frequency
        self.pwm.duty_u16(32768)  # 50% duty cycle (silence)
        
    def load_allophones(self):
        """Load allophone sample data"""
        try:
            # Import your generated allophone data
            from allophones import allophones, get_allophone_count
            self.allophones = allophones
            print(f"Loaded {get_allophone_count()} allophones from file")
        except ImportError:
            print("Warning: allophone data file not found, using placeholder data")
            # Fallback to placeholder data
            self.allophones = {}
            
            # Calculate sample counts for pauses at 11025 Hz
            pause_durations = [
                int(0.010 * 11025),  # PA1: 10ms
                int(0.030 * 11025),  # PA2: 30ms  
                int(0.050 * 11025),  # PA3: 50ms
                int(0.100 * 11025),  # PA4: 100ms
                int(0.200 * 11025),  # PA5: 200ms
            ]
            
            # Initialize all 64 allophones with placeholder data
            for i in range(64):
                if i < 5:  # Pauses PA1-PA5
                    self.allophones[i] = bytes([0x80] * pause_durations[i])
                else:
                    # Placeholder data - replace with your actual allophone samples
                    self.allophones[i] = bytes([0x80] * 200)  # ~18ms of silence
                    
            print(f"Loaded {len(self.allophones)} placeholder allophones")
    
    def read_address(self):
        """Read 6-bit address from GPIO pins"""
        address = 0
        for i, pin in enumerate(self.addr_pins):
            if pin.value():
                address |= (1 << i)
        return address
        
    def play_allophone(self, allophone_id):
        """Play a single allophone"""
        if allophone_id >= 64 or allophone_id not in self.allophones:
            print(f"Invalid allophone ID: {allophone_id}")
            return
            
        sample_data = self.allophones[allophone_id]
        
        # Set busy state
        self.lrq_pin.value(1)  # Busy (high)
        self.sby_pin.value(0)  # Not in standby
        self.busy = True
        
        # Calculate precise timing for 11025 Hz
        sample_period_us = int(1000000 / 11025)  # ~90.7 microseconds
        
        # Play samples
        start_time = time.ticks_us()
        for i, sample in enumerate(sample_data):
            # Convert 8-bit sample to 16-bit PWM duty cycle
            duty = int((sample / 255.0) * 65535)
            self.pwm.duty_u16(duty)
            
            # Wait for precise timing
            next_sample_time = start_time + (i + 1) * sample_period_us
            while time.ticks_diff(time.ticks_us(), next_sample_time) < 0:
                pass
            
        # Return to ready state
        self.pwm.duty_u16(32768)  # Back to silence (50% duty cycle)
        self.lrq_pin.value(0)  # Ready (low)
        self.sby_pin.value(1)  # Standby
        self.busy = False
        
    def play_allophones(self, allophone_list):
        """Play a sequence of allophones"""
        for allophone_id in allophone_list:
            self.play_allophone(allophone_id)
            
    def monitor_interface(self):
        """Monitor the interface for address load signals"""
        last_ald = self.ald_pin.value()
        
        while True:
            current_ald = self.ald_pin.value()
            
            # Detect falling edge on ALD (address load)
            if last_ald == 1 and current_ald == 0:
                if not self.busy:
                    # Read address from input pins
                    address = self.read_address()
                    print(f"Playing allophone: {address}")
                    
                    # Play the requested allophone
                    self.play_allophone(address)
                    
            last_ald = current_ald
            time.sleep_us(10)  # Small delay to prevent excessive polling

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
    
    # Optional startup demo (set to True to enable)
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