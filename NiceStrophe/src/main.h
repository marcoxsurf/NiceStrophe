/* globals */
#include "nice.h"
#include "thread_handler.h"
#include <strophe.h>

extern const char* const prog_version;
extern const char* const prog_name;

GMainLoop *gloop;

FILE *fileSend, *fileRecv;
gchar *file_send, *file_recv;
size_t total_byte, sent_byte, read_byte;

gboolean prog_running;
