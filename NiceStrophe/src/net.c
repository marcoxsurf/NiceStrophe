#include "net.h"
#include <strophe.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "io.h"
#include "main.h"
#include "util.h"
#include "msg.h"
#include "roster.h"
#include "nice.h"

static xmpp_ctx_t *ctx = NULL;
static xmpp_conn_t *conn = NULL;

int handle_version(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza,
		void * const userdata) {
	xmpp_stanza_t *reply, *query, *name, *version, *text;
	char *ns;
	xmpp_ctx_t *ctx = (xmpp_ctx_t*) userdata;
	io_printfln("Received version request from %s",
			xmpp_stanza_get_attribute(stanza, "from"));

	reply = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(reply, "iq");
	xmpp_stanza_set_type(reply, "result");
	xmpp_stanza_set_id(reply, xmpp_stanza_get_id(stanza));
	xmpp_stanza_set_attribute(reply, "to",
			xmpp_stanza_get_attribute(stanza, "from"));

	query = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(query, "query");
	ns = xmpp_stanza_get_ns(xmpp_stanza_get_children(stanza));
	if (ns) {
		xmpp_stanza_set_ns(query, ns);
	}

	name = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(name, "name");
	xmpp_stanza_add_child(query, name);

	text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(text, prog_name);
	xmpp_stanza_add_child(name, text);

	version = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(version, "version");
	xmpp_stanza_add_child(query, version);

	text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(text, prog_version);
	xmpp_stanza_add_child(version, version);

	xmpp_stanza_add_child(reply, query);

	xmpp_send(conn, reply);
	xmpp_stanza_release(reply);
	return 1;
}

int handle_roster_reply(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza,
		void * const userdata) {
	xmpp_stanza_t *query, *item;
	char *type, *name;

	type = xmpp_stanza_get_type(stanza);
	if (strcmp(type, "error") == 0) {
		io_error("Roster IQ query have failed.");
		return 0;
	}

	query = xmpp_stanza_get_child_by_name(stanza, "query");
	for (item = xmpp_stanza_get_children(query); item; item = xmpp_stanza_get_next(item)) {
		if ((!(name = xmpp_stanza_get_attribute(item, "name")))) {
			name = "";
		}
		roster_item_received(
				xmpp_stanza_get_attribute(item, "jid")
				, name
				, roster_subscription_to_enum(xmpp_stanza_get_attribute(item, "subscription")));
	}
	return 0;
}

int handle_message(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza,
		void * const userdata) {
	char* from;
	char *intext;
	msg_queue_t* q;

	if (!xmpp_stanza_get_child_by_name(stanza, "body"))
		return 1;
	if (!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "error"))
		return 1;
	//nice message, do not handle this
	if (!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "nice"))
			return 1;
	intext = xmpp_stanza_get_text(
			xmpp_stanza_get_child_by_name(stanza, "body"));
	from = xmpp_jid_bare(ctx, xmpp_stanza_get_attribute(stanza, "from"));
	q = msg_queue_get(from);
	xmpp_free(ctx, from);
	msg_queue_write(q, intext);
	xmpp_free(ctx, intext);
	return 1;
}
int handle_presence(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza,
		void * const userdata) {
	
	return 1;
}
int handle_nice(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza,
		void * const userdata) {
	char* from;
	char *intext;
	char *action, *key64;
	xmpp_stanza_t *body, *nice, *_action, *_key64;
	nice_acceptable_t act;
//	msg_queue_t* q;
	if (!xmpp_stanza_get_child_by_name(stanza, "body"))
		return 1;
	if (!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "error"))
		return 1;
	body = xmpp_stanza_get_child_by_name(stanza, "body");
	nice = xmpp_stanza_get_child_by_name(body, "nice");
	if (nice == NULL) {
		io_error("Nice tag needed");
		return 1;
	}
	_action = xmpp_stanza_get_child_by_name(nice, "action");
	if (_action == NULL) {
		io_error("Action tag needed");
		return 1;
	}
	action = xmpp_stanza_get_type(_action);
	_key64 = xmpp_stanza_get_child_by_name(nice, "key64");
	if (_key64 == NULL) {
		io_error("Key64 tag needed");
		return 1;
	}
	//Create a bare JID from a JID
	from = xmpp_jid_bare(ctx, xmpp_stanza_get_attribute(stanza, "from"));
	if (strcmp(from,getOtherJid())==0){
		io_error("Received action from wrong jid, %s",from);
	}
	key64=xmpp_stanza_get_attribute(_key64,"value");
	intext = xmpp_stanza_get_text(body);
	//need to print nice message meaning
	if ((act=action_is_correct(action))==NICE_AC_NO){
		io_error("The action received is not correct");
		return 1;
	}
	io_notification("RECV: %s from jid: %s",getActionName(act),from);
	switch (act) {
		case NICE_AC_REQUEST:
			//received a request action
			//saving jid&key
			setOtherJid(from);
			setOtherKey(from,key64);
			break;
		case NICE_AC_ACCEPTED:
			//received an accepted action
			//saving key, know that if jid != problem
			if (!strcmp(setOtherKey(from,key64),key64)==0){
				//key64 is different from otherkey.
				//it means that received an accept request from an unwanted jid
				return 1;
			}
			break;
		case NICE_AC_DENIED:
			break;
		case NICE_AC_END:
			break;
		case NICE_AC_NO:
			return 1;
			break;
	}
	if(!state_machine(NICE_RECV,act)){
		io_error("The selected action is not valid right now!");
	}
