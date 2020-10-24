#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>

#include "files.h"
#include "my_constants.h"
#include "debug_print.h"
#include "pgmread.h"


/* ================================
 * ======= FILE INTERACTION =======
 * ================================
 */
bool valid_filename(char filename[])
{
		int res;
		res = open(filename, O_RDONLY);
		if (-1 == res) {
				/* If other error than nonexistent file, print msg */
				if (ENOENT != errno)
						perror("Error in valid_filename");
				fprintf(stderr, "Filename '%s' is not valid.\n", filename);
				return false;
		}
		res = close(res);
		if (-1 == res)
				perror("Error in valid_filename");
		return true;
}

bool is_reg_file(char path[])
{
		struct stat statbuf;
		if ((lstat(path, &statbuf)) != 0) {
				perror("is_reg_file, lstat");
				return false;
		}
		if ((statbuf.st_mode & S_IFMT) != S_IFREG) {
				snprintf(debug_buf, DEBUG_BUFSIZE,
						 RED "Warning:" NRM " File '%s' is not a regular file.\n", path);
				debugf(debug_buf);
				return false;
		}
		return true;
}

bool file_exists(char filename[]) {
		int res;
		res = access(filename, F_OK);
		if (0 == res)
				return true;
		return false;
}

bool file_accessible(char filename[])
{
		int res;
		res = access(filename, R_OK | W_OK);
		if (0 == res)
				return true;
		fprintf(stderr, "In file_accessible, %s: ", filename);
		perror("");
		return false;
}

int32_t get_filesize(FILE *fd)
{
		int res;
		struct stat st;
		res = fstat(fileno(fd), &st);
		if (-1 == res) {
				perror("Error in get_filesize");
				return FAILURE;
		}
		return (int32_t) st.st_size;
}

FILE *open_file(char filename[], char mode[])
{
		FILE *fh = fopen(filename, mode);
		if (fh == NULL) {
				fprintf(stderr, "Error when trying to open file called '%s':\n      ", filename);
				perror("");
		}
		return fh;
}

char *get_basename(char path[])
{
		/* Ensures a malloced, self-managed
		 * C-String, and does *not* modify argument.
		 */
		char *copy_orig, *bn_tmp, *bn;
		copy_orig = strdup(path);
		if (!copy_orig) {
				perror("get_basename: malloc");
				return NULL;
		}
		/* tmp basename, *not* to be freed */
		bn_tmp = basename(copy_orig);
		/* Copy result, is returned at end */
		bn = strdup(bn_tmp);
		free(copy_orig);
		return bn;
}

int read_strings_from_dir(struct string_array *sa, char dir[])
{
		DIR *dirp;
		struct dirent *ptr;
		char filename[256];  /* Assuming no filenames larger than UNIX-std */

		dirp = opendir(dir);
		if (NULL == dirp) {
				perror("read_string_from_dir, opendir");
				return FAILURE;
		}
		/* Get dirent struct and go through this
		* adding filenames of valid files to string_array sa
		*/
		errno = 0;
		while ((ptr = readdir(dirp))) {
				strcpy(filename, dir);
				strcat(filename, "/");
				strcat(filename, ptr->d_name);
				if (is_reg_file(filename) && file_accessible(filename))
						add_filename(sa, filename);
		}
		if (errno != 0)
				perror("Error in read_strings_from_dir");
		if (0 != closedir(dirp)) {
				perror("read_strings_from_dir, closedir");
				return FAILURE;
		}
		return SUCCESS;
}

int read_strings_from_file(struct string_array *sa, char filename[])
{
		FILE *fd;
		fd = open_file(filename, "r");
		if (NULL == fd)
				return FAILURE;

		/* Assuming no filename is longer than 255 */
		int len = 256;
		char *line, c;
		char buffer[len];
		memset(buffer, 0, len);

		/* Read line by line from file */
		do {
				line = fgets(buffer, len, fd);
				if (line == NULL && !(error_flag_file(fd, "read_strings_from_file")))
						return FAILURE;
				/* Replace newline with 0 */
				buffer[strlen(buffer) - 1] = '\0';
				/* Add filename to filename-array if a valid filename */
				if (valid_filename(buffer))
						add_filename(sa, buffer);

				/* Check if EOF */
				c = getc(fd);
				if(feof(fd))
						break;
				ungetc(c, fd);
		} while (true);

		if (fclose(fd) != 0)
				perror("Error when closing file desc in read_strings_from_file");
		debug("Finished reading file.\n");
		return SUCCESS;
}

