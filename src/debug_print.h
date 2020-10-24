#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H

#include <stdint.h>
#include "network.h"


/* ==============================
 * ====== DEBUG FUNCTIONS =======
 * ==============================
 */

/* Buffer required for printing formatted strings as debug messages */
extern char debug_buf[];
extern int debug_mode;
/* -------------------------
 * -------- GENERAL --------
 * -------------------------
 */
/* Prints msg with perror() if debug_mode is true */
void debug(char *msg);

/* Takes pointer to a buffer (0-terminated) and prints content.
 * Sets all bytes in buffer to 0 after printing.
 * (bufsize given by constant DEBUG_BUFSIZE).
 */
void debugf(char *msg);

/* Prints all strings in C-string array of size c */
void debug_print_array(char *array[], int c);

/* -------------------------
 * -------- NETWORK --------
 * -------------------------
 */
/* [OBSOLETE] Calls print_addr */
void debug_print_addr(struct sockaddr_in *addr);

/* Calls print_packet_meta */
void debug_print_packet_meta(struct packet *p);

/* Calls print_packet */
void debug_print_packet(struct packet *p);

/* Calls print_payload_meta */
void debug_print_payload_meta(struct payload *pl);

/* Calls print_payload */
void debug_print_payload(struct payload *pl);

/* -------------------------
 * --------- FILES ---------
 * -------------------------
 */
/* Calls print_filenames */
void debug_print_filenames(struct string_array *s);

/* Calls print_file */
void debug_print_file(struct file *f);

/* Call print_file_array */
void debug_print_file_array(struct file_array *fa);


/* =============================
 * ===== PRINTER FUNCTIONS =====
 * =============================
 */
/* -------------------------
 * -------- GENERAL --------
 * -------------------------
 */
/* Function to print c-string array of len c */
void print_array(char *array[], int c);

/* Functions picked up from Cbra-lesson.
 * Used to print bytes in binary representation.
 */
void print_bits(uint8_t byte);

void print_n_bits(uint8_t *ptr, int n);

/* Prints <n> number of bytes from buffer.
 * Prints in hex, 16 bytes a row.
*/
void print_bytes(char* buffer, long n);

/* Wrapper which calls print_bytes to print byte content of file-struct */
void print_file_bytes(struct file *f);

/* -------------------------
 * -------- NETWORK --------
 * -------------------------
 */
/* [OBSOLETE] Print contents of net address pointed to by *addr */
void print_addr(struct sockaddr_in *addr);

/* Print packet info (except payload) */
void print_packet_meta(struct packet *p);

/* Print complete packet (including payload, calls print_payload) */
void print_packet(struct packet *p);

/* Print payload (excluding img_bytes) */
void print_payload_meta(struct payload *pl);

/* Print whole payload (including img-bytes) */
void print_payload(struct payload *pl);

/* -------------------------
 * --------- FILES ---------
 * -------------------------
 */
/* Print content of struct file_array */
void print_filenames(struct string_array *s);

/* Print filename and number of bytes */
void print_file(struct file *f);

/* Prints info on all entries in file array */
void print_file_array(struct file_array *fa);

/* -------------------------
 * ------ LINKED LIST ------
 * -------------------------
 */
/* Print info on node:
 * - timestamp (sec)
 * - seqnum of corresponding packet
 * - seqnum of next packet)
*/
void print_node(struct node *n);

/* Call print_node on all entries in list */
void print_list(struct node **list);

#endif /* DEBUG_PRINT_H */
