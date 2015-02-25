/* globals */
#include <glib.h>

extern const char* const prog_version;
extern const char* const prog_name;
extern int prog_running;

typedef struct _info {
	char *my_jid;
	char *my_key64;
	char *other_key64;
	char *other_jid;
} Nice_info;

GMainLoop *gloop;
Nice_info *nice_info;
//char *my_jid;
//char *my_key64;
//char *other_key64;
//char *other_jid;
