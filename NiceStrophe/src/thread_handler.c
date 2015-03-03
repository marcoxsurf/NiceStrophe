/*
 * thread_handler.c
 *
 *  Created on: 01/mar/2015
 *      Author: marco
 */
#include "thread_handler.h"
#include "main.h"
#include <string.h>
#include <stdlib.h>

void init_struct_nice() {
	g_mutex_lock (&nice_info_gmutex);
	nice_info->my_key64=NULL;
	nice_info->other_jid=NULL;
	nice_info->other_key64=NULL;
	g_mutex_unlock(&nice_info_gmutex);
}

char* getMyJid() {
	char* ret;
	g_mutex_lock (&nice_info_gmutex);
	ret=strdup(nice_info->my_jid);
	g_mutex_unlock(&nice_info_gmutex);
	return ret;
}

char* getMyKey() {
	char* ret;
	g_mutex_lock (&nice_info_gmutex);
	ret=strdup(nice_info->my_key64);
	g_mutex_unlock(&nice_info_gmutex);
	return ret;
}

char* getOtherKey() {
	char* ret;
	g_mutex_lock (&nice_info_gmutex);
	ret=strdup(nice_info->other_key64);
	g_mutex_unlock(&nice_info_gmutex);
	return ret;
}

char* getOtherJid() {
	char* ret;
	g_mutex_lock (&nice_info_gmutex);
	ret=strdup(nice_info->other_jid);
	g_mutex_unlock(&nice_info_gmutex);
	return ret;
}

char* setMyJid(char* jid) {
	char* ret;
	g_mutex_lock (&nice_info_gmutex);
	nice_info->my_jid = strdup(jid);
	ret=strdup(nice_info->my_jid);
	g_mutex_unlock(&nice_info_gmutex);
	return ret;
}
char* setMyKey(char* key64) {
	char* ret;
	g_mutex_lock (&nice_info_gmutex);
	nice_info->my_key64 = strdup(key64);
	ret=strdup(nice_info->my_key64);
	g_mutex_unlock(&nice_info_gmutex);
	return ret;
}

char* setOtherKey(char* otherJ, char* otherK) {
	char* ret;
	if (getOtherJid()==NULL){
		g_mutex_lock (&nice_info_gmutex);
		nice_info->other_key64 = NULL;
		ret=NULL;
		g_mutex_unlock(&nice_info_gmutex);
		return ret;
	}
	//know for sure that key corrispond to jid
	if (strcmp(otherJ, getOtherJid()) == 0) {
		g_mutex_lock (&nice_info_gmutex);
		nice_info->other_key64 = strdup(otherK);
		ret=strdup(nice_info->other_key64);
		g_mutex_unlock(&nice_info_gmutex);
	}
	return ret;
}
char* setOtherJid(const char* otherJ) {
	char* ret;
	if (_nice_status == NICE_ST_IDLE) {
		g_mutex_lock (&nice_info_gmutex);
		nice_info->other_jid = strdup(otherJ);
		ret=strdup(nice_info->other_jid);
		g_mutex_unlock(&nice_info_gmutex);
	}
	return ret;
}

int getControllingState() {
	int ret;
	g_mutex_lock (&controlling_state_mutex);
	ret=controlling_state;
	g_mutex_unlock(&controlling_state_mutex);
	return ret;
}
int setControllingState(int newState) {
	int ret;
	g_mutex_lock (&controlling_state_mutex);
	controlling_state = newState;
	ret=controlling_state;
	g_mutex_unlock(&controlling_state_mutex);
	return ret;
}
nice_status_t getNiceStatus(){
	nice_status_t ret;
	g_mutex_lock(&nice_status_mutex);
	ret = _nice_status;
	g_mutex_unlock(&nice_status_mutex);
	return ret;
}
nice_status_t setNiceStatus(nice_status_t new_state){
	nice_status_t ret;
	g_mutex_lock(&nice_status_mutex);
	_nice_status = new_state;
	ret = _nice_status;
	g_mutex_unlock(&nice_status_mutex);
	return ret;
}

NiceAgent* getAgent(){
	NiceAgent *ret;
	g_mutex_lock(&agent_mutex);
	ret=agent;
	g_mutex_unlock(&agent_mutex);
	return ret;
}
void setAgent(NiceAgent *newAgent){
	g_mutex_lock(&agent_mutex);
	agent=newAgent;
	g_mutex_unlock(&agent_mutex);
}
