#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// initialize errno for error messages; initialize preventBackground flag for SIGTSTP custom handler toggle
extern int errno;
volatile sig_atomic_t preventBackground = 0;

// header required for custom SIGTSTP handler
void preventBackgroundOff(int);

/*******************************************************************************
 *  @struct commandLine
 *  @brief  struct for holding parsed information of a command, retrieved from the user.
 ******************************************************************************/
struct commandLine
{
	char* command;
	char* arguments;
	char* inputFile;
	char* outputFile;
	int backgroundFlag;
	int builtinCmd;
};

/*******************************************************************************
 *  @fn    freeCommand
 *  @brief frees all allocated memory from a commandLine struct, including the struct itself.
 * 
 *  @param currCommand - commandLine struct to be freed
 ******************************************************************************/
void freeCommand(struct commandLine* currCommand)
{
	// free any allocated attributes if exist
	if (currCommand->command != NULL)
	{
		free(currCommand->command);
	}
	if (currCommand->arguments != NULL)
	{
		free(currCommand->arguments);
	}
	if (currCommand->inputFile != NULL)
	{
		free(currCommand->inputFile);
	}
	if (currCommand->outputFile != NULL)
	{
		free(currCommand->outputFile);
	}

	// lastly, free the command itself
	free(currCommand);
}

/*******************************************************************************
 *  @fn     getInput
 *  @brief  retrieves entire command from the user in a single string, to be parsed.
 * 
 *  @retval - pointer to the retrieved user input string
 ******************************************************************************/
char* getInput()
{
	// max input for a command is 2048
	int maxInput = 2048;
	size_t maxInputLen = maxInput;
	char* userInput;

	// allocate storage for input, then get user input
	userInput = calloc(maxInput + 1, sizeof(char));
	getline(&userInput, &maxInputLen, stdin);

	// return the line of input
	return userInput;
}

/*******************************************************************************
 *  @fn     expandVar
 *  @brief  expands the variable $$ in an unparsed user input string, replacing $$ with
 *          the provided pid of the shell
 * 
 *  @param  line       - pointer to user input string (unparsed command)
 *  @param  smallshPid - pid of the smallsh shell
 *  @retval            - new string with $$ expanded into smallshPid
 ******************************************************************************/
char* expandVar(char* line, pid_t smallshPid)
{
	// convert PID to str
	char pid[21];
	sprintf(pid, "%d", smallshPid);

	// check for occurrences of $$ in line, replace with pid if so
	char* endPtr = strstr(line, "$$");
	while (endPtr != NULL)
	{
		// get character count of string prior to $$ occurrence and set endPtr forward 2 places
		int charCount = strlen(line) - strlen(endPtr);
		endPtr = endPtr + 2;

		// allocate memory for new string, then copy/concat into a single string
		char* copy = calloc(strlen(pid) + strlen(line) - 1, sizeof(char));
		strncpy(copy, line, charCount);
		strcat(copy, pid);
		strcat(copy, endPtr);

		// free line, then set equal to copy and check for next occurrence of $$
		free(line);
		line = copy;
		endPtr = strstr(line, "$$");
	}
	return line;
}

/*******************************************************************************
 *  @fn     createCommandLine
 *  @brief  creates a commandLine struct by parsing a user input string
 * 
 *  @param  smallshPid - pid of the smallsh shell
 *  @retval            - filled commandLine struct with parsed command information
 ******************************************************************************/
struct commandLine* createCommandLine(pid_t smallshPid)
{
	struct commandLine* currCommand = malloc(sizeof(struct commandLine));
	currCommand->backgroundFlag = 0;
	currCommand->builtinCmd = 0;

	char* saveptr;
	char* line = expandVar(getInput(), smallshPid);

