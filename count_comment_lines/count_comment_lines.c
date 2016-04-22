/* © Copyright 2014 Li Zheng. All Rights Reserved. */

/*
 * INCLUDE_FILES
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

/*
 * DEFINES
 */
#define FILE_PATH_MAX_LEN 100

typedef enum _bool {
	false = 0,
	true = !false
} bool;

/*
 * VARIABLES
 */
static unsigned int g_code_lines = 0;
static unsigned int g_comment_lines = 0;
static unsigned int g_mix_lines = 0;
static unsigned int g_blank_lines = 0;

bool g_is_code = false;
static bool g_is_comment = false;

/*
 * LOCAL_PROTOTYPES
 */
static size_t get_file_size(const char *path);

inline static void count_line();
inline static void clean_flag();

static int handle_entry(const void *p_file_content, const size_t file_size);
static int handle_oneline_comment(char **p_data, size_t *size);
static int handle_multilines_comment(char **p_data, size_t *size);
static int handle_quotes(char **p_data, size_t *size);

/*
 * LOCAL_FUNCTIONS
 */
int main(int argc, const char *argv[])
{
	char filename[FILE_PATH_MAX_LEN];
	void *p_file_content;
	size_t file_size;
	FILE *f;

	printf("Input the file name: ");
	scanf("%s", filename);

	if ((file_size = get_file_size(filename)) == 0) {
		printf("Empty file.\n");
		return 0;
	}

	if ((f = fopen(filename, "r")) == NULL) {
		printf("open error for %m\n");
		exit(1);
	}

	if ((p_file_content = malloc(file_size + 1)) == NULL) {
		printf("malloc error.\n");
		exit(1);
	}
	fread(p_file_content, file_size, 1, f);
	fclose(f);

	handle_entry(p_file_content, file_size);
	free(p_file_content);

	printf("The code lines: %d;\nThe comment lines: %d;\nThe blank lines: %d;\nThe mix lines: %d.\n",
			g_code_lines, g_comment_lines, g_blank_lines, g_mix_lines);

	return 0;
}

static size_t get_file_size(const char *path)
{
	size_t filesize = 0;
	struct stat statbuff;
	if (stat(path, &statbuff) < 0) {
		printf("get file stat error for %m\n");
		exit(1);
	} else {
		filesize = statbuff.st_size;
	}
	return filesize;
}

inline static void count_line()
{
	g_is_comment ?
		(g_is_code ? g_mix_lines++ : g_comment_lines++) :
		(g_is_code ? g_code_lines++ : g_blank_lines++);
}

inline static void clean_flag()
{
	g_is_code = false;
	g_is_comment = false;
}

static int handle_entry(const void *p_file_content, const size_t file_size)
{
	char *p_data = (char *)p_file_content;
	size_t size = file_size;

	if (p_data == NULL || size <= 0) {
		return -1;
	}

	while (size > 0) {
		switch (*p_data) {
		/* exclude spaces, tabs */
		case ' ':
		case '\t':
			break;

		/* line break */
		case '\n':
			count_line();
			clean_flag();
			break;

		/* symbol */
		case '/':
			if (p_data[1] == '/') handle_oneline_comment(&p_data, &size);
			if (p_data[1] == '*') handle_multilines_comment(&p_data, &size);
			break;
		case '\"':
			handle_quotes(&p_data, &size);
			break;

		/* code */
		default:
			g_is_code = true;
			break;
		}
		p_data++;
		size--;
	}

	return 0;
}

/* When return, the ptr point to the char before '\n' */
static int handle_oneline_comment(char **p_data, size_t *size)
{
	if (p_data == NULL || size == NULL) {
		return -1;
	}

	g_is_comment = true;

	while (**p_data != '\n' && *size > 0) {
		(*p_data)++;
		(*size)--;
	}

	(*p_data)--;
	(*size)++;
	return 0;
}

/* When return, the ptr point to '/' */
static int handle_multilines_comment(char **p_data, size_t *size)
{
	if (p_data == NULL || size == NULL) {
		return -1;
	}

	g_is_comment = true;

	while (*size > 0) {
		switch (**p_data) {
		case '*':
			if ((*p_data)[1] == '/') goto out;
			else break;
		case '\n':
			count_line();
			g_is_code = false;
			break;
		default:
			break;
		}
		(*p_data)++;
		(*size)--;
	}

out:
	(*p_data)++;
	(*size)--;
	return 0;
}

/* When return, the ptr point to '\"' */
static int handle_quotes(char **p_data, size_t *size)
{
	if (p_data == NULL || size == NULL) {
		return -1;
	}

	g_is_code = true;

	while (*size > 0) {
		switch (**p_data) {
		case '\"':
			return 0;
		case '\n':
			count_line();
			g_is_comment = false;
			break;
		default:
			(*p_data)++;
			(*size)--;
			break;
		}
	}

	printf("error: file end without '\"'.\n");
	return -1;
}
