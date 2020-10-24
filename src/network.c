#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include "my_constants.h"
#include "debug_print.h"
#include "files.h"
#include "send_packet.h"
#include "network.h"


/* =======================
 * ======= PACKETS =======
 * =======================
 */
bool valid_packet(struct packet *p)
{
		char flag;
		flag = p->flag;
		/* Check that unused byte is correctly set */
		if (p->unused != 0x7f) return false;
		/* Check that flag is correctly set */
		if(!(flag == DATA || flag == ACK || flag == TERM))
				return false;
		if(   ((flag == DATA) && (flag == ACK))
		   || ((flag == DATA) && (flag == TERM))
		   || ((flag == ACK) && (flag == TERM)))
				return false;
		/* Check that packet size does not exceed buffer size */
		if (ntohl(p->len) > PKT_BUFSIZE) {
				fprintf(stderr, "Warning: total length of packet [%d] is supposedly bigger than defined max size for packet [%d]", ntohl(p->len), PKT_BUFSIZE);
				return false;}
		return true;
}

struct packet *get_packet_header(char *buf)
{
		char *ptr;
		struct packet *pkt;
		ptr = buf;
		pkt = malloc(sizeof(struct packet));
		pkt->len = *(int32_t*)ptr; ptr += 4;
		pkt->seqnum = *(uint8_t*)ptr++;
		pkt->seqnum_last_recv = *(uint8_t*)ptr++;
		pkt->flag = *(uint8_t*)ptr++;
		pkt->unused = *(uint8_t*)ptr++;
		pkt->pl = NULL;
		if(!valid_packet(pkt)) {
				free(pkt);
				return NULL;
		}
		return pkt;
}

/* Ideally I would seperate these two functions (prep_packet and prep_payload)
 * in two clearly seperated functions where prep_packet would take a payload
 * as an argument (to seperate the layers from each other).
 * But prep_packet requires info on file-struct to obtain total size of pkt.
 * And therefore it needs to be passed the file-struct as argument (info on
 * file size is in the application layer, but is not part of the payload header).
 */
struct packet *prep_packet(uint8_t type, int8_t seqnum, int8_t seqnum_last_recv, void *opt_data, int32_t pl_id)
{
		struct packet *pkt;
		struct payload *pl;
		struct file *f;
		int32_t fn_len, total_len;
		pkt = malloc(sizeof(struct packet));
		if (NULL == pkt) {
				perror("Error in prep_packet during malloc");
				return NULL;
		}
		pkt->seqnum = seqnum;
		pkt->seqnum_last_recv = seqnum_last_recv;
		pkt->flag = type;
		pkt->unused = 0x7f;

		if (DATA == type) {
				f = (struct file*) opt_data;
				pl = prep_payload(f, pl_id);
				pkt->pl = pl;
				fn_len = ntohl(pl->filename_len);

				/* Number 8: Payload id (int) and int describing filename-len */
				total_len = PKT_HEADER_SIZE + 8 + fn_len + f->n_bytes;
				pkt->len = htonl(total_len);

				snprintf(debug_buf, DEBUG_BUFSIZE, "in prep_packet, total_len: %d\n", total_len); /* DEBUG */
				debugf(debug_buf); /* DEBUG */
		}
		else if (ACK == type) {
				pkt->pl = NULL;
				pkt->len = htonl(PKT_HEADER_SIZE);
		}
		else if (TERM == type) {
				pkt->pl = NULL;
				pkt->len = htonl(PKT_HEADER_SIZE);
		} else {
				fprintf(stderr, "Unknown packet type passed to prep_packet.\n");
				return NULL;
		}
		if (opt_data != NULL && type != DATA)
				fprintf(stderr, "Warning! File data sent to a packet although flag is not DATA! (func: prep_packet)\n");
		/* Check valid packet */
		if (!valid_packet(pkt)) {
				fprintf(stderr, "Warning! Packet in prep_packet seems to not be valid!\n");
		}
		return pkt;
}

struct payload *prep_payload(struct file *f, int32_t pl_id)
{
		struct payload *pl;
		char *fn, *bytes;
		pl = malloc(sizeof(struct payload));
		bytes = malloc(sizeof(char) * f->n_bytes);
		if (NULL == pl || NULL == bytes) {
				fprintf(stderr, RED "Critical error " NRM);
				perror("in prep_payload during malloc");
				return NULL;
		}
		/* Copy filename (memory allocated by get_basename)
		 * and copy bytes from file-struct to malloced array.
		 */
		fn = get_basename(f->filename);
		memcpy(bytes, f->bytes, f->n_bytes);
		/* Set pointers accordingly */
		pl->id = htonl(pl_id);
		pl->filename_len = htonl(strlen(fn) + 1);
		pl->filename = fn;
		pl->bytes = bytes;
		return pl;
}

