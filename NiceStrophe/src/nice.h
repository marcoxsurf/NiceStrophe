/*
 * nice.h
 *
 *  Created on: 30/gen/2015
 *      Author: marco
 */

#ifndef NICE_H_
#define NICE_H_

#include <string.h>
#include <stdlib.h>
#include <string.h> //memset

#include <ctype.h>
#include <unistd.h>
#include <agent.h>

#include "tools.h"
#include "io.h"

typedef enum {
	NICE_ST_IDLE,
	NICE_ST_WAITING_FOR,
	NICE_ST_ACCEPTED,
	NICE_ST_DENIED,
	NICE_ST_BUSIED,
	NICE_ST_ENDED,
	NICE_ST_INIT
} nice_status_t;

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

typedef enum{
	NICE_SS_DISCONNECTED,
	NICE_SS_GATHERING,
	NICE_SS_CONNECTING,
	NICE_SS_CONNECTED,
	NICE_SS_READY,
	NICE_SS_FAILED
} nice_session;

typedef struct _info {
	char *own_key64;
	char *other_key64;
	char *other_jid;
} Nice_info;

static Nice_info* nice_info;
static nice_status_t _nice_status;
static int controlling_state;
static GMainLoop *gloop;
static gchar *stun_addr = NULL;
static guint stun_port;

gboolean exit_thread, candidate_gathering_done, negotiation_done;
GMutex gather_mutex, negotiate_mutex;
GCond gather_cond, negotiate_cond;
static const gchar *state_name[] = { "disconnected", "gathering", "connecting",
		"connected", "ready", "failed" };
static gchar *def_stun_server, *def_stun_port, *port_err = NULL;
GThread *gthread_nice;
NiceAgent *agent;

void nice_nonblock_handle();
void nice_init();
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
/**
 * Return session name based on enum
 */
const char* getSessionName (nice_session session);

void clean_other_var();

void init_struct_nice();

/**
 * return Own key 64
 */
char* getOwnKey();
char* getOtherKey();
char* getOtherJid();

static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer data);
static void cb_component_state_changed(NiceAgent *agent, guint stream_id,
		guint component_id, guint state, gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data);
static void * _thread_nice(void *data);

char* setOtherKey(char* otherJ,char* otherK);
char* setOtherJid(const char* otherJ);

int getControllingState();
int setControllingState(int newState);

#endif /* NICE_H_ */