struct file *read_bytes_from_file(FILE *fd)
{
		struct file *f;
		int32_t filesize, rc;
		char *read_bytes;
		/* Get file size */
		filesize = get_filesize(fd);
		snprintf(debug_buf, DEBUG_BUFSIZE, "Filesize is: %d\n", filesize);  /* DEBUG */
		debugf(debug_buf);                                                   /* DEBUG */
		/* Allocate memory to actual number of bytes */
		read_bytes = malloc(filesize * sizeof(char));
		/* Write to malloced memory */
		rc = fread(read_bytes, sizeof(char), filesize, fd);
		snprintf(debug_buf, DEBUG_BUFSIZE, "Read count is: %d\n", rc);  /* DEBUG */
		debugf(debug_buf);                                               /* DEBUG */
		if (rc != filesize) {
				error_flag_file(fd, "read_bytes_from_file");
				fprintf(stderr, "Read count does not equal filesize\n");
				return NULL;
		}
		f = malloc(sizeof(struct file));
		if (NULL == f) {
				perror("Error during malloc in read_bytes_from_file");
				return NULL;
		}
		/* Set file struct pointers and size info */
		f->n_bytes = filesize;
		f->bytes = read_bytes;
		return f;
}

struct file *get_file(char filename[])
{
		int res;
		FILE *fd;
		struct file *f;
		char *fn;

		snprintf(debug_buf, DEBUG_BUFSIZE, "Getting file: %s\n", filename); /* DEBUG */
		debugf(debug_buf);                                                  /* DEBUG */


		/* Open file */
		fd = open_file(filename, "rb");
		if (NULL == fd)
				return NULL;

		/* Get pointer to malloced file struct */
		f = read_bytes_from_file(fd);
		if (NULL == f)
				return NULL;

		/* Duplicate filename, and add to struct file */
		fn = strdup(filename);
		if (NULL == fn) {
				perror("Error during strdup in get_file");
				return NULL;
		}
		f->filename = fn;

		/* Close file */
		res = fclose(fd);
		if (0 != res) {
				error_flag_file(fd, "get_file");
		}
		return f;
}

int write_to_file(char *line, FILE *fd)
{
		int wc;
		wc = fwrite(line, strlen(line), 1, fd);
		if (wc < 0) {
				error_flag_file(fd, "write_to_file");
				fprintf(stderr, "Unknown error in write_to_file\n");
				fprintf(stderr, "wc:           %d\n", wc);
				fprintf(stderr, "strlen(line): %ld\n", strlen(line));
				return FAILURE;
		}
		return SUCCESS;
}

bool error_flag_file(FILE *fh, char calling_function[])
{
		if (ferror(fh)) {
				fprintf(stderr, "Error when reading from or writing to file. ");
				perror("");
				fprintf(stderr, "Calling function: %s\n", calling_function);
				return true;
		} else {
				return false;
		}
}

/* ================================
 * ==== DATA STRUCT FUNCTIONS  ====
 * ================================
 */
void set_pointers_null(void **array, int n)
{
		int i;
		for (i = 0; i < n; i++)
				array[i] = NULL;
}

int realloc_byte_array(struct byte_array *b)
{
		int initial_size, current_size, new_size;
		void **ptr;
		/* If empty: initialize to 8 pointers */
		if (b->total_size == 0) {
				initial_size = 8;
				ptr = malloc(initial_size * sizeof(void*));
				if (ptr == NULL) {
						fprintf(stderr, "Error occured in realloc_byte_array during malloc.\n");
						return FAILURE;
				}
				b->pointers = ptr;
				b->total_size = initial_size;
				set_pointers_null(b->pointers, b->total_size);
		} else {
				/* On realloc: double amount */
				current_size = b->total_size;
				new_size = b->total_size * 2;
				ptr = realloc(b->pointers, (sizeof(void*) * new_size));
				if (ptr == NULL) {
						fprintf(stderr, "Critical error in realloc_byte_array during realloc.\n");
						return FAILURE;
				}
				/* ... and initialize newly allocated memory to zero */
				set_pointers_null(&(ptr[current_size]), (new_size - current_size));
				b->total_size = new_size;
				b->pointers = ptr;
		}
		return SUCCESS;
}

