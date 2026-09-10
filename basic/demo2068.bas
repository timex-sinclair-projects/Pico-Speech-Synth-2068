   1 REM ZX Voice replica demo for the TS2068
   2 REM OUT 23 = allophone, IN 39 bit 7 = ready, OUT 55 = text/control
  10 PRINT "ZX Voice replica demo"
  20 REM --- allophones, the original way: "hello" ---
  30 LET a$="27,7,45,15,53,4"
  40 GO SUB 900
  50 REM --- wait until the chip is idle ---
  60 IF IN 39<128 THEN GO TO 60
  70 REM --- text mode ---
  80 LET t$="Hello. I am the Timex Sinclair 2068."
  90 GO SUB 950
 100 LET t$="This text was sent with OUT 55."
 110 GO SUB 950
 120 REM --- switch to the CTS256 engine and say it again ---
 130 OUT 55,209
 140 LET t$="Now with the CTS 256 rules."
 150 GO SUB 950
 160 OUT 55,208
 170 REM --- speed and pitch: 0xA0+n speed, 0x90+n pitch, n=0..15 -> 50..200% ---
 180 OUT 55,163: OUT 55,146
 190 LET t$="Slow and low."
 200 GO SUB 950
 210 OUT 55,173: OUT 55,157
 220 LET t$="Fast and high!"
 230 GO SUB 950
 240 OUT 55,165: OUT 55,149
 250 STOP
 900 REM send comma-separated allophone numbers in a$, waiting on IN 39
 910 FOR i=1 TO LEN a$
 920 LET n=0: LET j=i
 925 IF j<=LEN a$ AND a$(j)<>"," THEN LET n=n*10+CODE a$(j)-48: LET j=j+1: GO TO 925
 930 IF IN 39<128 THEN GO TO 930
 935 OUT 23,n
 940 LET i=j: NEXT i
 945 RETURN
 950 REM send t$ to the text port, then a carriage return
 960 FOR i=1 TO LEN t$: OUT 55,CODE t$(i): NEXT i
 970 OUT 55,13
 980 RETURN
