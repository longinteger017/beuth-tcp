
CFLAGS += -std=gnu99 -Wall -Wextra -g
CFLAGS += -lgcc

all: server.elf

clean:
	rm -f *.o *.elf

%.elf: %.c
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c %s
	$(CC) $(CFLAGS) -c $< -o $@

run: server.elf
	strace ./server.elf
