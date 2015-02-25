/*
 * nice.c
 *
 *  Created on: 03/feb/2015
 *      Author: marco
 */
#include "nice.h"
#include <string.h>
#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "main.h"
//#include <glib.h> incluso in agent.h

#include "tools.h"

#define N_STATE 4
static const gchar *state_name[] = {"disconnected", "gathering", "connecting",
                                    "connected", "ready", "failed"};
/**
 * Nice
 */
static gchar *stun_addr = NULL;
static guint stun_port;
static gchar *port_err = NULL;

static const char *acceptable[N_STATE] = {"request", "accept"
		, "denied", "ended"};
static const nice_acceptable_t acceptable_[N_STATE] = {NICE_AC_REQUEST, NICE_AC_ACCEPTED
		, NICE_AC_DENIED, NICE_AC_END};
//static char *own_key64, *other_key64, *other_jid;

//static nice_status_t _nice_status=NICE_ST_INIT;
static int controlling_state;
//Nice setting
static gboolean candidate_gathering_done, negotiation_done;
static GMutex gather_mutex, negotiate_mutex;
static GCond gather_cond, negotiate_cond;
NiceAgent *agent;
GIOChannel* io_stdin;

guint stream_id;
gchar *line = NULL;
gchar *sdp, *sdp64, *key64;

nice_acceptable_t action_is_correct(const char* const action){
	int i;
	for(i=0;i<N_STATE;i++){
		if (strcmp(action,acceptable[i])==0){
			return acceptable_[i];
		}
	}
	return NICE_AC_NO;
}

nice_acceptable_t get_status(const char* const action){
	int i;
	for (i=0;i<N_STATE;i++){
		if (strcmp(action,acceptable[i])==0){
			return acceptable_[i];
		}
	}
	//you'll never get here if you check action correctness in advance
	return NICE_AC_NO;
}

void nice_init(){
	//creare qui la connessione con il server stun
	//ottenere la chiave64
	//Cambio stato se ottengo la chiave
	init_struct_nice();
	char *def_stun_server = "stun.stunprotocol.org";
	char *def_stun_port = "3478";
	controlling_state=1;
	//risolvo indirizzo ip dell'url del server
	//TODO stun_addr può essere pensato come una lista di indirizzi IP
	stun_addr = hostname_to_ip(def_stun_server, def_stun_port);
	stun_port = strtoul(def_stun_port, &port_err, 10);
	if (*port_err != '\0'){
		io_error("Wrong port number [%s].\n",def_stun_port);
		return;
	}
	io_notification("Using stun server '[%s]:%u'\n", stun_addr, stun_port);
	agent = nice_agent_new(g_main_loop_get_context(gloop),NICE_COMPATIBILITY_RFC5245);
	//Build agent
	if (agent == NULL) {
		io_error("Impossible to create the agent");
	}
	if (stun_addr) {
		g_object_set(agent, "stun-server", stun_addr, NULL);
		g_object_set(agent, "stun-server-port", stun_port, NULL);
	}
	g_object_set(agent, "controlling-mode", controlling_state, NULL);
	//Connessione ai segnali
	g_signal_connect(agent,"candidate-gathering-done",G_CALLBACK(cb_candidate_gathering_done), NULL);
	g_signal_connect(agent,"new-selected-pair",       G_CALLBACK(cb_new_selected_pair), NULL);
	g_signal_connect(agent,"component-state-changed" ,G_CALLBACK(cb_component_state_changed), NULL);
	//Create a new stream with one component
	stream_id = nice_agent_add_stream(agent, 1);
	if (stream_id == 0) {
		io_error("Failed to add stream");
		return ;
	}
	nice_agent_set_stream_name(agent, stream_id, "text");
	// Attach to the component to receive the data
	// Without this call, candidates cannot be gathered
	nice_agent_attach_recv(agent, stream_id, 1, g_main_loop_get_context(gloop), cb_nice_recv, NULL);
	//Start to gather local candidates
	if (!nice_agent_gather_candidates(agent, stream_id)) {
		io_error("Failed to start candidate gathering");
	}
	io_notification("waiting for candidate-gathering-done signal.\n");
	g_mutex_lock(&gather_mutex);
	while (!prog_running && !candidate_gathering_done) {
		g_cond_wait(&gather_cond, &gather_mutex);
	}
	g_mutex_unlock(&gather_mutex);
	if (!prog_running){
		g_object_unref(agent);
		g_main_loop_unref(gloop);
		io_notification("Ending thread.");
		return;
	}
	//Fine acquisizione candidati. Stampa dei risultati
	sdp = nice_agent_generate_local_sdp(agent);
	io_notification("Generated SDP:\n%s\n\n", sdp);
	//	printf("Questa linea deve essere inviata al server di RV:\n");
	key64 = g_base64_encode((const guchar *) sdp, strlen(sdp));
	io_notification("\n%s\n", key64);
	g_free(sdp);
	nice_info->my_key64 = strdup(key64);
//	my_key64 = strdup(key64);
//	setMyKey(key64);
	g_free(key64);
	_nice_status = NICE_ST_IDLE;
}
/**
 * Function that handle the nice communication state
 */
