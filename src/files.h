#ifndef FILES_H
#define FILES_H

#include <stdbool.h>

/* ================================
 * ====== STRUCT DEFINITIONS ======
 * ================================
 */

/* Struct with pointers to C-strings (here: filenames).
* Use corresponding functions to do reallocs and edit initialized struct.
 * entries: number of actual pointers in 'strings' (pointers to C-strings).
 * total_size: number of pointers (initialized or NULL) which memory is allocated for.
 */
struct string_array {
		int entries;
		int total_size;
		char **strings;
};

/* Struct with pointers to array if file structs (here: image-files).
 * Use corresponding functions to do reallocs and edit initialized struct.
 * entries: number of actual pointers in 'bytes' (containing byte-arrays).
 * total_size: number of pointers (initialized or NULL) which memory is allocated for.
 */
struct file_array {
		int entries;
		int total_size;
		struct file **files;
};

/* File struct, pointed to by file_array.files.
 * Contains 'n_bytes' number of raw bytes, and pointer to the raw bytes.
 */
struct file {
		int32_t n_bytes;
		char *filename;
		char *bytes;
};


/* struct which file_array and string_array can be cast to
 * so that realloc_byte_array() can adjust their sizes.
*/
struct byte_array {
		int entries;
		int total_size;
		void **pointers;
};


/* ================================
 * ======= FILE INTERACTION =======
 * ================================
 */

/* Checks if given filename is valid.
 * Prints error message if not, and returns false.
 */
bool valid_filename(char filename[]);

/* Check that given path is a regular file (as defined in 'man 7 inode') */
bool is_reg_file(char path[]);

/* Check if file exists */
bool file_exists(char filename[]);

/* Check if program has read and write permissions to file */
bool file_accessible(char filename[]);

/* Get the number of bytes in file */
int32_t get_filesize(FILE *fd);

/* Wrapper around libgen-basename function, but this function ensures that the string
 * passed is *not* modified (not guaranteed by all basename-implementations).
 * Also, pointer returned from libgen basename-func shall, according to man-pages,
 * not be freed. To have full control over the memory allocations I copy the content
 * of the string to self-managed malloced memory.
 * Function returns a pointer to the malloced memory (a C-string).
 */
char *get_basename(char path[]);

/* Function returns filehandler to an opened file given by argument char[] filename.
 * Tests result, prints error message if fopen is unsuccessful
 * Filehandler is returned on success, NULL on failure
 */
FILE *open_file(char filename[], char mode[]);

/* Read all valid filenames in given directory to the string array.
 * Internally this function uses the functions above to check validity.
 */
int read_strings_from_dir(struct string_array *sa, char dir[]);

/* Read line by line from file (here: filenames listed in file 'filename')
 * and fill struct string_array s with filenames.
 * Prints error messages and returns FAILURE on error.
 */
int read_strings_from_file(struct string_array *s, char filename[]);

/* Allocate memory according to get_filesize,
 * read from file to this memory area and return the corresponding pointer.
 * (Pointer must either be freed seperately, or added to a struct file_array which
 * is freed with corresponding cleanup-function).
 */
struct file *read_bytes_from_file(FILE *fd);

/* Opens file given by <filename> and uses read_bytes_from_file
 * to obtain a pointer to a file-struct.
 * Returns pointer to this struct. Closes file when finished,
 * prints error message and returns NULL on error.
 * (Is used by add_to_file_array to obtain file-structs).
 */
struct file *get_file(char filename[]);

/* Writes line to file */
int write_to_file(char *line, FILE *fd);

/* Check error indicator for given file <FILE *fh>, and print error message if set.
 * Return true if set and false otherwise.
 */
bool error_flag_file(FILE *fh, char calling_function[]);


/* ================================
 * ==== DATA STRUCT FUNCTIONS  ====
 * ================================
 */

/* Set pointers in array from idx 0 to n to NULL */
void set_pointers_null(void **array, int n);

/* Allocate memory to string-array in struct s.
 * Does an allocate with an initial size if number of entries in s is 0.
 * Does reallocate if already entries in s->strings.
 * Prints error message and returns FAILURE on error.
 */
int realloc_byte_array(struct byte_array *s);

/* Adds filename dynamically to C-string array in struct s.
 * Calls realloc_byte_array if number of entries is equal to total_size
 * (array is full).
 * Prints error message and returns FAILURE on error.
 */
int add_filename(struct string_array *s, char filename[]);

/* Create a new file-struct with <filename> and add this to file-array
 * pointed to by file_array struct fa.
 * Calls realloc_byte_array if number of entries is equal to total_size
 * (array is full).
 * Prints error message and returns FAILURE on error.
 * Uses get_file internally to obtain a file-struct (with bytes
 * from file) and add this to file_array.
 */
int add_file_to_array(struct file_array *fa, char filename[]);

/* Compares two file-structs byte for byte.
 * Returns true if content of byte-array are identical
 * [TODO: implement]
 */
/* bool compare_files(struct file*, struct file*); */

/* Compare content of two file-structs and returns true if equal.
 * Internally this function uses Image_compare supplied by pgm.h
 */
bool compare_images(struct file*, struct file*);

/* Uses compare_files to compare content of file-struct with
 * all entries in file_array-struct.
 * Returns pointer to first matching file, NULL if no matches.
 */
struct file *compare_to_all_files(struct file_array*, struct file*);


/* ================================
 * ============ MISC ==============
 * ================================
 */

/* Concatenate two strings. Also adds a newline.
 * Returns ptr to malloced string, which must be freed.
 * (Used to concatenate filenames which are then sent to write_to_file).
 */
char *concat_strings_nl(char *line1, char *line2);


/* ================================
 * =========== CLEANUP  ===========
 * ================================
 */

/* Free content in file-struct */
void free_file(struct file *f);

/* Free content of file_array-struct */
void free_file_array(struct file_array *fa);

/* Frees all memory pointed to by pointers in s->strings.
 * (But not the s->strings pointer itself!)
 */
void free_string_array(struct string_array *sa);

#endif /* FILES_H */
