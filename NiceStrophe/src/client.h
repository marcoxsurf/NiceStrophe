/*
 * client.h
 *
 *  Created on: 11/feb/2015
 *      Author: marco
 */

#ifndef CLIENT_H_
#define CLIENT_H_

static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer data);
static void cb_component_state_changed(NiceAgent *agent, guint stream_id,
		guint component_id, guint state, gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data);
static void * _thread_nice(void *data);
static void * _thread_xmpp(void *data);

#endif /* CLIENT_H_ */
