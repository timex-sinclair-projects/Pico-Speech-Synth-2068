# Pico Speech Synth 2068

A modern replacement for the classic General Instruments SP0256-AL2 speech synthesizer chip using the Raspberry Pi Pico (RP2040). This project provides hardware-accurate emulation for the Timex/Sinclair 1000 and 2068.

![SP0256 Emulator](https://img.shields.io/badge/Platform-RP2040-green) ![Language](https://img.shields.io/badge/Language-MicroPython-blue) ![Status](https://img.shields.io/badge/Status-Active-brightgreen)

## Features

- **Hardware-accurate SP0256-AL2 emulation** with authentic allophone samples
- **Dual-core operation** - Core 0 handles commands, Core 1 manages speech synthesis
- **Interactive command interface** for testing without vintage hardware
- **Comprehensive debugging** with categorized logging to Thonny console
- **Authentic audio timing** at 11025 Hz sample rate with PWM output
- **Complete interface compatibility** with original SP0256 (LRQ, SBY, ALD signals)
- **Easy allophone data conversion** from C arrays to Python bytes objects

##  Hardware Requirements

### Components

- **Raspberry Pi Pico** (RP2040-based board)
- **74LVC245** - Bidirectional level shifter and bus buffer
- **74HC138** - 3-to-8 line decoder for I/O port selection
- **2N3904** - NPN transistor for signal inversion
- Various passive components (resistors, capacitors)
- **Timex/Sinclair expansion connector**

### Pin Connections

```
RP2040 Pico Pin Assignments:
├── GP0-GP5    : Address inputs A1-A6 (from 74LVC245)
├── GP8        : ALD input (Address Load from 74HC138)
├── GP11       : PWM audio output
├── GP13       : LRQ output (Load Request, active low)
└── GP14       : SBY output (Standby)

Z80 Interface:
├── OUT 17,val : Send phoneme to Pico
└── IN 37,val  : Check LRQ status (bit 7)
```

## Quick Start

### 1. Hardware Assembly

1. Assemble the circuit according to the provided schematic
2. Connect to your Timex/Sinclair expansion port
3. Verify all connections with a multimeter

### 2. Software Setup

1. Install MicroPython on your Pico
2. Copy files to Pico:
   - `main.py`
   - `allophones.py` 

### 3. Running the Emulator

1. Open Thonny IDE

2. Run `main.py`

3. Use the interactive command interface:

   ```
   > HELLO> SPEAK HH EH LL OW PA2 WW OR LL DD1> LIST> STATUS
   ```

## Interactive Commands

The emulator includes a powerful command interface for testing:

| Command              | Description                   | Example             |
| -------------------- | ----------------------------- | ------------------- |
| `SPEAK <allophones>` | Play allophone sequence       | `SPEAK HH EH LL OW` |
| `LIST`               | Show all available allophones | `LIST`              |
| `HELLO` / `WORLD`    | Quick test phrases            | `HELLO`             |
| `STATUS`             | System status and statistics  | `STATUS`            |
| `MEMORY`             | Memory usage information      | `MEMORY`            |
| `GPIO`               | Current GPIO pin states       | `GPIO`              |
| `TEST`               | Hardware pin test             | `TEST`              |
| `DEBUG <category>`   | Toggle debug output           | `DEBUG TIMING`      |
| `HELP`               | Show all commands             | `HELP`              |

### Example Words & Phrases

```
Hello World: SPEAK HH EH LL OW PA2 WW OR LL DD1
Computer:    SPEAK KK1 AX MM PP YY1 UW1 TT2 ER
Speech:      SPEAK SS PA3 PP IY CH
Test:        SPEAK TT2 EH SS TT2
Ready:       SPEAK RR1 EH DD1 IY
```

## Allophone Reference

The SP0256-AL2 contains 64 allophones (speech sounds):

### Pauses

- **PA1** (0) - 10ms pause
- **PA2** (1) - 30ms pause
- **PA3** (2) - 50ms pause
- **PA4** (3) - 100ms pause
- **PA5** (4) - 200ms pause

### Vowels & Consonants

- **HH** (27) - "He" sound
- **EH** (7) - "End" sound
- **LL** (45) - "Lake" sound
- **OW** (53) - "Beau" sound
- **WW** (46) - "Wool" sound
- **OR** (58) - "Store" sound
- ...and 58 more!

Use `LIST` command for complete reference with descriptions.

## Debug Features

Comprehensive debugging with categorized output:

```python
# Debug configuration (in sp0256_emulator.py)
DEBUG_ENABLED = True      # Master switch
DEBUG_GPIO = True         # Pin state changes
DEBUG_AUDIO = True        # Playback progress
DEBUG_TIMING = True       # Sample timing accuracy
DEBUG_INTERFACE = True    # Command processing
DEBUG_SYSTEM = True       # System status
DEBUG_VERBOSE = False     # Extra detail
```

**Sample debug output:**

```
[00001234] SYSTEM: SP0256 Emulator initialization complete
[00001456] INTERFACE: *** ALD FALLING EDGE DETECTED #1 ***
[00001467] GPIO: Address loaded: 5 (OY)
[00001469] AUDIO: === Starting playback of allophone 5 (OY) ===
[00001590] TIMING: Playback completed: 99.9% accurate
```

## Performance

- **Sample Rate**: 11025 Hz (authentic SP0256 timing)
- **Audio Resolution**: 8-bit PCM converted to 16-bit PWM
- **Timing Accuracy**: Typically >99% accurate sample timing
- **Memory Usage**: ~200KB for full allophone set
- **Latency**: <100µs response to ALD edge detection

## Hardware Interface

The emulator maintains full compatibility with original SP0256 timing:

### Z80 Programming Example

```assembly
; Send allophone 27 (HH sound)
LD A, 27
OUT (17), A        ; Send to speech synthesizer

; Wait for completion
WAIT_LOOP:
IN A, (37)         ; Read status
BIT 7, A           ; Check LRQ bit
JR NZ, WAIT_LOOP   ; Wait while busy
```

### Signal Timing

- **LRQ (Load Request)**: Active low when ready for next allophone
- **SBY (Standby)**: High when not speaking
- **ALD (Address Load)**: Falling edge latches address

## Troubleshooting

### Common Issues

**No audio output:**

- Check PWM pin connection (GP11)
- Verify audio filtering circuit
- Test with `SPEAK PA1` (should be brief silence)

**Commands not recognized:**

- Check Z80 interface connections
- Verify 74LVC245 direction and enable signals
- Use `GPIO` command to check pin states

**Timing errors:**

- Reduce debug output: `DEBUG TIMING` to toggle off
- Check system load and memory usage: `MEMORY`
- Verify 11025 Hz sample rate timing

**Memory errors:**

- Reduce allophone data size
- Check available RAM: `MEMORY`
- Restart if memory fragmented

### Debug Commands

```
> STATUS          # Check system health
> TEST            # Verify hardware connections  
> GPIO            # Check pin states
> MEMORY          # Check memory usage
> DEBUG SYSTEM    # Toggle system debugging
```

## Documentation

- **SP0256 Datasheet**: Original chip specifications and timing
- **Allophone Guide**: Complete phoneme usage and pronunciation
- **Hardware Guide**: Step-by-step assembly instructions
- **Programming Examples**: Z80 assembly and BASIC code samples

## Contributing

Contributions welcome! Areas of interest:

- Additional vintage computer interfaces (TRS-80, Apple II, etc.)
- PCB design optimization
- Enhanced audio filtering
- Additional allophone sets or languages
- Performance optimizations

### Development Setup

1. Fork the repository
2. Create feature branch: `git checkout -b feature-name`
3. Test thoroughly with hardware
4. Submit pull request with detailed description

## License

This project is licensed under the GNU GENERAL PUBLIC LICENSE - see the [LICENSE](https://claude.ai/chat/LICENSE) file for details.

## Acknowledgments

**Wilf Rigter** - Original ZX Voice design inspiration

- https://archive.org/details/zx-appeal/ZX-Appeal%20Oct%2086/page/n7/mode/1up
- https://archive.org/details/zx-appeal/ZX-Appeal%20Jul-Aug%2086/page/n1/mode/2up
- https://archive.org/details/zx-appeal/ZX-Appeal%20Mar%2087/page/n7/mode/2up
- https://archive.org/details/zx-appeal/ZX-Appeal%20Apr%2087/page/8/mode/1up
- https://archive.org/details/analog-computing-magazine-29/page/n59/mode/2up

This project builds on the work of these two projects:

- https://github.com/blackjetrock/pico-sp0256
- https://github.com/ExtremeElectronics/SP0256-AL2-Pico-Emulation-Detail

and contains code that drives the PWM to generate the audio from the phoneme data which is at this project:

https://www.cpcwiki.eu/index.php/SP0256_Allophones

## Support

- **Issues**: Use GitHub Issues for bug reports and feature requests
- **Discussions**: Use GitHub Discussions for questions and tips
- **Documentation**: Check the `docs/` folder for detailed guides

