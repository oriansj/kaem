/*
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
#include "kaem.h"

/* Prototypes from other files */
int array_length(char** array);
char* env_lookup(char* variable);

/* Tokenize strings in variable output */
struct Token* variable_tokenize_string(struct Token* n, int i, char* input)
{
	if(!match(n->value, ""))
	{ /* Need to make a new node */
		/* Create the new node */
		/* m is for n->next jumping around */
		struct Token* m = calloc(1, sizeof(struct Token));
		n->next = m;
		/* Now we don't need m */
		n = n->next;
		n->value = calloc(MAX_STRING, sizeof(char));
	}
	/* Set the type to the appropriate string */
	if(input[i] == '\'') n->type = SSTRING;
	if(input[i] == '"') n->type = STRING;
	/* Copy into n->value */
	int j = 0;
	while((input[i] != '\'' && input[i] != '"') || input[i] == '\\')
	{
		n->value[j] = input[i];
		i = i + 1;
		j = j + 1;
	}
	return n;
}

/* Tokenize variable output */
struct Token* variable_tokenize(struct Token* n)
{
	int i;
	int j;
	int value_length = string_length(n->value);
	char* input = calloc(MAX_STRING, sizeof(char));
	copy_string(input, n->value);
	/* Reset n->value */
	for(i = 0; i < MAX_STRING; i = i + 1)
	{
		n->value[i] = 0;
	}
	struct Token* m = n;
	int k = 0;
	for(i = 0; i < value_length; i = i + 1)
	{
		if(input[i] == '\\')
		{
			n->value[k] = '\\';
			k = k + 1;
			i = i + 1;
			n->value[k] = input[i];
			k = k + 1;
			continue;
		}
		else if(input[i] == '\'' || input[i] == '"')
		{ /* Collect string */
			n = variable_tokenize_string(n, i, input);
			/* Traverse i to end of string */
			i = i + 1;
			while(input[i] != '\'' && input[i] != '"')
			{
				i = i + 1;
			}
			k = 0;
		}
		else if(input[i] == ' ' || input[i] == '\t')
		{ /* Split out into a new token */
			/* Create the new node */
			/* m is for n->next jumping around */
			m = calloc(1, sizeof(struct Token));
			/* Inherit parent's ->type */
			m->type = n->type;
			m->next = n->next;
			n->next = m;
			/* Now we don't need m */
			n = n->next;
			n->value = calloc(MAX_STRING, sizeof(char));
			k = 0;
		}
		else
		{
			n->value[k] = input[i];
			k = k + 1;
		}
	}
	return n;
}

/*
 * VARIABLE HANDLING FUNCTIONS
 */

/* Substitute a variable into n->value */
int run_substitution(char* var_name, struct Token* n)
{
	char* value = env_lookup(var_name);
	/* If there is nothing to substitute, don't substitute anything! */
	if(value != NULL && !match(value, ""))
	{
		n->value = prepend_string(n->value, value);
		return TRUE;
	}
	return FALSE;
}

/* Handle ${var:-text} format of variables - i.e. ifset format */
int variable_substitute_ifset(char* input, struct Token* n, int index)
{
	/*
	 * In ${var:-text} format, we evaluate like follows.
	 * If var is set as an envar, then we substitute the contents of that
	 * envar. If it is not set, we substitute alternative text.
	 *
	 * In this function, we assume that input is the raw token,
	 * n->value is everything already done in variable_substitute,
	 * index is where we are up to in input. offset is for n->value.
	 */

	/* 
	 * Check if we should even be performing this function.
	 * We perform this function when we come across ${var:-text} syntax.
	 */
	int index_old = index;
	int perform = FALSE;
	int input_length = string_length(input);
	while(index < input_length)
	{ /* Loop over each character */
		if(input[index] == ':' && input[index + 1] == '-')
		{ /* Yes, this is (most likely) ${var:-text} format. */
			perform = TRUE;
			break;
		}
		index = index + 1;
	}

	/* Don't perform it if we shouldn't */
	if(perform == FALSE) return index_old;
	index = index_old;
	
	/* 
	 * Get offset.
	 * offset is the difference between the index of the variable we write to
	 * in the following blocks and input.
	 * This stays relatively constant.
	 */
	int offset = index;

	/* Get the variable name */
	char* var_name = calloc(MAX_STRING, sizeof(char));
	require(var_name != NULL, "Memory initialization of var_name in variable_substitute_ifset failed\n");
	while(input[index] != ':')
	{ /* Copy into var_name until :- */
		var_name[index - offset] = input[index];
		index = index + 1;
	}

	/* Skip over :- */
	index = index + 2;
	offset = index;

	/* Get the alternative text */
	char* text = calloc(MAX_STRING, sizeof(char));
	require(text != NULL, "Memory initialization of text in variable_substitute_ifset failed\n");
	while(input[index] != '}')
	{ /* Copy into text until } */
		require(input_length > index, "IMPROPERLY TERMINATED VARIABLE\nABORTING HARD\n");
		text[index - offset] = input[index];
		index = index + 1;
	}

	/* Do the substitution */
	if(run_substitution(var_name, n) == FALSE)
	{ /* The variable was not found. Substitute the alternative text. */
		n->value = prepend_string(n->value, text);
	}

	return index;
}

