/*
 * nice_action.c
 *
 *  Created on: 03/mar/2015
 *      Author: marco
 *      Description: Test library for nice test
 */
#include <string.h>

#include "nice_action.h"
#include "io.h"
#include "nice.h"

void * no_thread(void *data){
//	thread_has_done = FALSE;
	io_notification("Thread");
	g_mutex_lock(&thread_mutex);
	thread_has_done = TRUE;
	g_cond_signal(&thread_cond);
	g_mutex_unlock(&thread_mutex);
	return 0;
}

void * text_thread(void *data){
	NiceAgent *agent;
	gchar *line;
//	guint stream_id;
	negotiate();
	agent = getAgent();
	if (getControllingState()){
		line = strdup("Sono io il tuo capo");
	}else {
		line = strdup("Si padrone");
	}

	nice_agent_send(agent, stream_id,1,strlen(line),line);
	g_free(line);
	//send CTRL-D to terminate
//	nice_agent_send(agent, stream_id,1,1,"\0");

//	g_mutex_lock(&thread_mutex);
//	thread_has_done = TRUE;
//	g_cond_signal(&thread_cond);
//	g_mutex_unlock(&thread_mutex);
	return 0;
}
