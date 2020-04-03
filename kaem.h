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

/*
 * DEFINES
 */

#define FALSE 0
//CONSTANT FALSE 0
#define TRUE 1
//CONSTANT TRUE 1
#define MAX_STRING 4096
//CONSTANT MAX_STRING 4096
#define MAX_ARRAY 256
//CONSTANT MAX_ARRAY 256

/* Imported */
int match(char* a, char* b);
void file_print(char* s, FILE* f);
void require(int bool, char* error);
char* copy_string(char* target, char* source);
char* prepend_string(char* add, char* base);
int string_length(char* a);
char* numerate_number(int a);

/*
 * GLOBALS
 */

int command_done;
int VERBOSE;
int STRICT;
int INIT_MODE; 
int FUZZING;
int WARNINGS;

/* Types for the token */
#define NORMAL 0
// CONSTANT NORMAL 0
#define COMMENT 1
// CONSTANT COMMENT 1
#define STRING 2
// CONSTANT STRING 2
#define SSTRING 3
// CONSTANT SSTRING 3

/*
 * Here is the token struct. It is used for both the token linked-list and
 * env linked-list.
 */
struct Token 
{
	/*
	 * For the token linked-list, this stores the token; for the env linked-list
	 * this stores the value of the variable.
	 */
	char* value;
	/* Also for the token linked-list, stores the type of the object inside it. */
	int type;
	/*
	 * Used only for the env linked-list. It holds a string containing the
	 * name of the var.
	 */
	char* var;
	/* 
	 * This struct stores a node of a singly linked list, store the pointer to
	 * the next node.
	 */
	struct Token* next;
};

/* Token linked-list; stores the tokens of each line */
struct Token* token;
/* Env linked-list; stores the environment variables */
struct Token* env;
