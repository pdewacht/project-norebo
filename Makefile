CFLAGS = -g -O2 -Wall -Wextra -Wconversion -Wno-sign-conversion -Wno-unused-parameter -std=c99

norebo: Runtime/norebo.c Runtime/risc-cpu.c Runtime/risc-cpu.h
	$(CC) -o $@ Runtime/norebo.c Runtime/risc-cpu.c $(CFLAGS)

clean:
	rm -f norebo
