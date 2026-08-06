CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11
OBJS = dialog.o filepicker.o msgbox.o splash.o

all: libcommdlg.a

libcommdlg.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.a
