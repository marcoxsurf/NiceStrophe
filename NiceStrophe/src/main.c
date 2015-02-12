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



int main() {

	io_init();
	li_init();
	msg_init();
	net_init();
	nice_init();

	io_printfln("Welcome in %s, %s by %s.", prog_name, prog_version, prog_author);
	io_printfln("Type /help for assistance.");

	while (prog_running) {
		io_nonblock_handle();
		net_nonblock_handle();
		nice_nonblock_handle();
	}

	io_printfln("Exiting...");
	net_deinit();
	msg_deinit();
	li_deinit();
	io_deinit();
	return 0;
}
