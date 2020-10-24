#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <sys/time.h>
#include <arpa/inet.h>

#include "my_constants.h"
#include "network.h"
#include "files.h"
#include "debug_print.h"

/* =============================
 * =========== DEBUG ===========
 * =============================
 */

/* -------------------------
 * -------- GENERAL --------
 * -------------------------
 */
void debug(char *msg) {
		if (msg == NULL) {
				perror("Error printing debug msg. Pointer is NULL.");
		} else if (debug_mode)
				printf("==DEBUG== %s\n", msg);
}

void debugf(char *msg)
{
		if (msg == NULL) {
				perror("Error printing formatted debug msg. Pointer is NULL.");
		} else if (debug_mode) {
				printf("==DEBUG== %s", msg);
		}
		memset(msg, 0, DEBUG_BUFSIZE);
}

void debug_print_array(char *array[], int c)
{
		if (array == NULL) {
				perror("Error printing debug msg. Pointer is NULL.");
		} else if (debug_mode) {
				int i;
				for (i = 0; i < c; i++)
						printf("==DEBUG== %s\n", array[i]);
		}
}

/* -------------------------
 * -------- NETWORK --------
 * -------------------------
 */

/* OBSOLETE! */
void debug_print_addr(struct sockaddr_in *addr)
{
		if (debug_mode)
				print_addr(addr);
}

void debug_print_packet_meta(struct packet *p)
{
		if (debug_mode) {
				puts("\n===== DEBUG - printing packet metadata =====");
				print_packet_meta(p);
		}
}

void debug_print_packet(struct packet *p)
{
		if (debug_mode) {
				puts("\n===== DEBUG - printing complete packet =====");
				print_packet(p);
		}
}

void debug_print_payload_meta(struct payload *pl)
{
		if (debug_mode) {
				puts("\n===== DEBUG - printing payload metadata =====");
				print_payload_meta(pl);
		}
}

void debug_print_payload(struct payload *pl)
{
		if (debug_mode) {
				puts("\n===== DEBUG - Printing payload ======");
				print_payload(pl);
		}
}

/* -------------------------
 * --------- FILES ---------
 * -------------------------
 */
void debug_print_filenames(struct string_array *sa)
{
		if (debug_mode) {
				puts("\n===== DEBUG - Printing filenames in struct ======");
				print_filenames(sa);
		}
}

void debug_print_file_array(struct file_array *fa)
{
		if (debug_mode) {
				puts("\n===== DEBUG - Printing array with loaded files ======");
				print_file_array(fa);
		}
}

void debug_print_file(struct file *f)
{
		if (debug_mode) {
				puts("\n===== DEBUG - Printing file info ======");
				print_file(f);
		}
}


/* =============================
 * =========== PRINT ===========
 * =============================
 */

/* -------------------------
 * -------- GENERAL --------
 * -------------------------
 */
void print_array(char *array[], int c)
{
		int i;
		for (i = 0; i < c; i++)
				printf("%s\n", array[i]);
}

void print_bits(uint8_t byte)
{
		uint8_t needle = 0x80;  /* 0b10000000 */
		while(needle) {
				printf("%c", (needle & byte) ? '1' : '0');
				needle >>= 1;
		}
		printf("\n");
}

void print_n_bits(uint8_t *ptr, int n)
{
		while(n--)
				print_bits(*ptr++);
}

void print_bytes(char *buffer, long n)
{
		char *c = buffer;
		long i, j, k;
		for (i = 0; i < n && i < n;) {
				printf("    ");
				for (j = 0; j < 8 && i < n; j++) {
						for (k = 0; k < 2 && i < n; k++) {
								printf("%02x", *c++);
								i++;
						}
						printf(" ");
				}
				printf("\n");
		}
		printf("\n");
}

void print_file_bytes(struct file *f)
{
		char *c = f->bytes;
		long n = (long) f->n_bytes;
		printf("--- File: '%s' - Bytes (hex) ---\n", f->filename);
		print_bytes(c, n);
}


/* -------------------------
 * -------- NETWORK --------
 * -------------------------
 */

