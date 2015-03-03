/*
 * nice_action.h
 *
 *  Created on: 03/mar/2015
 *      Author: marco
 */

#ifndef NICE_ACTION_H_
#define NICE_ACTION_H_

#include <glib-2.0/glib.h>

GCond thread_cond;
GMutex thread_mutex;
gboolean thread_has_done;

void * no_thread(void *data);
void * text_thread(void *data);



#endif /* NICE_ACTION_H_ */