	// handle empty input
	if (strcmp(line, "\n") == 0 || line[0] == "#"[0])
	{
		currCommand->command = NULL;
		currCommand->arguments = NULL;
		currCommand->inputFile = NULL;
		currCommand->outputFile = NULL;
	}
	else
	{
		line[strlen(line) - 1] = '\0';
		char* token = strtok_r(line, " ", &saveptr);

		// allocate mem for command
		currCommand->command = calloc(strlen(token) + 1, sizeof(char));
		strcpy(currCommand->command, token);

		// check if the command is a built in, set flag if so
		if (strcmp(currCommand->command, "exit") == 0 ||
			strcmp(currCommand->command, "cd") == 0 ||
			strcmp(currCommand->command, "status") == 0)
		{
			currCommand->builtinCmd = 1;
		}

		// handle & (background flag) at end of input, if exists
		token = saveptr;
		if (token[strlen(token) - 1] == "&"[0])
		{
			// only set the backgroundFlag if preventBackground flag is not set
			if (!preventBackground)
			{
				currCommand->backgroundFlag = 1;
			}

			// token string has more arguments
			if (strlen(token) > 1)
			{
				token[strlen(token) - 2] = '\0';
			}

			// no arguments other than &
			else
			{
				token[strlen(token) - 1] = '\0';
			}

		}

		// allocate mem for arguments
		currCommand->arguments = calloc(strlen(token) + 1, sizeof(char));
		strcpy(currCommand->arguments, token);

		// check for input file arg
		char* inFilePtr = strstr(saveptr, "< ");
		int inFileStart = -1;
		if (inFilePtr)
		{
			// if < is found in the line, parse and save inputFile to struct
			inFileStart = strlen(inFilePtr);
			inFilePtr = inFilePtr + 2;
			token = strtok_r(inFilePtr, " ", &saveptr);
			currCommand->inputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->inputFile, token);
			inFilePtr = NULL;
		}
		else
		{
			// otherwise, set to NULL
			currCommand->inputFile = NULL;
		}

		// check for output file arg
		char* outFilePtr = strstr(saveptr, "> ");
		int outFileStart = -1;
		if (outFilePtr)
		{
			// if > is found in the line, parse and save outputFile to struct
			outFileStart = strlen(outFilePtr);
			outFilePtr = outFilePtr + 2;
			token = strtok_r(outFilePtr, " ", &saveptr);
			currCommand->outputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->outputFile, token);
			outFilePtr = NULL;
		}
		else
		{
			// otherwise, set to NULL
			currCommand->outputFile = NULL;
		}

		// remove input/output from arguments string if present
		if (inFileStart != -1 || outFileStart != -1)
		{
			// determine where arguments end and file input/output begins
			int argEnd;
			if (inFileStart == -1)
			{
				argEnd = outFileStart;
			}
			else
			{
				argEnd = inFileStart;
			}

			// copy line upto that point and save as arguments
			int charCount = strlen(currCommand->arguments) - argEnd;
			char* copy = calloc(charCount + 1, sizeof(char));
			strncpy(copy, currCommand->arguments, charCount);
			free(currCommand->arguments);
			currCommand->arguments = copy;
		}

		// if arguments are empty at this point, set to NULL
		if (strcmp(currCommand->arguments, "") == 0)
		{
			free(currCommand->arguments);
			currCommand->arguments = NULL;
		}
	}

	// free input line and return the command
	free(line);
	return currCommand;
}

/*******************************************************************************
 *  @fn    buildArgv
 *  @brief builds an array in the correct format to call execvp()
 *         this format is: { command, arg1, arg2, ... , argN, NULL }
 * 
 *  @param currCommand - the current commandLine struct to be built from
 *  @param argv        - array to store arguments
 ******************************************************************************/
void buildArgv(struct commandLine* currCommand, char* argv[])
{
	char* saveptr;
	char* token = currCommand->command;
	int i = 0;

	// the current command has arguments
	if (currCommand->arguments != NULL)
	{
		// make a copy of the arguments to parse
		char* args = calloc(strlen(currCommand->arguments) + 1, sizeof(char));
		strcpy(args, currCommand->arguments);
		
		// break each individual argument into a token and store in next slot in array
		while (token != NULL)
		{
			argv[i] = calloc(strlen(token) + 1, sizeof(char));
			strcpy(argv[i], token);
			token = strtok_r(args, " ", &saveptr);
			args = saveptr;
			i++;
		}
	}
	// the current command has no arguments; set first element of array equal to command
	else
	{
		argv[i] = calloc(strlen(token) + 1, sizeof(char));
		strcpy(argv[i], token);
	}
}