/* Controls substitution for ${variable} and derivatives */
int variable_substitute(char* input, struct Token* n, int index)
{
	/* NOTE: index is the pos of input */
	index = index + 1; /* We don't want the { */

	/* 
	 * Check for "special" types
	 * If we do find a special type we delegate the substitution to it
	 * and return here; as we are done... there's nothing more do do in
	 * that case.
	 */
	int index_old = index;
	index = variable_substitute_ifset(input, n, index);
	if(index != index_old) return index;

	/* Reset index */
	index = index_old;

	/* 
	 * If we reach here it is a normal substitution
	 * Let's do it!
	 */
	/* Initialize var_name and offset */
	char* var_name = calloc(MAX_STRING, sizeof(char));
	require(var_name != NULL, "Memory initialization of var_name in variable_substitute failed\n");
	int offset = index;

	/* Get the variable name */
	int substitute_done = FALSE;
	while(substitute_done == FALSE)
	{
		char c = input[index];
		require(MAX_STRING > index, "LINE IS TOO LONG\nABORTING HARD\n");
		if(EOF == c || '\n' == c || index > string_length(input))
		{ /* We never should hit EOF, EOL or run past the end of the line 
			 while collecting a variable */
			file_print("IMPROPERLY TERMINATED VARIABLE!\nABORTING HARD\n", stderr);
			exit(EXIT_FAILURE);
		}
		else if('\\' == c)
		{
			var_name[index - offset] = '\\';
			index = index + 1;
			var_name[index - offset] = c;
			index = index + 1;
		}
		else if('}' == c)
		{ /* End of variable name */
			substitute_done = TRUE;
		}
		else
		{
			var_name[index - offset] = c;
			index = index + 1;
		}
	}

	/* Substitute the variable */
	run_substitution(var_name, n);

	return index;
}

/* Function to concatenate all command line arguments */
void variable_all(char** argv, struct Token* n, int index)
{
	fflush(stdout);
	int argv_length = array_length(argv);
	int i;
	int j;
	int argv_element_length;
	char* argv_element = calloc(MAX_STRING, sizeof(char));

	/* We don't want argv[0], as that contains the path to kaem */
	/*
	 * Loop through the arguments until we find the delimter between kaem args
	 * and script args.
	 */
	for(i = 1; i < argv_length; i = i + 1)
	{
		/* Reset argv_element */
		for(j = 0; j < MAX_STRING; j = j + 1)
		{
			argv_element[j] = 0;
		}
		/* If we don't do this we get jumbled results in M2-Planet */
		copy_string(argv_element, argv[i]);
		if(match(argv_element, "-f"))
		{
			/*
			 * Skip over the next argument, otherwise the next block will catch
			 * it in an incorrect way for -f.
			 */
			i = i + 1;
			/*
			 * Skip the rest of the loop to get for's i = i + 1 done without
			 * triggering the next block.
			 */
			continue;
		}
		if(match(argv_element, "--") || argv_element[0] != '-')
		{ /* -- or the script name signifies everything after this */
			i = i + 1;
			break;
		}
	}

	/* Change index from the position of input to the position of n->value */
	index = index + (string_length(n->value) - index);
	/* We now know that argv[i] contains the first script argument */
	for(i; i < argv_length; i = i + 1)
	{
		/* Reset argv_element */
		for(j = 0; j < MAX_STRING; j = j + 1)
		{
			argv_element[j] = 0;
		}
		copy_string(argv_element, argv[i]);
		/*
		 * If there are spaces in argv[i] element we need to make sure that the
		 * tokenization process afterwards doesn't tokenize argv[i].
		 * To do this we places quotes around the element.
		 */
		argv_element_length = string_length(argv_element);
		for(j = 0; j < argv_element_length; j = j + 1)
		{
			if(argv_element[j] == ' ')
			{
				n->value[index] = '"';
				index = index + 1;
				break;
			}
		}
		/* Copy argv_element into n->value */
		for(j = 0; j < argv_element_length; j = j + 1)
		{
			n->value[index] = argv_element[j];
			index = index + 1;
		}
		/* Terminate the string, if there is one */
		if(n->value[index - j - 1] == '"')
		{
			n->value[index] = '"';
			index = index + 1;
		}
		/* Add space on the end */
		n->value[index] = ' ';
		index = index + 1;
	}

	/* Remove trailing space */
	index = index - 1;
	n->value[index] = 0;
}

