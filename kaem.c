/* Copyright (C) 2016-2020 Jeremiah Orians
 * Copyright (C) 2020 fosslinux
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
#include "kaem.h"

/* Prototypes from other files */
struct Token* handle_variables(char** argv, struct Token* n, int last_rc);

/*
 * UTILITY FUNCTIONS
 */

/* Function to find a character in a string */
char* find_char(char* string, char a)
{
	if(0 == string[0]) return NULL;
	while(a != string[0])
	{
		string = string + 1;
		if(0 == string[0]) return string;
	}
	return string;
}

/* Function to find the length of a char**; an array of strings */
int array_length(char** array)
{
	int length = 0;
	while(array[length] != NULL)
	{
		length = length + 1;
	}
	return length;
}

/* Search for a variable in the env linked-list */
char* env_lookup(char* variable)
{
	/* Start at the head */
	struct Token* n = env;
	/* Loop over the linked-list */
	while(n != NULL)
	{
		if(match(variable, n->var))
		{ /* We have found the correct node */
			return n->value; /* Done */
		}
		/* Nope, try the next */
		n = n->next;
	}
	/* We didn't find anything! */
	return NULL;
}

/* Find the full path to an executable */
char* find_executable(char* name)
{
	if(('.' == name[0]) || ('/' == name[0]))
	{ /* assume names that start with . or / are relative or absolute */
		return name;
	}

	char* trial = calloc(MAX_STRING, sizeof(char));
	require(trial != NULL, "Memory initialization of trial in find_executable failed\n");
	char* MPATH = calloc(MAX_STRING, sizeof(char));
	require(MPATH != NULL, "Memory initialization of MPATH in find_executable failed\n");
	copy_string(MPATH, env_lookup("PATH"));
	FILE* t;
	char* next = find_char(MPATH, ':');
	int index;
	int offset;
	int mpath_length;
	int name_length;
	int trial_length;
	while(NULL != next)
	{
		/* Reset trial */
		trial_length = string_length(trial);
		for(index = 0; index < trial_length; index = index + 1)
		{
			trial[index] = 0;
		}

		next[0] = 0;

		/* prepend_string(MPATH, prepend_string("/", name)) */
		mpath_length = string_length(MPATH);
		for(index = 0; index < mpath_length; index = index + 1)
		{
			require(MAX_STRING > index, "Element of PATH is too long\n");
			trial[index] = MPATH[index];
		}
		trial[index] = '/';
		offset = string_length(trial);
		name_length = string_length(name);
		for(index = 0; index < name_length; index = index + 1)
		{
			require(MAX_STRING > index, "Element of PATH is too long\n");
			trial[index + offset] = name[index];
		}

		/* Try the trial */
		require(string_length(trial) < MAX_STRING, "COMMAND TOO LONG!\nABORTING HARD\n");
		t = fopen(trial, "r");
		if(NULL != t)
		{
			fclose(t);
			return trial;
		}

		MPATH = next + 1;
		next = find_char(MPATH, ':');
	}
	return NULL;
}

/* Function to convert a Token linked-list into an array of strings */
char** list_to_array(struct Token* s)
{
	struct Token* n;
	n = s;
	char** array = calloc(MAX_ARRAY, sizeof(char*));
	require(array != NULL, "Memory initialization of array in conversion of list to array failed\n");
	char* element = calloc(MAX_STRING, sizeof(char));
	require(element != NULL, "Memory initalization of element in conversion of list to array failed\n");
	int index = 0;
	int i;
	int value_length;
	int var_length;
	int offset;
	while(n != NULL)
	{ /* Loop through each node and assign it to an array index */
		array[index] = calloc(MAX_STRING, sizeof(char));
		require(array[index] != NULL, "Memory initialization of array[index] in conversion of list to array failed\n");
		/* Bounds checking */
		/* No easy way to tell which it is, output generic message */
		require(index < MAX_ARRAY, "SCRIPT TOO LONG or TOO MANY ENVARS\nABORTING HARD\n");
		if(n->var == NULL)
		{ /* It is a line */
			array[index] = n->value;
		}
		else
		{ /* It is a var */
			/* prepend_string(n->var, prepend_string("=", n->value)) */
			var_length = string_length(n->var);
			for(i = 0; i < var_length; i = i + 1)
			{
				element[i] = n->var[i];
			}
			element[i] = '=';
			i = i + 1;
			offset = i;
			value_length = string_length(n->value);
			for(i = 0; i < value_length; i = i + 1)
			{
				element[i + offset] = n->value[i];
			}
		}
		copy_string(array[index], element);

		n = n->next;
		index = index + 1;

		/* Reset element */
		for(i = 0; i < MAX_STRING; i = i + 1) element[i] = 0;
	}
	return array;
}

