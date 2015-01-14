/*
 * tools.h
 *
 *  Created on: 02/gen/2015
 *      Author: marco
 */

#ifndef NICESTROPHE_SRC_TOOLS_H_
#define NICESTROPHE_SRC_TOOLS_H_

#include "header.h"

//uses the getaddrinfo function to retrieve information about a hostname/domain name
gchar* hostname_to_ip(char *hostname,char *port);

#endif /* NICESTROPHE_SRC_TOOLS_H_ */