//	q = msg_queue_get(from);
	xmpp_free(ctx, from);
//	msg_queue_write(q, intext);
	xmpp_free(ctx, intext);
	return 1;
}

void net_send(const char* const str) {
	msg_queue_t* q;
	const char* jid;

	xmpp_stanza_t *msg, *body, *text;
	if (!conn) {
		io_error("Not connected.");
		return;
	}
	if (!(q = msg_active_queue_get())) {
		io_error("No recipient selected.");
		return;
	}

	jid = msg_queue_jid(q);

	msg = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(msg, "message");
	xmpp_stanza_set_type(msg, "chat");
	xmpp_stanza_set_attribute(msg, "to", jid);

	body = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(body, "body");

	text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(text, str);
	xmpp_stanza_add_child(body, text);
	xmpp_stanza_add_child(msg, body);

	xmpp_send(conn, msg);
	xmpp_stanza_release(msg);

	io_message(xmpp_conn_get_bound_jid(conn), str);
}

void net_nice(const char* const action, const char* const jid) {
//	msg_queue_t* q;

	//va inserita la chiave64 generata dalla procedura NICE
	char* key64 = getOwnKey();
//	io_notification("key is %s",key64);
	char *ans;
	xmpp_stanza_t *msg, *body, *text, *_nice, *_action, *_key64;
	nice_acceptable_t act=NICE_AC_NO;

	if (!conn) {
		io_error("Not connected.");
		return;
	}
	if (!jid){
		io_error("No recipient selected");
	}
//	if (!(q = msg_active_queue_get())) {
//		io_error("No recipient selected.");
//		return;
//	}
	/**
	 * Controllo se azione scelta è tra le disponibili
	 * request, accept, denied
	 */
	if ((act=action_is_correct(action))==NICE_AC_NO){
		io_error("The action you selected is not correct");
		io_error("You can choose from: \"request\", \"accept\""
				", \"denied\", \"end\" ");
		return;
	}
	msg = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(msg, "message");
	xmpp_stanza_set_type(msg,"nice");
	xmpp_stanza_set_attribute(msg, "to", jid);

	body = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(body, "body");

	_nice = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(_nice, "nice");

	_action=xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(_action, "action");
	xmpp_stanza_set_type(_action, action);
	_key64=xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(_key64, "key64");
	switch (act) {
		case NICE_AC_REQUEST:
			xmpp_stanza_set_attribute(_key64, "value", key64);
			setOtherJid(jid);
			break;
		case NICE_AC_ACCEPTED:
			xmpp_stanza_set_attribute(_key64, "value", key64);
			break;
		case NICE_AC_DENIED:
			xmpp_stanza_set_attribute(_key64, "value", "");
			break;
		case NICE_AC_END:
			xmpp_stanza_set_attribute(_key64, "value", "");
			break;
		case NICE_AC_NO:
			break;
	}
	ans=malloc(sizeof(strlen(action)+strlen(key64)+1+1)); //aggiungo uno spazio + \0
	strcpy(ans,action);
	strcat(ans," ");
	strcat(ans,key64);

	xmpp_stanza_add_child(_nice, _action);
	xmpp_stanza_add_child(_nice, _key64);
	xmpp_stanza_add_child(body, _nice);

	text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(text, ans);
	xmpp_stanza_add_child(body, text);

	xmpp_stanza_add_child(msg, body);
	//set nice session status
	//check if status change correct
	if(!state_machine(NICE_SEND,act)){
		io_error("The selected action is not valid right now!");
		return;
	}
	xmpp_send(conn, msg);
	xmpp_stanza_release(msg);
	io_printfln("SENT nice: %s to %s having key: %s",action,jid,key64);
//	io_message(xmpp_conn_get_bound_jid(conn), ans);
}

