CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter
ROM     ?= ../rom/sp0256-al2.bin

harness: harness.c ../core/sp0256.c ../core/sp0256.h
	$(CC) $(CFLAGS) -o $@ harness.c ../core/sp0256.c

harness_trace: harness.c ../core/sp0256.c ../core/sp0256.h
	$(CC) $(CFLAGS) -DSP0256_TRACE -o $@ harness.c ../core/sp0256.c

TTS_SRC = ../tts/tts.c ../tts/rules.c ../tts/rules_nrl.c ../tts/rules_cts.c ../tts/phon2allo.c ../tts/nrl_numbers.c ../tts/lexicon.c

say: say.c wav.h $(TTS_SRC) ../core/sp0256.c ../core/sp0256_names.h ../tts/*.h
	$(CC) $(CFLAGS) -I../core -I../tts -o $@ say.c $(TTS_SRC) ../core/sp0256.c

.PHONY: all durations wavs sampler hello clean
all: harness say

durations: harness
	./harness -r $(ROM) -d

wavs: harness
	mkdir -p out && ./harness -r $(ROM) -a out

sampler: harness
	./harness -r $(ROM) -S sampler.wav

hello: harness
	./harness -r $(ROM) -s "HH1 EH LL AX OW PA5 WW ER1 LL DD1" -o hello.wav

clean:
	rm -rf harness harness_trace say out *.wav
