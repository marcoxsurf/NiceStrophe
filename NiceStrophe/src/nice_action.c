/*
 * nice_action.c
 *
 *  Created on: 03/mar/2015
 *      Author: marco
 *      Description: Test library for nice test
 */
#include <string.h>

#include "nice_action.h"
#include "io.h"
#include "nice.h"

void init() {
	g_mutex_init(&thread_mutex);
	g_cond_init(&thread_cond);
	g_mutex_lock(&thread_mutex);
	thread_has_done = FALSE;
	g_mutex_unlock(&thread_mutex);
}

void deinit() {
	g_cond_clear(&thread_cond);
	g_mutex_clear(&thread_mutex);

}

void * no_thread(void *data) {
	io_notification("Thread");
	init();
	g_mutex_lock(&thread_mutex);
	thread_has_done = TRUE;
	g_cond_signal(&thread_cond);
	g_mutex_unlock(&thread_mutex);
	deinit();
	return NULL;
}
/**
 * Simple send a text and then terminate
 */
void * text_thread(void *data) {
	gchar *line;
	init();
	negotiate();
	if (getControllingState()) {
		line = strdup("Sono io il tuo capo");
	} else {
		line = strdup("Si padrone");
	}

	nice_agent_send(agent, stream_id, 1, strlen(line), line);
	g_free(line);
	//send CTRL-D to terminate
//	nice_agent_send(agent, stream_id,1,1,"\0");

	g_mutex_lock(&thread_mutex);
	thread_has_done = TRUE;
	g_cond_signal(&thread_cond);
	g_mutex_unlock(&thread_mutex);
	deinit();
	return NULL;
}

void * file_thread(void *user_data) {
//	GMainContext *main_context;
//	main_context = g_main_context_new();
//	g_main_context_push_thread_default(main_context);
	//init();
	//negotiate sdp
	negotiate();
	if (getControllingState()) {
		io_printfln("Waiting to be ready to send.....\n");
	}

	/*Wait for stream to be writeable*/
	g_mutex_lock(&write_mutex);
	while (!(stream_open && stream_ready))
		g_cond_wait(&write_cond, &write_mutex);
	g_mutex_unlock(&write_mutex);
	if (getControllingState()) {
		io_printfln("yeahhh I'm ready to send now\n");
	}
	if (getControllingState()) {
		write_thread_gsource_cb(output_stream, user_data);
	}
	g_mutex_lock(&thread_mutex);
	thread_has_done = TRUE;
	g_cond_signal(&thread_cond);
	g_mutex_unlock(&thread_mutex);
	deinit();
//	g_main_context_pop_thread_default(main_context);
//	g_main_context_unref(main_context);
	return NULL;
}

gboolean read_stream_cb(GObject *pollable_stream, gpointer _user_data) {
	GError *error = NULL;
	gchar *buf = NULL;
	gsize buf_len = 0;
	gssize len;
	/* Initialise a receive buffer. */
	buf_len = CHUNK_SIZE;
	buf = g_malloc(sizeof(gchar) * buf_len);
	/* Trim the receive buffer to avoid consuming the ‘done’ message. */
	buf_len = MIN(buf_len, total_byte - read_byte);
	/* Try to receive some data. */
	len = g_pollable_input_stream_read_nonblocking(
			G_POLLABLE_INPUT_STREAM(pollable_stream), buf, buf_len, NULL,
			&error);
	if (len == -1) {
		g_assert_error(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);
		g_error_free(error);
		g_free(buf);
		return G_SOURCE_CONTINUE;
	}
	io_printfln("Read %lu bytes \n", len);
	g_assert_no_error(error);
	io_printfln("Arrived: %s\n", buf);
	//TODO Save buf to file
	read_byte += len;
	/* Termination time? */
	if (read_byte == total_byte) {
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

void read_thread_gsource_cb(GInputStream *input_stream, gpointer user_data) {
	GMainContext *main_context;
	GMainLoop *main_loop;
	GSource *stream_source;

	main_context = g_main_context_ref_thread_default();
	main_loop = g_main_loop_new(main_context, FALSE);
	stream_source = g_pollable_input_stream_create_source(
			G_POLLABLE_INPUT_STREAM(input_stream), NULL);
	g_source_set_callback(stream_source, (GSourceFunc) read_stream_cb,
			&main_loop, NULL);
	g_source_attach(stream_source, main_context);
	/* Run the main loop. */
	g_main_loop_run(main_loop);

	g_source_destroy(stream_source);
	g_source_unref(stream_source);
	g_main_loop_unref(main_loop);
	g_main_context_unref(main_context);
}

gboolean write_stream_cb(GObject *pollable_stream, gpointer _user_data) {
	GError *error = NULL;
	gchar *buf = NULL;
	gsize buf_len = 0;
	gssize len;
	while (sent_byte < total_byte) {
		/* Generate a buffer to trasmit. */
		//BUFFER_SIZE_CONSTANT_SMALL policy
		/* For other strategies read test-send-recv*/
		buf_len = CHUNK_SIZE; //Max packet size on the received side is 1284
		buf_len = MIN(buf_len, (total_byte - sent_byte));
		//malloc of the size real needed
		buf = g_malloc(buf_len);
		//no control on file read error
		fread(buf, 1, buf_len, fileSend);

		/* Try to transmit some data. */
		len = g_pollable_output_stream_write_nonblocking(
				G_POLLABLE_OUTPUT_STREAM(pollable_stream), buf, buf_len, NULL,
				&error);

		if (len == -1) {
			g_assert_error(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);
			g_free(buf);
			return G_SOURCE_CONTINUE;
		}

		g_assert_no_error(error);

		/* Update the test’s buffer generation state machine. */
		sent_byte += len;
		io_printfln("Sent %lu bytes \n", len);
		io_printfln("Sent %d\%\n",(int) (sent_byte*100/total_byte));
		/* Termination time? */
		if (sent_byte == total_byte) {
			io_printfln("Sent completed");
			return G_SOURCE_REMOVE;
		}
	}
	return G_SOURCE_CONTINUE;
}

void write_thread_gsource_cb(GOutputStream *output_stream, gpointer user_data) {
	GMainContext *main_context;
	GMainLoop *main_loop;
	GSource *stream_source;

	main_context = g_main_context_ref_thread_default();
	main_loop = g_main_loop_new(main_context, FALSE);

	stream_source = g_pollable_output_stream_create_source(
			G_POLLABLE_OUTPUT_STREAM(output_stream), NULL);

	g_source_set_callback(stream_source, (GSourceFunc) write_stream_cb,
			&main_loop, NULL);
	g_source_attach(stream_source, main_context);

	/* Run the main loop. */
	g_main_loop_run(main_loop);

	g_source_unref(stream_source);
	g_main_loop_unref(main_loop);
	g_main_context_unref(main_context);
}
