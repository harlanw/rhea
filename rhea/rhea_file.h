#ifndef RHEA_FILE_H
#define RHEA_FILE_H

typedef enum filetype { FT_NONE = 0, FT_IHEX, FT_ELF } filetype_t;

typedef struct file
{
	const char *path;
	filetype_t type;
} file_t;

#endif
