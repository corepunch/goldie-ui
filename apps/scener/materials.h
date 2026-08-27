#ifndef MATERIALS_H
#define MATERIALS_H

#define TEX_SIZE 256
#define NUM_MATERIALS 8

void materials_init(unsigned int *textures);
void materials_free(unsigned int *textures);
unsigned int materials_create_white_texture(void);
int materials_index_for_name(const char *name);

#endif
