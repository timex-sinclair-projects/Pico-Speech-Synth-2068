#!/usr/bin/env python3
"""revb_netlist.py -- the Rev B board as a pin-level netlist.

Writes, under hardware/revb/:
  zxvoice-revb.net       Protel/Altium netlist (EasyEDA Pro: File > Import > Netlist)
  zxvoice-revb-nets.csv  every connection as net, ref, pin, pin name
  zxvoice-revb-bom.csv   bill of materials with LCSC numbers where confirmed

Every pin of every part is either assigned to a net or listed in NC below,
and the script refuses to emit anything if a pin is used twice or a net has
a single member.  Edit the tables, rerun, re-import.

Pinouts verified against datasheets: RP2040 (QFN-56), W25Q32JVSSIQ, ABM8,
AP2112K, SN74LVC245A, SN74LVC1G125, CD74HCT688 (SCHS196E), SN74HCT138,
MAX98357A TQFN-16 (19-6552).
"""
import csv, os, sys
from collections import OrderedDict, defaultdict

out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'hardware', 'revb')

# ---------------------------------------------------------------------------
# Parts: ref -> (value/MPN, footprint, LCSC, {pin: name})
# ---------------------------------------------------------------------------
def pins(spec):
    return OrderedDict((str(k), v) for k, v in spec)

parts = OrderedDict()

parts['U1'] = ('RP2040', 'QFN-56_L7.0-W7.0-P0.40-BL-EP3.2', 'C2040', pins([
    (1,'IOVDD'),(2,'GP0'),(3,'GP1'),(4,'GP2'),(5,'GP3'),(6,'GP4'),(7,'GP5'),(8,'GP6'),(9,'GP7'),
    (10,'IOVDD'),(11,'GP8'),(12,'GP9'),(13,'GP10'),(14,'GP11'),(15,'GP12'),(16,'GP13'),(17,'GP14'),
    (18,'GP15'),(19,'TESTEN'),(20,'XIN'),(21,'XOUT'),(22,'IOVDD'),(23,'DVDD'),(24,'SWCLK'),(25,'SWD'),
    (26,'RUN'),(27,'GP16'),(28,'GP17'),(29,'GP18'),(30,'GP19'),(31,'GP20'),(32,'GP21'),(33,'IOVDD'),
    (34,'GP22'),(35,'GP23'),(36,'GP24'),(37,'GP25'),(38,'GP26'),(39,'GP27'),(40,'GP28'),(41,'GP29'),
    (42,'IOVDD'),(43,'ADC_AVDD'),(44,'VREG_VIN'),(45,'VREG_VOUT'),(46,'USB_DM'),(47,'USB_DP'),
    (48,'USB_VDD'),(49,'IOVDD'),(50,'DVDD'),(51,'QSPI_SD3'),(52,'QSPI_SCLK'),(53,'QSPI_SD0'),
    (54,'QSPI_SD2'),(55,'QSPI_SD1'),(56,'QSPI_SS_N'),(57,'GND_EP')]))
parts['U2'] = ('W25Q32JVSSIQ', 'SOIC-8_L5.3-W5.3-P1.27-LS8.0-BL', 'C82317', pins([
    (1,'nCS'),(2,'DO_IO1'),(3,'nWP_IO2'),(4,'GND'),(5,'DI_IO0'),(6,'CLK'),(7,'nHOLD_IO3'),(8,'VCC')]))
parts['Y1'] = ('ABM8-272-T3 12MHz', 'OSC-SMD_4P-L3.2-W2.5-BL', 'C20625', pins([(1,'XTAL1'),(2,'GND'),(3,'XTAL2'),(4,'GND')]))
parts['U3'] = ('AP2112K-3.3TRG1', 'SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR', 'C51118', pins([(1,'VIN'),(2,'GND'),(3,'EN'),(4,'NC'),(5,'VOUT')]))
lvc245 = lambda: pins([(1,'DIR'),(2,'A1'),(3,'A2'),(4,'A3'),(5,'A4'),(6,'A5'),(7,'A6'),(8,'A7'),(9,'A8'),(10,'GND'),
    (11,'B8'),(12,'B7'),(13,'B6'),(14,'B5'),(15,'B4'),(16,'B3'),(17,'B2'),(18,'B1'),(19,'nOE'),(20,'VCC')])