int add_filename(struct string_array *s, char filename[])
{
		int res, current_pos;
		char *ptr;
		/* If string-array is full, realloc */
		if (s->entries == s->total_size) {
				res = realloc_byte_array((struct byte_array*)s);
				if (FAILURE == res)
						return FAILURE;
		}
		/* Get next free pointer in array and allocate memory for string */
		current_pos = s->entries;
		ptr = malloc((strlen(filename) + 1)  * sizeof(char));
		if (ptr == NULL) {
				fprintf(stderr, "Error in add_filename during malloc\n");
				return FAILURE;
		}
		s->strings[current_pos] = ptr;
		/* Copy string to allocated area */
		strncpy(s->strings[current_pos], filename, (strlen(filename) + 1));
		s->entries += 1;
		return SUCCESS;
}

int add_file_to_array(struct file_array *fa, char filename[])
{
		int res;
		struct file *f;
		/* If array is full, realloc */
		if (fa->entries == fa->total_size) {
				debug("Reallocating file_array");
				res = realloc_byte_array((struct byte_array*)fa);
				if (FAILURE == res)
						return FAILURE;
		}
		f = get_file(filename);
		if (NULL == f) {
				fprintf(stderr, "Error in add_file_to_array\n");
				return FAILURE;
		}

		fa->files[fa->entries] = f;
		fa->entries += 1;

		return SUCCESS;
}

bool compare_files(struct file *f1, struct file *f2)
{
		/* Due to bug in Image_create, buffer must be copied before passed to Image_create */
		int res;
		struct Image *file1, *file2;
		char *tmp_buf1, *tmp_buf2;
		snprintf(debug_buf, DEBUG_BUFSIZE, "Comparing %25s    to %25s\n", f1->filename, f2->filename);   /* DEBUG */
		debugf(debug_buf);  /* DEBUG */

		/* Check if equal size */
		if (f1->n_bytes != f2->n_bytes) {
				debug("Images has different sizes!");
				return false;
		}

		tmp_buf1 = malloc(f1->n_bytes * sizeof(char));
		tmp_buf2 = malloc(f2->n_bytes * sizeof(char));
		if (NULL == tmp_buf1 || NULL == tmp_buf2) {
				fprintf(stderr, "Error during malloc, in compare_files.\n");
				return false;
		}

		/* Copy buffer */
		memcpy(tmp_buf1, f1->bytes, f1->n_bytes);
		memcpy(tmp_buf2, f2->bytes, f2->n_bytes);
		file1 = Image_create(tmp_buf1);
		file2 = Image_create(tmp_buf2);
		free(tmp_buf1);
		free(tmp_buf2);

		snprintf(debug_buf,
				 DEBUG_BUFSIZE,
				 "\n    file1 width and height: [%d, %d]\n    file2 width and height: [%d, %d]\n",
				 file1->width, file1->height, file2->width, file2->height);   /* DEBUG */
		debugf(debug_buf);  /* DEBUG */

		res = 1;
		res = Image_compare(file1, file2);
		Image_free(file1);
		Image_free(file2);

		if (1 != res) return false;
		return true;
}

struct file *compare_to_all_files(struct file_array *fa, struct file *f)
{
		struct file *cmp_f;
		int size, i;
		bool result;
		size = fa->entries;
		for (i = 0; i < size; i++) {
				cmp_f = fa->files[i];
				/* If not null */
				if (cmp_f) {
						result = compare_files(cmp_f, f);
						if (true == result) {
								debug("Found equal file!");
								return cmp_f;
						}
				}
		}
		return NULL;
}

/* ================================
 * ============ MISC ==============
 * ================================
 */
char *concat_strings_nl(char *line1, char *line2)
{
		char *ptr;
		ptr = malloc(sizeof(char) * (strlen(line1) + strlen(line2) + 3));
		strcpy(ptr, line1);
		strcat(ptr, " ");
		strcat(ptr, line2);
		strcat(ptr, "\n");
		return ptr;
}


/* ================================
 * =========== CLEANUP  ===========
 * ================================
 */
void free_file(struct file *f)
{
		free(f->filename);
		free(f->bytes);
		free(f);
}

void free_file_array(struct file_array *fa)
{
		int i;
		struct file *f;
		for (i = 0; i < fa->total_size; i++) {
				f = fa->files[i];
				if (f)
						free_file(f);
		}
		free(fa->files);
}

void free_string_array(struct string_array *sa)
{
		int i;
		for (i = 0; i < sa->total_size; i++)
				free(sa->strings[i]);
		free(sa->strings);
}
