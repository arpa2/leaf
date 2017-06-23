/*
 * MITM an LDAP connection.
 *
 * This code connects to an LDAP server and then listens to
 * another port; all data received is passed to the (real)
 * LDAP server, and also dumped to output files (one for
 * input data, one for output data).
 *
 * Typical usage:
 *
 * Suppose you have an LDAP server at db.example.com:389, so
 * that this query returns something:
 *     ldapsearch -h db.example.com -p 389 '(objectclass=device)'
 * It's important that you don't use TLS or similar encryption,
 * because this tool currently doesn't break into that.
 *
 * To man-in-the-middle log a query to that LDAP server, run
 * ldap-mitm with the same -h and -p flags, while giving
 * -H and -P flags for where it should listen; these default
 * to localhost and 3899.
 *     ldap-mitm -h db.example.com -p 389 -H localhost -P 3899
 * Now the entire conversation of a search can be logged to
 * files by running the query against the new listening server:
 *     ldapsearch -h localhost -p 3899 '(objectclass=device)'
 *
 * The MITM server quits after handling a single conversation.
 * Each chunk of data (e.g. LDAPMessage) is dumped to its own
 * file, numbered serially from 0, and named msg.<serial>.<fd>.bin,
 * the fd can be used to distinguish data from client (i.e. ldapsearch)
 * from data from the server (i.e. the real LDAP server). Generally
 * the conversation is started by the client, so msg.000000.<fd>.bin
 * will tell you which one is the client; if you don't have file-
 * descriptor randomisation in the kernel, 5 is usually the client
 * and 3 is the server.
 *
 * The MITM happens in one of two ways:
 *  - with raw sockets and no processing of the messages
 *  - with the LillyDAP processing stack
 * Use the -l flag to select the LillyDAP processing stack.
 *
 * With raw sockets, there is no guarantee that the dumped chunks
 * will be single, complete LDAP messages; a single chunk may be
 * more than one message, or only part of one -- it depends
 * on network and socket buffering.
 *
 * With LillyDAP processing, each individual LDAP message is
 * logged to a separate file.
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
#include <unistd.h> /* getopt */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <lillydap/api.h>

#include "lib/network.h"
#include "lib/region.h"

/* Print usage string and exit with an error. */
void usage()
{
	fprintf(stderr, "\nUsage: ldap-mitm [-h dsthost] [-p dstport] [-H lsthost] [-P lstport] [-l]\n"
		"\tdsthost and dstport specify the target host and port, like options\n"
		"\t-h and -p for ldapsearch(1).\n\n"
		"\tlsthost and lstport specify the hostname and port to listen on.\n"
		"\tThen use those values as -h and -p for ldapsearch(1) instead.\n\n"
		"\tThe -l flag selects for LillyDAP-processing instead of raw packets.\n\n");
	exit(1);
}

/***
 * Lilly-packet dumping.
 *
 * Uses the LillyDAP processing stack to do the same as raw-packet dumping,
 * except that the LillyDAP stack takes care to split things up into
 * individual LDAP messages, so that each dumped serial-file contains
 * exactly one message (which could be dumped by hexio, or processed
 * by lilly-dump).
 *
 * Main entry point is dump_lilly_packets(), which sets up the processing
 * stack and then waits for data, calling lilly() repeatedly to move data
 * from one side to the other.
 */

/* Since the LDAP structure is passed up and down the processing stack,
 * we can play some memory-allocation tricks with it: we can add private
 * data to the end of the structure by embedding it in a larger (longer)
 * struct, and then casting the LDAP pointers to our own structure
 * type (which is OK as long as we know the pointer we allocate at the
 * beginning is large enough).
 *
 * Here, just add a serial number to the structure. Since we're going
 * to increase the serial number on each packet received, but we have
 * two LillyDAP stacks (one client-to-server, one server-to-client),
 * use a pointer to a shared variable.
 */
typedef struct { struct LillyConnection ldap; int *serial; } LDAPX;

int lillydump_dercursor (struct LillyConnection *lil, LillyPool qpool, dercursor dermsg)
{
	static char serialfile[64];
	static char buf[20480];

	LDAPX *ldap = (LDAPX *)lil;

	snprintf(serialfile, sizeof(serialfile), "msg.%06d.%d.bin", (*(ldap->serial))++, lil->get_fd);
	int serialfd = open(serialfile, O_CREAT | O_WRONLY, 0644);
	if (serialfd < 0)
	{
		fprintf(stderr, "Could not open data file '%s'.\n", serialfile);
		return -1;
	}
	else
	{
		if (write_buf(serialfd, (char *)dermsg.derptr, dermsg.derlen, 0) < 0)
		{
			close(serialfd);
			return -1;
		}
		close(serialfd);
	}

	return lillyput_dercursor(lil, qpool, dermsg);
}

