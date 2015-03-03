#include "main.h"
#include "io.h"
//#include "li.h"
#include "net.h"
#include "nice.h"
#include "msg.h"
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int prog_running;
const char* const prog_version = "2.0";
const char* const prog_author = "Marco Fantasia <fantasia.marco@gmail.com>";
const char* const prog_name = "NiceStrophe";


/**
 * Thread per la gestione delle connessioni 'Nice'
 */
static void* _thread_nice(void *data) {
	nice_init();
	while(prog_running){
		nice_nonblock_handle();
	}
	nice_deinit();
	return 0;
}

static void* _thread_xmpp(void *data) {
	io_init();
	//li_init();
	msg_init();
	net_init();
	while (prog_running) {
		io_nonblock_handle();
		net_nonblock_handle();
	}
	io_printfln("Exiting...");
	net_deinit();
	msg_deinit();
	//li_deinit();
	io_deinit();
	return 0;
}

int main() {
	nice_info=malloc(sizeof(Nice_info));
	_nice_status=NICE_ST_INIT;
	GThread *gthread_nice;
	GThread *gthread_xmpp;
	gloop = g_main_loop_new(NULL, FALSE);
	printf("Welcome in %s, %s by %s.\n", prog_name, prog_version,prog_author);
	printf("Type /help for assistance.");
	//Eseguo il main_loop e il thread
	prog_running=TRUE;
	gthread_xmpp = g_thread_new("_thread_xmpp", &_thread_xmpp, NULL);
	gthread_nice = g_thread_new("_thread_nice", &_thread_nice, NULL);
	//esegue il loop fino a che non viene chiamato g_main_loop_quit
	g_main_loop_run(gloop);
	prog_running=FALSE;
	//aspetto che termini
	g_thread_join(gthread_xmpp);
	g_thread_join(gthread_nice);
	g_main_loop_unref(gloop);
	return EXIT_SUCCESS;
}
