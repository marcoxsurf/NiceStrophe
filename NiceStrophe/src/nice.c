/*
 * nice.c
 *
 *  Created on: 03/feb/2015
 *      Author: marco
 */
#include "nice.h"
#include <string.h>
#include "io.h"
#include <stdlib.h>

#define N_STATE 4

static const char *acceptable[N_STATE] = {"request", "accept"
		, "denied", "ended"};
static const nice_acceptable_t acceptable_[N_STATE] = {NICE_AC_REQUEST, NICE_AC_ACCEPTED
		, NICE_AC_DENIED, NICE_AC_END};
//static char *own_key64, *other_key64, *other_jid;
static Nice_info* nice_info;
static nice_status_t _nice_status=NICE_ST_INIT;
static int controlling_state;

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
	//todo creare qui la connessione con il server stun
	//todo ottenere la chiave64
	//Cambio stato se ottengo la chiave
	init_struct_nice();
	nice_info->own_key64="1";
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
char* setOtherJid(char* otherJ){
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