/* Function to substitute the last return code */
void variable_rc(int n_rc, struct Token* n, int index)
{
	/* Change index from the position of input to the position of n->value */
	index = index + (string_length(n->value) - index);
	char* rc = numerate_number(n_rc);
	int i;
	int rc_length = string_length(rc);
	for(i = 0; i < rc_length; i = i + 1)
	{
		n->value[index] = rc[i];
		index = index + 1;
	}
}

/* Function controlling substitution of variables */
struct Token* handle_variables(char** argv, struct Token* n, int last_rc)
{
	/* NOTE: index is the position of input */
	int index = 0;

	/* Create input */
	char* input = calloc(MAX_STRING, sizeof(char));
	require(input != NULL, "Memory initialization of input in collect_variable failed\n");
	copy_string(input, n->value);
	/* Reset n->value */
	n->value = calloc(MAX_STRING, sizeof(char));
	require(n->value != NULL, "Memory initialization of n->value in collect_variable failed\n");

	/* Copy everything up to the $ */
	/* 
	 * TODO: Not need allocation of input before this check if there is no
	 * variable in it.
	 */
	while(input[index] != '$')
	{
		if(input[index] == 0)
		{ /* No variable in it */
			n->value = input;
			return n; /* We don't need to do anything more */
		}
		n->value[index] = input[index];
		index = index + 1;
	}

	/* Must be outside the loop */
	int offset;

substitute:
	index = index + 1; /* We are uninterested in the $ */
	/* Run the substitution */
	if(input[index] == '{')
	{ /* Handle everything ${ related */
		index = variable_substitute(input, n, index);
		index = index + 1; /* We don't want the closing } */
	}
	else if(input[index] == '@')
	{ /* Handles $@ */
		index = index + 1; /* We don't want the @ */
		variable_all(argv, n, index);
	}
	else if(input[index] == '?')
	{
		index = index + 1;
		variable_rc(last_rc, n, index);
	}
	else
	{ /* We don't know that */
		file_print("IMPROPERLY USED VARIABLE!\nOnly ${foo} and $@ format are accepted at this time.\nABORTING HARD\n", stderr);
		exit(EXIT_FAILURE);
	}

	offset = string_length(n->value) - index;
	/* Copy everything from the end of the variable to the end of the token */
	while(input[index] != 0)
	{
		if(input[index] == '$')
		{ /* We have found another variable */
			fflush(stdout);
			goto substitute;
		}
		n->value[index + offset] = input[index];
		index = index + 1;
	}

	/* ------------------------------------------------------------------------
	 * | We need to tokenize the variables we have substituted now. You might |
	 * | ask, why not just tokenize everything after variable substitution?   |
	 * | It is because the tokenization process is rather different. For      |
	 * | example, if we found a #, we don't want that to be a line comment.   |
	 * | Mainly, we just want to seperate on spaces and tabs.                 |
	 * ----------------------------------------------------------------------*/

	return variable_tokenize(n);
}
