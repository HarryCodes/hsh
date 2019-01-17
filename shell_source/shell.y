/* CS-252 shell.y: parser for shell [Harrison Chen | chen1637] */

%token	<string_val> WORD

%token 	NOTOKEN GREAT LESS NEWLINE PIPE GREATER GREATAMP GREATERAMP AMP

%union	{
		char *string_val;
}

%{

// #define yylex yylex
#include <stdio.h>
#include "command.h"
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <algorithm>

void yyerror(const char * s);
int yylex();

int hidden = 0;
regex_t re;
regmatch_t match;
std::vector<std::string> directoryArray;
char * d;
void expandWildCard(char * prefix, char * suffix);


void expandWildcardsIfNecessary(char * arg) {
	char * args = strdup(arg);
	/* if regex does not exist in the string, insert as a usual argument */
	if (!(strchr(args, '*') || strchr(args, '?'))) {
		Command::_currentSimpleCommand->insertArgument(args);
		return;
	}

	/* Call wildCard Expansion now that it is necessary */
	char * prefix;
	char * suffix;
	if (args != NULL) suffix = strdup(args);

	/* we wait for the directoryArray to be populated by the expansion of wildcards */
	expandWildCard(NULL, suffix);
	free(suffix);
	/* the resulting output arguments should be sorted */
	std::sort(directoryArray.begin(), directoryArray.end());
	int limit = directoryArray.size();
	char * s;
	char * t;

	/*  in all of the sorted arguments, prepend a slash if needed,
		and decide whether or not to insert it based on hidden dot flags and character checks */
	for (int i = 0; i < limit; ++i) {
		s = strdup(directoryArray.at(i).c_str());
		
		t = s; // make a copy to help with edge case character checking

		if (t = strchr(t, '.')) { // at the dot,
			--t;
			if (*t == '/') {      // check if it is a /. hidden,
				if (hidden) {
					Command::_currentSimpleCommand->insertArgument(s);
				}
			} else if (*t == 0) { // or if it is a null. hidden
				if (hidden) {
					Command::_currentSimpleCommand->insertArgument(s);
				}                 // if so, insert, otherwise don't.
			} else { Command::_currentSimpleCommand->insertArgument(s); }
		} else { Command::_currentSimpleCommand->insertArgument(s); }
	}

	/* reset the needed variables in order to run the next command cleanly */
	directoryArray.clear();
	hidden = 0;
	regfree(&re);
	free(args);
}



void expandWildCard(char * prefix, char * suffix) {
	
	if (!strcmp(suffix, "")) { // this is the base case for the recursion
		char * dInsert = strdup(prefix);
		directoryArray.push_back(dInsert);
		free(dInsert);
		return;
	} /* if the suffix is not empty, then we have something to expand */
	

	/* SUFFIX/PREFIX Checker: Checks and prepares either for wildcard or finalized situations */
	char * s = strchr(suffix, '/'); // find the location of the first '/'
	char component[1024] = "";
	if (s != NULL) { // shove from current location until char * s into component
		strncpy(component, suffix, s - suffix);
		suffix = s + 1; // this is to move past the '/'
	} else { // otherwise, we are at the final component, just add it.
		strncpy(component, suffix, strlen(suffix));
		suffix += strlen(suffix); // suffix moves past itself to the next component or NULL
	}
	char newPrefix[1024] = "";
	/* No wildcard in component */
	if (!(strchr(component, '*') || strchr(component, '?'))) {
		/* if prefix is empty, the component is the final match */
		if (prefix == NULL) sprintf(newPrefix, "%s", component);
		/* otherwise, the prefix and the final match are the new prefix combined */
		else sprintf(newPrefix, "%s/%s", prefix, component);
		expandWildCard(newPrefix, suffix);
		return;
	}
	/* END OF SUFFIX/PREFIX Checker: */


	/* There is still regex to convert and match at this point */
	/* convert: [* -> .*], [? -> .], [. -> \\.] and add ^ and $0 around the regex */
	char * reg = (char *)malloc(2 * strlen(component) + 10);
	char * a = component; // a references the arguments passed
	char * r = reg;       // r references the finalized regex
	*r = '^'; r++;
	while (*a) {
		if      (*a == '*') { *r = '.'; r++; *r = '*'; r++; }
		else if (*a == '?') { *r = '.'; r++; }
		else if (*a == '.') { *r = '\\'; r++; *r = '.'; r++; }
		else if (*a == '/') {}
		else                { *r = *a; r++; }
		a++;
	} *r = '$'; r++; *r = 0; // pad in the end with ^ and 0 chars
	/* compile the regex using regcomp so regexec can understand */
	int compiledRegex = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
	free(reg);
	if (!(compiledRegex == 0)) {
		return;
	}
	
	DIR * dResult;
	if (prefix == NULL) {
		dResult = opendir(".\0");
	} else if (!strcmp(prefix, "")) {
		dResult = opendir("/\0");
	} else dResult = opendir(prefix);

	if (dResult == NULL) {
		regfree(&re);
		return;
	}
	
	struct dirent * dirEntry; // the iterator for each return from readdir(dir)
	std::vector<char *> hallway; // all possible directories in this iteration that matched.
	int temp = 0;
	while ((dirEntry = readdir(dResult)) != NULL) { // while the return is not NULL
		if (regexec(&re, dirEntry->d_name, 1, &match, 0) == 0) {
			if (component[0] == '.') { hidden = 1; }
			if (prefix == NULL) sprintf(newPrefix, "%s", dirEntry->d_name); else
			sprintf(newPrefix, "%s/%s", prefix, dirEntry->d_name);
			hallway.push_back(strdup(newPrefix));
		}
	}
	closedir(dResult);
	int end = hallway.size();
	for (temp = 0; temp < end; ++temp) {
		if (strcmp(hallway.at(temp), "")) {
			char * check = (char *)malloc(1024);
			sprintf(check, "%s%s", prefix, component);
			if (!strcmp(hallway.at(temp), check)) continue;
			else expandWildCard(hallway.at(temp), suffix);
			free(hallway.at(temp));
			free(check);
		}
	}
}





%}

