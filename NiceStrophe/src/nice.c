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
#include <sys/stat.h>

#include "io.h"
#include "nice.h"
#include "main.h"
#include "tools.h"
#include "nice_action.h"



#define N_STATE 4
static const gchar *state_name[] = { "disconnected", "gathering", "connecting",
		"connected", "ready", "failed" };
static const char *acceptable[N_STATE] =
		{ "request", "accept", "denied", "end" };
static const nice_acceptable_t acceptable_[N_STATE] = { NICE_AC_REQUEST,
		NICE_AC_ACCEPTED, NICE_AC_DENIED, NICE_AC_END };

gchar *port_err = NULL;
gchar *sdp, *key64;
GThread *execThread;
struct stat sstr;


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

void reset_signal() {
	g_mutex_init(&gather_mutex);
	g_mutex_init(&negotiate_mutex);
	g_mutex_init(&write_mutex);
	g_cond_init(&gather_cond);
	g_cond_init(&negotiate_cond);
	g_cond_init(&write_cond);
	//set to false gathering signal
	g_mutex_lock(&gather_mutex);
	g_mutex_lock(&negotiate_mutex);
	g_mutex_lock(&write_mutex);
	candidate_gathering_done = FALSE;
	negotiation_done = FALSE;
	reading_done = FALSE;
	stream_open = FALSE;
	stream_ready = FALSE;
	g_mutex_unlock(&write_mutex);
	g_mutex_unlock(&negotiate_mutex);
	g_mutex_unlock(&gather_mutex);
}

void nice_init() {
	//reset other* and mykey
	init_struct_nice();
	reset_signal();

	char *def_stun_server = "stun.stunprotocol.org";
	char *def_stun_port = "3478";
	deadlock_timeout = 2000;
	file_send="test.txt";
	file_recv="testFile-received.txt";

	//solve ip address from url
	stun_addr = hostname_to_ip(def_stun_server, def_stun_port);
	stun_port = strtoul(def_stun_port, &port_err, 10);
	if (*port_err != '\0') {
		io_error("Wrong port number [%s].\n", def_stun_port);
		return;
	}
	io_notification("Using stun server '[%s]:%u'", stun_addr, stun_port);
	setNiceStatus(NICE_ST_IDLE);
}

