/* CS252: Shell project || Harrison Chen || chen1637 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "command.h"
#include <regex.h>

extern char ** environ;

char * customPrompt = (char *)"( ͡° ᴥ ͡°)  > "; // variable prompt
char * cPromptBackup = (char *)"";

SimpleCommand::SimpleCommand()
{
	// Create available space for 5 arguments
	_numOfAvailableArguments = 5;
	_numOfArguments = 0;
	_arguments = (char **) malloc( _numOfAvailableArguments * sizeof( char * ) );
}
 
void
SimpleCommand::insertArgument( char * argument )
{
	/* ENV EXPANSION */
	regex_t re;
	regmatch_t match;
	char * envReg = (char *)"^.*\\$\\{[^\\}][^\\}]*\\}.*$";
	int compiledRegex = regcomp(&re, envReg, REG_EXTENDED);
	if (!(compiledRegex == 0)) {
		return;
	}
	while (!regexec(&re, argument, 1, &match, 0)) {
		char * begin = (char *)malloc(sizeof(char) * 1024);
		char * end = (char *)malloc(sizeof(char) * 1024);
		char * name = (char *)malloc(sizeof(char) * 1024);
		char * contains = (char *)malloc(sizeof(char) * 1024);
		for (int i = 0; i < 1024; ++i) {
			begin[i] = '\0';
			end[i] = '\0';
			name[i] = '\0';
			contains[i] = '\0';
		}

		int loki = (strlen(argument) - strlen(strchr(argument, '$')));
		strncpy(begin, argument, loki); // the chars until ${
		int thor = (strlen(argument) - strlen(strchr(argument, '}')));
		strncpy(end, argument + thor + 1, strlen(argument) - thor); // chars after }
		strncpy(name, argument + loki + 2, thor - loki - 2); // chars in between
		if (!strcmp(name, "$")) {
			sprintf(contains, "%d", getpid());
		} else if (getenv(name) == NULL) {
			free(begin);
			free(end);
			free(name);
			free(contains);
			break;
		} else {
			strcpy(contains, getenv(name));
		}

		strcpy(argument, begin);
		strcat(argument, contains);
		strcat(argument, end);
		
		free(begin);
		free(end);
		free(name);
		free(contains);
	}
	regfree(&re);

	/* tilde time */
	if (argument[0] == '~') {
		if (strlen(argument) == 1) { 
			free(argument); argument = strdup(getenv("HOME")); } 
		else {
			char * directory = (char *)malloc(1024);
			strcpy(directory, "/homes/");
			char * argument2 = strdup(argument + 1); 
			strcat(directory, strdup(argument + 1));
			free(argument);
			argument = strdup(directory);
			free(directory);
		}
	}
	/* tilde time */
	if ( _numOfAvailableArguments == _numOfArguments  + 1 ) {
		// Double the available space
		_numOfAvailableArguments *= 2;
		_arguments = (char **) realloc( _arguments,
				  _numOfAvailableArguments * sizeof( char * ) );
	}
	// getenv if tilde and concat check for arg length beforehand.
	_arguments[ _numOfArguments ] = argument;

	// Add NULL argument at the end
	_arguments[ _numOfArguments + 1 ] = NULL;
	
	_numOfArguments++;
}

Command::Command()
{
	// Create available space for one simple command
	_numOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc( _numOfSimpleCommands * sizeof( SimpleCommand * ) );

	_numOfSimpleCommands = 0;
	_outFile = 0;
	_inFile = 0;
	_errFile = 0;
	_background = 0;
}

void
Command::insertSimpleCommand( SimpleCommand * simpleCommand )
{
	if ( _numOfAvailableSimpleCommands == _numOfSimpleCommands ) {
		_numOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **) realloc( _simpleCommands,
			 _numOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );
	}
	
	_simpleCommands[ _numOfSimpleCommands ] = simpleCommand;
	_numOfSimpleCommands++;
}

void
Command:: clear()
{
	for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
		for ( int j = 0; j < _simpleCommands[ i ]->_numOfArguments; j ++ ) {
			free ( _simpleCommands[ i ]->_arguments[ j ] );
		}
		
		free ( _simpleCommands[ i ]->_arguments );
		delete ( _simpleCommands[ i ] );
	}

	// MODIFIED: edge case for when both files point to the same free

	if ( _outFile == _errFile ) {
		free( _errFile );
	} else {
		free( _errFile );
		free( _outFile );
	}
	if ( _inFile ) {
		free( _inFile );
	}
	
	_numOfSimpleCommands = 0;
	_outFile = 0;
	_inFile = 0;
	_errFile = 0;
	_background = 0;
}

void
Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");
	
	for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
		printf("  %-3d ", i );
		for ( int j = 0; j < _simpleCommands[i]->_numOfArguments; j++ ) {
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
		}
	}

	printf( "\n\n" );
	printf( "  Output       Input        Error        Background\n" );
	printf( "  ------------ ------------ ------------ ------------\n" );
	printf( "  %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
		_inFile?_inFile:"default", _errFile?_errFile:"default",
		_background?"YES":"NO");
	printf( "\n\n" );
	
}

