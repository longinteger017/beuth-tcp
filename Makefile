
CFLAGS += -std=gnu99 -Wall -Wextra -g
CFLAGS += -lgcc

all: server.elf

clean:
	rm -f *.o *.elf program

%.elf: %.c
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c %s
	$(CC) $(CFLAGS) -c $< -o $@

strace: server.elf
	strace ./server.elf

run:
	gcc -o program server.c
	./program
