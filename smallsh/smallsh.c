#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int MAXINPUTLEN = 2048;
int MAXARGS = 512;

char* getInput()
{
	char* userInput;
	char* currLine = NULL;
	size_t maxInputLen = MAXINPUTLEN;

	userInput = calloc(MAXINPUTLEN + 1, sizeof(char));
	printf(": ");
	fflush(stdout);

	getline(&currLine, &maxInputLen, stdin);
	userInput = currLine;
	return userInput;
}

struct commandLine
{
	char* command;
	char* arguments;
	char* inputFile;
	char* outputFile;
	int backgroundFlag;
};

struct commandLine* createCommandLine()
{
	struct commandLine* currCommand = malloc(sizeof(struct commandLine));
	currCommand->backgroundFlag = 0;

	char* saveptr;
	char* line = getInput();
	char* token = strtok_r(line, " ", &saveptr);

	if (strcmp(token, "\n") == 0 || token[0] == "#"[0])
	{
		currCommand->command = NULL;
		currCommand->arguments = NULL;
		currCommand->inputFile = NULL;
		currCommand->outputFile = NULL;
	}
	else
	{
		currCommand->command = calloc(strlen(token) + 1, sizeof(char));
		strcpy(currCommand->command, token);

		// handle & at end of input if exists
		token = saveptr;
		if (token[strlen(token) - 2] == "&"[0])
		{
			currCommand->backgroundFlag = 1;
			token[strlen(token) - 3] = '\0';
			currCommand->arguments = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->arguments, token);
		}
		else
		{
			currCommand->arguments = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->arguments, token);
		}

		// check for input file arg
		char* inFile = strstr(saveptr, "< ");
		if (inFile)
		{
			inFile = inFile + 2;
			token = strtok_r(inFile, " ", &saveptr);
			currCommand->inputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->inputFile, token);
		}
		else
		{
			currCommand->inputFile = NULL;
		}

		// check for output file arg
		char* outFile = strstr(saveptr, "> ");
		if (outFile)
		{
			outFile = outFile + 2;
			token = strtok_r(outFile, " ", &saveptr);
			currCommand->outputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->outputFile, token);
		}
		else
		{
			currCommand->outputFile = NULL;
		}
	}

	return currCommand;
}


void processInput()
{

}

int main()
{
	pid_t smallshPID = getppid();

	struct commandLine* currCommand = createCommandLine();

	while (strcmp(currCommand->command, "exit") != 0)
	{
		currCommand = createCommandLine();
	}

	return 0;
}
