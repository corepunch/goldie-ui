#include "scene.h"
#include "math.h"

enum { DBG_NONE=0, DBG_WIRE_SHADOWVOL, DBG_SHOW_STENCIL, DBG_COUNT };

void render_frame(Scene *s, int w,int h, mat4 proj, mat4 view, int debugMode);