parts['U4'] = ('SN74LVC245APWR (data in)',   'TSSOP-20_L6.5-W4.4-P0.65-LS6.4-BL', 'C6112', lvc245())
parts['U5'] = ('SN74LVC245APWR (strobes in)','TSSOP-20_L6.5-W4.4-P0.65-LS6.4-BL', 'C6112', lvc245())
parts['U6'] = ('SN74LVC245APWR (status out)','TSSOP-20_L6.5-W4.4-P0.65-LS6.4-BL', 'C6112', lvc245())
parts['U7'] = ('SN74LVC1G125DBVR', 'SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR', '', pins([(1,'nOE'),(2,'A'),(3,'GND'),(4,'Y'),(5,'VCC')]))
parts['U8'] = ('CD74HCT688M', 'SOIC-20_L12.8-W7.5-P1.27-LS10.3-BL', 'C553159', pins([
    (1,'nE'),(2,'P0'),(3,'Q0'),(4,'P1'),(5,'Q1'),(6,'P2'),(7,'Q2'),(8,'P3'),(9,'Q3'),(10,'GND'),
    (11,'P4'),(12,'Q4'),(13,'P5'),(14,'Q5'),(15,'P6'),(16,'Q6'),(17,'P7'),(18,'Q7'),(19,'nPEQ'),(20,'VCC')]))
parts['U9'] = ('SN74HCT138DR', 'SOIC-16_L9.9-W3.9-P1.27-LS6.0-BL', '', pins([
    (1,'A'),(2,'B'),(3,'C'),(4,'nG2A'),(5,'nG2B'),(6,'G1'),(7,'Y7'),(8,'GND'),
    (9,'Y6'),(10,'Y5'),(11,'Y4'),(12,'Y3'),(13,'Y2'),(14,'Y1'),(15,'Y0'),(16,'VCC')]))
parts['U10'] = ('MAX98357AETE+T', 'TQFN-16_L3.0-W3.0-P0.50-BL-EP1.7', 'C910544', pins([
    (1,'DIN'),(2,'GAIN_SLOT'),(3,'GND'),(4,'SD_MODE'),(5,'NC'),(6,'NC'),(7,'VDD'),(8,'VDD'),
    (9,'OUTP'),(10,'OUTN'),(11,'GND'),(12,'NC'),(13,'NC'),(14,'LRCLK'),(15,'GND'),(16,'BCLK'),(17,'EP')]))
parts['D1'] = ('B5819W', 'SOD-123_L2.8-W1.8-LS3.7-RD', 'C8598', pins([(1,'K'),(2,'A')]))
parts['D2'] = ('B5819W', 'SOD-123_L2.8-W1.8-LS3.7-RD', 'C8598', pins([(1,'K'),(2,'A')]))
parts['F1'] = ('PTC 0.5A hold', 'F1206', '', pins([(1,'1'),(2,'2')]))
edge = [(f'{i}A', f'{i}A') for i in range(1, 24)] + [(f'{i}B', f'{i}B') for i in range(1, 24)]
parts['J1'] = ('TS1000 edge connector 2x23', 'EDGE-2X23-P2.54-KEY3', '', OrderedDict(edge))
parts['J2'] = ('USB-C 16P receptacle', 'USB-C-SMD_TYPE-C-31-M-12', 'C165948', pins([
    ('A1','GND'),('A4','VBUS'),('A5','CC1'),('A6','DP'),('A7','DM'),('A8','SBU1'),('A9','VBUS'),('A12','GND'),
    ('B1','GND'),('B4','VBUS'),('B5','CC2'),('B6','DP'),('B7','DM'),('B8','SBU2'),('B9','VBUS'),('B12','GND'),
    ('S1','SHELL'),('S2','SHELL'),('S3','SHELL'),('S4','SHELL')]))
parts['J3'] = ('B2B-PH-K-S speaker', 'CONN-TH_2P-P2.00_B2B-PH-K-S', 'C131337', pins([(1,'SPK+'),(2,'SPK-')]))
parts['J4'] = ('3.5mm jack PJ-320A', 'AUDIO-SMD_PJ-320A', '', pins([(1,'SLEEVE'),(2,'TIP'),(3,'RING'),(4,'SWITCH'),(5,'SWITCH2')]))
parts['J5'] = ('SWD 1x3', 'HDR-TH_3P-P2.54-V', '', pins([(1,'SWCLK'),(2,'GND'),(3,'SWDIO')]))
for sw in ('SW1', 'SW2'):
    parts[sw] = ('TS-1187A tactile' + (' BOOTSEL' if sw == 'SW1' else ' RESET'), 'SW-SMD_4P-L6.0-W3.5', 'C318884',
                 pins([(1,'1'),(2,'1B'),(3,'2'),(4,'2B')]))
