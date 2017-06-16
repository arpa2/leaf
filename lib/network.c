/*
 * MITM an LDAP connection.
 *
 * This code connects to an LDAP server and then listens to
 * another port.
 *
 * Typical usage: use connect_server() and listen_client() to set up
 * network connections which can then be processed with LillyDAP
 * in the LEAF middleware.
 *
 */

/*
 *  Copyright 2017, Adriaan de Groot <groot@kde.org>
 *
 *  Redistribution and use is allowed according to the terms of the two-clause BSD license.
 *     https://opensource.org/licenses/BSD-2-Clause
 *     SPDX short identifier: BSD-2-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>  /* INT_MAX */
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "network.h"

int set_port(int *port, const char *arg)
{
	errno = 0;
	long l = strtol(arg, NULL, 10);
	if ((l < 1) || (l > INT_MAX) || errno)
	{
		fprintf(stderr, "Could not understand port '%s'.\n", arg);
		return -1;
	}
	if (port)
	{
		*port = (int)l;
	}
	return 0;
}

int set_nonblocking(int fd, int blocking)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		perror("Can't get socket flags");
		close(fd);
		return -1;
	}

	int newflags = flags;
	if (blocking)
	{
		newflags |= O_NONBLOCK;
	}
	else
	{
		newflags &= (~O_NONBLOCK);
	}

	if (flags == newflags)
	{
		/* Nothing to do. */
		return 0;
	}

	if (fcntl(fd, F_SETFL, newflags) < 0)
	{
		perror("Can't set socket flags");
		close(fd);
		return -1;
	}
	return 0;
}


int connect_server(const char *hostname, int port, int nonblocking)
{
	struct hostent *server = gethostbyname(hostname);
	if (!server)
	{
		fprintf(stderr, "Could not look up host '%s'.\n", hostname);
		return -1;
	}

	int sid = socket(PF_INET, SOCK_STREAM, 0);
	if (sid < 0)
	{
		fprintf(stderr, "Could not open socket for '%s:%d'.\n", hostname, port);
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

	if (connect(sid, (struct sockaddr *)(&addr), sizeof(addr)) < 0)
	{
		perror("Unable to connect:");
		close(sid);
		fprintf(stderr, "Could not connect to '%s:%d'.\n", hostname, port);
		return -1;
	}

	if (nonblocking)
	{
		if (set_nonblocking(sid, 1) < 0)
		{
			fprintf(stderr, "Could not set connection options to '%s:%d'.\n", hostname, port);
			return -1;
		}
	}

	return sid;
}

int listen_client(const char *hostname, int port, int nonblocking)
{
	struct hostent *server = gethostbyname(hostname);
	if (!server)
	{
		fprintf(stderr, "Could not look up host '%s'.\n", hostname);
		return -1;
	}

	int sid = socket(PF_INET, SOCK_STREAM, 0);
	if (sid < 0)
	{
		fprintf(stderr, "Could not open socket for '%s:%d'.\n", hostname, port);
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

	if (bind(sid, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("Unable to bind:");
		close(sid);
		fprintf(stderr, "Could not bind to '%s:%d'.\n", hostname, port);
		return -1;
	}

	/* Intentionally a one-connection-at-a-time server. */
	if (listen(sid, 1) < 0)
	{
		perror("Unable to listen:");
		close(sid);
		fprintf(stderr, "Could not listen to '%s:%d'.\n", hostname, port);
		return -1;
	}

	int client_fd = accept(sid, NULL, NULL);
	if (client_fd < 0)
	{
		perror("Unable to accept:");
		close(sid);
		fprintf(stderr, "Could not accept connection on '%s:%d'.\n", hostname, port);
		return -1;
	}
	close(sid);

	if (nonblocking)
	{
		if (set_nonblocking(client_fd, 1) < 0)
		{
			fprintf(stderr, "Could not set connection options to '%s:%d'.\n", hostname, port);
			return -1;
		}
	}

	return client_fd;
}

int write_buf(int destfd, const char *buf, int r, int verbose)
{
	int w = 0;
	int w_d = 0;
	while (w < r)
	{
		w_d = write(destfd, buf+w, r-w);
		if (w_d < 0)
		{
			perror("Unable to write:");
			return -1;
		}
		w += w_d;
		if (verbose)
		{
			fprintf(stdout,"  %d (of %d)\n", w, r);
		}
	}

	return 0;
}