void setting_connection() {
	component_id = NICE_COMPONENT_TYPE_RTP;
	agent = nice_agent_new_reliable(g_main_loop_get_context(gloop),
			NICE_COMPATIBILITY_RFC5245);
	reliable = 1;
	//Build agent
	if (agent == NULL) {
		io_error("Failed to create the agent");
	}
	if (stun_addr) {
		g_object_set(agent, "stun-server", stun_addr, NULL);
		g_object_set(agent, "stun-server-port", stun_port, NULL);
	}
	g_object_set(agent, "controlling-mode", getControllingState(), "upnp",
	FALSE, NULL);
	if (getControllingState()) {
		//check file dim
		stat(file_send, &sstr);
		sent_byte = 0;
		total_byte = sstr.st_size;
		fileSend = fopen(file_send, "rb");
	} else {
		fileRecv = fopen(file_recv, "ab");
		read_byte = 0;
	}
	//Connessione ai segnali
	g_signal_connect(agent, "candidate-gathering-done",
			G_CALLBACK(cb_candidate_gathering_done), NULL);
	g_signal_connect(agent, "new-selected-pair",
			G_CALLBACK(cb_new_selected_pair), NULL);
	g_signal_connect(agent, "component-state-changed",
			G_CALLBACK(cb_component_state_changed), NULL);
	if (reliable) {
		g_signal_connect(G_OBJECT (agent), "reliable-transport-writable",
				(GCallback ) cb_reliable_transport_writable, NULL);
	} else {
		stream_open = TRUE;
	}
	/* Add a timer to catch deadlocks. */
//	g_timeout_add_seconds(deadlock_timeout, cb_timer, NULL);
	//Create a new stream with one component
	stream_id = nice_agent_add_stream(agent, component_id);
	if (stream_id == 0) {
		io_error("Failed to add stream");
		return;
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
	if (reliable) {
		io_stream = G_IO_STREAM(
				nice_agent_get_io_stream(agent, stream_id, component_id));
		g_object_set_data(G_OBJECT(agent), "io-stream", io_stream);
	} else {
		io_stream = NULL;
	}

	//Fine acquisizione candidati. Stampa dei risultati nice_agent_parse_remote_sdp()
	sdp = nice_agent_generate_local_sdp(agent);
	io_notification("Generated SDP:\n%s", sdp);
	key64 = g_base64_encode((const guchar *) sdp, strlen(sdp));
	g_free(sdp);
	setMyKey(key64);
	g_free(key64);
	return;
}

void nice_deinit() {
	g_cond_clear(&gather_cond);
	g_mutex_clear(&gather_mutex);
	g_cond_clear(&negotiate_cond);
	g_mutex_clear(&negotiate_mutex);
	g_cond_clear(&write_cond);
	g_mutex_clear(&write_mutex);
	if (io_stream != NULL)
		g_object_unref(io_stream);
	if (stream_id != 0)
		nice_agent_remove_stream(agent, stream_id);
	if (agent != NULL)
		g_object_unref(agent);
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
	;
}
void handleWaitingState() {
	;
}
void handleDeniedState() {
	io_notification("Nice request has been rejected to %s", getOtherJid());
	setNiceStatus(NICE_ST_INIT);
	io_notification("State changed to %s", getStatusName(getNiceStatus()));
}
void handleAcceptedState() {
	io_notification("Nice request has been accepted from %s", getOtherJid());
	setNiceStatus(NICE_ST_BUSIED);
	io_notification("State changed to %s", getStatusName(getNiceStatus()));
}
void handleBusyedState() {

	//todo iniziare qui la comunicazione
	//creo il thread e rimane in attesa finchè non termina
//	niceThread = g_thread_new("nice_action_thread", &text_thread, NULL);
	execThread = spawn_thread("write/read thread", &file_thread, NULL);
	g_mutex_lock(&thread_mutex);
	while (!thread_has_done) {
		g_cond_wait(&thread_cond, &thread_mutex);
	}
	g_mutex_unlock(&thread_mutex);
	g_thread_join(execThread);
	setNiceStatus(NICE_ST_ENDED);
}
void handleEndedState() {
	io_notification("Nice trasmission has ended.");
	setNiceStatus(NICE_ST_INIT);
	io_notification("State changed to %s", getStatusName(getNiceStatus()));
}

void clean_other_var() {
	setMyKey(NULL);
	setOtherJid(NULL);
	setOtherKey(NULL, NULL);
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

void negotiate() {
	gchar *sdp;
	gsize sdp_len;
	sdp = (gchar *) g_base64_decode(getOtherKey(), &sdp_len);
	if (!(sdp && nice_agent_parse_remote_sdp(agent, sdp) > 0)) {
		io_error("Failed to parse remote data");
		setNiceStatus(NICE_ST_ENDED);
		return;
	}
	io_notification("Waiting for state READY or FAILED signal...");
	g_mutex_lock(&negotiate_mutex);
	while (prog_running && !negotiation_done) {
		g_cond_wait(&negotiate_cond, &negotiate_mutex);
	}
	g_mutex_unlock(&negotiate_mutex);
	if (!prog_running) {
		io_notification("Ending thread.");
		g_main_loop_quit(gloop);
		return;
	}
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

void cb_component_state_changed(NiceAgent *agent, guint _stream_id,
		guint component_id, guint state, gpointer user_data) {
	printf("SIGNAL: state changed %d %d %s[%d]\n", _stream_id, component_id,
			state_name[state], state);
	if (state == NICE_COMPONENT_STATE_READY) {
		NiceCandidate *local, *remote;
		// Get current selected candidate pair and print IP address used
		if (nice_agent_get_selected_pair(agent, _stream_id, component_id,
				&local, &remote)) {
			gchar ipaddr[INET6_ADDRSTRLEN];
			nice_address_to_string(&local->addr, ipaddr);
			printf("\nNegotiation complete: ([%s]:%d,", ipaddr,
					nice_address_get_port(&local->addr));
			nice_address_to_string(&remote->addr, ipaddr);
			printf(" [%s]:%d)\n", ipaddr, nice_address_get_port(&remote->addr));

			/* Signal stream state. */
			g_mutex_lock(&negotiate_mutex);
			negotiation_done = TRUE;
			g_cond_broadcast(&negotiate_cond);
			g_mutex_unlock(&negotiate_mutex);

			g_mutex_lock(&write_mutex);
			stream_ready = TRUE;
			g_cond_broadcast(&write_cond);
			g_mutex_unlock(&write_mutex);
		}
	} else if (state == NICE_COMPONENT_STATE_FAILED) {
		g_main_loop_quit(gloop);
	}
}

void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data) {
	//if received \0 quit the program
	if (len == 1 && buf[0] == '\0') {
		g_main_loop_quit(gloop);
		return;
	}
}

void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
		gchar *lfoundation, gchar *rfoundation, gpointer user_data) {
	printf("SIGNAL: selected pair %s %s\n", lfoundation, rfoundation);
}

void cb_reliable_transport_writable(NiceAgent *agent, guint stream_id,
		guint component_id, gpointer user_data) {
	if (!reliable) {
		fprintf(stderr, "Errore chiamato rtw_cb quando non reliable");
		return;
	}
	/* Signal writeability. */
	g_mutex_lock(&write_mutex);
	stream_open = TRUE;
	g_cond_broadcast(&write_cond);
	g_mutex_unlock(&write_mutex);

	io_stream = g_object_get_data(G_OBJECT(agent), "io-stream");
	if (io_stream == NULL) {
		fprintf(stderr, "Error null io_Stream");
		return;
	}
	if (getControllingState()) {
		output_stream = g_io_stream_get_output_stream(io_stream);
	} else {
		input_stream = g_io_stream_get_input_stream(io_stream);
	}

}

gboolean cb_timer(gpointer pointer) {
	fprintf(stderr, "test-thread:%s: %p\n", G_STRFUNC, pointer);
	/* note: should not be reached, abort */
	fprintf(stderr, "ERROR: test has got stuck, aborting...\n");
	exit(-1);
}