// temp for testing, remove before submission
void printCommand(struct commandLine* command)
{
	printf("command: %s\n", command->command);
	printf("arguments: %s\n", command->arguments);
	printf("inputFile: %s\n", command->inputFile);
	printf("outputFile: %s\n", command->outputFile);
	printf("backgroundFlag: %i\n", command->backgroundFlag);
	printf("builtinCmd: %i\n\n", command->builtinCmd);
	fflush(stdout);
}

/*******************************************************************************
 *  @fn    executeBuiltInCmd
 *  @brief executes three built in commands for the smallsh shell - exit, cd, and status.
 * 
 *		     exit:	kills any uncompleted background processes and exits the shell
 *		       cd:	changes the working directory of the smallsh shell
 *		   status:	prints out the exit status or term signal of the last run foreground process
 * 
 *  @param currCommand        - commandLine struct to be run
 *  @param status             - int status of last run foreground process
 *  @param backgroundChildren - array of background process pids
 *  @param childCount         - int count of background child processes
 ******************************************************************************/
void executeBuiltInCmd(struct commandLine* currCommand, int status, pid_t backgroundChildren[], int childCount)
{
	// requested command is exit
	if (strcmp(currCommand->command, "exit") == 0)
	{
		// kill all children present in the backgroundChildren array
		for (int i = 0; i < childCount; i++)
		{
			if (backgroundChildren[i] != 0)
			{
				kill(backgroundChildren[i], SIGKILL);
			}
			else
			{
				break;
			}
		}

		// free the last command and exit success
		freeCommand(currCommand);
		exit(0);
	}

	// requested command is cd
	else if (strcmp(currCommand->command, "cd") == 0)
	{
		int result;

		// no argument in command, set current directory to HOME env var
		if (currCommand->arguments == NULL)
		{
			result = chdir(getenv("HOME"));
		}

		// command has an argument, check if path is absolute or relative
		else
		{
			// path argument is absolute
			if (currCommand->arguments[0] == "/"[0])
			{
				result = chdir(currCommand->arguments);
			}

			// path argument is relative
			else
			{
				// linux max path is 4096 characters
				int maxPath = 4096;
				char buffer[maxPath + 1];
				getcwd(buffer, maxPath + 1);

				// if the argument provided starts with ./, remove them from the string
				if (currCommand->arguments[0] == "."[0] && currCommand->arguments[1] == "/"[0])
				{
					currCommand->arguments = currCommand->arguments + 2;
				}

				// allocate memory for new string, then copy/concat into a single string
				int charCount = strlen(buffer) + strlen(currCommand->arguments);
				char* path = calloc(charCount + 2, sizeof(char));
				strcpy(path, buffer);
				strcat(path, "/");
				strcat(path, currCommand->arguments);

				// change directory, free mem allocated for path
				result = chdir(path);
				free(path);
			}

			// print error if occurred
			if (result == -1)
			{
				printf("%s\n", strerror(errno));
				fflush(stdout);
			}
		}

	}

	// requested command is status
	else if (strcmp(currCommand->command, "status") == 0)
	{
		// print status of last run foreground process
		if (WIFSIGNALED(status))
		{
			printf("terminated by signal %d\n", WTERMSIG(status));
			fflush(stdout);
		}
		else if (WIFEXITED(status))
		{
			printf("exit value %d\n", WEXITSTATUS(status));
			fflush(stdout);
		}
	}
}

/*******************************************************************************
 *  @fn    executeOtherCmd
 *  @brief attempts to execute a non-built-in command using execvp, as long as the command exists
 *         in PATH. If so, the command is executed by the child process and exits.
 * 
 *  @param currCommand - commandLine struct to be run
 ******************************************************************************/
