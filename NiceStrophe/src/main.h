/* globals */
#include <glib.h>

extern const char* const prog_version;
extern const char* const prog_name;
extern int prog_running;



static GMainLoop *gloop;
static GMutex gather_mutex, negotiate_mutex;
static gboolean exit_thread;
