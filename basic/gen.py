#!/usr/bin/env python3
"""gen.py -- write demo1000.bas (driver embedded in line 1 REM, for zmakebas -p)
and loader1000.bas (hex loader for typing in by hand) from zxvoice1000.bin."""
code = open('zxvoice1000.bin', 'rb').read()
assert 0x76 not in code, "a NEWLINE byte in the code would break the REM line"
rem = ''.join('\\{0x%02X}' % b for b in code)
open('demo1000.bas', 'w').write(f"""1 REM {rem}
10 PRINT "ZX VOICE REPLICA DEMO"
20 LET A$=CHR$ 27+CHR$ 7+CHR$ 45+CHR$ 15+CHR$ 53+CHR$ 4
30 LET N=USR 16514
40 LET A$="HELLO. I AM THE TIMEX SINCLAIR 1000."
50 LET N=USR 16517
60 LET A$=CHR$ 209
70 LET N=USR 16520
80 LET A$="NOW WITH THE CTS 256 RULES."
90 LET N=USR 16517
100 LET A$=CHR$ 208
110 LET N=USR 16520
120 PRINT "TYPE A SENTENCE, OR NOTHING TO STOP"
130 INPUT A$
140 IF A$="" THEN STOP
150 PRINT A$
160 LET N=USR 16517
170 GO TO 130
""")
hexs = code.hex().upper()
open('loader1000.bas', 'w').write(f"""1 REM {'X' * len(code)}
10 LET H$="{hexs}"
20 FOR I=0 TO {len(code) - 1}
30 POKE 16514+I,16*(CODE H$(2*I+1)-28)+CODE H$(2*I+2)-28
40 NEXT I
50 PRINT "DRIVER LOADED, NOW DELETE LINES 10-50 AND SAVE"
""")
print(len(code), 'bytes')
