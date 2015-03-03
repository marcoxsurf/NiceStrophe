/*
 * nice.c
 *
 *  Created on: 03/feb/2015
 *      Author: marco
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "io.h"
#include "nice.h"
#include "main.h"
#include "tools.h"
#include "nice_action.h"

#define N_STATE 4
static const gchar *state_name[] = { "disconnected", "gathering", "connecting",
		"connected", "ready", "failed" };

static gchar *stun_addr = NULL;
static guint stun_port;
static gchar *port_err = NULL;

static const char *acceptable[N_STATE] =
		{ "request", "accept", "denied", "end" };
static const nice_acceptable_t acceptable_[N_STATE] = { NICE_AC_REQUEST,
		NICE_AC_ACCEPTED, NICE_AC_DENIED, NICE_AC_END };

//gchar *line = NULL;
gchar *sdp, *sdp64, *key64;

nice_acceptable_t action_is_correct(const char* const action) {
	int i;
	for (i = 0; i < N_STATE; i++) {
		if (strcmp(action, acceptable[i]) == 0) {
			return acceptable_[i];
		}
	}
	return NICE_AC_NO;
}

nice_acceptable_t get_status(const char* const action) {
	int i;
	for (i = 0; i < N_STATE; i++) {
		if (strcmp(action, acceptable[i]) == 0) {
			return acceptable_[i];
		}
	}
	//you'll never get here if you check action correctness in advance
	return NICE_AC_NO;
}

void nice_init() {
	init_struct_nice();
	char *def_stun_server = "stun.stunprotocol.org";
	char *def_stun_port = "3478";
	//solve ip address from url
	stun_addr = hostname_to_ip(def_stun_server, def_stun_port);
	stun_port = strtoul(def_stun_port, &port_err, 10);
	if (*port_err != '\0') {
		io_error("Wrong port number [%s].\n", def_stun_port);
		return;
	}
	io_notification("Using stun server '[%s]:%u'", stun_addr, stun_port);
	setControllingState(1);
//	candidate_gathering_done=FALSE;
	setNiceStatus(NICE_ST_IDLE);
}

void setting_connection() {
	NiceAgent *agent;
	agent= nice_agent_new(g_main_loop_get_context(gloop),
			NICE_COMPATIBILITY_RFC5245);
	//Build agent
	if (agent == NULL) {
		io_error("Failed to create the agent");
	}
	if (stun_addr) {
		g_object_set(agent, "stun-server", stun_addr, NULL);
		g_object_set(agent, "stun-server-port", stun_port, NULL);
	}
	io_notification("controlling is %d",controlling_state);
	g_object_set(agent, "controlling-mode", getControllingState(), NULL);
	//Connessione ai segnali
	g_signal_connect(agent, "candidate-gathering-done",
			G_CALLBACK(cb_candidate_gathering_done), NULL);
	g_signal_connect(agent, "new-selected-pair",
			G_CALLBACK(cb_new_selected_pair), NULL);
	g_signal_connect(agent, "component-state-changed",
			G_CALLBACK(cb_component_state_changed), NULL);
	//Create a new stream with one component
	stream_id = nice_agent_add_stream(agent, 1);
	if (stream_id == 0) {
		io_error("Failed to add stream");
		return ;
	}
	nice_agent_set_stream_name(agent, stream_id, "text");
	// Attach to the component to receive the data
	// Without this call, candidates cannot be gathered
	nice_agent_attach_recv(agent, stream_id, 1, g_main_loop_get_context(gloop),
			cb_nice_recv, NULL);
	//Start to gather local candidates
	if (!nice_agent_gather_candidates(agent, stream_id)) {
		io_error("Failed to start candidate gathering");
	}
	io_notification("Waiting for candidate-gathering-done signal.");
	g_mutex_lock(&gather_mutex);
	while (prog_running && !candidate_gathering_done) {
		g_cond_wait(&gather_cond, &gather_mutex);
	}
	g_mutex_unlock(&gather_mutex);
	if (!prog_running) {
		g_object_unref(agent);
		g_main_loop_unref(gloop);
		io_notification("Ending thread.");
		return;
	}
	//Fine acquisizione candidati. Stampa dei risultati nice_agent_parse_remote_sdp()
	sdp = nice_agent_generate_local_sdp(agent);
	io_notification("Generated SDP:\n%s", sdp);
	//	printf("Questa linea deve essere inviata al server di RV:\n");
	key64 = g_base64_encode((const guchar *) sdp, strlen(sdp));
	g_free(sdp);
	setMyKey(key64);
	g_free(key64);
	setAgent(agent);
	return;
}

void nice_deinit() {
	if (getAgent()!=NULL){
		g_object_unref(agent);
	}
	g_main_loop_quit(gloop);
}

/**
 * Function that handle the nice communication state
 */
int state_machine(nice_action_s s_r, nice_acceptable_t action) {
	int resp = 0;
	switch (getNiceStatus()) {
	case NICE_ST_INIT:
		//no action here
		//wait until communication is ready
		io_notification("Communication is not ready yet");
		io_notification("Your last action will be lost");
		break;
	case NICE_ST_IDLE:
		//can accept only SEND/RECV Request action
		if (action == NICE_AC_REQUEST) {
			setNiceStatus(NICE_ST_WAITING_FOR);
			resp = 1;
		}
		break;
	case NICE_ST_WAITING_FOR:
		if (action == NICE_AC_ACCEPTED) {
			setNiceStatus(NICE_ST_ACCEPTED);
			resp = 1;
		} else if (action == NICE_AC_DENIED) {
			setNiceStatus(NICE_ST_DENIED);
			resp = 1;
		}
		break;
	case NICE_ST_DENIED:
		break;
	case NICE_ST_ACCEPTED:
		break;
	case NICE_ST_BUSIED:
		if (action == NICE_AC_END) {
			resp = 1;
			setNiceStatus(NICE_ST_ENDED);
		}
		break;
	case NICE_ST_ENDED:
		break;
	default:
		break;
	}
	if (resp) {
		io_notification("State changed to %s", getStatusName(getNiceStatus()));
	}
	return resp;
}

