CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11
OBJS = dialog.o

all: libcommdlg.a

libcommdlg.a: $(OBJS)
	$(AR) rcs $@ $^

dialog.o: dialog.c
	$(CC) $(CFLAGS) -c dialog.c -o dialog.o

clean:
	rm -f *.o *.a
