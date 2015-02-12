/*
 * nice.c
 *
 *  Created on: 03/feb/2015
 *      Author: marco
 */
#include "nice.h"


#define N_STATE 4

static const char *acceptable[N_STATE] = {"request", "accept"
		, "denied", "ended"};
static const nice_acceptable_t acceptable_[N_STATE] = {NICE_AC_REQUEST, NICE_AC_ACCEPTED
		, NICE_AC_DENIED, NICE_AC_END};
//static char *own_key64, *other_key64, *other_jid;


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
	_nice_status=NICE_ST_INIT;
	init_struct_nice();
	//Default 1
	//if the node is the receiving one, it will set to 0 and gathering reset
	controlling_state=1;
	def_stun_server = "stun.stunprotocol.org";
	def_stun_port = "3478";
	//risolvo indirizzo ip dell'url del server
	//TODO stun_addr può essere pensato come una lista di indirizzi IP
	stun_addr = hostname_to_ip(def_stun_server, def_stun_port);
	stun_port = strtoul(def_stun_port, &port_err, 10);
	if (*port_err != '\0'){
		io_error("Numero porta errato [%s].",def_stun_port);
		return ;
	}
	#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
	#endif
	//TODO da ripensare in versione thread
	gloop = g_main_loop_new(NULL, FALSE);

	exit_thread = FALSE;
	gthread_nice = g_thread_new("_thread_nice", &_thread_nice, NULL);
//	gthread_xmpp = g_thread_new("_thread_xmpp", &_thread_xmpp,NULL);
	//esegue il loop fino a che non viene chiamato g_main_loop_quit
	g_main_loop_run(gloop);
	exit_thread = TRUE;
	//aspetto che termini
	g_thread_join(gthread_nice);

//	_thread_nice(NULL);
	//Cambio stato se ottengo la chiave
}