/*
 * TOKEN COLLECTION FUNCTIONS
 */

/* Function for skipping over line comments */
void collect_comment(FILE* input)
{
	int c;
	/* Eat up the comment, one character at a time */
	/* 
	 * Sanity check that the comment ends with \n.
	 * Remove the comment from the FILE*
	 */
	do
	{ 
		c = fgetc(input);
		/* We reached an EOF!! */
		require(EOF != c, "IMPROPERLY TERMINATED LINE COMMENT!\nABORTING HARD\n");
	} while('\n' != c); /* We can now be sure it ended with \n -- and have purged the comment */
}

/* Function for collecting strings and removing the "" pair that goes with them */
int collect_string(FILE* input, struct Token* n, int index, char initial)
{
	int string_done = FALSE;
	char c;
	do
	{
		/* Bounds check */
		require(MAX_STRING > index, "LINE IS TOO LONG\nABORTING HARD\n");
		c = fgetc(input);
		require(EOF != c, "IMPROPERLY TERMINATED STRING!\nABORTING HARD\n");

		if(initial == c)
		{ /* End of string */
			string_done = TRUE;
		}
		else
		{
			require(MAX_STRING > index, "LINE IS TOO LONG\nABORTING HARD\n");
			n->value[index] = c;
			index = index + 1;
		}
	} while(string_done == FALSE);
	return index;
}

/* Function to parse and assign token->value */
int collect_token(FILE* input, struct Token* n)
{
	char c;
	int token_done = FALSE;
	char* token = calloc(MAX_STRING, sizeof(char));
	require(token != NULL, "Memory initialization of token in collect_token failed\n");
	int index = 0;
	do
	{ /* Loop over each character in the token */
		c = fgetc(input);
		/* Bounds checking */
		require(MAX_STRING > index, "LINE IS TOO LONG\nABORTING HARD\n");
		if(EOF == c)
		{ /* End of file -- this means script complete */
			/* We don't actually exit here. This logically makes more sense;
			 * let the code follow its natural path of execution and exit
			 * sucessfuly at the end of main().
			 */
			token_done = TRUE;
			command_done = TRUE;
			return -1;
		}
		else if((' ' == c) || ('\t' == c))
		{ /* Space and tab are token seperators */
			token_done = TRUE;
		}
		else if('\n' == c)
		{ /* Command terminates at end of a line */
			command_done = TRUE;
			token_done = TRUE;
		}
		else if('"' == c || '\'' == c)
		{ /* Handle strings -- everything between a pair of "" or '' */
			if('"' == c) n->type = STRING;
			if('\'' == c) n->type = SSTRING;
			index = collect_string(input, n, index, c);
			token_done = TRUE;
		}
		else if('#' == c)
		{ /* Handle line comments */
			n->type = COMMENT;
			collect_comment(input);
			command_done = TRUE;
			token_done = TRUE;
		}
		else if('\\' == c)
		{ /* Support for escapes; drops the char after */
			/******************************************************************
			 * This is a design decision; it is primarily made for newlines.  *
			 * Unlike the way normal shells do escapes (making the next char  *
			 * actually do something), this just eats it up. Why? Simply, to  *
			 * aid formatting (mostly through newlines). Eventually, we will  *
			 * make it do it the proper way... but for now it's staying like  *
			 * this. Feel free to send in a patch if you have a solution!     *
			 * Because of this, I have decided that since the behaviour is    *
			 * signficant enough, when warnings are enabled, we will give a   *
			 * warning about this.                                            *
			 ******************************************************************/
			c = fgetc(input); /* Skips over \, gets the next char */
			if(WARNINGS && c != '\n')
			{
				file_print("WARNING: The character '", stdout);
				fputc(c, stdout);
				file_print("' just got eaten up because of an unsupported escape sequence; see kaem.c:collect_token for more information.\n", stdout);
			}
			index = index + 2;
		}
		else if(0 == c)
		{ /* We have come to the end of the token */
			token_done = TRUE;
		}
		else
		{ /* It's a character to assign */
			require(MAX_STRING > index, "LINE IS TOO LONG\nABORTING HARD\n");
			n->value[index] = c;
			index = index + 1;
		}
	} while (token_done == FALSE);
	return index;
}

