#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

struct commandLine
{
	char* command;
	char* arguments;
	char* inputFile;
	char* outputFile;
	int backgroundFlag;
};

char* getInput()
{
	int maxInput = 2048;
	size_t maxInputLen = maxInput;
	char* userInput;
	char* currLine = NULL;

	userInput = calloc(maxInput + 1, sizeof(char));
	printf(": ");
	fflush(stdout);

	getline(&currLine, &maxInputLen, stdin);
	userInput = currLine;
	return userInput;
}

char* expandVar(char* line, pid_t smallshPID)
{
	// convert PID to str
	char pid[21];
	sprintf(pid, "%d", smallshPID);

	// check for occurrences of $$ in line, replace with pid if so
	char* endPtr = strstr(line, "$$");
	while (endPtr != NULL)
	{
		// get character count of string prior to $$ occurrence and set endPtr forward 2 places
		int charCount = strlen(line) - strlen(endPtr);
		endPtr = endPtr + 2;

		// allocate memory for new string, then copy/concat into a single string
		char* copy = calloc(strlen(pid) + strlen(line) - 3, sizeof(char));
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

struct commandLine* createCommandLine(pid_t smallshPID)
{
	struct commandLine* currCommand = malloc(sizeof(struct commandLine));
	currCommand->backgroundFlag = 0;

	char* saveptr;
	char* line = expandVar(getInput(), smallshPID);

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

		// handle & (background flag) at end of input, if exists
		token = saveptr;
		if (token[strlen(token) - 1] == "&"[0])
		{
			currCommand->backgroundFlag = 1;
			token[strlen(token) - 2] = '\0';
		}

		currCommand->arguments = calloc(strlen(token) + 1, sizeof(char));
		strcpy(currCommand->arguments, token);

		// check for input file arg
		char* inFilePtr = strstr(saveptr, "< ");
		int inFileStart = -1;
		if (inFilePtr)
		{
			inFileStart = strlen(inFilePtr);
			inFilePtr = inFilePtr + 2;
			token = strtok_r(inFilePtr, " ", &saveptr);
			currCommand->inputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->inputFile, token);
			inFilePtr = NULL;
		}
		else
		{
			currCommand->inputFile = NULL;
		}

		// check for output file arg
		char* outFilePtr = strstr(saveptr, "> ");
		int outFileStart = -1;
		if (outFilePtr)
		{
			outFileStart = strlen(outFilePtr);
			outFilePtr = outFilePtr + 2;
			token = strtok_r(outFilePtr, " ", &saveptr);
			currCommand->outputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand->outputFile, token);
			outFilePtr = NULL;
		}
		else
		{
			currCommand->outputFile = NULL;
		}

		if (inFileStart != -1 || outFileStart != -1)
		{
			int argEnd;
			if (inFileStart == -1)
			{
				argEnd = outFileStart;
			}
			else
			{
				argEnd = inFileStart;
			}

			int charCount = strlen(currCommand->arguments) - argEnd;
			char* copy = calloc(charCount + 1, sizeof(char));
			strncpy(copy, currCommand->arguments, charCount);
			free(currCommand->arguments);
			currCommand->arguments = copy;
		}

		if (strcmp(currCommand->arguments, "") == 0)
		{
			free(currCommand->arguments);
			currCommand->arguments = NULL;
		}
	}

	return currCommand;
}

void exitCmd()
{

}

void cdCmd()
{

}

void statusCmd()
{

}

void printCommand(struct commandLine* command)
{
	printf("command: %s\n", command->command);
	printf("arguments: %s\n", command->arguments);
	printf("inputFile: %s\n", command->inputFile);
	printf("outputFile: %s\n", command->outputFile);
	printf("backgroundFlag: %i\n\n", command->backgroundFlag);
}

int main()
{
	pid_t smallshPID = getppid();

	struct commandLine* currCommand = createCommandLine(smallshPID);

	for(;;)
	{
		printCommand(currCommand);
		currCommand = createCommandLine(smallshPID);
	}

	return 0;
}