int load_and_send_packet(struct packet *pkt, char *buf, int sockfd, struct sockaddr *dest_addr, socklen_t addrlen)
{
		int32_t total_len, remaining_bytes;
		ssize_t wc;
		char *ptr;
		total_len = ntohl(pkt->len);
		remaining_bytes = total_len;

		if (ACK == pkt->flag || TERM == pkt->flag) {
				snprintf(debug_buf, DEBUG_BUFSIZE,"Sending %s.\n", (pkt->flag == ACK)? "an ACK" : "a TERM"); /* DEBUG */
				debugf(debug_buf);    /* DEBUG */
				memcpy(buf, pkt, PKT_HEADER_SIZE);
				wc = send_packet(sockfd, buf, total_len, 0, dest_addr, addrlen);
				snprintf(debug_buf, DEBUG_BUFSIZE, "In load_and_send_packet – Number of bytes sent: %ld\n", wc); /* DEBUG */
				debugf(debug_buf);
				return wc;
		} else if (DATA == pkt->flag) {
				debug("Sending packet with payload.");
				ptr = buf;
				/* Copy packet header to buffer */
				memcpy(ptr, pkt, PKT_HEADER_SIZE); ptr += PKT_HEADER_SIZE; remaining_bytes -= PKT_HEADER_SIZE;
				/* Copy payload identifier and filename len to buffer*/
				memcpy(ptr, pkt->pl, 8); ptr += 8; remaining_bytes -= 8;
				/* Copy filename (incl. '\0'-byte) to buffer */
				memcpy(ptr, pkt->pl->filename, ntohl(pkt->pl->filename_len));
				ptr += ntohl(pkt->pl->filename_len);
				remaining_bytes -= ntohl(pkt->pl->filename_len);
				/* Copy image bytes to buffer */
				memcpy(ptr, pkt->pl->bytes, remaining_bytes);
				/* Send packet */
				wc = send_packet(sockfd, buf, total_len, 0, dest_addr, addrlen);
				memset(buf, 0, PKT_BUFSIZE);
				if (-1 == wc)
						return FAILURE;
				snprintf(debug_buf, DEBUG_BUFSIZE, "In load_and_send_packets – Number of bytes sent: %ld\n", wc);
				debugf(debug_buf);
				return (int) wc;
		} else {
				fprintf(stderr, "Error: Unknown packet flag – in load_and_send_packet.");
		}
		return FAILURE;
}


/* --- SERVER SIDE --- */
struct file *unpack_payload(char *pl_buf, int32_t payload_len)
{
		struct file *f;
		int32_t remaining_bytes, id, filename_len;
		char *ptr, *fn, *bytes;

		debug("--- Unpacking payload ---");
		ptr = pl_buf;
		remaining_bytes = payload_len;
		/* Consume payload identifier */
		id = ntohl(*(uint32_t*)(ptr)); ptr += sizeof(int32_t); remaining_bytes -= 4;

		printf("Payload id: "YEL"%d"NRM"\n", id);
		/* /\* DEBUG *\/ */
		/* snprintf(debug_buf, DEBUG_BUFSIZE, "Payload identifier: " YEL "%3d" NRM "\n", id); */
		/* debugf(debug_buf); */

		snprintf(debug_buf, DEBUG_BUFSIZE, "w/total payload len: %4d\n", payload_len);  /* DEBUG */
		debugf(debug_buf);  /* DEBUG */

		f = malloc(sizeof(struct file));
		if (NULL == f) {
				perror("Error in unpack_payload during malloc (1)");
				return NULL;
		}
		/* Consume filename_len */
		filename_len = ntohl(*(int32_t*)(ptr)); ptr += 4; remaining_bytes -= 4;
		fn = malloc(filename_len * sizeof(char));   /* Filename buffer */
		if (NULL == fn) {
				perror("Error in unpack_payload during malloc (2)");
				return NULL;
		}
		/* Consume filename */
		strncpy(fn, ptr, filename_len); ptr += filename_len; remaining_bytes -= filename_len;
		fn[filename_len - 1] = '\0';  /* Ensure null-byte */
		f->filename = fn;
		/* Copy bytes and return struct */
		f->n_bytes = remaining_bytes;
		bytes = malloc(remaining_bytes * sizeof(char));
		if (NULL == bytes) {
				perror("Error in unpack_payload during malloc (3)");
				return NULL;
		}
		memcpy(bytes, ptr, remaining_bytes);
		f->bytes = bytes;
		return f;
}

void free_packet(struct packet *p)
{
		if (p && p->pl)
				free_payload(p->pl);
		free(p);
}

void free_payload(struct payload *pl)
{
		if (pl) {
				free(pl->filename);
				free(pl->bytes);
				free(pl);
		}
}


bool already_received(uint8_t seqnum, uint8_t exp_seqnum, uint8_t window_size, uint8_t max_no_seqnums)
{
		for (uint8_t i = 1; i < (window_size + 1); i++) {
				if ((seqnum + i) % max_no_seqnums == exp_seqnum)
						return true;
		}
		return false;
}

/* =========================
 * ====== LINKED LIST ======
 * =========================
 */
struct node *get_new_node(struct packet *pkt)
{
		struct node *ptr = malloc(sizeof(struct node));
		int rv;
		/* Set node vals/pointers */
		if (!ptr) {
				perror("get_new_node, malloc");
				return ptr;
		}
		rv = gettimeofday(&(ptr->timestamp), NULL);
		if (rv != 0)
				perror("get_new_node, gettimeofday");
		ptr->pkt = pkt;
		ptr->next = NULL;
		return ptr;
}

void add_node(struct node **list, struct packet *p)
{
		struct node *n, *head;
		head = *list;
		if (!head) {
				/* If empty list, add head */
				n = get_new_node(p);
				*list = n;
		} else {
				/* Go to end of list and add node */
				for (n = head; n->next != NULL; n = n->next) {;}
				n->next = get_new_node(p);
		}
}

void remove_head(struct node **list)
{
		/* Removes first node (also frees pkt) and sets new head (FIFO)
		 * Both sets the new head given by argument 'list'
		 * and also returns pointer to next head (for use in delete_list-func)
		 */
		struct node *new_head, *head;

		if(!(*list)) {
				fprintf(stderr, RED "Warning:" NRM " trying to remove node from empty list\n.");
				return;
		}
		head = *list;
		new_head = head->next;
		head->next = NULL;
		free_packet(head->pkt);
		free(head);
		*list = new_head;
		return;
}

void delete_list(struct node **list)
{
		while(*list) {
			    remove_head(list);
		}
}

int listsize(struct node **list)
{
		int s = 0;
		for (struct node *n = *list; n != NULL; n = n->next)
				s++;
		return s;
}
