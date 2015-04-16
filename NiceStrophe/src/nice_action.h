/*
 * nice_action.h
 *
 *  Created on: 03/mar/2015
 *      Author: marco
 */

#ifndef NICE_ACTION_H_
#define NICE_ACTION_H_

#include <glib-object.h>
#include <gio/gio.h>
#include "main.h"

GCond thread_cond;
GMutex thread_mutex;
gboolean thread_has_done;

void * no_thread(void *data);
void * text_thread(void *data);
void * file_thread(void *data);

gboolean read_stream_cb (GObject *pollable_stream, gpointer _user_data);
void read_thread_gsource_cb (GInputStream *input_stream, gpointer user_data);
gboolean write_stream_cb (GObject *pollable_stream, gpointer _user_data);
void write_thread_gsource_cb(GOutputStream *output_stream, gpointer user_data);

#endif /* NICE_ACTION_H_ */