int state_machine (nice_action_s s_r, nice_acceptable_t action){
	int resp=0;
	switch (_nice_status) {
		case NICE_ST_INIT:
			//no action here
			//wait until communication is ready
			io_notification("Communication is not ready yet");
			io_notification("Your last action will be lost");
			break;
		case NICE_ST_IDLE:
			//can accept only SEND/RECV Request action
			if (action==NICE_AC_REQUEST){
				_nice_status=NICE_ST_WAITING_FOR;
				if (s_r==NICE_RECV){
					controlling_state=0;
				}
				resp=1;
			}
			break;
		case NICE_ST_WAITING_FOR:
			if (action==NICE_AC_ACCEPTED){
				_nice_status=NICE_ST_ACCEPTED;
				resp=1;
			}else if (action==NICE_AC_DENIED){
				_nice_status=NICE_ST_DENIED;
				resp=1;
			}
			break;
		case NICE_ST_DENIED:
			break;
		case NICE_ST_ACCEPTED:
			break;
		case NICE_ST_BUSIED:
			if (action==NICE_AC_END){
				resp=1;
				_nice_status=NICE_ST_ENDED;
			}
			break;
		case NICE_ST_ENDED:
			break;
		default:
			break;
	}
	if (resp){
		io_notification("State changed to %s",getStatusName(_nice_status));
	}
	return resp;
}

void nice_nonblock_handle(){
	switch (_nice_status) {
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

void handleIdleState(){
	//verify if vars are clean
	//clean_other_var();
}
void handleWaitingState(){

}
void handleDeniedState(){
	io_notification("Nice request has been rejected from %s",nice_info->other_jid);//other_jid);//getOtherJid());
	clean_other_var();
	_nice_status=NICE_ST_INIT;
	io_notification("State changed to %s",getStatusName(_nice_status));
}
void handleAcceptedState(){
	io_notification("Nice request has been accepted from %s",nice_info->other_jid);//other_jid);//getOtherJid());
	io_notification("This is the key: %s",nice_info->other_key64);//getOtherKey());
	_nice_status=NICE_ST_BUSIED;
	io_notification("State changed to %s",getStatusName(_nice_status));
}
void handleBusyedState(){
	//todo iniziare qui la comunicazione
	while(1){
		io_notification("Busied");
		sleep(10);
	}

}
void handleEndedState(){
	io_notification("Nice trasmission has ended.");
	_nice_status=NICE_ST_INIT;
	io_notification("State changed to %s",getStatusName(_nice_status));
	clean_other_var();
}

void clean_other_var(){
	nice_info->other_jid=NULL;
	nice_info->other_key64=NULL;
}

char* getMyJid(){
	return nice_info->my_jid;
}

char* getMyKey(){
	return nice_info->my_key64;
//	return my_key64;
}

char* getOtherKey(){
	return nice_info->other_key64;
//	return other_key64;
}

char* getOtherJid(){
	return nice_info->other_jid;
//	return other_jid;
}

char* setMyJid(char* jid){
	nice_info->my_jid=strdup(jid);
	return nice_info->my_jid;
}
char* setMyKey(char* key64){
	nice_info->my_key64=strdup(key64);
	return nice_info->my_key64;
}

char* setOtherKey(char* otherJ,char* otherK){
	//know for sure that key corrispond to jid
	if (strcmp(otherJ,getOtherJid())==0){
		nice_info->other_key64=strdup(otherK);
//		other_key64=strdup(otherK);
		io_notification("Change other key to %s",nice_info->other_key64);
//		io_notification("Change other key to %s",other_key64);
	}
	return nice_info->other_key64;
//	return other_key64;
}
char* setOtherJid(const char* otherJ){
	if (_nice_status==NICE_ST_IDLE){
		nice_info->other_jid=strdup(otherJ);
//		other_jid=strdup(otherJ);
	}
	return nice_info->other_jid;
//	return other_jid;
}

const char* getStatusName( nice_status_t status){
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
			return "Idle";
		case NICE_ST_INIT:
			return "Init";
		default:
			return "Init";
	}
	return "";
}

const char* getActionName( nice_acceptable_t action){
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

void init_struct_nice(){
	nice_info = malloc(sizeof(Nice_info));
//	nice_info->my_jid=malloc(sizeof(char)*255);
//	nice_info->my_key64=malloc(sizeof(char)*1255);
//	nice_info->other_key64=malloc(sizeof(char)*255);
//	nice_info->other_jid=malloc(sizeof(char)*1255);
//	my_jid=strdup("");
//	my_key64=strdup("");
//	other_key64=strdup("");
//	other_jid=strdup("");
}

int getControllingState(){
	return controlling_state;
}
int setControllingState(int newState){
	controlling_state=newState;
	return controlling_state;
}

/**
 * Funzioni Nice
 */
static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer data) {
	g_debug("SIGNAL candidate gathering done\n");
	g_mutex_lock(&gather_mutex);
	candidate_gathering_done = TRUE;
	g_cond_signal(&gather_cond);
	g_mutex_unlock(&gather_mutex);
}

static void cb_component_state_changed(NiceAgent *agent, guint stream_id,
		guint component_id, guint state, gpointer data) {
	io_notification("SIGNAL: state changed %d %d %s[%d]\n", stream_id, component_id,
			state_name[state], state);
	if (state == NICE_COMPONENT_STATE_READY) {
		g_mutex_lock(&negotiate_mutex);
		negotiation_done = TRUE;
		g_cond_signal(&negotiate_cond);
		g_mutex_unlock(&negotiate_mutex);
	} else if (state == NICE_COMPONENT_STATE_FAILED) {
		g_main_loop_quit(gloop);
	}
}

static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data) {
	if (len == 1 && buf[0] == '\0')
		g_main_loop_quit(gloop);
	//TODO controllare ricezioni messaggi nice
	printf("%.*s", len, buf);
//	fflush(stdout);
}
static void cb_new_selected_pair(NiceAgent *agent, guint stream_id,
    guint component_id, gchar *lfoundation,gchar *rfoundation, gpointer data){
	io_notification("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}
