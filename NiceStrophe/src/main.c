#include "main.h"
#include "io.h"
#include "li.h"
#include "net.h"
#include "nice.h"
#include "msg.h"
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int prog_running = 1;
const char* const prog_version = "2.0";
const char* const prog_author = "Marco Fantasia <fantasia.marco@gmail.com>";
const char* const prog_name = "NiceStrophe";


/**
 * Thread per la gestione delle connessioni 'Nice'
 */
static void * _thread_nice(void *data) {
	nice_init();
	while(prog_running){
		nice_nonblock_handle();
	}
}

static void * _thread_xmpp(void *data) {
	io_init();
	li_init();
	msg_init();
	net_init();
	io_printfln("Welcome in %s, %s by %s.", prog_name, prog_version,
			prog_author);
	io_printfln("Type /help for assistance.");
	while (prog_running) {
		io_nonblock_handle();
		net_nonblock_handle();
	}
	io_printfln("Exiting...");
	net_deinit();
	msg_deinit();
	li_deinit();
	io_deinit();
	return 0;
}

int main() {
	GThread *gthread_nice;
	GThread *gthread_xmpp;
	gloop = g_main_loop_new(NULL, FALSE);
	//Eseguo il main_loop e il thread
	exit_thread = FALSE;
	gthread_nice = g_thread_new("_thread_nice", &_thread_nice, NULL);
	gthread_xmpp = g_thread_new("_thread_xmpp", &_thread_xmpp, NULL);
	//esegue il loop fino a che non viene chiamato g_main_loop_quit
	g_main_loop_run(gloop);
	exit_thread = TRUE;
	//aspetto che termini
	g_thread_join(gthread_nice);
	g_thread_join(gthread_xmpp);
	g_main_loop_unref(gloop);
	return EXIT_SUCCESS;

	nice_init();

}