int pump_lilly(LDAPX *ldap)
{
	fprintf(stdout, "Lilly %d -> %d (msg.%d).\n", ldap->ldap.get_fd, ldap->ldap.put_fd, *(ldap->serial));
	int r;
	int zero_reads = 0;

	while ((r = lillyget_event(&ldap->ldap)) > 0)
	{
		fprintf(stdout, "  Got %d\n", r);
	}
	if ((r < 0) && (errno != EAGAIN))
	{
		perror("get_event");
		return r;
	}
	if (0 == r)
	{
		zero_reads = 1;
	}
	while ((r = lillyput_event(&ldap->ldap)) > 0)
	{
		fprintf(stdout,"  Send %d\n", r);
	}
	if ((r < 0) && (errno != EAGAIN))
	{
		perror("put_event");
		return r;
	}
	if ((r <= 0) && zero_reads)
	{
		/* 0 bytes were read (by lillyget_event()) and there was nothing to write. */
		return -1;
	}

	return 0;
}

static struct LillyStructural lillydap_dump_put = {
	.lillyget_dercursor = lillydump_dercursor,
	.lillyput_dercursor = lillyput_dercursor,
};

void dump_lilly_packets(int server_fd, int client_fd)
{
	/* Configure memory allocation functions -- and be silly about it */
	lillymem_newpool_fun = leaf_newpool;
	lillymem_endpool_fun = leaf_endpool;
	lillymem_alloc_fun = leaf_alloc;

	/* LillyDAP creates and destroys pools as needed, but we need one
	 * for the LDAP structure and some other allocations.
	 */
	LillyPool *pool = lillymem_newpool();
	if (pool == NULL)
	{
		perror("newpool");
		return;
	}

	/* This is for messages going server -> client */
	LDAPX *ldap_server = lillymem_alloc0(pool, sizeof(LDAPX));
	ldap_server->ldap.def = &lillydap_dump_put;
	ldap_server->ldap.get_fd = server_fd;
	ldap_server->ldap.put_fd = client_fd;

	LDAPX *ldap_client = lillymem_alloc0(pool, sizeof(LDAPX));
	ldap_client->ldap.def = &lillydap_dump_put;
	ldap_client->ldap.get_fd = client_fd;
	ldap_client->ldap.put_fd = server_fd;


	int serial = 0;
	ldap_server->serial = &serial;
	ldap_client->serial = &serial;
	while(1)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(server_fd, &readfds);
		FD_SET(client_fd, &readfds);

		if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
		{
			perror("select(2):");
			break;
		}

		if (FD_ISSET(server_fd, &readfds))
		{
			if (pump_lilly(ldap_server) < 0)
			{
				break;
			}
		}

		if (FD_ISSET(client_fd, &readfds))
		{
			if (pump_lilly(ldap_client) < 0)
			{
				break;
			}
		}
	}
}

int main(int argc, char **argv)
{
	char *hflag = NULL; /* -h, hostname of server */
	int portval = 389; /* -p, port of server */
	char *ownhflag= NULL; /* -H, hostname for self */
	int ownportval = 3899; /* -P, port for self */

	static const char localhost[] = "localhost";

	int ch;
	while ((ch = getopt(argc, argv, "h:p:H:P:")) != -1)
	{
		switch (ch)
		{
		case 'p':
			if (set_port(&portval, optarg) < 0)
			{
				usage();
			}
			break;
		case 'P':
			if (set_port(&ownportval, optarg) < 0)
			{
				usage();
			}
			break;
		case 'h':
			hflag = optarg;
			break;
		case 'H':
			ownhflag = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
	{
		usage();
	}

	int server_fd = connect_server((hflag ? hflag : localhost), portval, 1);
	if (server_fd < 0)
	{
		usage();
	}

	int client_fd = listen_client((ownhflag ? ownhflag : localhost), ownportval, 1);
	if (client_fd < 0)
	{
		close(server_fd);
		usage();
	}

	dump_lilly_packets(server_fd, client_fd);

	close(client_fd);
	close(server_fd);
	return 0;
}
