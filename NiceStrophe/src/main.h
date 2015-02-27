/* globals */
#include <glib-2.0/glib.h>
#include "nice.h"

extern const char* const prog_version;
extern const char* const prog_name;
extern int prog_running;

GMainLoop *gloop;
Nice_info *nice_info;
nice_status_t _nice_status;
