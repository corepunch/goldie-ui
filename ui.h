#ifndef __UI_H__
#define __UI_H__

// Main UI framework header - includes all UI subsystems

// User subsystem (window management)
#include "user/user.h"
#include "user/messages.h"
#include "user/text.h"
#include "user/draw.h"
#include "user/rect.h"
#include "user/theme.h"
#include "user/accel.h"
#include "user/image.h"
#include "user/database.h"

// Kernel subsystem (event management)
#include "kernel/kernel.h"

// Common controls subsystem
#include "commctl/commctl.h"

// Common dialogs subsystem
#include "commdlg/commdlg.h"

// Shared dialog button IDs.
enum {
    ID_OK = 1,
    ID_CANCEL = 2,
    ID_CONTROL_BASE = 1000,
    ID_COMMAND_BASE = 2000,
};

#ifndef STATIC_ARRAY
#define STATIC_ARRAY(a) (a), ARRAY_LEN(a)
#endif

#endif
