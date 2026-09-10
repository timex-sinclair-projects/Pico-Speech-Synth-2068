# ZX81 / TS1000 driver: assemble, embed in a REM line, build the .P file.
# Needs sjasmplus, zmakebas and python3.
all: zxvoice1000.p loader1000.bas

zxvoice1000.bin: zxvoice1000.asm
	sjasmplus --nologo $<

demo1000.bas loader1000.bas: zxvoice1000.bin gen.py
	python3 gen.py

zxvoice1000.p: demo1000.bas
	zmakebas -p -o $@ $<

clean:
	rm -f zxvoice1000.bin demo1000.bas loader1000.bas zxvoice1000.p
