#ifndef __FE_PROJECT_IO_H__
#define __FE_PROJECT_IO_H__

#include "../../ui.h"

// Forward declarations
struct form_doc_t;
struct form_project_t;

// Project XML I/O
bool fe_project_load(const char *path);
bool fe_project_save(const char *path);

#endif // __FE_PROJECT_IO_H__