static void * _thread_nice(void *data) {
	guint stream_id;
	gchar *sdp, *key64;
	//Create a new nice agent
	agent = nice_agent_new(g_main_loop_get_context(gloop)
			,NICE_COMPATIBILITY_RFC5245);
	if (agent == NULL) {
			io_error("Impossible to create the agent");
		}
	// Rimuovo UDP
	//g_object_set (G_OBJECT (agent), "ice-udp", FALSE,  NULL);
	// Set the STUN settings and controlling mode
	if (stun_addr) {
		g_object_set(agent, "stun-server", stun_addr, NULL);
		g_object_set(agent, "stun-server-port", stun_port, NULL);
	}
	//TODO Scoprire se essenziale controlling_state
	//g_object_set(agent, "controlling-mode", controlling_state, NULL);
	//connecting to signals
	g_signal_connect(agent, "candidate-gathering-done",
			G_CALLBACK(cb_candidate_gathering_done), NULL);
	g_signal_connect(agent, "component-state-changed",
			G_CALLBACK(cb_component_state_changed), NULL);
	//Create new stream
	stream_id = nice_agent_add_stream(agent, 1);
	if (stream_id == 0) {
		io_error("Impossible to add a new stream");
		return 1;
	}
	nice_agent_set_stream_name(agent, stream_id, "text");
	// Attach to the component to receive the data
	// Without this call, candidates cannot be gathered
	nice_agent_attach_recv(agent, stream_id, 1, NULL,cb_nice_recv, NULL);
	if (!nice_agent_gather_candidates(agent, stream_id)) {
		io_error("Failed to start candidate gathering");
	}
	io_notification("Waiting for candidate-gathering-done signal");
	g_mutex_lock(&gather_mutex);
	while (!exit_thread && !candidate_gathering_done) {
//	while (!candidate_gathering_done) {
		g_cond_wait(&gather_cond, &gather_mutex);
	}
	g_mutex_unlock(&gather_mutex);
//	if (exit_thread) {
//	goto end;
//	}
	//Fine acquisizione candidati. Stampa dei risultati
	sdp = nice_agent_generate_local_sdp(agent);
	io_notification("SDP generato dall'agente :%s", sdp);
	key64 = g_base64_encode((const guchar *) sdp, strlen(sdp));
	g_free(sdp);
	io_notification("Own key is %s", key64);
	nice_info->own_key64=key64;
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
				controlling_state=1;
				//save jid
				resp=1;
			}
			break;
		case NICE_ST_WAITING_FOR:
			if (action==NICE_AC_ACCEPTED){
				_nice_status=NICE_ST_ACCEPTED;
				controlling_state=0;
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
	clean_other_var();
}
void handleWaitingState(){

}
void handleDeniedState(){
	io_notification("Nice request has been rejected from %s",getOtherJid());
	clean_other_var();
	_nice_status=NICE_ST_INIT;
	io_notification("State changed to %s",getStatusName(_nice_status));
}
void handleAcceptedState(){
	io_notification("Nice request has been accepted from %s",getOtherJid());
	io_notification("This is the key: %s",getOtherKey());
	_nice_status=NICE_ST_BUSIED;
	io_notification("State changed to %s",getStatusName(_nice_status));
}
void handleBusyedState(){
	//todo iniziare qui la comunicazione
//	while(1){
//
//	}
//	io_notification("Busied");
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

char* getOwnKey(){
	return nice_info->own_key64;
}

char* getOtherKey(){
	return nice_info->other_key64;
}

char* getOtherJid(){
	return nice_info->other_jid;
}

char* setOtherKey(char* otherJ,char* otherK){
	//know for sure that key corrispond to jid
	if (strcmp(otherJ,getOtherJid())==0){
		nice_info->other_key64=strdup(otherK);
	}
	return nice_info->other_key64;
}
char* setOtherJid(const char* otherJ){
	if (_nice_status==NICE_ST_IDLE){
		nice_info->other_jid=strdup(otherJ);
	}
	return nice_info->other_jid;
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

const char* getSessionName (nice_session session){
	switch (session) {
		case NICE_SS_DISCONNECTED:
			return "disconnected";
		case NICE_SS_GATHERING:
			return "gathering";
		case NICE_SS_CONNECTING:
			return "connecting";
		case NICE_SS_CONNECTED:
			return "connected";
		case NICE_SS_READY:
			return "ready";
		case NICE_SS_FAILED:
			return "failed";
	}
	return "";
}

void init_struct_nice(){
	nice_info = malloc(sizeof(Nice_info));
	nice_info->own_key64=malloc(sizeof(char)*255);
	nice_info->other_key64=malloc(sizeof(char)*255);
	nice_info->other_jid=malloc(sizeof(char)*255);
}

int getControllingState(){
	return controlling_state;
}
int setControllingState(int newState){
	controlling_state=newState;
	return controlling_state;
}

static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer data) {
	io_notification("SIGNAL candidate gathering done");
	g_mutex_lock(&gather_mutex);
	candidate_gathering_done = TRUE;
	g_cond_signal(&gather_cond);
	g_mutex_unlock(&gather_mutex);
}

static void cb_component_state_changed(NiceAgent *agent, guint stream_id,
		guint component_id, guint state, gpointer data) {
	io_notification("SIGNAL: state changed %d %d %s[%d]"
			, stream_id, component_id,state_name[state], state);
	if (state == NICE_COMPONENT_STATE_READY) {
		g_mutex_lock(&negotiate_mutex);
		negotiation_done = TRUE;
		g_cond_signal(&negotiate_cond);
		g_mutex_unlock(&negotiate_mutex);
	}
//	else if (state == NICE_COMPONENT_STATE_FAILED) {
//		g_main_loop_quit(gloop);
//	}
}

static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data) {
//	if (len == 1 && buf[0] == '\0') {
//		g_main_loop_quit(gloop);
//	}
	printf("%.*s", len, buf);
//	fflush(stdout);
}
