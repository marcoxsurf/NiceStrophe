/*
 * tools.c
 *
 *  Created on: 02/gen/2015
 *      Author: marco
 */
#include "tools.h"

gchar* hostname_to_ip(char *hostname,char *port){
	struct addrinfo hints, *res, *res0;
	int error;
	int s;
	const char *cause = NULL;
	int index=0;
	struct sockaddr_in *h;
	gchar *ip=malloc(sizeof(gchar)*100);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(hostname, port, &hints, &res0);

	if(error){
		errx(1, "%s", gai_strerror(error));
		/*NOTREACHED*/
	}
	s = -1;
	for (res = res0; res; res = res->ai_next){
		index++;
		s = socket(res->ai_family, res->ai_socktype,
				res->ai_protocol);
		if (s < 0)
		{
			printf("socket creation failed");
			cause = "socket";
			continue;
		}
		h = (struct sockaddr_in *) res->ai_addr;
		strcpy(ip , inet_ntoa( h->sin_addr ) );
		break;  /* okay we got one */
	}
	if (s < 0){
		err(1, "%s", cause);
		/*NOTREACHED*/
	}
	freeaddrinfo(res0);
	return ip;
}