/* OBSOLETE! */
void print_addr(struct sockaddr_in *addr)
{
		char *addr_buf = malloc(INET_ADDRSTRLEN * sizeof(char));
		char *str_buf = malloc(DEBUG_BUFSIZE * sizeof(char));
		if (addr_buf == NULL || str_buf == NULL) {
				fprintf(stderr, "Error: malloc in debug_print_addr. Returning.\n");
				return;
		}
		printf("Content address struct:");
		printf("\n          Family:  %d\n          Portnum: %d\n          Address: %s\n",
			   addr->sin_family,
			   ntohs(addr->sin_port),
			   inet_ntop(AF_INET,
						 &(addr->sin_addr.s_addr),
						 addr_buf,
						 INET_ADDRSTRLEN));
		free(addr_buf);
		free(str_buf);
}


void print_packet_meta(struct packet *p)
{
		printf("\n----------------- PACKET SEQ NUM " YEL "%d" NRM " -----------------\n", p->seqnum);
		printf("-Field-            -Decimal-    -Hex-       -Binary-\n");
		printf(" Packet length:        %4d     %4x\n", ntohl(p->len), ntohl(p->len));
		printf(" Sequence number:      %4d     %4x\n", p->seqnum, p->seqnum);
		printf(" Seq num last recv:    %4d     %4x\n", p->seqnum_last_recv, p->seqnum_last_recv);
		printf(" Flag:                          %4x       ", p->flag);
		print_bits(p->flag);
}

void print_packet(struct packet *p)
{
		print_packet_meta(p);
		if (p->pl) {
				print_payload_meta(p->pl);
		}
		printf("\n");
}

void print_payload_meta(struct payload *pl)
{
		if (pl) {
				printf("\n------------ PAYLOAD " YEL "%d" NRM " ------------\n", ntohl(pl->id));
				printf("-Field-       -Content-\n");
				printf("id:           %8d\n", ntohl(pl->id));
				printf("filename_len: %8d\n", ntohl(pl->filename_len));
				printf("filename:     '%s'\n", pl->filename);
		} else {
				printf("\n[No payload]\n");
		}
}

void print_payload(struct payload *pl)
{
		print_payload_meta(pl);
		if (pl)
				printf("\n   -- [!] Not printing byte content yet [!] --");
}

/* -------------------------
 * --------- FILES ---------
 * -------------------------
 */
void print_filenames(struct string_array *s)
{
		printf("Number of entries: %d\n", s->entries);
		printf("Total size:        %d\n", s->total_size);
		int i;
		for (i = 0; i < s->total_size; i++)
				if (s->strings[i] != NULL)
						printf("%s\n", s->strings[i]);
}

void print_file(struct file *f)
{
		if (f)
				printf("[filename]: %25s,     [n bytes]: %4d\n", f->filename, f->n_bytes);
		else
				printf("file: (null)");
}

void print_file_array(struct file_array *fa)
{
		int i;
		struct file *f;
		for (i = 0; i < fa->total_size; i++) {
				f = fa->files[i];
				if (f) {
						printf("[file num] %3d,     ", i);
						print_file(f);
				}
				else
						printf("Idx        %3d is null\n", i);
		}
}

/* -------------------------
 * --------- FILES ---------
 * -------------------------
 */

void print_node(struct node *n)
{
		if (!n) {
				printf("Node is (null)\n");
				return;
		}
		printf("\n--- NODE ---\n");
		printf("Timestamp (sec):   " YEL "%10ld" NRM "\n", n->timestamp.tv_sec);
		printf("Packet seqnum:     " YEL "%10d" NRM "\n", n->pkt->seqnum);
		if(n->pkt && n->pkt->pl)
				printf("Payload identifier:" YEL "%10d" NRM "\n", ntohl(n->pkt->pl->id));
		if (n->next)
				printf("Next seqnum:       " YEL "%10d" NRM "\n", n->next->pkt->seqnum);
		else
				printf("Next seqnum:       %10s\n", "(null)");
}

void print_list(struct node **list)
{
		struct node *n, *head;
		head = *list;
		if (head)
				for (n = head; n != NULL; n = n->next)
						print_node(n);
		else
				printf("(list empty)\n");
}
