#ifndef SEND_PACKET_H
#define SEND_PACKET_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>

/* Removed due to copyright
 */
void set_loss_probability( float x );

/* Description removed due to copyright
 * [lossy send_packet function, used to test protocol retransmission]
 */
ssize_t send_packet( int sock, const char* buffer, size_t size, int flags, const struct sockaddr* addr, socklen_t addrlen );

#endif /* SEND_PACKET_H */
