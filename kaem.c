/* Copyright (C) 2016 Jeremiah Orians
 * This file is part of mescc-tools.
 *
 * mescc-tools is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mescc-tools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mescc-tools.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define FALSE 0
//CONSTANT FALSE 0
#define TRUE 1
//CONSTANT TRUE 1
#define max_string 4096
//CONSTANT max_string 4096
#define max_args 256
//CONSTANT max_args 256

char** tokens;
int command_done;

/* Function for purging line comments */
void collect_comment(FILE* input)
{
	int c;
	do
	{
		c = fgetc(input);
		if(-1 == c)
		{
			file_print("IMPROPERLY TERMINATED LINE COMMENT!\nABORTING HARD\n", stderr);
			exit(EXIT_FAILURE);
		}
	} while('\n' != c);
}

/* Function for collecting RAW strings and removing the " that goes with them */
int collect_string(FILE* input, int index, char* target)
{
	int c;
	do
	{
		c = fgetc(input);
		if(-1 == c)
		{ /* We never should hit EOF while collecting a RAW string */
			file_print("IMPROPERLY TERMINATED RAW string!\nABORTING HARD\n", stderr);
			exit(EXIT_FAILURE);
		}
		else if('"' == c)
		{ /* Made it to the end */
			c = 0;
		}
		target[index] = c;
		index = index + 1;
	} while(0 != c);
	return index;
}

/* Function to collect an individual argument or purge a comment */
char* collect_token(FILE* input)
{
	char* token = calloc(max_string, sizeof(char));
	char c;
	int i = 0;
	do
	{
		c = fgetc(input);
		if(-1 == c)
		{ /* Deal with end of file */
			file_print("execution complete\n", stderr);
			exit(EXIT_SUCCESS);
		}
		else if((' ' == c) || ('\t' == c))
		{ /* space and tab are token seperators */
			c = 0;
		}
		else if('\n' == c)
		{ /* Command terminates at end of line */
			c = 0;
			command_done = 1;
		}
		else if('"' == c)
		{ /* RAW strings are everything between a pair of "" */
			i = collect_string(input, i, token);
			c = 0;
		}
		else if('#' == c)
		{ /* Line comments to aid the humans */
			collect_comment(input);
			c = 0;
			command_done = 1;
		}
		else if('\\' == c)
		{ /* Support for end of line escapes, drops the char after */
			fgetc(input);
			c = 0;
		}
		token[i] = c;
		i = i + 1;
	} while (0 != c);

	if(1 == i)
	{ /* Nothing worth returning */
		return NULL;
	}
	return token;
}

int main(int argc, char** argv, char** envp)
{
	char* filename = "kaem.run";
	FILE* script = NULL;

	int i = 1;
	if(NULL != argv[1])
	{
		filename = argv[1];
	}

	script = fopen(filename, "r");

	if(NULL == script)
	{
		file_print("The file: ", stderr);
		file_print(filename, stderr);
		file_print(" can not be opened!\n", stderr);
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		tokens = calloc(max_args, sizeof(char*));

		int i = 0;
		int status = 0;
		command_done = 0;
		do
		{
			char* result = collect_token(script);
			if(0 != result)
			{ /* Not a comment string but an actual argument */
				tokens[i] = result;
				i = i + 1;
			}
		} while(0 == command_done);

		if(0 < i)
		{
			file_print(" +> ", stdout);
			int j;
			for(j = 0; j < i; j = j + 1)
			{
				file_print(tokens[j], stdout);
				fputc(' ', stdout);
			}
			file_print("\n", stdout);
		}

		if(0 < i)
		{ /* Not a line comment */
			char* program = tokens[0];
			if(NULL == program)
			{
				file_print(tokens[0], stderr);
				file_print("Some weird shit went down with: ", stderr);
				file_print("\n", stderr);
				exit(EXIT_FAILURE);
			}

			int f = fork();
			if (f == -1)
			{
				file_print("fork() failure", stderr);
				exit(EXIT_FAILURE);
			}
			else if (f == 0)
			{ /* child */
				/* execve() returns only on error */
				execve(program, tokens, envp);
				/* Prevent infinite loops */
				exit(EXIT_SUCCESS);
			}

			/* Otherwise we are the parent */
			/* And we should wait for it to complete */
			waitpid(f, &status, 0);

			if(0 != status)
			{ /* Clearly the script hit an issue that should never have happened */
				file_print("Subprocess error\nABORTING HARD\n", stderr);
				/* stop to prevent damage */
				exit(EXIT_FAILURE);
			}
			/* Then go again */
		}
	}

	return EXIT_SUCCESS;
}