void nice_nonblock_handle() {
	switch (getNiceStatus()) {
	case NICE_ST_INIT:
		nice_init();
		break;
	case NICE_ST_IDLE:
		handleIdleState();
		break;
	case NICE_ST_WAITING_FOR:
		handleWaitingState();
		break;
	case NICE_ST_DENIED:
		handleDeniedState();
		break;
	case NICE_ST_ACCEPTED:
		handleAcceptedState();
		break;
	case NICE_ST_BUSIED:
		handleBusyedState();
		break;
	case NICE_ST_ENDED:
		handleEndedState();
		break;
	default:
		break;
	}
}

void handleIdleState() {
	//clean_other_var();
}
void handleWaitingState() {

}
void handleDeniedState() {
	io_notification("Nice request has been rejected to %s",getOtherJid());
	setNiceStatus(NICE_ST_INIT);
	io_notification("State changed to %s", getStatusName(getNiceStatus()));
}
void handleAcceptedState() {
	io_notification("Nice request has been accepted from %s",getOtherJid());
	setNiceStatus(NICE_ST_BUSIED);
	io_notification("State changed to %s", getStatusName(getNiceStatus()));
}
void handleBusyedState() {
	//todo iniziare qui la comunicazione
	//1 - avvio un thread per la comunicazione tra i due
	//2^ prova - invio file
	//creo il thread e rimane in attesa finchè non termina
	GThread *niceThread;
	niceThread = g_thread_new("nice_action_thread", &text_thread,NULL);
//	g_thread_join(niceThread);
	g_mutex_lock(&thread_mutex);
	while(!thread_has_done){
		g_cond_wait(&thread_cond,&thread_mutex);
	}
	g_mutex_unlock(&thread_mutex);
//	io_notification("Busied");
//	sleep(10);
	//thread ended;
	//--> Ended in auto
	setNiceStatus(NICE_ST_ENDED);
}
void handleEndedState() {
	io_notification("Nice trasmission has ended.");
//	clean_other_var();
	//prog_running=0;
	setNiceStatus(NICE_ST_INIT);
	io_notification("State changed to %s", getStatusName(getNiceStatus()));
}

void clean_other_var() {
	setMyKey(NULL);
	setOtherJid(NULL);
	setOtherKey(NULL,NULL);
}

const char* getStatusName(nice_status_t status) {
	switch (status) {
	case NICE_ST_IDLE:
		return "Idle";
	case NICE_ST_WAITING_FOR:
		return "Waiting for";
	case NICE_ST_ACCEPTED:
		return "Accepted";
	case NICE_ST_DENIED:
		return "Denied";
	case NICE_ST_BUSIED:
		return "Busied";
	case NICE_ST_ENDED:
		return "Ended";
	case NICE_ST_INIT:
		return "Init";
	default:
		return "Init";
	}
	return "";
}

const char* getActionName(nice_acceptable_t action) {
	switch (action) {
	case NICE_AC_REQUEST:
		return "Request";
	case NICE_AC_ACCEPTED:
		return "Accept";
	case NICE_AC_DENIED:
		return "Denied";
	case NICE_AC_END:
		return "End";
	case NICE_AC_NO:
	default:
		return "No Action";
	}
	return "";
}

void negotiate(){
	gchar *sdp;
	gsize sdp_len;
	NiceAgent *agent;
	agent=getAgent();
	sdp = (gchar *) g_base64_decode(getOtherKey(),&sdp_len);
	if (!(sdp && nice_agent_parse_remote_sdp(agent,sdp)>0)){
		io_error("Failed to parse remote data");
		setNiceStatus(NICE_ST_ENDED);
		return;
	}
	io_notification("Waiting for state READY or FAILED signal...");
	g_mutex_lock(&negotiate_mutex);
	while(prog_running && !negotiation_done){
		g_cond_wait(&negotiate_cond,&negotiate_mutex);
	}
	g_mutex_unlock(&negotiate_mutex);
	if (!prog_running){
		g_object_unref(agent);
		io_notification("Ending thread.");
		g_main_loop_quit(gloop);
	}
	setAgent(agent);
	io_notification("Negotiate ended, Nodes are connected now :D");
}


void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer data) {
	io_notification("SIGNAL candidate gathering done");
	g_mutex_lock(&gather_mutex);
	candidate_gathering_done = TRUE;
	g_cond_signal(&gather_cond);
	g_mutex_unlock(&gather_mutex);
}

void cb_component_state_changed(NiceAgent *agent, guint stream_id,
		guint component_id, guint state, gpointer data) {
	io_notification("SIGNAL: state changed %d %d %s[%d]", stream_id,
			component_id, state_name[state], state);
	if (state == NICE_COMPONENT_STATE_READY) {
		g_mutex_lock(&negotiate_mutex);
		negotiation_done = TRUE;
		g_cond_signal(&negotiate_cond);
		g_mutex_unlock(&negotiate_mutex);
	} else if (state == NICE_COMPONENT_STATE_FAILED) {
		g_main_loop_quit(gloop);
	}
}

void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data) {
	if (len == 1 && buf[0] == '\0')
		g_main_loop_quit(gloop);
	io_notification("%.*s", len, buf);
//	fflush(stdout);
}
void cb_new_selected_pair(NiceAgent *agent, guint stream_id,
		guint component_id, gchar *lfoundation, gchar *rfoundation,
		gpointer data) {
	io_notification("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}
