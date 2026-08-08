CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11 $(shell pkg-config --cflags sdl2)
LIBS    ?= $(shell pkg-config --libs sdl2) -framework OpenGL -framework ApplicationServices -lm

BINDIR  = build/bin
OBJDIR  = build/obj

SRCS = main.c math.c mesh.c scene.c shadow.c render.c shader.c gizmo.c materials.c
OBJS = $(addprefix $(OBJDIR)/, $(SRCS:.c=.o))

simplegl: $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LIBS)

$(OBJDIR)/%.o: %.c simplegl.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

screenshot: screenshot.c math.c mesh.c scene.c shadow.c render.c shader.c gizmo.c materials.c simplegl.h | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ screenshot.c math.c mesh.c scene.c shadow.c render.c shader.c gizmo.c materials.c $(LIBS)

zpass: simplegl.h | $(BINDIR)
	$(CC) $(CFLAGS) -DUSE_ZPASS -o $(BINDIR)/simplegl-zpass $(SRCS) $(LIBS)

screenshot-zpass: simplegl.h | $(BINDIR)
	$(CC) $(CFLAGS) -DUSE_ZPASS -o $(BINDIR)/screenshot-zpass screenshot.c math.c mesh.c scene.c shadow.c render.c shader.c gizmo.c materials.c $(LIBS)

tests-zpass: simplegl.h | $(BINDIR)
	$(CC) $(CFLAGS) -DUSE_ZPASS -o $(BINDIR)/tests-zpass tests.c math.c mesh.c shadow.c scene.c gizmo.c materials.c $(LIBS)

test-zpass: zpass tests-zpass
	./$(BINDIR)/tests-zpass

run-zpass: zpass
	./$(BINDIR)/simplegl-zpass scenes/sample_room.blks

run: simplegl
	./$(BINDIR)/simplegl scenes/sample_room.blks

test: simplegl tests
	./$(BINDIR)/tests

tests: tests.c math.c mesh.c shadow.c scene.c gizmo.c materials.c simplegl.h | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ tests.c math.c mesh.c shadow.c scene.c gizmo.c materials.c $(LIBS)

render-scene: screenshot
	./$(BINDIR)/screenshot scenes/books/wondertown/workshop.blks -all

render-shrine: screenshot
	./$(BINDIR)/screenshot scenes/eclipse_shrine.blks -all

clean:
	rm -rf build

.PHONY: run test zpass screenshot-zpass tests-zpass test-zpass run-zpass clean render-scene render-shrine
