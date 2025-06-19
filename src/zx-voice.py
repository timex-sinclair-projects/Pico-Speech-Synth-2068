import machine
import time
import _thread
from machine import Pin, PWM

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
    sp0256 = SP0256Emulator()
    
    # Test sequence
    test_sequence = [HH, EH, LL, AX, OW, PA5, WW, OR, LL, DD1]
    
    # Uncomment for test playback
    # sp0256.play_allophones(test_sequence)
    
    # Monitor interface
    sp0256.monitor_interface()

def main():
    """Main program"""
    print("*********************")
    print("*  SP0256 Emulator  *") 
    print("*********************")
    
    # Launch speech synthesis on second core
    _thread.start_new_thread(core1_main, ())
    
    # Main core can handle other tasks
    while True:
        time.sleep(1)

if __name__ == "__main__":
    main()