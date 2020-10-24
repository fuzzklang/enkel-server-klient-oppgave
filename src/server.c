#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "my_constants.h"
#include "debug_print.h"
#include "network.h"
#include "files.h"
#include "send_packet.h"

/* Necessary for formatted debug printing.
 * Also used by external functions.
 */
char debug_buf[DEBUG_BUFSIZE];
int debug_mode;

int main(int argc, char *argv[])
{
		/* Network struct declarations */
		struct addrinfo hints, *addrs, *addr_ptr;
		struct sockaddr_storage from_addr;
		struct packet *recv_pkt, *ack_packet;
		float loss_prob;
		socklen_t from_addrlen;
		int32_t pl_len;
		int result, sockfd, rc, max_no_seqnums;
		int8_t exp_seqnum, last_received;

		char pkt_buffer[PKT_BUFSIZE], *tmp_string;

		/* File/data handling declarations */
		struct string_array sa;
		struct file_array fa;
		struct file *matching_file, *recv_f;
		FILE *output_fd;

		/* Check arguments */
	    if (!(argc == 4 || argc == 5 || argc == 6)) {
				/* If wrong number of args: */
				printf("Usage: ./server <portnum> <directory w/imgs> <output filename> [<pkt loss percentage (int)>] [-d]\n");
				printf("%d arguments supplied:\n", argc);
				print_array(argv, argc);
				exit(EXIT_FAILURE);
		}
		if (!(valid_filename(argv[3]))) {
				/* Or invalid filename: */
				fprintf(stderr, "Exiting.\n");
				exit(EXIT_FAILURE);
		}
		/* Check optionals */
		if (argc == 5 && (strcmp(argv[4], "-d") == 0)) {
				/* Only debug parameter has been set */
				printf("----- DEBUG MODE -----\n");
				debug_mode = true;
				loss_prob = 0.0f;
		} else if (argc == 5) {
				/* Only loss percentage has been set */
				debug_mode = false;
				loss_prob = ((float) atoi(argv[4])) / 100;
		} else if (argc == 6 && (strcmp(argv[5], "-d") == 0)) {
				/* Both. First must be loss percentage, second debug param */
				printf("----- DEBUG MODE -----\n");
				loss_prob = ((float) atoi(argv[4])) / 100;
				debug_mode = true;
		} else {
				loss_prob = 0.08f; /* Default 8% loss prob */
		}

		/* DEBUG: print arguments */
		snprintf(debug_buf, DEBUG_BUFSIZE, "argc: %d\n", argc);  /* DEBUG */
		debugf(debug_buf);                                       /* DEBUG */
		debug_print_array(argv, argc);                           /* DEBUG */


		/* ----- NETWORK SETUP ----- */

		/* Ensure pkt_buffer is zero */
		memset(pkt_buffer, 0, PKT_BUFSIZE);

		from_addrlen = sizeof(struct sockaddr_storage);

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;

		if ((result = getaddrinfo(NULL, argv[1], &hints, &addrs)) != 0) {
				fprintf(stderr, "Error main - getaddrinfo: %s\n", gai_strerror(result));
				exit(EXIT_FAILURE);
		}

		/* Loop through all addrs and get a socket */
		addr_ptr = addrs;
		while (addr_ptr) {
				sockfd = socket(addr_ptr->ai_family,
								addr_ptr->ai_socktype,
								addr_ptr->ai_protocol);
				if (-1 == sockfd) {
						perror("main: Error creating socket");
				} else {
						break;
				}
				addr_ptr = addr_ptr->ai_next;
		}
		/* If we got to end of list without a sockfd */
		if (!sockfd) {
				fprintf(stderr, "Got no socket\n.");
				exit(EXIT_FAILURE);
		}
		/* Bind socket to port. */
		if (-1 == bind(sockfd, addr_ptr->ai_addr, addr_ptr->ai_addrlen)) {
				perror("main, bind:");
				exit(EXIT_FAILURE);
		}
		/* Allow reuse of address/socket */
		int yes = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
				perror("main setsockopt");
				exit(EXIT_FAILURE);
		}


		/* --- FILES --- */

		/* Initialize string array and file-array (important!) */
		sa.entries = 0; sa.total_size = 0;
		realloc_byte_array((struct byte_array*)&sa);
		fa.entries = 0; fa.total_size = 0;
		realloc_byte_array((struct byte_array*)&fa);

		/* Get filenames of all valid files from argv <directory>. */
		read_strings_from_dir(&sa, argv[2]);

		/* Add all files to file_array */
		int i;
		for (i = 0; i < sa.entries; i++)
				add_file_to_array(&fa, sa.strings[i]);

		/* Open file which image matching results are written to */
		output_fd = open_file(argv[3], "w");

		set_loss_probability(loss_prob);
		exp_seqnum = 0;
		max_no_seqnums = WINSIZE + 1;

		/* ----- Server loop ----- */
		while (1) {
				debug("Waiting for packets\n");
				/* Receive packet */
				rc = (int) recvfrom(sockfd, pkt_buffer,
									PKT_BUFSIZE,
									0,
									(struct sockaddr*)&from_addr,
									&from_addrlen);
				snprintf(debug_buf, DEBUG_BUFSIZE, "Received %d bytes\n", rc); /* DEBUG */
				debugf(debug_buf);                                             /* DEBUG */

				/* Get packet type */
				recv_pkt = get_packet_header(pkt_buffer);

				printf(GRN "\n--- Received packet ---"NRM"\n");
				printf("Seqnum: %u, expecting seqnum: %u\n", recv_pkt->seqnum, exp_seqnum);

				if (TERM == recv_pkt->flag) {
						printf("Connection terminated.\n");
						free(recv_pkt);
						break;
				}
				/* If received seqnum is as expected, handle payload.
				 * Otherwise, discard and wait for correct packet.
				 */
				if (exp_seqnum == recv_pkt->seqnum) {
						debug("Handling payload");
						last_received = recv_pkt->seqnum;
						exp_seqnum = (recv_pkt->seqnum + 1) % max_no_seqnums;
						debug_print_packet(recv_pkt);

						/* Copy payload (from buffer) to dynamically allocated data structures */
						pl_len = ntohl(recv_pkt->len) - PKT_HEADER_SIZE;    /* Get payload len */
						recv_f = unpack_payload((pkt_buffer + PKT_HEADER_SIZE), pl_len);
						debug_print_file(recv_f);  /* DEBUG */

						/* Send ACK (for each received packet) */
						ack_packet = prep_packet(ACK, exp_seqnum, last_received, NULL, 0);
						debug_print_packet(ack_packet);
						load_and_send_packet(ack_packet,
											 pkt_buffer,
											 sockfd,
											 (struct sockaddr*)&from_addr,
											 from_addrlen);
						free_packet(ack_packet);

						/* Handle image (create struct and compare to loaded file array) */
						/* Combine basename with directory (from argv) */
						matching_file = compare_to_all_files(&fa, recv_f);
						if (matching_file) {
								tmp_string = concat_strings_nl(recv_f->filename, matching_file->filename);
						} else {
								debug("No matching image!");
								tmp_string = concat_strings_nl(recv_f->filename, "UNKOWN");
						}
						/* Write result from image compare to output file */
						write_to_file(tmp_string, output_fd);
						free(tmp_string);
						free_file(recv_f);


				} /* else if (last_received == recv_pkt->seqnum) */
				else if (already_received(recv_pkt->seqnum, exp_seqnum, WINSIZE, max_no_seqnums)) {
						/* (re)acknowledge a packet which is already received */
						debug("Already received: ack and discard packet\n");
						ack_packet = prep_packet(ACK, exp_seqnum, recv_pkt->seqnum, NULL, 0);
						debug_print_packet(ack_packet);
						load_and_send_packet(ack_packet,
											 pkt_buffer,
											 sockfd,
											 (struct sockaddr*)&from_addr,
											 from_addrlen);
						free_packet(ack_packet);
				} else {
						debug(RED "Unexpected error" NRM ": couldn't identify seqnum. Might be out of bounds.\n");
				}
				free_packet(recv_pkt);
		}

		/* Cleanup */
		free_file_array(&fa);
		free_string_array(&sa);
		fclose(output_fd);
		close(sockfd);
		freeaddrinfo(addrs);

		printf("\n--- Successfully finished ---\n");
		return 0;
}
