#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/fcntl.h>
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
		/* Network declarations */
		struct addrinfo hints, *addrs, *addr_ptr;
		struct packet *pkt, *ack_pkt;
		struct node *head, *n;
		struct timeval default_timeout, current_time, timeout;
		fd_set readfds;
		int result, sockfd, wc, max_no_seqnums;
		int32_t payload_identifier;
		uint8_t seqnum, seqnum_last_recv;
		char pkt_buffer[PKT_BUFSIZE];

		/* Data handling & file declarations */
		struct string_array filenames;
		struct file_array file_arr;
		int file_idx;
		char *filename;

		/* Check arguments */
		if (!(argc == 5 || argc == 6)) {
				printf("Usage: ./client <ipv4-address/hostname> <portnum> <list of filenames (txt-file)> <loss-percentage (int)> [-d]\n");
				printf("%d arguments supplied:\n", argc);
				print_array(argv, argc);
				fprintf(stderr, "Exiting.\n");
				exit(EXIT_FAILURE);
		} else if (!(valid_filename(argv[3]))) {
				fprintf(stderr, "Exiting.\n");
				exit(EXIT_FAILURE);
		}

		if (argc == 6 && (strcmp(argv[5], "-d") == 0)) {
				printf("----- DEBUG MODE -----\n");
				debug_mode = 1;
		} else {
				debug_mode = 0;
		}

		/* DEBUG: Print arguments */
		snprintf(debug_buf, DEBUG_BUFSIZE, "argc: %d\n", argc);  /* DEBUG */
		debugf(debug_buf);                                       /* DEBUG */
		debug_print_array(argv, argc);                           /* DEBUG */

		/* Ensure pkt buffer is zero */
		memset(pkt_buffer, 0, PKT_BUFSIZE);


		/* ----- NETWORK ----- */

		/* Get addr */
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;

		if ((result = getaddrinfo(argv[1], argv[2], &hints, &addrs)) != 0) {
				fprintf(stderr, "Error getaddrinfo: %s\n", gai_strerror(result));
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

		snprintf(debug_buf, DEBUG_BUFSIZE, "Socket fd: %d\n", sockfd);  /* DEBUG */
		debugf(debug_buf);                                              /* DEBUG */

		if (fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0)
				perror("fcntl");

		/* ----- FILES -----*/

		/* Important: entries and total_size must be initialized to 0! */
		/* Realloc initalizes string-array pointers to NULL */
		filenames.entries = 0;
		filenames.total_size = 0;
		realloc_byte_array((struct byte_array*)&filenames);

		/* Read strings from file to string-array */
		read_strings_from_file(&filenames, argv[3]);
		debug_print_filenames(&filenames);    /* DEBUG */

		/* Important: must also be initalized to 0. */
		file_arr.entries = 0;
		file_arr.total_size = 0;
		realloc_byte_array((struct byte_array*)&file_arr);

		/* Read all files listed in filenames-array, and load to file-struct-array */
		int i;
		for (i = 0; i < filenames.entries; i++) {
				filename = filenames.strings[i];
				add_file_to_array(&file_arr, filename);
		}

		debug_print_file_array(&file_arr);

		/* Set loss probability */
		float p = ((float) atoi(argv[4])) / 100;
		set_loss_probability(p);

		snprintf(debug_buf, DEBUG_BUFSIZE, "Loss probability set to %f.\n", p);
		debugf(debug_buf);

		/* Set up Select */
		default_timeout.tv_sec = 5;
		default_timeout.tv_usec = 0;
		timeout = default_timeout;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);

		/* Sequence numbers and payload info */
		seqnum = 0;
		seqnum_last_recv = 0;  /* Strictly speaking not relevant client-side */
		max_no_seqnums = WINSIZE + 1;
		file_idx = 0;
		payload_identifier = 0;
		/* For list*/
		pkt = NULL;
		head = NULL;

		/* Fill linked list up to WINSIZE and while more packets to send */
		/* while (listsize(&head) < WINSIZE && file_arr[file_idx]) */
		while (listsize(&head) < WINSIZE && file_arr.files[file_idx]) {
				pkt = prep_packet(DATA,
								  seqnum,
								  seqnum_last_recv,
								  file_arr.files[file_idx++],
								  payload_identifier++);
				add_node(&head, pkt);
				seqnum = (seqnum + 1) % max_no_seqnums;
		}

		/* Sending packets to server.
		 * Continue as long as there are packets in window to send.
		 */
		while ((listsize(&head) > 0)) {

				/* (Re)sending whole window */
				printf(YEL "RESENDING WHOLE WINDOW\n"NRM);
				for (n = head; n != NULL; n = n->next) {
						wc = load_and_send_packet(n->pkt,
												  pkt_buffer,
												  sockfd,
												  addr_ptr->ai_addr,
												  addr_ptr->ai_addrlen);
						/* Timestamp node (when timeout occurs)*/
						gettimeofday(&current_time, NULL);
						timeradd(&current_time, &default_timeout, &(head->timestamp));
						snprintf(debug_buf, DEBUG_BUFSIZE, "Sent %d bytes\n\n", wc);  /* DEBUG */
						debugf(debug_buf);                                            /* DEBUG */
				}

				/* Incrementally advancing send window for each ACK.
				 * Continues as long as there are packets in window to send.
				 */
				while (listsize(&head) > 0) {
						/* Reset select-set each time */
						FD_SET(sockfd, &readfds);

						/* Reset time according to timestamp of oldest packet
						 * (remaining time before next timeout)
						 */
						gettimeofday(&current_time, NULL);
						if (timercmp(&current_time, &(head->timestamp), >)) {
								/* If clock has passed timestamp of oldest packet: no wait */
								timerclear(&timeout);
						} else {
								/* else: get remaining time to timeout */
								timersub(&head->timestamp, &current_time, &timeout);
						}
						/* DEBUG */
						snprintf(debug_buf, DEBUG_BUFSIZE,
								 "Current time to timeout (sec): "YEL"%ld.%06ld"NRM"\n",
								 timeout.tv_sec, timeout.tv_usec);
						debugf(debug_buf);

						/* Wait for ACK */
						if (select(sockfd+1, &readfds, NULL, NULL, &timeout) == -1)
								perror("select");
						debug("Waiting for ACK\n");

						if (FD_ISSET(sockfd, &readfds)) {
								/* Packet received */
								recv(sockfd, pkt_buffer, PKT_BUFSIZE, 0);
								ack_pkt = get_packet_header(pkt_buffer);
								printf("Seqnum of ACKs last received: "GRN"%d"NRM, ack_pkt->seqnum_last_recv);
								printf(", seqnum oldest unacked packet: "GRN"%d"NRM"\n", head->pkt->seqnum);

								/* Check: seqnum of ACK's last recv = seqnum of oldest pkt */
								if (ACK == ack_pkt->flag && ack_pkt->seqnum_last_recv == head->pkt->seqnum) {
										/* Oldest packet has been ack'ed */
										debug("Received ACK");             /* DEBUG */
										debug_print_packet_meta(ack_pkt);  /* DEBUG */
										seqnum_last_recv = ack_pkt->seqnum;

										/* Remove oldest pkt from list */
										remove_head(&head);

										/* Add new packet to list (if more files to send)
										 * and send new packet.
										 */
										if (file_arr.files[file_idx])
												/* if (file_idx < files_to_send) */ {
												pkt = prep_packet(DATA,
																  seqnum,
																  seqnum_last_recv,
																  file_arr.files[file_idx],
																  payload_identifier);
												add_node(&head, pkt);
												printf("Packet seqnum: %d\n", pkt->seqnum);
												payload_identifier++;
												seqnum = (seqnum + 1) % max_no_seqnums;
												file_idx++;

												/* Sending the last packet added */
												for(n = head; n->next != NULL; n = n->next) {;}
												wc = load_and_send_packet(n->pkt,
																		  pkt_buffer,
																		  sockfd,
																		  addr_ptr->ai_addr,
																		  addr_ptr->ai_addrlen);
										}
								} else if (!(valid_packet(ack_pkt))) {
										fprintf(stderr, RED "Warning:" NRM " received unknown packet.\n");
								}
								free_packet(ack_pkt); ack_pkt = NULL;
						} else {
								printf("- Timeout -\n");
								break; /* Go to outer loop to resend window */
						}
				}
		}

		/* Send TERM-packet */
		printf("Terminating connection.\n");
		pkt = prep_packet(TERM, seqnum, 0, NULL, 0);
		wc = load_and_send_packet(pkt, pkt_buffer, sockfd,
								  addr_ptr->ai_addr, addr_ptr->ai_addrlen);
		free_packet(pkt);

		/* Cleanup */
		free_string_array(&filenames);
		free_file_array(&file_arr);
		freeaddrinfo(addrs);
		if (SUCCESS != close(sockfd))
				perror("Error closing socket");

		printf("\n--- Successfully finished ---\n");
		return 0;
}
