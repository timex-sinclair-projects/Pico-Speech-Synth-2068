; zxvoice1000.asm -- ZX Voice replica driver for the TS1000 / ZX81
;
; Sinclair BASIC on the ZX81 has no OUT or IN, so this routine lives in a
; REM statement at line 1 (code starts at 16514) and is called with USR.
; It reads the string variable A$ from the variables area, so nothing has
; to be POKEd:
;
;   LET A$=CHR$ 27+CHR$ 7+CHR$ 45+CHR$ 15+CHR$ 53+CHR$ 4
;   LET N=USR 16514          speak A$ as allophone numbers (OUT 23),
;                            waiting on IN 39 bit 7 before each one;
;                            stops at the end of A$ or at a byte >= 128
;                            (Rigter's terminator); N = bytes sent
;   LET A$="HELLO WORLD."
;   LET N=USR 16517          say A$ as text: ZX81 characters are converted
;                            to ASCII and sent to OUT 55, then a CR
;   LET A$=CHR$ 209
;   LET N=USR 16520          send the bytes of A$ unchanged to OUT 55
;                            (control bytes: 209 = CTS256 engine, 208 = NRL,
;                            128 = stop, 144+n pitch, 160+n speed)
;
;   POKE 16523,CODE "B"-32   use B$ instead of A$ (any single letter)
;
; The ready wait times out after about a second if no ZX Voice answers, so
; a missing board cannot hang the machine.  Assemble with sjasmplus.

        DEVICE ZXSPECTRUM48
        ORG $4082                   ; 16514

PORT_ALD    EQU $17                 ; OUT 23  allophone
PORT_STAT   EQU $27                 ; IN 39   bit 7 = ready
PORT_TXT    EQU $37                 ; OUT 55  text / control
VARS        EQU $4010

; ---- entry points -------------------------------------------------------
entry_speak:    JP do_speak         ; 16514
entry_say:      JP do_say           ; 16517
entry_raw:      JP do_raw           ; 16520
varname:        DB $46              ; 16523: 010LLLLL, A$ = $46

; ---- speak: A$ = allophone numbers ---------------------------------------
do_speak:
        CALL find
        JR NC,none
        LD D,B                      ; DE = remaining length
        LD E,C
        LD BC,0                     ; BC = count sent (USR result)
sp_loop:
        LD A,D
        OR E
        RET Z
        LD A,(HL)
        BIT 7,A
        RET NZ                      ; terminator byte
        CALL wait_ready
        RET NC                      ; timed out: give up
        LD A,(HL)
        OUT (PORT_ALD),A
        INC HL
        DEC DE
        INC BC
        JR sp_loop

; ---- say: A$ = text --------------------------------------------------------
do_say:
        CALL find
        JR NC,none
        LD D,B
        LD E,C
        LD BC,0
sy_loop:
        LD A,D
        OR E
        JR Z,sy_end
        LD A,(HL)
        CALL xlat
        OUT (PORT_TXT),A
        CALL pause
        INC HL
        DEC DE
        INC BC
        JR sy_loop
sy_end:
        LD A,13
        OUT (PORT_TXT),A
        RET

; ---- raw: A$ bytes unchanged to the text port ---------------------------
do_raw:
        CALL find
        JR NC,none
        LD D,B
        LD E,C
        LD BC,0
rw_loop:
        LD A,D
        OR E
        RET Z
        LD A,(HL)
        OUT (PORT_TXT),A
        CALL pause
        INC HL
        DEC DE
        INC BC
        JR rw_loop

none:   LD BC,0
        RET

; ---- wait_ready: poll IN 39 until bit 7 set; carry set = ready -----------
wait_ready:
        PUSH DE
        LD DE,0                     ; 65536 tries, about a second
wr_loop:
        IN A,(PORT_STAT)
        RLCA
        JR C,wr_ok                  ; bit 7 was set: ready (carry set)
        DEC DE
        LD A,D
        OR E
        JR NZ,wr_loop
        POP DE
        OR A                        ; carry clear: timeout
        RET
wr_ok:  POP DE
        RET

; ---- pause: about 100 us, lets the text buffer keep up -------------------
pause:  LD A,20
pa_loop:
        DEC A
        JR NZ,pa_loop
        RET

; ---- xlat: ZX81 character code in A -> ASCII in A ------------------------
xlat:   AND $7F                     ; inverse video -> normal
        CP $1C
        JR C,xl_table               ; $00..$1B: punctuation table
        CP $26
        JR C,xl_digit               ; $1C..$25: 0..9
        CP $40
        JR C,xl_letter              ; $26..$3F: A..Z
        LD A,' '                    ; tokens, newline, graphics: space
        RET
xl_digit:
        ADD A,$14
        RET
xl_letter:
        ADD A,$1B
        RET
xl_table:
        PUSH HL
        LD HL,table
        ADD A,L
        LD L,A
        JR NC,xl_t1
        INC H
xl_t1:  LD A,(HL)
        POP HL
        RET
table:  DB " "                      ; $00 space
        DB "          "             ; $01..$0A block graphics
        DB '"'                      ; $0B
        DB " "                      ; $0C pound sign
        DB "$:?()><=+-*/;,."        ; $0D..$1B

; ---- find: locate the string variable named at varname -------------------
;       returns carry set, HL -> first character, BC = length
find:   LD HL,(VARS)
fd_loop:
        LD A,(HL)
        CP $80
        JR Z,fd_none                ; end of variables
        LD C,A
        LD A,(varname)
        CP C
        JR Z,fd_found
        LD A,C
        AND $E0
        CP $60
        JR Z,fd_skip6               ; 011: number, one-letter name
        CP $E0
        JR Z,fd_skip18              ; 111: FOR control variable
        CP $A0
        JR Z,fd_long                ; 101: number, long name
        INC HL                      ; 010 100 110: length word follows
        LD C,(HL)
        INC HL
        LD B,(HL)
        INC HL
        ADD HL,BC
        JR fd_loop
fd_skip6:
        LD BC,6
        ADD HL,BC
        JR fd_loop
fd_skip18:
        LD BC,18
        ADD HL,BC
        JR fd_loop
fd_long:
        INC HL
        BIT 7,(HL)                  ; last letter of the name has bit 7 set
        JR Z,fd_long
        LD BC,6
        ADD HL,BC
        JR fd_loop
fd_found:
        INC HL
        LD C,(HL)
        INC HL
        LD B,(HL)
        INC HL
        SCF
        RET
fd_none:
        OR A
        RET

code_end:
        DISPLAY "driver size: ", /D, code_end - $4082, " bytes"
        SAVEBIN "zxvoice1000.bin", $4082, code_end - $4082