void Command::sshell(char * clist) {
		/* SETUP */
		int neo;
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

		/* PARENT AND FORK PIPING */
		neo = fork();
		if (neo == 0) {
			close(pipeA[1]);
			close(pipeB[0]);
			dup2(pipeA[0], 0);
			close(pipeA[0]);
			dup2(pipeB[1], 1);
			close(pipeB[1]);
			
			execvp(arguments[0], arguments);
			perror("FORK EXEC");
			exit(1);
		} else if (neo < 0) {
			perror("PARENT FORK");
			exit(2);
		}
		close(pipeA[0]);
		close(pipeB[1]);
		
		/* WRITE THE BACKTICKED TEXT TO CHILD AND THEN TO LEX */
		write(pipeA[1], clist, strlen(clist));

		write(pipeA[1], "\nexit\n", 6);
		close(pipeA[1]);
		waitpid(neo, NULL, 0);
		
		int counter = 0;
		int err = 0;
		while ((err = read(pipeB[0], &yyBuff[counter], 1)) > 0) {
			++counter;
			if (counter == strlen(yyBuff)) {
				/* REALLOC NEW SPACE TO YYBUFF */
				yyBuff = (char *)realloc(yyBuff ,sizeof(char) * (strlen(yyBuff) * 2));
			}
		}
		if (err == -1) {
			perror(" READ BUFF ");
			exit(2);
		}
		close(pipeB[0]); yyBuff[strlen(yyBuff) - 1] = '\0';

		printf("%s\n", yyBuff);
		free(yyBuff);
}

void
Command::execute()
{
	// Immediately return, if there are no simple-commands.
	// This is usually the case when the operator presses the return-key with an empty command list
	if ( _numOfSimpleCommands == 0 ) {
		prompt();
		return;
	}

	// DEBUG STATEMENTS
	// printf("Command found is %s\n", temp);
	// Print contents of Command data structure
	// print();

	// io redirection occurs here
	int tempIn = dup(0);
	int tempOut = dup(1);
	int tempErr = dup(2);

	// inFile opener
	int fdin;
	if (_inFile) {
		fdin = open(_inFile, O_RDONLY);
	} else {
		fdin = dup(tempIn);
	}

	// "outFile and errFile" instantiation
	int fdout;
	int fderr;

	// forking and execvping
	// neo returns 0 for in child and >0 in parent, or else <0 is error
	int neo;
	for (int i = 0; i < _numOfSimpleCommands; ++i) {

		// let 0 take hold of fdin
		dup2(fdin, 0);
		close(fdin);

		if (i == (_numOfSimpleCommands - 1)) {
			// we are in the last command
			if (_outFile) {
				if (append) {
					fdout = open(_outFile, O_CREAT | O_WRONLY | O_APPEND, 0664);
					if (fdout < 0) {
						perror("open");
						exit(1);
					}
				} else {
					fdout = open(_outFile, O_CREAT | O_WRONLY | O_TRUNC, 0664);
					if (fdout < 0) {
						perror("open");
						exit(1);
					}
				}
			} else {
				fdout = dup(tempOut);
			}
			// redirections of output
			dup2(fdout, 1);
			close(fdout);
			if (_errFile) {
				if (append) {
					fderr = open(_errFile, O_CREAT | O_WRONLY | O_APPEND, 0664);
					if (fderr < 0) {
						perror("open");
						exit(1);
					}
				} else {
					fderr = open(_errFile, O_CREAT | O_WRONLY | O_TRUNC, 0664);
					if (fderr < 0) {
						perror("open");
						exit(1);
					}
				}
			} else {
				fderr = dup(tempErr);
			}
			// redirections of error
			dup2(fderr, 2);
			close(fderr);
		} else {
			// we are not in the last command
			int fdpipe[2];
			if (pipe(fdpipe) == -1) {
				perror("pipe");
				exit(1);
			} else {
				pipe(fdpipe);
			}
			fdout = fdpipe[1];
			fdin  = fdpipe[0];
		}

		// redirect output
		dup2(fdout, 1);
		close(fdout);

		/*______________________BuiltIn Calls________________________*/
		// store the first command into a temp 'builtin' variable to examine for possible builtin calls.
		char * builtin = _simpleCommands[i]->_arguments[0];
		/* EXIT FUNCTION */
		if (!strcmp(builtin, "exit")) {
			prompt();
			if (isatty(0)) printf("Shell Purged ... see you soon, operator.\n");
			close(tempIn);
			close(tempOut);
			close(tempErr);
			exit(0);
		}

		/* SOURCE FUNCTION */
		if (!strcmp(builtin, "source")) {
			char * absPath = (char *)malloc(sizeof(char) * 1024);
			strcpy(absPath, "./");
			strcat(absPath, _simpleCommands[i]->_arguments[1]);
			FILE * fd;
			if (fd = fopen(absPath, "r\0")) {
				char * testLine = (char *)malloc(sizeof(char) * 1024);
				while (fgets(testLine, 1024, fd) != NULL) {
					Command::sshell(testLine);
				}
				free(testLine);
			}
			free(absPath);
			continue;
		}

		/* CD */
		if (!strcmp(_simpleCommands[i]->_arguments[0], "cd")) {
			int localVar = 0;
			if (_simpleCommands[i]->_numOfArguments == 1) { localVar = chdir(getenv("HOME")); continue; }
			localVar = chdir(_simpleCommands[i]->_arguments[1]);
			if (localVar == -1) { perror("cd error"); continue; }
			continue;
		}

		/* CUSTOM FEATURE: LENNY */
		if (!strcmp(builtin, "lenny")) {
			lenny(_simpleCommands[i]->_arguments[1]);
			continue;
		}
		/*______________________BuiltIn Calls________________________*/

		/*--------------------------SetEnv-------------------------------*/
		if (!strcmp(_simpleCommands[i]->_arguments[0], "setenv")) {
			if (_simpleCommands[i]->_numOfArguments != 3) { perror("setenv argument count"); continue; }
			setenv(_simpleCommands[i]->_arguments[1], _simpleCommands[i]->_arguments[2], 1);
			continue;
		}
		/*--------------------------SetEnv-------------------------------*/
		/*--------------------------UnEnv--------------------------------*/
		if (!strcmp(_simpleCommands[i]->_arguments[0], "unsetenv")) {
			if (_simpleCommands[i]->_numOfArguments != 2) { perror("unsetenv argument count"); continue; }
			unsetenv(_simpleCommands[i]->_arguments[1]);
			continue;
		}
		/*--------------------------UnEnv--------------------------------*/
		/*--------------------------PrintEnv-----------------------------*/
		if (!strcmp(_simpleCommands[i]->_arguments[0], "printenv")) {
			char ** prenv = environ;
			if (!prenv) { perror("environ"); exit(1); }
			neo = fork();
			if (neo == 0) {
				while (*prenv != NULL) {
					printf("%s\n", *prenv);
					++prenv;
				}
				exit(0);
			} else if (neo < 0) { perror("printenv fork"); exit(1); }
			continue;
		}
		/*--------------------------PrintEnv-----------------------------*/

		/*--------------------------DefaultFork--------------------------*/
		// time for children to be born into the matrix.
		neo = fork();
		if (neo == 0) {
			// the chosen one.
			execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
			perror("execvp");
			_exit(1);
		} else if (neo < 0) {
			// fork failed
			// Neo, there is no spoon.
			perror("fork");
			return;
		}
		/*--------------------------DefaultFork--------------------------*/
	} // end of for loop

	if (!_background) {
		// then we are in parent process waiting for latest child
		waitpid(neo, NULL, 0);
	}

	// move the temps back in place.
	dup2(tempIn, 0);
	close(tempIn);
	dup2(tempOut, 1);
	close(tempOut);
	dup2(tempErr, 2);
	close(tempErr);

	// Clear to prepare for next command
	clear();
	// Print new prompt
	prompt();
}

