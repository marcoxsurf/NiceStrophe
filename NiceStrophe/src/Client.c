/*
 ============================================================================
 Name        : Client.c
 Author      : Marco Fantasia
 Version     :
 Copyright   : 
 Description : Codice client della connessione P2P con SDP (session description protocol)
 ============================================================================
 */
#include "header.h"

static GMainLoop *gloop;
static gchar *stun_addr = NULL;
static guint stun_port;
//static gboolean controlling;
static gboolean exit_thread, candidate_gathering_done, negotiation_done;
static GMutex gather_mutex, negotiate_mutex;
static GCond gather_cond, negotiate_cond;

static const gchar *state_name[] = { "disconnected", "gathering", "connecting",
		"connected", "ready", "failed" };
//static const char DEF_STUN_SERVER[] = "stun.l.google.com";
//static const char DEF_STUN_PORT[] = "19302";

static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer data);
static void cb_component_state_changed(NiceAgent *agent, guint stream_id,
		guint component_id, guint state, gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
		guint len, gchar *buf, gpointer data);
static void * _thread(void *data);

int main(int argc, char *argv[]) {
	GThread *gthread;
	char *def_stun_server = "stun.l.google.com";
	char *def_stun_port = "19302", *port_err = NULL;
	//risolvo indirizzo ip dell'url del server
	//TODO stun_addr può essere pensato come una lista di indirizzi IP
	stun_addr = hostname_to_ip(def_stun_server, def_stun_port);
	stun_port = strtoul(def_stun_port, &port_err, 10);
	if (*port_err != '\0'){
		fprintf(stderr, "Numero porta errato [%s].\n",def_stun_port);
		return 1;
	}
	//printf("%s resolved to %s" , def_stun_server , stun_addr);
	printf("Using stun server '[%s]:%u'\n", stun_addr, stun_port);

#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	gloop = g_main_loop_new(NULL, FALSE);
	//Eseguo il main_loop e il thread
	exit_thread = FALSE;
	gthread = g_thread_new("_thread", &_thread, NULL);
	//esegue il loop fino a che non viene chiamato g_main_loop_quit
	g_main_loop_run(gloop);
	exit_thread = TRUE;
	//aspetto che termini
	g_thread_join(gthread);
	g_main_loop_unref(gloop);
	return EXIT_SUCCESS;
}

static void * _thread(void *data) {
	NiceAgent *agent;
	GIOChannel* io_stdin;
	guint stream_id;
	gchar *line = NULL;
	gchar *sdp, *sdp64, *key64;
	io_stdin = g_io_channel_unix_new(fileno(stdin));
	g_io_channel_set_flags(io_stdin, G_IO_FLAG_NONBLOCK, NULL);
	//Creo l'agente nice
	agent = nice_agent_new(g_main_loop_get_context(gloop),
			NICE_COMPATIBILITY_RFC5245);
	if (agent == NULL) {
		printf("Impossibile creare l'agente");
	}
	// Rimuovo UDP
	//g_object_set (G_OBJECT (agent), "ice-udp", FALSE,  NULL);
	// Set the STUN settings and controlling mode
	if (stun_addr) {
		g_object_set(agent, "stun-server", stun_addr, NULL);
		g_object_set(agent, "stun-server-port", stun_port, NULL);
	}
	//g_object_set(agent, "controlling-mode", controlling, NULL);
	//Connessione ai segnali
	g_signal_connect(agent, "candidate-gathering-done",
			G_CALLBACK(cb_candidate_gathering_done), NULL);
	g_signal_connect(agent, "component-state-changed",
			G_CALLBACK(cb_component_state_changed), NULL);
	//Creo un nuovo stream con un componente
	stream_id = nice_agent_add_stream(agent, 1);
	if (stream_id == 0) {
		fprintf(stderr, "Impossibile aggiungere stream");
		return 1;
	}
	nice_agent_set_stream_name(agent, stream_id, "text");
	// Attach to the component to receive the data
	// Without this call, candidates cannot be gathered
	nice_agent_attach_recv(agent, stream_id, 1, g_main_loop_get_context(gloop),
			cb_nice_recv, NULL);
	//Inizio a ottenere i candidati locali
	if (!nice_agent_gather_candidates(agent, stream_id)) {
		fprintf(stderr, "Failed to start candidate gathering");
	}
	printf("In attesa del segnale di candidate-gathering-done.\n");
	g_mutex_lock(&gather_mutex);
	while (!exit_thread && !candidate_gathering_done) {
		g_cond_wait(&gather_cond, &gather_mutex);
	}
	g_mutex_unlock(&gather_mutex);
	if (exit_thread) {
		goto end;
	}
	//Fine acquisizione candidati. Stampa dei risultati
	sdp = nice_agent_generate_local_sdp(agent);
	printf("SDP generato dall'agente :\n%s\n\n", sdp);
	printf("Questa linea deve essere inviata al server di RV:\n");
	key64 = g_base64_encode((const guchar *) sdp, strlen(sdp));
	printf("\n%s\n", key64);
	//TODO Istanziare comunicazione con server esterno, invio key64
	//TODO Rimanere in attesa di comunicazioni in entrata
	g_free(sdp);

	end: g_object_unref(agent);
	g_io_channel_unref(io_stdin);
	g_main_loop_quit(gloop);

	return NULL;

}

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
	printf("SIGNAL: state changed %d %d %s[%d]\n", stream_id, component_id,
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
	if (len == 1 && buf[0] == '\0') {
		g_main_loop_quit(gloop);
	}
	printf("%.*s", len, buf);
	fflush(stdout);
}