/*
 * EXECUTION FUNCTIONS
 * Note: All of the builtins return FALSE (0) when they exit successfully
 * and TRUE (1) when they fail.
 */

/* Function to check if the token is an envar */
int is_envar(char* token)
{
	int i = 0;
	int token_length = string_length(token);
	while(i < token_length)
	{
		if(token[i] == '=')
		{
			return TRUE;
		}
		i = i + 1;
	}
	return FALSE;
}

/* Add an envar */
int add_envar(char* var, char* value)
{
	/* If we are in init-mode and this is the first var env == NULL, rectify */
	if(env == NULL)
	{
		env = calloc(1, sizeof(struct Token));
		require(env != NULL, "Memory initialization of env failed\n");
	}

	struct Token* n;
	n = env;
	/* Traverse to end of linked-list, stopping if the var already exists */
	while(n->next != NULL)
	{
		if(match(n->var, var))
		{ /* It already exists, overwrite it */
			break;
		}
		n = n->next;
	}
	/* Initialize new node */
	/* 
	 * See first comment, this situation means no new node.
	 * This can also occur when we have found this variable already set and we
	 * want to overwrite it.
	 */
	if((n->value != NULL || n->var != NULL) && n->next == NULL)
	{
		if(!match(n->var, var))
		{
			n->next = calloc(1, sizeof(struct Token));
			require(n->next != NULL, "Memory initialization of next env node in add_envar failed\n");
			n = n->next;
		}
	}

	/* Write n->var */
	if(n->var == NULL)
	{
		n->var = calloc(MAX_STRING, sizeof(char));
	}
	else
	{
		/* Reset n->var */
		int i;
		for(i = 0; i < MAX_STRING; i = i + 1)
		{
			n->var[i] = 0;
		}
	}

	copy_string(n->var, var);

	/* Write n->value */
	if(n->value == NULL)
	{
		n->value = calloc(MAX_STRING, sizeof(char));
	}
	else
	{
		/* Reset n->value */
		int i;
		for(i = 0; i < MAX_STRING; i = i + 1)
		{
			n->value[i] = 0;
		}
	}
	copy_string(n->value, value);

	return FALSE;
}

/* Handle envars */
int handle_envar()
{
	/* Get var_name */
	char* var_name = calloc(MAX_STRING, sizeof(char));
	require(var_name != NULL, "Memory initialization of n->var in add_envar failed\n");
	int index = 0;
	int token_length = string_length(token->value);
	/* Copy into var_name up to = */
	while(token->value[index] != '=')
	{
		if(index >= token_length) return TRUE;
		var_name[index] = token->value[index];
		index = index + 1;
	}

	/* Get value */
	char* value = calloc(MAX_STRING, sizeof(char));
	index = index + 1; /* Skip over = */
	int offset = index;
	value = calloc(MAX_STRING, sizeof(char));
	require(value != NULL, "Memory initialization of n->value in add_envar failed\n");
	/* Copy into n->value up to end of token */
	while(token->value[index] != 0)
	{
		if(index >= token_length) return TRUE;
		value[index - offset] = token->value[index];
		index = index + 1;
	}

	/* Write in */
	return add_envar(var_name, value);
}

/* cd builtin */
int cd()
{
	if(NULL == token->next) return TRUE;
	token = token->next;
	if(NULL == token->value) return TRUE;
	/* TODO: Add detection mechaninism for if chdir() fails. */
	chdir(token->value);
	return FALSE;
}

/* pwd builtin */
int pwd()
{
	char* path = calloc(MAX_STRING, sizeof(char));
	require(path != NULL, "Memory initialization of path in pwd failed\n");
	getcwd(path, MAX_STRING);
	require(path != "", "getcwd() failed\n");
	file_print(path, stdout);
	file_print("\n", stdout);
	return FALSE;
}

