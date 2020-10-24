#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>

#include <arpa/inet.h>

#include "files.h"


/* =============================
 * ====== CONSTS and VARS ======
 * =============================
 */
#define DATA 0x1
#define ACK  0x2
#define TERM 0x4

#define WINSIZE 7

/* payload identifier used in application layer */
extern int32_t pl_identifier;

/* =======================
 * ======= STRUCTS =======
 * =======================
 */

/* Payload struct. Filename cannot contain directories.
 *
 * Bytes-ptr in this struct points to the bytes in struct file *f.
 * The filename-ptr points to the filename in a file struct (C-string).
 * In other words, each payload struct does *not* copy the referenced data,
 * but rather points to dynamic data structures (file structs).
 *
 * id:           unique number for each request.
 * filename_len: length of filename in byte (including terminating 0).
 * filename:     C-string.
 * bytes:        bytes of images transferred.
 */
struct payload {
		int32_t id;
		int32_t filename_len;
		char *filename;
		char *bytes;
}__attribute__((packed));


/* Packet implemented as a struct according to protocol in assignment.
 * Values should be in network byte order (for cross-platform compatibility).
 * Payload is a pointer to a payload-struct, since payload-sizes varies.
 * If packet is an ACK or term-packet, payload-pointer is NULL (no payload).
 *
 * len:              total length (including payload).
 * seqnum:           sequence number of this packet.
 * seqnum_last_recv: sequence number of last received packet (ACK).
 * flag: (only one can be set at any time).
 *       0x1: 1 if packet contains data.
 *       0x2: 1 if packet contains an ACK (no payload).
 *       0x4: 1 if packet is terminating connection.
 * unused:           unused byte, should always be 0x7f
 * pl:               pointer to payload.
 */
struct packet {
		int32_t len;
		uint8_t seqnum;
		uint8_t seqnum_last_recv;
		uint8_t flag;
		uint8_t unused;
		struct payload* pl;
}__attribute__((packed));


/* Node for linked list.
 * timestamp is obtained with gettimeofday()
 * pkt: pointer to a packet.
 * next: pointer to next node (or NULL if tail)
 */
struct node {
		struct timeval timestamp;
		struct packet *pkt;
		struct node *next;
};

/* =======================
 * ======= PACKETS =======
 * =======================
 */
/* Checks if flag is valid, and unused bit is set correctly */
bool valid_packet(struct packet *p);

/* Returns a malloced packet struct with no payload-pointer (NULL).
 * If bytes in buf (from byte 0 to 7) is not a valid packet-header, NULL is returned.
 */
struct packet *get_packet_header(char *buf);


/* Prepare a packet with the values given (see struct above for details),
 * and return pointer to this struct.
 * Handles ACK, TERM and DATA-type packets (macros defined at top of this header)
 * For ACK and TERM-packet, the opt_data and ()pl_id arguments are ignored.
 * For DATA-packets, opt_data must be a pointer to a file struct, and payload-identifier
 * (pl_id) should be the next valid value for application layer to receive.
 */
struct packet *prep_packet(uint8_t type,
						   int8_t seqnum,
						   int8_t seqnum_last_recv,
						   void *opt_data,
						   int32_t pl_id);

/* Returns a pointer to a payload-struct
 * (See struct definition above for details).
 * Is used by prep_packet internally.
 */
struct payload *prep_payload(struct file *f, int32_t pl_id);

/* Function loads packet passed as arg (prepared with above functions) to tmp buffer
 * and sends content of this buffer to address given.
 * Sets buffer <buf> to zero unless error occurs.
 * Does not modify or free any of the structs passed.
 */
int load_and_send_packet(struct packet *pkt,
						 char *buf,
						 int sockfd,
						 struct sockaddr *dest_addr,
						 socklen_t addrlen);

/* Used server side to unpack payload (copy from buffer to data structs)
 * Returns pointer to dynamically allocated file struct.
 */
struct file *unpack_payload(char *pl_buf, int32_t payload_len);

/* Frees all malloced memory in packet struct,
 * including payload (if any) and the packet-ptr itself.
 * Calls free_payload to free any payload structs.
 */
void free_packet(struct packet*);

/* Frees all malloced memory pointed to by the content of the payload struct.
 * Includes bytes, filename as well as the struct ptr itself
 */
void free_payload(struct payload*);

/* Given the expected seqnum, window size and maximum number of seqnums,
 * check if received seqnum is within boundary of already received packets
 * (and thus should be reACKed upon reception)
 */
bool already_received(uint8_t seqnum, uint8_t exp_seqnum, uint8_t window_size, uint8_t max_no_seqnums);

/* =========================
 * ====== LINKED LIST ======
 * =========================
 */

/* Returns pointer to a new malloced node.
 * Should not be used directly (!), use add_node instead to add nodes to the list.
 * Node.ptr is set to the packet passed.
 * Malloced memory is freed with remove_head().
 */
struct node *get_new_node(struct packet *pkt);

/* Adds a new node to the end of the linked list (FIFO).
 * list is a double pointer to the first node (head).
 * Uses get_new_node internally. Also handles empty lists.
 */
void add_node(struct node **list, struct packet *p);

/* Removes the head of the list supplied.
 * Frees the corresponding structs (pkts),
 * and sets the head (pointed to by **list) to the next node, or NULL.
 */
void remove_head(struct node **list);

/* Does iterative calls on remove_head to remove all entries in list */
void delete_list(struct node **list);

int listsize(struct node **list);

#endif /* NETWORK_H */