parts['LED1'] = ('LED green SBY', 'LED0603-RD', '', pins([(1,'K'),(2,'A')]))
def two(ref, value, fp='R0603', lcsc=''):
    parts[ref] = (value, fp, lcsc, pins([(1,'1'),(2,'2')]))
def three(ref, value):
    parts[ref] = (value, 'SolderJumper-3_P1.3', '', pins([(1,'1'),(2,'C'),(3,'3')]))

# resistors
two('R1','27R USB'); two('R2','27R USB'); two('R3','5.1k CC1'); two('R4','5.1k CC2')
two('R5','1k XOUT'); two('R6','10k RUN'); two('R7','10k QSPI_SS'); two('R8','1k BOOTSEL')
two('R9','1k SBY LED'); two('R10','1k SD_MODE')
two('R11','3.3k line-out 1'); two('R12','3.3k line-out 2a'); two('R13','30k line-out 2b'); two('R14','100k bleed')
for i in range(8): two(f'R2{i}', f'33R RB{i}')
two('R28','33R D7 status')
# capacitors
for i in range(1, 18): two(f'C{i}', '100nF', 'C0603')
two('C20','1uF DVDD','C0603'); two('C21','1uF DVDD','C0603')
two('C22','10uF LDO in','C0805'); two('C23','10uF LDO out','C0805'); two('C24','10uF amp VDD','C0805'); two('C25','10uF line-out AC','C0805')
two('C26','15pF XIN','C0603'); two('C27','15pF XOUT','C0603'); two('C28','22nF line-out 1','C0603'); two('C29','22nF line-out 2','C0603')
two('C30','220uF 6.3V amp bulk','CAP-SMD_BD6.3-L6.6-W6.6-FD','')
# jumpers
for i in range(1, 7): three(f'JP{i}', f'port bit Q{i-1}: 1=+5V C=Q 3=GND')
three('JP7', 'LOOSE decode: 1=nPEQ C=G2A 3=A7')
two('JP8', 'VINTAGE: bridged=3.3k, open=33k', 'SolderJumper-2_P1.3')
two('JP9', 'board ID bit 0 to GND', 'SolderJumper-2_P1.3'); two('JP10', 'board ID bit 1 to GND', 'SolderJumper-2_P1.3')

# ---------------------------------------------------------------------------
# Nets: name -> [REF-PIN ...]
# ---------------------------------------------------------------------------
N = OrderedDict()
def net(name, *members):
    N.setdefault(name, []).extend(members)

# --- edge connector (TS1000 numbering, slot at position 3)
net('D0','J1-4A'); net('D1','J1-5A'); net('D2','J1-6A'); net('D3','J1-9A'); net('D4','J1-10A')
net('D5','J1-8A'); net('D6','J1-7A'); net('D7','J1-1A')
net('A0','J1-7B'); net('A1','J1-8B'); net('A2','J1-9B'); net('A3','J1-10B')
net('A4','J1-22B'); net('A5','J1-21B'); net('A6','J1-20B'); net('A7','J1-19B')
net('nIORQ','J1-15A'); net('nRD','J1-16A'); net('nM1','J1-22A'); net('nRESET','J1-21A'); net('CLK','J1-6B')
net('5V_BUS','J1-1B'); net('GND','J1-4B','J1-5B')

# --- decode: 74HCT688 (P = address, Q = jumpers), 74HCT138
net('nIORQ','U8-1','U9-5'); net('+5V','U8-20','U9-16')
net('A0','U8-2'); net('A1','U8-4'); net('A2','U8-6'); net('A3','U8-8'); net('A6','U8-11'); net('A7','U8-13')
net('GND','U8-10','U8-15','U8-16','U8-17','U8-18')
for i, pin in enumerate((3, 5, 7, 9, 12, 14)):
    net(f'Q{i}', f'U8-{pin}', f'JP{i+1}-2'); net('+5V', f'JP{i+1}-1'); net('GND', f'JP{i+1}-3')
net('nPEQ','U8-19','JP7-1'); net('DEC_EN','JP7-2','U9-4'); net('A7','JP7-3')
net('A4','U9-1'); net('A5','U9-2'); net('nRD','U9-3'); net('nM1','U9-6'); net('GND','U9-8')
net('nALD','U9-10'); net('nTXT','U9-7'); net('nRDSTAT','U9-13'); net('nRDEXT','U9-12')