/* set builtin */
int set()
{
	/* Get the options */
	char* options = calloc(MAX_STRING, sizeof(char));
	require(options != NULL, "Memory initialization of options in set failed\n");
	int i;
	if(NULL == token->next) return TRUE;
	token = token->next;
	if(NULL == token->value) return TRUE;

	int last_position = string_length(token->value) - 1;
	for(i = 0; i < last_position; i = i + 1)
	{
		options[i] = token->value[i + 1];
	}
	/* Parse the options */
	int options_length = string_length(options);
	for(i = 0; i < options_length; i = i + 1)
	{
		if(options[i] == 'a')
		{ /* set -a is on by default and cannot be disabled at this time */
			if(WARNINGS)
			{
				file_print("set -a is on by default and cannot be disabled\n", stdout);
			}
			continue;
		}
		else if(options[i] == 'e')
		{ /* Fail on failure */
			STRICT = TRUE;
		}
		else if(options[i] == 'x')
		{ /* Show commands as executed */
			/* TODO: this currently behaves like -v. Make it do what it should */
			VERBOSE = TRUE;
			/* 
			 * Output the set -x because VERBOSE didn't catch it before.
			 * We don't do just -x because we support multiple options in one command,
			 * eg set -ex.
			 */
			file_print(" +> set -", stdout);
			file_print(options, stdout);
			file_print("\n", stdout);
			fflush(stdout);
		}
		else
		{ /* Invalid */
			fputc(options[i], stderr);
			file_print(" is an invalid set option!\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	return FALSE;
}

/* echo builtin */
void echo()
{
	if(token->next == NULL)
	{ /* No arguments */
		file_print("\n", stdout);
		return;
	}
	if(token->next->value == NULL)
	{ /* No arguments */
		file_print("\n", stdout);
		return;
	}
	token = token->next; /* Skip the actual echo */
	while(token != NULL)
	{ /* Output each argument to echo to stdout */
		/* M2-Planet doesn't let us do this in the while */
		if(token->value == NULL) break;
		file_print(token->value, stdout);
		if(token->next != NULL) file_print(" ", stdout);
		token = token->next;
	}
	file_print("\n", stdout);
}

/* unset builtin */
void unset()
{
	struct Token* e;
	/* We support multiple variables on the same line */
	struct Token* t;
	t = token->next;
	while(t != NULL)
	{
		e = env;
		/* Look for the variable; we operate on ->next because we need to remove ->next */
		while(e->next != NULL)
		{
			if(match(e->next->var, t->value))
			{
				break;
			}
			e = e->next;
		}
		t = t->next;
		/* If it's NULL nothing was found */
		if(e->next == NULL) continue;
		/* Otherwise there is something to unset */
		e->next = e->next->next;
	}
}

/* Handle shebangs */
char* handle_shebang(char* program)
{
	FILE* script = fopen(program, "r");

	/* Check if the first two characters are #! */
	char c = fgetc(script);
	if(c != '#') return program;
	c = fgetc(script);
	if(c != '!') return program;

	/* Get the rest of the shebang */
	char* shebang = calloc(MAX_STRING, sizeof(char));
	require(shebang != NULL, "Memory initialization of shebang in handle_shebang failed\n");
	int i = 0;
	c = fgetc(script);
	while(c != '\n')
	{
		shebang[i] = c;
		c = fgetc(script);
		i = i + 1;
	}

	/* Is it something we need to look up in PATH? */
	if(shebang[0] != '/' && shebang[0] != '.')
	{
		shebang = find_executable(shebang);
		if(shebang == NULL) return program;
	}

	/* Add to tokens */
	struct Token* n = calloc(1, sizeof(struct Token));
	require(n != NULL, "Memory initialization of n in handle_shebang failed\n");
	n->value = shebang;
	n->next = token;
	token = n;

	/* Recursive shebangs! */
	return handle_shebang(shebang);
}

/* Execute program */
int execute(char** argv)
{ /* Run the command */

	/* rc = return code */
	int rc;

	/* Actually do the execution */
	if(is_envar(token->value) == TRUE)
	{
		rc = handle_envar();
		if(STRICT) require(rc == FALSE, "Adding of an envar failed!\n");
		return 0;
	}
	else if(match(token->value, "cd"))
	{
		rc = cd();
		if(STRICT) require(rc == FALSE, "cd failed!\n");
		return 0;
	}
	else if(match(token->value, "set"))
	{
		rc = set();
		if(STRICT) require(rc == FALSE, "set failed!\n");
		return 0;
	}
	else if(match(token->value, "pwd"))
	{
		rc = pwd();
		if(STRICT) require(rc == FALSE, "pwd failed!\n");
		return 0;
	}
	else if(match(token->value, "echo"))
	{
		echo();
		return 0;
	}
	else if(match(token->value, "unset"))
	{
		unset();
		return 0;
	}
	else if(match(token->value, ""))
	{
		return 0;
	}

	/* If it is not a builtin, run it as an executable */
	char** array;
	char** envp;
	/* Get the full path to the executable */
	char* program = find_executable(token->value);
	/* Check we can find the executable */
	if(NULL == program && NULL != token->value)
	{
		if(STRICT)
		{
			file_print("WHILE EXECUTING ", stderr);
			file_print(token->value, stderr);
			file_print(" NOT FOUND!\nABORTING HARD\n", stderr);
			exit(EXIT_FAILURE);
		}
		/* If we are not strict simply return */
		return 0;
	}
	token->value = program;

	/* Handle shebangs */
	program = handle_shebang(program);

	int f = fork();
	/* Ensure fork succeeded */
	if (f == -1)
	{ 
		file_print("WHILE EXECUTING ", stderr);
		file_print(token->value, stderr);
		file_print("fork() FAILED\nABORTING HARD\n", stderr);
		exit(EXIT_FAILURE);
	}
	else if (f == 0)
	{ /* Child */
		/**************************************************************
		 * Fuzzing produces random stuff; we don't want it running    *
		 * dangerous commands. So we just don't execve.               *
		 * But, we still do the list_to_array calls to check for      *
		 * segfaults.                                                 *
		 **************************************************************/
		array = list_to_array(token);
		envp = list_to_array(env);

		if(FALSE == FUZZING)
		{ /* We are not fuzzing */
			/* execve() returns only on error */
			execve(program, array, envp);
		}
		/* Prevent infinite loops */
		_exit(EXIT_SUCCESS);
	}

	/* Otherwise we are the parent */
	/* And we should wait for it to complete */
	waitpid(f, &rc, 0);

	/*
	 * waitpid(2)'s 'status' includes both the actual return code from
	 * the program and the reason that the program terminated. We do not
	 * want the 'reason code'. Hence, divide by 256 (bitshift of 8) to
	 * remove the 'reason code'.
	 */
	rc = rc / 256;

	return rc;
}

/* Remove spaces from the end of each token */
void strip_whitespace(struct Token* n)
{
	int length = string_length(n->value) - 1;
	while(n->value[length] == ' ')
	{
		n->value[length] = 0;
		length = length - 1;
	}
}

int collect_command(FILE* script, char** argv, int last_rc)
{
	command_done = FALSE;
	/* Initialize token */
	token = calloc(1, sizeof(struct Token));
	require(token != NULL, "Memory initialization of token in collect_command failed\n");
	struct Token* n;
	n = token;
	int index = 0;
	/* Get the tokens */
	while(command_done == FALSE)
	{
		n->value = calloc(MAX_STRING, sizeof(char));
		require(n->value != NULL, "Memory initialization of n->value in collect_command failed\n");
		n->type = NORMAL;
		index = collect_token(script, n);
		/* Don't allocate another node if the current one yielded nothing, OR
		 * if we are done.
		 */
		if((n->value != NULL && match(n->value, "") == FALSE) && command_done == FALSE)
		{
			n->next = calloc(MAX_STRING, sizeof(char));
			require(n->next != NULL, "Memory initialization of next token node in collect_command failed\n");
			n = n->next;
		}
	}
	/* -1 means the script is done */
	if(-1 == index) return index;

	n = token;
	while(n != NULL)
	{ /* Substitute variables into each token */
		if(n->value == NULL) break;
		if(n->type != SSTRING) n = handle_variables(argv, n, last_rc);
		/* There is potential that it could have moved n to the last node */
		if(n->next == NULL) break;
		/* Advance to next node */
		n = n->next;
	}

	/* Strip trailing whitespaces from each token */
	n = token;
	while(n != NULL)
	{
		if(n->value == NULL) break;
		if(n->type != STRING && n->type != SSTRING) strip_whitespace(n);
		/* Advance to next node */
		n = n->next;
	}
	/* Remove empty tokens */
	n = token;
	while(n->next != NULL)
	{
		if(match(n->next->value, "") &&
		  	(n->next->type != STRING && n->next->type != SSTRING))
		{
			n->next = n->next->next;
			if(n->next == NULL) break;
		}
		n = n->next;
	}

	/* Output the command if verbose is set */
	/* Also if there is nothing in the command skip over */
	if(VERBOSE && match(token->value, "") == FALSE)
	{
		n = token;
		file_print(" +> ", stdout);
		while(n != NULL)
		{ /* Print out each token token */
			/* M2-Planet doesn't let us do this in the while */
			if(n->value == NULL) break;
			file_print(n->value, stdout);
			file_print(" ", stdout);
			n = n->next;
		}
		fputc('\n', stdout);
		fflush(stdout);
	}
	return index;
}

/* Function for executing our programs with desired arguments */
void run_script(FILE* script, char** argv)
{
	int status;
	while(1)
	{
		/*
		 * Tokens has to be reset each time, as we need a new linked-list for
		 * each line.
		 * See, the program flows like this as a high level overview:
		 * Get line -> Sanitize line and perform variable replacement etc ->
		 * Execute line -> Next.
		 * We don't need the previous lines once they are done with, so tokens
		 * are hence for each line.
		 */
		int index = collect_command(script, argv, status);
		/* -1 means the script is done */
		if(-1 == index) break;

		/* Stuff to exec */
		status = execute(argv);
		if(STRICT && status != 0)
		{ /* Clearly the script hit an issue that should never have happened */
			exit(status);
		}
	}
}

/* Function to populate env */
void populate_env(char** envp)
{
	/* Initialize env and n */
	env = calloc(1, sizeof(struct Token));
	require(env != NULL, "Memory initialization of env failed\n");
	struct Token* n;
	n = env;

	int i;
	for(i = 0; i < array_length(envp); i = i + 1)
	{
		n->var = calloc(MAX_STRING, sizeof(char));
		require(n->var != NULL, "Memory initialization of n->var in population of env failed\n");
		n->value = calloc(MAX_STRING, sizeof(char));
		require(n->value != NULL, "Memory initialization of n->var in population of env failed\n");
		int j = 0;
		/*
		 * envp is weird.
		 * When referencing envp[i]'s characters directly, they were all jumbled.
		 * So just copy envp[i] to envp_line, and work with that - that seems
		 * to fix it.
		 */
		char* envp_line = calloc(MAX_STRING, sizeof(char));
		require(envp_line != NULL, "Memory initialization of envp_line in population of env failed\n");
		copy_string(envp_line, envp[i]);
		while(envp_line[j] != '=')
		{ /* Copy over everything up to = to var */
			n->var[j] = envp_line[j];
			j = j + 1;
		}
		/* If we get strange input, we need to ignore it */
		if(n->var == NULL) continue;
		j = j + 1; /* Skip over = */
		int k = 0; /* As envp[i] will continue as j but n->value begins at 0 */
		while(envp_line[j] != 0)
		{ /* Copy everything else to value */
			n->value[k] = envp_line[j];
			j = j + 1;
			k = k + 1;
		}
		/* Sometimes, we get lines like VAR=, indicating nothing is in the variable */
		if(n->value == NULL) n->value = "";
		/* Advance to next part of linked list */
		n->next = calloc(1, sizeof(struct Token));
		require(n->next != NULL, "Memory initialization of n->next in population of env failed\n");
		n = n->next;
	}
	/* Get rid of node on the end */
	n = NULL;
	/* Also destroy the n->next reference */
	n = env;
	while(n->next->var != NULL) n = n->next;
	n->next = NULL;
}

int main(int argc, char** argv, char** envp)
{
	VERBOSE = FALSE;
	STRICT = FALSE;
	FUZZING = FALSE;
	WARNINGS = FALSE;
	char* filename = "kaem.run";
	FILE* script = NULL;

	/* Initalize structs */
	token = calloc(1, sizeof(struct Token));
	require(token != NULL, "Memory initialization of token failed\n");

	int i = 1;
	/* Loop over arguments */
	while(i <= argc)
	{
		if(NULL == argv[i])
		{ /* Ignore the argument */
			i = i + 1;
		}
		else if(match(argv[i], "-h") || match(argv[i], "--help"))
		{ /* Help information */
			file_print("Usage: ", stdout);
			file_print(argv[0], stdout);
			file_print(" [OPTIONS] filename [--] arguments \n", stdout);
			file_print(" -h, --help           Output usage\n", stdout);
			file_print(" -V, --version        Output the version\n", stdout);
			file_print(" -i, --init-mode      Strip out the environment\n", stdout);
			file_print(" -v, --verbose        Output each command before it is run\n", stdout);
			file_print(" --strict             Exit on subprocess failure\n", stdout);
			file_print(" --warn               Output warnings for potentially undesired behaviour (useful in development)\n", stdout);
			file_print(" --fuzz               Don't actually run the commands, so fuzzing dosen't execute dangerous commands\n", stdout);
			file_print(" -f, --file           Legacy way to specify the filename\n", stdout);
			file_print(" --                   Seperates kaem arguments from script arguments, only used but required when -f is used\n", stdout);
			exit(EXIT_SUCCESS);
		}
		else if(match(argv[i], "-f") || match(argv[i], "--file"))
		{ /* Set the filename */
			if(argv[i + 1] != NULL)
			{
				filename = argv[i + 1];
			}
			i = i + 2;
		}
		else if(match(argv[i], "-i") || match(argv[i], "--init-mode"))
		{ /* init mode does not populate env */
			INIT_MODE = TRUE;
			i = i + 1;
		}
		else if(match(argv[i], "-V") || match(argv[i], "--version"))
		{ /* Output version */
			file_print("kaem version 0.8.0\n", stdout);
			exit(EXIT_SUCCESS);
		}
		else if(match(argv[i], "-v") || match(argv[i], "--verbose"))
		{ /* Set verbose */
			VERBOSE = TRUE;
			i = i + 1;
		}
		else if(match(argv[i], "--strict"))
		{ /* Set strict */
			STRICT = TRUE;
			i = i + 1;
		}
		else if(match(argv[i], "--warn"))
		{ /* Set warnings */
			WARNINGS = TRUE;
			i = i + 1;
		}
		else if(match(argv[i], "--fuzz"))
		{ /* Set fuzzing */
			FUZZING = TRUE;
			i = i + 1;
		}
		else if(match(argv[i], "--"))
		{ /* Nothing more after this */
			break;
		}
		else if(argv[i][0] != '-')
		{ /* Assume this is the name of the script - after we get the name, nothing more */
			filename = argv[i];
			break;
		}
		else
		{ /* We don't know this argument */
			file_print("UNKNOWN ARGUMENT\n", stdout);
			exit(EXIT_FAILURE);
		}
	}

	/* Populate env */
	if(INIT_MODE == FALSE)
	{
		populate_env(envp);
	}

	/* Populate USERNAME variable */
	char* USERNAME = env_lookup("LOGNAME");

	/* Handle edge cases */
	if((NULL == env_lookup("PATH")) && (NULL == USERNAME))
	{ /* We didn't find either of PATH or USERNAME -- use a generic PATH */
		add_envar("PATH", "/root/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
	}
	else if(NULL == env_lookup("PATH"))
	{ /* We did find a username but not a PATH -- use a generic PATH but with /home/USERNAME */
		add_envar("PATH", prepend_string("/home/", prepend_string(USERNAME,"/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games")));
	}

	/* Open the script */
	script = fopen(filename, "r");
	if(NULL == script)
	{
		file_print("The file: ", stderr);
		file_print(filename, stderr);
		file_print(" can not be opened!\n", stderr);
		exit(EXIT_FAILURE);
	}

	/* Run the commands */
	run_script(script, argv);

	/* Cleanup */
	fclose(script);
	return EXIT_SUCCESS;
}
