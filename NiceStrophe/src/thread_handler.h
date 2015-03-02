/*
 * thread_handler.h
 *
 *  Created on: 01/mar/2015
 *      Author: marco
 */

#ifndef THREAD_HANDLER_H_
#define THREAD_HANDLER_H_

#include <glib-2.0/glib.h>

typedef struct _info {
	char *my_jid;
	char *my_key64;
	char *other_key64;
	char *other_jid;
} Nice_info;

typedef enum {
	NICE_ST_IDLE,
	NICE_ST_WAITING_FOR,
	NICE_ST_ACCEPTED,
	NICE_ST_DENIED,
	NICE_ST_BUSIED,
	NICE_ST_ENDED,
	NICE_ST_INIT
} nice_status_t;

Nice_info *nice_info;
nice_status_t _nice_status;
int controlling_state;


GMutex nice_info_gmutex, controlling_state_mutex, nice_status_mutex;

void init_struct_nice();
char* getMyJid();
char* getMyKey();
char* getOtherKey();
char* getOtherJid();

char* setMyJid(char* jid);
char* setMyKey(char* key64);
char* setOtherKey(char* otherJ,char* otherK);
char* setOtherJid(const char* otherJ);

int getControllingState();
int setControllingState(int newState);

nice_status_t getNiceStatus();
nice_status_t setNiceStatus(nice_status_t new_state);

#endif /* THREAD_HANDLER_H_ */