/*
#####################################################################
# Auther: Ethan Patterson
# Class: CS344,	Assignment-3
# Date: March 5th 2018
# Discription: Runs a small shell for executing commands in the
# Unix os.
# ------------------Algorithm----------------------------------------
#	Parses user input looking for build in commands and then feeds
#	and exec function user input to execute their command
#	Also manages children processes.
#----------------------Terms to know---------------------------------
#	Stack may be reffered to list, argument may be reffered to as
#	word.
#----------------------NOTE------------------------------------------
#	There is a lot of optimization that can be done to this file
#	I took a few short cuts to meet time restrictions.
#	Was also lazy in my free calls here. Unix OS will
#	free everything on termination.
#####################################################################*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAXLENGTH 2048 // Length of user total user input.
#define MAXAREG 512 // Length of individual words or arguments.
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Defines a node. Nodes hold process ID's.
typedef struct Node{
	pid_t pid;
	struct Node* next;
}Node;
// Defines a Stack which is filled with nodes.
typedef struct Stack{
	int size; // Size of the stack that root points to.
	Node* root;
}Stack;
// inputHandler Hanldes userinput.
int inputHandler(char* buffer, char* argc[], int* flag, Stack* stack, sigset_t* intmask);
int cmdHandler(char* buffer, char* argc[], int* flag);// cmdHandler looks at user input for special commands.
int buildArgcBuff(char* argc[], char* buffer, Stack* stack, sigset_t* intmask);// Core function for parcing. Look at comment above function for more info.
int Process(char* in, char* out, char* data, char* argc[]);// Proccess parced user input.
int parseOnSpace(char* data, char* argc[]);// Parse words on space.
int ForkDefault(char* argc[], int size, Stack* stack, sigset_t* intmask);// Spawns child processes.
char* strReplace(char *str);// Replaces $$ with getpid();
Node* newNode(pid_t pid);// Makes a new node and returns it.
void push(Stack* stack, pid_t pid);// Pushes a new node onto a stack.
void printStack(Stack* stack);// Debug function for printing teh stack.
void ChildeCheck(Stack* stack);// Checks if a child has finished.
void removeNode(Stack* stack, pid_t pid);// Removes a node from the stack.
void ctrlZHandler(int sig);// Manages SIGTSIG signal.

int fgState = 0;// Manages forground state.

int main()
{
	// Get memory for the user.
	char* buffer = (char*) malloc(MAXLENGTH*sizeof(char));
	char* argc[MAXAREG];// Used to hold words from user input.
	Stack* stack = (Stack*) malloc(sizeof(Stack));// Builds a stack.
	stack -> size = 0; // Set stack size to 0
	stack -> root = NULL;// Set root of stack to NULL.

	sigset_t intmask;// Used to handle ctrl^c.
	struct sigaction SIGTSTP_action = {0};// Make sigaction for SIGTSTP signal called SIGTSTP_action.
	SIGTSTP_action.sa_handler = ctrlZHandler;// Make SIGTSTP_action point to ctrlZHandler function.
	SIGTSTP_action.sa_flags = SA_RESTART;// If SIGTSTP signal is detected reset other processes.
	sigfillset(&SIGTSTP_action.sa_mask);// Set SIGTSTP_action's mask.
	//SIGTSTP_action.sa_flags = 0;

	memset(buffer, '\0', (size_t)MAXLENGTH);// Default buffer to all '\0'
	// Set up SIGINT mask.
	if ((sigemptyset(&intmask) == -1) || (sigaddset(&intmask, SIGINT) == -1)){
		perror("Failed to initialize the signal mask...\n");
		return 1;
	}
	// Block all SIGINT signals for the shell.
	if (sigprocmask(SIG_BLOCK, &intmask, NULL) == -1)
		perror("Failed to block SIGINT...\n");

	sigaction(SIGTSTP, &SIGTSTP_action, NULL);// Initialize SIGTSTP_action.
	// Flag is the exit state of the previous command
	// 0 is normal, 1 is failed and -1 exits the program loop.
	int flag = 0;
	do{ // main loop

		printf(": ");fflush(stdout);// Default start line.
		flag = inputHandler(buffer, argc, &flag, stack,&intmask);// Handles user input and returns state of command
		//signal(SIGTSTP, ctrlZHandler);
	}while (flag != -1); 

	free(buffer);// Free user input memeory.
	return 0;
}
// inputHandler handles user input.
// buffer is user input, argc is each word or arg of user input, flag is
// a reffereance to the status of the last user command, stack is stack of proccess id's
// intmask handles ctrl c signals.
int inputHandler(char* buffer, char* argc[], int* flag, Stack* stack, sigset_t* intmask)
{
	size_t bufsize = MAXLENGTH-1;// Leaves one '\0' the buffer.
	int redFlag = 0;// Handles any flags in inputHandler. 0 is good, 1 is bad, n > 1 is also good.
	
	/*if (fgState > 0)*/ChildeCheck(stack); // Check if user is in ctl^z mode.
	getline(&buffer, &bufsize,stdin);// Fill buffer with user input.
	if (strstr(buffer, "$$") != NULL)
		buffer = strReplace(buffer);
	redFlag = cmdHandler(buffer, argc ,flag);// Look for any built in commands in user input.
	
	if (redFlag == 1)// ERROR ditected.
		return 1;
	else if(redFlag > 1)
		return 0;

	if (buildArgcBuff(argc, buffer, stack, intmask) == 1)// Parces buffer and fills argc with words in buffer.
		return 1;

	//signal(SIGCHLD,chiledHandler);fflush(stdout);
	return 0;
}
// cmdHandler Looks at user input for any built in commands.
// buffer is the users input, argc is depricated, flag addressed to flag in main loop
// setting it directely. 
int cmdHandler(char* buffer, char* argc[], int* flag)
{
	char* data = strdup(buffer);// Copy buffer into data, I dont dare touch the original data....
	char* pch = strtok(data, " \n");// Cut the first argument off data.
	data = strtok(NULL,"\n");// Store all other data back into data.
	char HOME[] = "/nfs/stak/users/patteret\0";// My home directory.

	if (pch == NULL)// If the empty spit error.
		return 1;
	if (strcmp(pch, "exit")== 0){ // If the user enters exit.
		free(buffer);
		argc[0] = "exit";
		//signal(SIGCHLD,chiledHandler);fflush(stdout);
		raise(SIGINT);
		exit(EXIT_SUCCESS);
	}
	if (strcmp(pch, "cd") == 0){// If the user enteres cd.
		if (data == NULL){
			printf("argc[1]:%s\n", argc[1]);
			if (chdir(HOME) == -1)// if cd is has no arguments after it, go HOME.
				return 1;
			return 5;
		}
		if (chdir(data) == -1)// Change directory to arg after cd.
			return 1;
		return 5;
	}
	
	if (strcmp(pch, "status") == 0){// If the user enters status print out the flag from the last cmd.
		printf("status is: %d\n", *flag);fflush(stdout);
		return 5;// Any number greater than or less than one is good, only 1 is bad.
	}
	if (*pch == '#'){
		return 5;
	}

	return 0;
}
// buildArgcBuff parces the buffer into argc, then ships argc off to be executed.
// argc will hold words, buffer is the users input, stack is a stack of pid's
// intmask handles the ctrl^c signal.
int buildArgcBuff(char* argc[], char* buffer, Stack* stack,sigset_t* intmask)
{

	char* data = strdup(buffer);// Make a copy of the buffer.
	char* in, *out;// To hold < > symboles.
	int procSize, sourceFD, targetFD; // For file navigation.
	int saved_stdout = dup(1);// Save the current output file.

	in = strpbrk(data, "<");// Look for stdin.
	out = strpbrk(data, ">");// Look for stdout.

	if (in != NULL && out != NULL){// If stdin and stdout are both in the input.
		in = strtok(data, "<");// Parse off stdin.
		out = strtok(NULL, ">");// Parse off stdout.
		data = strtok(NULL, " ");// Parse off any extra space.
		procSize = Process(in, out, data, argc);// Fill argc with parsed data. Return size of argc.
		targetFD = open(argc[procSize-1], O_WRONLY | O_CREAT | O_TRUNC, 0644);// Open a file specified by the user, store in targetFD.
		dup2(targetFD, 1);// Change output direction to opend file called targetFD
		argc[procSize-1] = NULL;// Set last argc to null for execvp in ForkDefault function.

		//FindPID(argc, procSize);// Look for $$ and replace it with getpid().
		if (ForkDefault(argc, procSize, stack, intmask) == 1)// Fork command to execvp.
			return 1;
		
		close(targetFD);
		dup2(saved_stdout, 1);// Set output stream back to original place.
	}
	else if (out != NULL){// If only stdout.
		in = strtok(data,">"); //Parse data out. Reuse in out and data.
		out = strtok(NULL,">");
		data = strtok(NULL," ");
		
		procSize = Process(in, out, data, argc);// Process parced data.
		targetFD = open(argc[procSize-1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open file specified by the user, store in targetFD.
		dup2(targetFD, 1);// Mode output stream to targetFD.
		argc[procSize-1] = NULL;// Set last argc to null for execvp, in ForkDefault function.

		//FindPID(argc, procSize);// Look for $$ and replace it with getpid().
		if (ForkDefault(argc, procSize, stack, intmask) == 1)// Fork command to execvp.
			return 1;
		
		close(targetFD);
		dup2(saved_stdout, 1);// Set output stream back to original place.
		return 0;
		//FrokOUT();
	}

	else if (in != NULL){// If only stdin.
		in = strtok(data, "<");// Parse data out. Reuse in out and data.
		out = strtok(NULL, "<");
		data = strtok(NULL, " ");
		procSize = Process(in, out, data, argc);// Process parced data.

		//FindPID(argc, procSize);// Look for $$ and replace it with getpid().
		if (ForkDefault(argc, procSize, stack, intmask) == 1)// Fork command to execvp.
			return 1;
	}

	else{
		procSize = parseOnSpace(data, argc);// Process parced data.
		//FindPID(argc, procSize);// Look for $$ and replace it with getpid().
		if (ForkDefault(argc, procSize, stack, intmask) == 1)
			return 1;
	}
	
	return 0;
}

// returns the count in argc[]. Must have first arg from buffer parssed parced!
// parses out all spaces and \n from data into argc.
// in is data left of < symbole, out is all data right of the < symbole,
// data holds all data from right of the > symbole.
int Process(char* in, char* out, char* data, char* argc[])
{
	int count = 0;// Store the count on number of spaces.
	char* pch = strtok(in, " \n");// Cut off all spaces from left of < 
	while (pch){		// Put them into argc.
		argc[count] = pch;
		pch = strtok(NULL, " \n");
		++count;
	}
	pch = strtok(out, " \n");// Cut off all spaces from right of <
	while (pch){		// put them into argc.
		argc[count] = pch;
		pch = strtok(NULL, " \n");
		++count;
	}
	pch= strtok(data, " \n");// Cut off all spaces from right of >
	while (pch){		// Put them into argc.
		argc[count] = pch;
		pch = strtok(NULL, " \n");
		++count;
	}

	argc[count] = NULL;// Make sure the last argc is null for execvp in ForkedDefault.
	return count;
}
// Depricated function used to cut on space, used for debuging, will Delete.
int parseOnSpace(char* buffer, char* argc[])
{
	int count = 0;
	char* data = strdup(buffer);// Make copy of original buffer into data.

	char* pch = strtok(data, " \n");// Start cutting at each space.
	while (pch){
		argc[count] = pch;// Fill argc with each word.
		pch = strtok(NULL, " \n");
		++count;
	}
	argc[count] = NULL;
	return count;
}
// ForkDefault takes a users input and feeds it to an execvp function.
// argc holds all words of a user, size holds the size for argc, stack,
// Holds a list of getpid() pros intmask manages ctrl^c.
int ForkDefault(char* argc[],int size, Stack* stack, sigset_t* intmask)
{
	// bg holds the current background state, also a global, uses AND.
	int bg = 0, chiledExitMethod = -5, result = 0, targetFD;
	pid_t spawnPid = -5;// Hold chiled process ID.
	
	if (argc[size-1] != NULL)// check if user wants the process backgrounded.
		if (*argc[size-1] == '&'){
			argc[size-1] = NULL;
			if (fgState == 0)// Check if user is in forground state or not.
				bg = 1;// set bg state to 1.
		}

	spawnPid = fork();// Make a chiled.
	switch (spawnPid){
		case -1:
			perror("ERROR: Failed to fork...\n");
			exit(1);
			break;
		case 0:// Unblock all ctl^c's
			if (sigprocmask(SIG_UNBLOCK, intmask, NULL) == -1)// Unblock SIGINT for child
				exit(1);
			if (bg == 1){// If chiled is a background process
				targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);// Send data to /dev/null.
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);// Reset stdout to default state.
				dup2(targetFD, 1);
			}
			// Do users bidding.
			if (execvp(argc[0], argc) == -1)
				exit(1);// If failed return 1;
			break;
	}	
	if(bg == 1){
		//printf("PID:%d\n",spawnPid);
		push(stack, spawnPid);// Push pid onto the stack.
		return result;
	}
	else{
		waitpid(spawnPid, &chiledExitMethod, 0);
		result = (WEXITSTATUS(chiledExitMethod) == 1)? 1 : 0;// Exit status of child.
		if (WIFSIGNALED(chiledExitMethod)){// If terminated by a signal.
			printf(ANSI_COLOR_RED "Terminated by signal %d" ANSI_COLOR_RESET "\n", chiledExitMethod);
			return 1;
		}
		//result = (chiledExitMethod > 0)? 1 : 0;// Check if chiled exited.
		return result;
	}

	return 0;
}
// Used to change fgstate if ctr^z is pressed
void ctrlZHandler(int sig)
{
	// Messages for forground state.
	char* message1 = "Entering foreground-only mode (& is now ignored)\n";
	char* message2 = "Exiting foreground-only mode\n";
	if (fgState == 0){// If forground state is off else is it on?
		fgState = 1;
		write(STDOUT_FILENO, message1, 50);
	}
	else{
		fgState = 0;
		write(STDOUT_FILENO, message2, 30);
	}
	
}
// Looks for 
char* strReplace(char *str)
{
	char input[MAXLENGTH];// Temp data holding point.
	char* output = (char*) malloc(MAXLENGTH-1 * sizeof(char)); 
	strcpy(input, str);// Copy str to input.
	char* dubdollahLoc = strstr(input, "$$");// look for $$
	int indexOfLoc = dubdollahLoc - input;// Pointer arithmatic.

	input[indexOfLoc] = '%';// Put % into first$.
	input[indexOfLoc + 1] = 'd';// Put d after the % and overwrite the other $. 

	snprintf(output, MAXLENGTH-1, input, getpid());// Put gitpid at the new %d.

	return output;
}
// Makes a new node, Takes a pid on creation.
Node* newNode(pid_t pid)
{
	Node* node = (Node*) malloc(sizeof(Node));
	node -> pid = pid;
	node -> next = NULL;
	return node;
}
// Push node onto a stack.
void push(Stack* stack, pid_t pid)
{
	Node* node = newNode(pid);// Make new node.
	node -> next = stack -> root;// New node should point to current root.
	stack -> root = node;// Make root of the stack the new node.
	stack -> size = stack -> size + 1;// Increase stack size by 1.
}
// Used for debuging.
void printStack(Stack* stack)
{
	Node* pCrawl = stack -> root;
	while (pCrawl){
		printf("%d -> ",pCrawl -> pid);fflush(stdin);
		pCrawl = pCrawl -> next;
	}
	printf("\n");fflush(stdin);
}
// Looks for any finished child processe's in the background, should be renamed to child
// leash, and report its status back to the user.
void ChildeCheck(Stack* stack)
{
	int status = -5;// -5 is a garbage variable.
	int tmp;// Holds current ID being checked.
	//pid_t result; 
	Node* pCrawl = stack -> root;// pCrawl points to the head of the stack list.

	while (pCrawl){// While pCrawl is not NULL.
		tmp = pCrawl -> pid;// Store pid into tmp.
		/*result =*/ waitpid(tmp, &status, WNOHANG);// Check if child is dead.
		if (WIFEXITED(status) != 0){// Look at chileds exit status.
			removeNode(stack, pCrawl -> pid);// Reap child pid off stack.
			int exit_code = WEXITSTATUS(status);// Get childs exit code.
			printf("Background pid %d is done: exit value is %d\n", tmp, exit_code);fflush(stdin);
			removeNode(stack, pCrawl -> pid);// Sanity check.
		}
		if (WTERMSIG(status) == 2) {// Cannot get WIFSIGNALED to work for ctr^c but WTERMSIG knows :)
			printf(ANSI_COLOR_RED "Terminated by %d" ANSI_COLOR_RESET "\n", WTERMSIG(status));
			fflush(stdin);	
		}		pCrawl = pCrawl -> next;// Move to next node on stack.
	}	
}
// Reaps any node with pid inside of stack.
void removeNode(Stack* stack, pid_t pid)
{
	Node* pCrawl, *past;// pCrawl is alias to stak root, past will
	pCrawl = stack -> root;  // keep track of pCrawl's past node.

	if (pCrawl != NULL && pCrawl -> pid == pid){// If root is the node to be reaped.
		stack -> root = pCrawl -> next;
		free(pCrawl);
		return;
	}
	
	while (pCrawl != NULL && pCrawl -> pid != pid){// Look for node in list to be reaped.
		past = pCrawl;
		pCrawl = pCrawl -> next;
	}
	if (pCrawl == NULL)// I pCrawl reaches the end of list without finding what it is looking for.
		return;	

	past -> next = pCrawl -> next;// Isolate pCrawl node from list.
	stack -> size = stack -> size -1;// Reduce the list size by 1.
	free(pCrawl);// Reap node pointed to by pCrawl.
}
