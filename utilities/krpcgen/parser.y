/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

%{

#include <stdlib.h>
#include "krpcgen.h"

#define YYERROR_VERBOSE
#define YYDEBUG 1

static void yyerror(const char *msg);

%}

%union {
	char *str;
	unsigned long num;
	variable_t *var;
	statement_t *stmt;
}

%token TOK_SERVICE
%token TOK_TYPE
%token TOK_STRUCT
%token TOK_FUNCTION
%token TOK_EVENT
%token TOK_OUT

%token <str> TOK_NAMESPACE;
%token <str> TOK_IDENTIFIER;
%token <num> TOK_NUMBER;

%type <stmt> service_body;
%type <stmt> statement;
%type <stmt> service_stmt;
%type <stmt> type_stmt;
%type <stmt> struct_stmt;
%type <stmt> function_stmt;
%type <stmt> event_stmt;
%type <var> struct_body;
%type <var> struct_entry;
%type <var> func_param_list;
%type <var> func_param;
%type <var> event_param_list;
%type <var> event_param;

%start input

%%

input
	: TOK_SERVICE TOK_NAMESPACE TOK_NUMBER '{' service_body '}' ';'
		{ set_service(new_service_stmt($2, $3, $5)); }
	;

service_body
	: statement service_body
		{ $$ = $1; $$->next = $2; }
	| statement
		{ $$ = $1; }
	;

statement
	: service_stmt ';'
		{ $$ = $1; }
	| type_stmt ';'
		{ $$ = $1; }
	| struct_stmt ';'
		{ $$ = $1; }
	| function_stmt ';'
		{ $$ = $1; }
	| event_stmt ';'
		{ $$ = $1; }
	;

service_stmt
	: TOK_SERVICE TOK_IDENTIFIER '{' service_body '}'
		{ $$ = new_service_stmt($2, 0, $4); }
	;

type_stmt
	: TOK_TYPE TOK_IDENTIFIER TOK_IDENTIFIER
		{ $$ = new_type_stmt($2, $3); }
	;

struct_stmt
	: TOK_STRUCT TOK_IDENTIFIER '{' struct_body '}'	
		{ $$ = new_struct_stmt($2, $4); }
	;

struct_body
	: struct_entry struct_body
		{ $$ = $1; $$->next = $2; }
	| struct_entry
		{ $$ = $1; }
	;

struct_entry
	: TOK_IDENTIFIER TOK_IDENTIFIER ';'
		{ $$ = new_variable($2, $1, false); }
	;

function_stmt
	: TOK_FUNCTION TOK_IDENTIFIER '(' func_param_list ')'
		{ $$ = new_function_stmt($2, $4); }
	| TOK_FUNCTION TOK_IDENTIFIER '(' ')'
		{ $$ = new_function_stmt($2, NULL); }
	;

func_param_list
	: func_param ',' func_param_list
		{ $$ = $1; $$->next = $3; }
	| func_param
		{ $$ = $1; }
	;

func_param
	: TOK_IDENTIFIER TOK_IDENTIFIER
		{ $$ = new_variable($2, $1, false); }
	| TOK_OUT TOK_IDENTIFIER TOK_IDENTIFIER
		{ $$ = new_variable($3, $2, true); }
	;

event_stmt
	: TOK_EVENT TOK_IDENTIFIER '(' event_param_list ')'
		{ $$ = new_event_stmt($2, $4); }
	;

event_param_list
	: event_param ',' event_param_list
		{ $$ = $1; $$->next = $3; }
	| event_param
		{ $$ = $1; }
	;

event_param
	: TOK_IDENTIFIER TOK_IDENTIFIER
		{ $$ = new_variable($2, $1, false); }
	;

%%

static void yyerror(const char *msg) {
	fprintf(stderr, "%s:%zu: %s\n", current_file, current_line, msg);
	exit(EXIT_FAILURE);
}