# --- U4 data in: A = bus, B = GP0..7
net('+3V3','U4-1','U4-20'); net('GND','U4-10','U4-19')
for i in range(8):
    net(f'D{i}', f'U4-{2+i}'); net(f'GP{i}', f'U4-{18-i}', f'U1-{2+i}')

# --- U5 strobes in
net('+3V3','U5-1','U5-20'); net('GND','U5-10','U5-19','U5-7','U5-8','U5-9')
net('nALD','U5-2');    net('GP8','U5-18','U1-11')
net('nTXT','U5-3');    net('GP9','U5-17','U1-12')
net('nRDSTAT','U5-4'); net('GP15','U5-16','U1-18')
net('nRESET','U5-5');  net('GP10','U5-15','U1-13')
net('CLK','U5-6');     net('GP12','U5-14','U1-15')

# --- U6 status byte out: B = GP20..27, A -> 33R -> bus
net('+3V3','U6-20'); net('GND','U6-1','U6-10'); net('nRDEXT','U6-19')
gp2027 = ['U1-31','U1-32','U1-34','U1-35','U1-36','U1-37','U1-38','U1-39']
for i in range(8):
    net(f'GP{20+i}', f'U6-{18-i}', gp2027[i])
    net(f'RB{i}', f'U6-{2+i}', f'R2{i}-1'); net(f'D{i}', f'R2{i}-2')

# --- U7 D7 status driver
net('nRDSTAT','U7-1'); net('READY','U7-2','U1-16'); net('GND','U7-3'); net('+3V3','U7-5')
net('RB7S','U7-4','R28-1'); net('D7','R28-2')

# --- power
net('5V_BUS','F1-1'); net('5V_FUSED','F1-2','D1-2'); net('+5V','D1-1','D2-1')
net('VBUS','J2-A4','J2-A9','J2-B4','J2-B9','D2-2')
net('+5V','U3-1','U3-3','C22-1','C30-1','U10-7','U10-8','C24-1','C17-1')
net('+3V3','U3-5','C23-1'); net('GND','U3-2','C22-2','C23-2','C30-2','C24-2','C17-2')

# --- RP2040 power and decoupling
for p in (1, 10, 22, 33, 42, 49, 48, 44, 43): net('+3V3', f'U1-{p}')
net('DVDD','U1-45','U1-23','U1-50','C20-1','C21-1'); net('GND','C20-2','C21-2','U1-19','U1-57')
for i in range(1, 10): net('+3V3', f'C{i}-1'); net('GND', f'C{i}-2')       # C1..C9 at RP2040 supply pins
net('+3V3','U2-8','C10-1'); net('GND','U2-4','C10-2')                      # C10 at flash
for i, u in zip(range(11, 17), ('U4','U5','U6','U7','U8','U9')):            # C11..C16 at logic
    net('+5V' if u in ('U8','U9') else '+3V3', f'C{i}-1'); net('GND', f'C{i}-2')

# --- crystal
net('XIN','U1-20','Y1-1','C26-1'); net('XOUT','U1-21','R5-1'); net('XOUT_R','R5-2','Y1-3','C27-1')
net('GND','Y1-2','Y1-4','C26-2','C27-2')

# --- QSPI flash, BOOTSEL
net('QSPI_SD0','U1-53','U2-5'); net('QSPI_SD1','U1-55','U2-2'); net('QSPI_SD2','U1-54','U2-3')
net('QSPI_SD3','U1-51','U2-7'); net('QSPI_SCLK','U1-52','U2-6')
net('QSPI_SS','U1-56','U2-1','R7-1','R8-1'); net('+3V3','R7-2'); net('BOOTSEL','R8-2','SW1-1','SW1-2'); net('GND','SW1-3','SW1-4')

# --- RUN, SWD
net('RUN','U1-26','R6-1','SW2-1','SW2-2'); net('+3V3','R6-2'); net('GND','SW2-3','SW2-4')
net('SWCLK','U1-24','J5-1'); net('GND','J5-2'); net('SWDIO','U1-25','J5-3')

