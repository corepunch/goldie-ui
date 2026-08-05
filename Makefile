CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11 $(shell pkg-config --cflags sdl2)
LIBS    ?= $(shell pkg-config --libs sdl2) -framework OpenGL -lm

BINDIR  = build/bin
OBJDIR  = build/obj

SRCS = main.c math.c mesh.c scene.c shadow.c render.c
OBJS = $(addprefix $(OBJDIR)/, $(SRCS:.c=.o))

simplegl: $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LIBS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

screenshot: screenshot.c math.c mesh.c scene.c shadow.c render.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ screenshot.c math.c mesh.c scene.c shadow.c render.c $(LIBS)

run: simplegl
	./$(BINDIR)/simplegl sample_room.xml

test: simplegl tests
	./$(BINDIR)/tests

tests: tests.c math.c mesh.c shadow.c scene.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ tests.c math.c mesh.c shadow.c scene.c $(LIBS)

clean:
	rm -rf build

.PHONY: run test clean