static void connected(xmpp_conn_t* const conn) {
	const char* full_jid = xmpp_conn_get_bound_jid(conn);

	roster_init();

	io_notification("Connected as `%s'.", full_jid);
	io_prompt_set(NET_ST_ONLINE);
}

static void net_query_roster() {
	xmpp_stanza_t* iq;
	xmpp_stanza_t* query;
	iq = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(iq, "iq");
	xmpp_stanza_set_type(iq, "get");
	xmpp_stanza_set_id(iq, "roster1");

	query = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(query, "query");
	xmpp_stanza_set_ns(query, XMPP_NS_ROSTER);

	xmpp_stanza_add_child(iq, query);

	/* we can release the stanza since it belongs to iq now */
	xmpp_stanza_release(query);

	/* set up reply handler */
	xmpp_id_handler_add(conn, handle_roster_reply, "roster1", ctx);

	/* send out the stanza */
	xmpp_send(conn, iq);

	/* release the stanza */
	xmpp_stanza_release(iq);
}

static void conn_handler(xmpp_conn_t * const _conn,
		const xmpp_conn_event_t status, const int error,
		xmpp_stream_error_t * const stream_error, void * const userdata) {
	xmpp_stanza_t* pres;
	char* text;

	switch (status) {
	case XMPP_CONN_CONNECT:
		connected(_conn);

		xmpp_handler_add(_conn, handle_version, "jabber:iq:version", "iq", NULL,ctx);
		xmpp_handler_add(_conn, handle_nice, NULL, "message", "nice",ctx);
		xmpp_handler_add(_conn, handle_message, NULL, "message", NULL, ctx);
		xmpp_handler_add(_conn, handle_roster_reply,XMPP_NS_ROSTER, "iq",NULL, ctx);
		xmpp_handler_add(_conn, handle_presence, NULL, "presence", NULL, ctx );

		//todo aggiungere controllo su presenza
		//todo aggiungere controllo roster
		pres = xmpp_stanza_new(ctx);
		xmpp_stanza_set_name(pres, "presence");
		xmpp_send(_conn, pres);
		xmpp_stanza_release(pres);

		net_query_roster();
		break;
	case XMPP_CONN_DISCONNECT:
		if (stream_error) {
			text = stream_error->text ? stream_error->text : "no error text";
			io_notification("Connection disconnected. (type: %d, text: %s)",
					stream_error->type, text);
		} else {
			io_notification("Disconnected.");
		}
		conn = 0;
		break;
	case XMPP_CONN_FAIL:
		if (stream_error) {
			text = stream_error->text ? stream_error->text : "no error text";
			io_notification("Connection failed. (type: %d, text: %s)",
					stream_error->type, text);
		} else {
			io_notification("Connection failed.");
		}
		break;
	default:
		assert(0);
		abort();
	}

}

void log_handler(void * const userdata, const xmpp_log_level_t level,
		const char * const area, const char * const msg) {
	io_debug("%s", msg);
}

xmpp_log_t logger;

void net_init(char* jid, char* passwd) {
	xmpp_initialize();

	/* create a context */
	logger.handler = log_handler;
	ctx = xmpp_ctx_new(NULL, &logger);

}

void net_disconnect() {
	if (conn != NULL) {
		roster_deinit();
		xmpp_conn_release(conn);
		conn = NULL;
		io_notification("Disconnected.");
	}
	io_prompt_set(NET_ST_DISCONNECTED);
}

void net_connect(const char* const jid, const char* const pass) {
	net_disconnect();
	io_debug("Connecting as `%s'.", jid);

	assert(!conn);
	conn = xmpp_conn_new(ctx);

	xmpp_conn_set_jid(conn, jid);
	xmpp_conn_set_pass(conn, pass);

	xmpp_connect_client(conn, NULL, 0, conn_handler, ctx);
}

void net_deinit() {
	net_disconnect();
	xmpp_ctx_free(ctx);
	xmpp_shutdown();
}

void net_nonblock_handle() {
	if (conn) {
		xmpp_run_once(ctx, 1);
	}
}