void executeOtherCmd(struct commandLine* currCommand)
{
	// redirect input if required
	if (currCommand->inputFile != NULL || currCommand->backgroundFlag == 1)
	{
		int fdInput;

		// if no input file specified and background flag is on, set to /dev/null
		if (currCommand->inputFile == NULL)
		{
			fdInput = open("/dev/null", O_RDONLY);
		}
		// otherwise, attempt to open specified file
		else
		{
			fdInput = open(currCommand->inputFile, O_RDONLY);
		}

		// if error occurred in opening file, exit 1
		if (fdInput == -1)
		{
			printf("%s\n", strerror(errno));
			fflush(stdout);
			freeCommand(currCommand);
			exit(1);
		}

		// redirect input, then close file
		dup2(fdInput, 0);
		close(fdInput);
	}

	// redirect output if required
	if (currCommand->outputFile != NULL || currCommand->backgroundFlag == 1)
	{
		int fdOutput;

		// if no output file specified and background flag is on, set to /dev/null
		if (currCommand->outputFile == NULL)
		{
			fdOutput = open("/dev/null", O_WRONLY);
		}
		// otherwise, attempt to open specified file
		else
		{
			fdOutput = open(currCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
		}

		// if error occurred in opening file, exit 1
		if (fdOutput == -1)
		{
			printf("%s\n", strerror(errno));
			fflush(stdout);
			freeCommand(currCommand);
			exit(1);
		}

		// redirect output, then close file
		dup2(fdOutput, 1);
		close(fdOutput);
	}

	// build argv array, which consists of command + 512 max args + NULL terminator (514 total)
	// currCommand is no longer needed after argv array is built
	char* argv[514] = { NULL };
	buildArgv(currCommand, argv);
	freeCommand(currCommand);

	// execute command; if no return, command was successful
	int result = execvp(argv[0], argv);

	// if returned, print error and exit 1
	if (result == -1)
	{
		printf("%s\n", strerror(errno));
		fflush(stdout);
		exit(1);
	}
}

/*******************************************************************************
 *  @fn    preventBackgroundOn
 *  @brief custom signal handler for SIGTSTP; turns the preventBackground flag on,
 *         then installs a new handler for SIGTSTP that will turn off the flag upon next signal.
 * 
 *  @param sig - signal number that is being handled
 *  @citation  - based off of/adapted from the following comment by Prof. Gambord:
 *				 https://edstem.org/us/courses/16718/discussion/1067170?comment=2456069
 ******************************************************************************/
void preventBackgroundOn(int sig)
{
	preventBackground = 1;
	write(2, "\nEntering foreground-only mode (& is now ignored)\n: ", 52);
	signal(SIGTSTP, &preventBackgroundOff);
}

/*******************************************************************************
 *  @fn    preventBackgroundOff
 *  @brief custom signal handler for SIGTSTP; turns the preventBackground flag off,
 *         then installs a new handler for SIGTSTP that will turn on the flag upon next signal.
 *
 *  @param sig - signal number that is being handled
 *  @citation  - based off of/adapted from the following comment by Prof. Gambord:
 *				 https://edstem.org/us/courses/16718/discussion/1067170?comment=2456069
 ******************************************************************************/
void preventBackgroundOff(int sig)
{
	preventBackground = 0;
	write(2, "\nExiting foreground-only mode\n: ", 32);
	signal(SIGTSTP, &preventBackgroundOn);
}

/*******************************************************************************
 *  @fn    main
 *  @brief main smallsh shell; this program will request the user to input a command with arguments,
 *         including input/output files, and a designation on whether or not the process should run
 *         in the foreground or background.
 *
 *         A command must be in the following format, with options in square brackets being optional:
 *         command [arg1 arg2 ...] [< input_file] [> output_file] [&]
 * 
 *         Notes:
 *         - comments can be entered into the shell by putting # at the begining of any input.
 *         - the special variable $$ will be expanded into the process ID of the shell.
 *         - built in commands include: exit, cd, and status.
 *         - other commands can be run as long as they exist in PATH.
 ******************************************************************************/
int main()
{
	// install signal handler for SIGINT and SIGTSTP
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, &preventBackgroundOn);

	// initialize variables for background (child) processes. Max processes is 200 + 1 for NULL terminator
	pid_t backgroundChildren[201] = { 0 };
	int backgroundStatus = 0;
	int childCount = 0;

	// get pid of smallsh, then get first command from user
	pid_t smallshPid = getpid();
	printf(": ");
	fflush(stdout);
	struct commandLine* currCommand = createCommandLine(smallshPid);
	int status = 0;

	// after getting input setup a signal mask to catch pending signals during foreground processes
	sigset_t mask, pendingMask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTSTP);
	int skipOutput = 0;

	for(;;)
	{
		// block SIGTSTP signal until process is complete
		sigprocmask(SIG_BLOCK, &mask, NULL);

		// current command is a built in command
		if (currCommand->builtinCmd == 1)
		{
			executeBuiltInCmd(currCommand, status, backgroundChildren, childCount);
		}

		// current command is not a built in command
		else if (currCommand->command != NULL)
		{
			pid_t childPid = fork();

			// fork failed, exit 1 immediately
			if (childPid == -1)
			{
				freeCommand(currCommand);
				perror("fork failed.");
				exit(1);
			}

			// run by the child process; set custom sig handlers and execute commands
			else if (childPid == 0)
			{
				// install signal to ignore SIGTSTP in child process for both foreground and background
				signal(SIGTSTP, SIG_IGN);

				// current command shall be run in the background if flag is set and is not a built in command
				if (currCommand->backgroundFlag == 1 && currCommand->builtinCmd != 1)
				{
					executeOtherCmd(currCommand);
				}

				// current command shall be run in the foreground (wait for child)
				else
				{
					// re-install default signal for SIGINT for foreground processes
					signal(SIGINT, SIG_DFL);
					executeOtherCmd(currCommand);
				}
			}
			
			// run by the parent process
			else
			{
				// if background command, do not wait for child to complete
				if (currCommand->backgroundFlag == 1 && currCommand->builtinCmd != 1)
				{
					// add childPid to the background array and increment childCount
					backgroundChildren[childCount] = childPid;
					childCount++;

					// print info to user about background pid
					printf("background pid is %d\n", childPid);
					fflush(stdout);
				}

				// if foreground command, wait for child to complete
				else
				{
					waitpid(childPid, &status, WUNTRACED);

					// unblock SIGTSTP and check if there was a pending signal for SIGTSTP while waiting for childpid
					sigpending(&pendingMask);
					sigprocmask(SIG_UNBLOCK, &mask, NULL);

					// if SIGTSTP was pending, set skipOutput to 1 (this is used to prevent double output of : )
					if (sigismember(&pendingMask, SIGTSTP))
					{
						skipOutput = 1;
					}

					// if the child was terminated before completion, print info to user
					if (WIFSIGNALED(status))
					{
						printf("terminated by signal %d\n", WTERMSIG(status));
						fflush(stdout);
					}
				}
			}
		}
		
		// check all pids in background array and see if any have completed
		for (int i = 0; i < childCount; i++)
		{
			if (backgroundChildren[i] == 0)
			{
				break;
			}

			// if child has an updated status, print to user information about its exit/term status
			else if (waitpid(backgroundChildren[i], &backgroundStatus, WNOHANG) != 0)
			{
				printf("background pid %d is done: ", backgroundChildren[i]);
				fflush(stdout);

				if (WIFSIGNALED(backgroundStatus))
				{
					printf("terminated by signal %d\n", WTERMSIG(backgroundStatus));
					fflush(stdout);
				}
				else if (WIFEXITED(backgroundStatus))
				{
					printf("exit value %d\n", WEXITSTATUS(backgroundStatus));
					fflush(stdout);
				}

				// remove the current child from the array
				backgroundChildren[i] = 0;

				// shift all elements in the array to remove the now empty slot
				for (int j = 0; j < childCount; j++)
				{
					if (backgroundChildren[j] == 0)
					{
						backgroundChildren[j] = backgroundChildren[j + 1];
						backgroundChildren[j + 1] = 0;
					}
				}
				// decrease child count
				childCount--;
			}
		}

		// if no pending signal was recieved during last run foreground process, print :
		if (!skipOutput)
		{
			printf(": ");
			fflush(stdout);
		}
		// otherwise, : has already been printed, so skip the output and set skipOutput back to 0
		else
		{
			skipOutput = 0;
		}

		// free current command, then get next command from user
		freeCommand(currCommand);
		currCommand = createCommandLine(smallshPid);
	}
	return 0;
}
