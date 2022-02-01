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

int main()
{
	char line = getInput();
	printf("%s", line);
	return 0;
}
