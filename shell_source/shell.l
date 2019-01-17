/*
 * Modified by Harrison Chen | chen1637 | Summer 2017
 * CS-252 Fall 2013
 * shell.l: lexical analyzer for shell
 */

%{

#include <string.h>
#include "y.tab.h"
#include <sys/wait.h>

static void yyunput (int c, char *buf_ptr);

void myunputc(int c) {
    unput(c);
}

%}

%%

\n		{
		return NEWLINE;
}

[ \t]	{
		/* Discard spaces and tabs */
}

[^\t\n `]*`[^\t\n]*`[^\t\n `]*	{ /* SUB SHELL */
		/* SETUP */
		int neo;
		char * tickBuff = (char *)malloc(1024);
		char * yyBuff = (char *)malloc(1024);
		int pipeA[2];
		int pipeB[2];
		if (pipe(pipeA) == -1) {
			perror("pipeA");
			exit(1);
		} if (pipe(pipeB) == -1) {
			perror("pipeB");
			exit(1);
		}
		char * arguments[2];
		arguments[0] = (char *)"/proc/self/exe\0";
		arguments[1] = NULL;

		/* BACKTICK PARSER */
		/* copies chars from past first tick to just before last tick */
		strncpy(tickBuff, strchr(yytext, '`') + 1, strlen(strchr(yytext, '`' ) - 2 - strlen(strrchr(yytext, '`'))));
		tickBuff[strlen(tickBuff) - 1] = '\0';

		/* PARENT AND FORK PIPING */
		neo = fork();
		if (neo == 0) {
			close(pipeA[1]);
			close(pipeB[0]);
			dup2(pipeA[0], 0);
			close(pipeA[0]);
			dup2(pipeB[1], 1);
			close(pipeB[1]);
			
			execve(arguments[0], arguments, NULL);
			perror("FORK EXEC");
			exit(1);
		} else if (neo < 0) {
			perror("PARENT FORK");
			exit(1);
		}
		close(pipeA[0]);
		close(pipeB[1]);
		
		/* WRITE THE BACKTICKED TEXT TO CHILD AND THEN TO LEX */
		write(pipeA[1], tickBuff, strlen(tickBuff));

		write(pipeA[1], "\nexit\n", 6);
		close(pipeA[1]);
		waitpid(neo, NULL, 0);
		
		int counter = 0;
		while (read(pipeB[0], &yyBuff[counter], 1)) {
			if (counter == strlen(yyBuff)) {
				/* REALLOC NEW SPACE TO YYBUFF */
				yyBuff = (char *)realloc(yyBuff ,sizeof(char) * (strlen(yyBuff) * 2));
			}
			++counter;
		}
		close(pipeB[0]); yyBuff[strlen(yyBuff) - 1] = '\0';
		int limit = strlen(yyBuff);
		for (int i = 0; i < limit; ++i) {
			if (yyBuff[i] == '\n') yyBuff[i] = ' ';
		}

		for (int j = (strlen(yyBuff) - 1); j > -1; --j) {
			myunputc(yyBuff[j]);
		}
		free(tickBuff);
		free(yyBuff);
}

">" 	{
		return GREAT;
}

"<"		{
		return LESS;
}

"|"		{
		return PIPE;
}

">&"	{
		return GREATAMP;
}

">>"	{
		return GREATER;
}

">>&"	{
		return GREATERAMP;
}

"&"		{
		return AMP;
}

\"[^\n\"]*\"	{ /* QUOTES HANDLING */
		++yytext;
		yytext[strlen(yytext) - 1] = '\00';
		yylval.string_val = strdup(yytext);
		return WORD;
}

[^ \t\n]*\\[^ \t\n]*	{ /* ESCAPE HANDLING */
		char * sect = yytext;
		char * where;
		while (where = strchr(sect, '\\')) {
			sect = where + 1;
			while (*where != 0) {
				*where = *(where + 1);
				++where;
			}
		}
		yylval.string_val = strdup(yytext);
		return WORD;
}

[^ \t\n|<>&][^ \t\n|<>&]*  {
		/* Assume that file names have only alpha chars */
		yylval.string_val = strdup(yytext);
		return WORD;
}
