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

#include "send_packet.h"
#include "my_constants.h"

static float loss_probability = 0.0f;

void set_loss_probability( float x )
{
		// Removed due to copyright
}

ssize_t send_packet( int sock, const char* buffer, size_t size, int flags, const struct sockaddr* addr, socklen_t addrlen )
{
		// Removed due to copyright
}
