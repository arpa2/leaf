/*
 * MITM an LDAP connection.
 *
 * This code connects to an LDAP server and then listens to
 * another port.
 */

/*
 *  Copyright 2017, Adriaan de Groot <groot@kde.org>
 *
 *  Redistribution and use is allowed according to the terms of the two-clause BSD license.
 *     https://opensource.org/licenses/BSD-2-Clause
 *     SPDX short identifier: BSD-2-Clause
 */

#ifndef LEAF_LISTEN_H
#define LEAF_LISTEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sets the value pointed to by @p port to the integer value obtained
 * from @p arg; returns 0 on success, -1 on failure (and prints an
 * error message).
 */
int set_port(int *port, const char *arg);

/* Switches the non-blocking flag (O_NONBLOCK) on the given file-
 * descriptor @p fd on (if @p blocking is non-zero) or off (if
 * @p blocking is zero). Returns -1 on error, 0 on success.
 */
int set_nonblocking(int fd, int blocking);

/* Connects to the (LDAP) server with @p hostname on @p port (usually 389).
 * If @p nonblocking is non-zero, sets up the file-descriptor used for
 * communicating with the server to be non-blocking. This is required
 * for LillyDAP, not required for raw packet processing.
 *
 * Returns a file descriptor for communicating with the server, or -1 on error.
 */
int connect_server(const char *hostname, int port, int nonblocking);

/* Sets up a listening socket, and waits until a client connects
 * to it. Listens on the given @p hostname and @p port combination.
 * If @p nonblocking is non-zero, the resulting file-descriptor
 * is set to non-blocking mode. This is required for LillyDAP
 * processing, not required for raw packet processing.
 *
 * Returns a file descriptor for communicating with the connected
 * client, or -1 on error.
 */
int listen_client(const char *hostname, int port, int nonblocking);

/* Write a buffer @p buf of length @p r to an open file descriptor @p destfd.
 * If the flag @p verbose is non-zero, print progress information as the
 * writes are done (in case of partial writes, this will lead to multiple
 * output lines as the total of @p r bytes are written).
 *
 * Returns 0 on succes, -1 on error.
 */
int write_buf(int destfd, const char *buf, int r, int verbose);

#ifdef __cplusplus
}
#endif

#endif