# --- USB
net('USB_DP','U1-47','R1-1'); net('USB_DP_C','R1-2','J2-A6','J2-B6')
net('USB_DM','U1-46','R2-1'); net('USB_DM_C','R2-2','J2-A7','J2-B7')
net('CC1','J2-A5','R3-1'); net('CC2','J2-B5','R4-1'); net('GND','R3-2','R4-2')
net('GND','J2-A1','J2-A12','J2-B1','J2-B12','J2-S1','J2-S2','J2-S3','J2-S4')

# --- I2S amplifier
net('I2S_DIN','U1-27','U10-1'); net('I2S_BCLK','U1-28','U10-16'); net('I2S_LRCLK','U1-29','U10-14')
net('AMP_EN','U1-30','R10-1'); net('SD_MODE','R10-2','U10-4')
net('GND','U10-3','U10-11','U10-15','U10-17')
net('SPK+','U10-9','J3-1'); net('SPK-','U10-10','J3-2')

# --- PWM line-out: 3.3k/22n, 3.3k(+30k)/22n, 10u, 100k, jack
net('PWM','U1-14','R11-1'); net('LO1','R11-2','C28-1','R12-1'); net('GND','C28-2')
net('LO1B','R12-2','R13-1','JP8-1'); net('LO2','R13-2','JP8-2','C29-1','C25-1'); net('GND','C29-2')
net('LO_OUT','C25-2','R14-1','J4-2','J4-3'); net('GND','R14-2','J4-1')

# --- LED, board ID
net('SBY','U1-17','R9-1'); net('LED_A','R9-2','LED1-2'); net('GND','LED1-1')
net('GP28','U1-40','JP9-1'); net('GP29','U1-41','JP10-1'); net('GND','JP9-2','JP10-2')

# --- deliberately unconnected pins
NC = {
    'U1': [],
    'U3': ['4'],
    'U5': ['11','12','13'],              # B6..B8: outputs of the three grounded spare inputs
    'U9': ['9','11','14','15'],          # Y6 Y4 Y1 Y0: ports 0x27 write, 0x07 either way, 0x17 read
    'U10': ['2','5','6','12','13'],       # GAIN_SLOT open = 9 dB
    'J2': ['A8','B8'],
    'J4': ['4','5'],
    'J1': [f'{i}A' for i in (2,3,11,12,13,14,17,18,19,20,23)] + [f'{i}B' for i in (2,3,11,12,13,14,15,16,17,18,23)],
}

# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------
used = defaultdict(list)
errors = []
for name, members in N.items():
    if len(members) < 2:
        errors.append(f'net {name} has one member: {members}')
    for m in members:
        ref, pin = m.rsplit('-', 1)
        if ref not in parts: errors.append(f'{m}: unknown part'); continue
        if pin not in parts[ref][3]: errors.append(f'{m}: unknown pin (has {list(parts[ref][3])[:6]}...)'); continue
        used[(ref, pin)].append(name)
for (ref, pin), nets in used.items():
    if len(nets) > 1: errors.append(f'{ref}-{pin} on two nets: {nets}')
for ref, (val, fp, lcsc, pmap) in parts.items():
    for pin in pmap:
        if (ref, pin) not in used and pin not in NC.get(ref, []):
            errors.append(f'{ref}-{pin} ({pmap[pin]}) unconnected and not declared NC')
for ref, pl in NC.items():
    for pin in pl:
        if (ref, pin) in used: errors.append(f'{ref}-{pin} declared NC but used')
if errors:
    print('\n'.join(errors)); sys.exit(1)

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
os.makedirs(out_dir, exist_ok=True)
with open(os.path.join(out_dir, 'zxvoice-revb.net'), 'w') as f:
    for ref, (val, fp, lcsc, pmap) in parts.items():
        f.write(f'[\n{ref}\n{fp}\n{val}\n\n\n\n]\n')
    for name, members in N.items():
        f.write(f'(\n{name}\n' + '\n'.join(members) + '\n)\n')
with open(os.path.join(out_dir, 'zxvoice-revb-nets.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['net','ref','pin','pin_name','part'])
    for name, members in N.items():
        for m in members:
            ref, pin = m.rsplit('-', 1)
            w.writerow([name, ref, pin, parts[ref][3][pin], parts[ref][0]])
with open(os.path.join(out_dir, 'zxvoice-revb-bom.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['ref','value','footprint','lcsc'])
    for ref, (val, fp, lcsc, pmap) in parts.items():
        w.writerow([ref, val, fp, lcsc])
print(f'{len(parts)} parts, {len(N)} nets, {sum(len(m) for m in N.values())} connections -> {out_dir}')