%%

goal:
	commands
	;

commands:
	command
	| commands command 
	;

command: simple_command
        ;

simple_command:
	pipe_list iomodifier_list background_op NEWLINE {
		// printf("   Yacc: Execute command\n");
		Command::_currentCommand.execute();
	}
	| NEWLINE 
	| error NEWLINE { yyerrok; }
	;

command_and_args:
	command_word argument_list {
		Command::_currentCommand.
			insertSimpleCommand( Command::_currentSimpleCommand );
	}
	;

argument_list:
	argument_list argument
	| /* can be empty */
	;

argument:
	WORD {
               // printf("   Yacc: insert argument \"%s\"\n", $1);

	       //Command::_currentSimpleCommand->insertArgument( $1 );
		   expandWildcardsIfNecessary($1);
	}
	;

command_word:
	WORD {
               // printf("   Yacc: insert command \"%s\"\n", $1);
	       
	       Command::_currentSimpleCommand = new SimpleCommand();
	       Command::_currentSimpleCommand->insertArgument( $1 );
	}
	;
	
pipe_list:
	pipe_list PIPE command_and_args
	| command_and_args
	;

iomodifier:
	GREAT WORD {
		// printf("   Yacc: insert output \"%s\"\n", $2);
		if (Command::_currentCommand._outFile == 0) {
			Command::_currentCommand._outFile = $2;
		} else {
			printf("Ambiguous output redirect");
			exit(1);
		}
	}
	|
	GREATER WORD {
		if (Command::_currentCommand._outFile == 0) {
			Command::_currentCommand._outFile = $2;
			Command::_currentCommand.append = 1;
		} else {
			printf("Ambiguous output redirect");
			exit(1);
		}
	}
	|
	GREATAMP WORD {
		if (Command::_currentCommand._outFile == 0) {
			Command::_currentCommand._outFile = $2;
			Command::_currentCommand._errFile = $2;
		} else {
			printf("Ambiguous output redirect");
			exit(1);
		}
	}
	|
	GREATERAMP WORD {
		if (Command::_currentCommand._outFile == 0) {
			Command::_currentCommand._outFile = $2;
			Command::_currentCommand._errFile = $2;
			Command::_currentCommand.append = 1;
		} else {
			printf("Ambiguous output redirect");
			exit(1);
		}
	}
	|
	LESS WORD {
		Command::_currentCommand._inFile = $2;
	}
	;

iomodifier_list:
	iomodifier_list iomodifier
	| /* can be empty */
	;

background_op:
	AMP {
		Command::_currentCommand._background = 1;
	}
	| /* can be empty */
	;

%%

void
yyerror(const char * s)
{
	fprintf(stderr,"%s", s);
}

#if 0
main()
{
	yyparse();
}
#endif