// Shell implementation

/* my custom builtin 'lenny': custom prompt setter. View the README */
void Command::lenny(char * cPrompt) {
	customPrompt = strdup(cPrompt);
	clear();
	prompt();
	return;
}

void
Command::prompt()
{	
	/* VARIABLE PROMPT */
	char * promptCheck = getenv("PROMPT");
	if (promptCheck != NULL) {
		cPromptBackup = strdup(customPrompt);
		customPrompt = promptCheck;
	} else if (strcmp(cPromptBackup, (char *)"")) {
		clear();
		customPrompt = strdup(cPromptBackup);
	}
	
	if (isatty(0)) {
		printf("%s", customPrompt);
		fflush(stdout);
	}
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);

extern "C" void handler(int sig) {
	if (sig == 2) {
		Command::_currentCommand.clear();
		puts("");
		Command::_currentCommand.prompt();
	}
	if (sig == 17) waitpid(-1, 0, 0);
}

main()
{
	Command c;
	/* SHELL RC UNFINISHED
	char * shellName = (char *)"/.shellrc";
	char * current = (char *)malloc(1024);
	char * tempCurrent = (char *)".";
	char * home = (char *)malloc(1024);
	char * tempHome = getenv("HOME");
	FILE * fd;
	strcat(current, tempCurrent);
	strcat(current, shellName);
	strcat(home, tempHome);
	strcat(home, shellName);
	fd = fopen(current, "r");
	if (fd == NULL) {
		fd = fopen(home, "r");
		if (fd == NULL) {
			// DO NOTHING AND CONTINUE AS USUAL.
		}
	}
	if (fd != NULL) {
		char * fileLine = (char *)malloc(sizeof(char) * 1024);
		while (fgets(fileLine, 1024, fd) != NULL) {
	}
	free(current);
	free(home); */

	/* sigChild */
	struct sigaction sig;
	sig.sa_handler = handler;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &sig, NULL)) {
		perror("CHILD");
		exit(1);
	}
	if (sigaction(SIGINT, &sig, NULL)) {
		perror("CTRL_C");
		exit(1);
	}
	
	Command::_currentCommand.prompt();
	yyparse();
}
