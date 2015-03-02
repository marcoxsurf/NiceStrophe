/* globals */
#include "nice.h"
#include "thread_handler.h"
#include <strophe.h>

extern const char* const prog_version;
extern const char* const prog_name;
extern int prog_running;

GMainLoop *gloop;
gboolean candidate_gathering_done, negotiation_done;
GMutex gather_mutex, negotiate_mutex;
GCond gather_cond, negotiate_cond;
NiceAgent *agent;
