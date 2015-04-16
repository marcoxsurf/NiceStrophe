/*
 * nice.h
 *
 *  Created on: 30/gen/2015
 *      Author: marco
 */

#ifndef NICE_H_
#define NICE_H_
#include "thread_handler.h"
#include <agent.h>

#define CHUNK_SIZE 1024

//typedef enum {
//	NICE_ST_IDLE,
//	NICE_ST_SENDING_REQ,
//	NICE_ST_RECEIVING_REQ,
//	NICE_ST_WAITING_FOR,
//	NICE_ST_ACCEPTED,
//	NICE_ST_DENIED,
//	NICE_ST_BUSIED,
//	NICE_ST_ENDED,
//	NICE_ST_INIT
//} nice_status_t;

typedef enum{
	NICE_SEND_REQUEST,
	NICE_RECV_REQUEST,
	NICE_SEND_ACCEPTED,
	NICE_RECV_ACCEPTED,
	NICE_SEND_DENIED,
	NICE_RECV_DENIED,
	NICE_SEND_END,
	NICE_RECV_END
} nice_action_t;

typedef enum{
	NICE_AC_REQUEST,
	NICE_AC_ACCEPTED,
	NICE_AC_DENIED,
	NICE_AC_END,
	NICE_AC_NO
} nice_acceptable_t;

typedef enum{
	NICE_RECV,
	NICE_SEND
} nice_action_s;

NiceAgent *agent;

gchar *stun_addr;
guint stun_port;
gboolean reliable;
guint stream_id, component_id ;

gboolean candidate_gathering_done, negotiation_done;
GMutex gather_mutex, negotiate_mutex, write_mutex;
GCond gather_cond, negotiate_cond, write_cond;
gboolean reading_done, stream_open, stream_ready;

guint deadlock_timeout;

//Recent libnice improvements
GIOStream *io_stream;
GInputStream *input_stream;
GOutputStream *output_stream;

void nice_nonblock_handle();

void nice_init();
void nice_deinit();
void setting_connection();
/**
 * Add nice capabilities
 */
void net_nice(const char* const action,const char* const jid);
/**
 * Check if action is in the acceptable set
 */
nice_acceptable_t action_is_correct(const char* const action);
/**
 * Get the enum value of action
 */
nice_acceptable_t get_status(const char* const action);

/**
 * Handlers for nice status
 */
void handleIdleState();
void handleWaitingState();
void handleDeniedState();
void handleAcceptedState();
void handleBusyedState();
void handleEndedState();

int state_machine(nice_action_s s_r, nice_acceptable_t action);
/**
 * Return state name based on enum
 */
const char* getStatusName( nice_status_t status);
/**
 * Return action name based on enum
 */
const char* getActionName( nice_acceptable_t action);

void clean_other_var();

void negotiate();


void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer data);
void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
		gchar *lfoundation, gchar *rfoundation, gpointer data);
void cb_component_state_changed(NiceAgent *agent, guint _stream_id,
		guint component_id, guint state, gpointer user_data);
void cb_reliable_transport_writable(NiceAgent *agent, guint stream_id,
		guint component_id, gpointer user_data);
void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data);
gboolean cb_timer(gpointer pointer);

#endif /* NICE_H_ */
