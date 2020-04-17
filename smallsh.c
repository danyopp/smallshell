// Author: Daniel Yopp
// Date: 5/23/19
// Program: Project 3 smallsh
// Description: shell program
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#define ARGLENGTH 50
#define MAXARGS 512
#define MAXBUFFER 2049 

//prototypes
void getArg(char*, char **, int *, int, int);
void passToBash(char** ArgsArray, int numOfArgs, int * curStat, int *numBackProc, int backProc[] , int * signalled);
void checkBackground(int * curStat, int * numBackProc, int backProc[]);
void signalCaught(int sig);
struct sigaction default_action = {0}, SIGTSTP_action = {0}, ignore_action = {0};	
int forgroundMode = 0;  




int main()
{
   //set up signal handlers
   default_action.sa_handler = SIG_DFL;	
   ignore_action.sa_handler = SIG_IGN;

   SIGTSTP_action.sa_handler = signalCaught;
   sigfillset(&SIGTSTP_action.sa_mask);
   SIGTSTP_action.sa_flags = SA_RESTART;

   //ignore SIGINT and run signalCaught(int) for SIGTSTP	
   sigaction(SIGINT, &ignore_action, NULL);
   sigaction(SIGTSTP, &SIGTSTP_action, NULL);
	
   //buffer declarations	
	int curStat = 0;
	int Signaled = 0;
	int numBackProc = 0;
	int backProc[100];

   //getline buffer
   char * getBuffer = malloc(sizeof(char[MAXBUFFER]));
   if(getBuffer == NULL)
	{printf("Malloc Failure - Buffer\n");
	 exit(1);
	}
   int numOfArgs = -1;
   //array of strings to hold tokens from getline buffer
   char * ArgsArray[MAXARGS];
   int count = 0;
   for (count = 0; count < MAXARGS; count++)
	{
	 ArgsArray[count] = malloc(sizeof(char[ARGLENGTH]));
	 if(ArgsArray[count] == NULL)
	   {printf("Malloc Failure - ArgArray\n");
	    exit(1);
	   }
 
	}
   //End of buffer declarations

   //main loop until exit call is recieved
   int exitCall = 0;
   while(exitCall == 0)
   {
	//check for background processes
	checkBackground(&curStat, &numBackProc, backProc);

	//prompt user for input
   	getArg(getBuffer, ArgsArray, &numOfArgs, MAXARGS, ARGLENGTH);	

	//if input is comment or blank entry
   	if(*ArgsArray[0] == '#' || *ArgsArray[0] == '\0')
   	 {     // printf("comment\n");
	 }

	//if input is exit call
        else if(strcmp(ArgsArray[0], "exit") == 0)
	{ exitCall = 1; }

	//if input is cd call
	else if(strcmp(ArgsArray[0], "cd") == 0)
	{
		//if no second arg - set to homedir
		if(strcmp(ArgsArray[1], "") ==0)
 		{ 
		  char * homeAddr;
		  homeAddr = getenv("HOME");
		  int result = chdir(homeAddr);
		  if(result == 1)
		    {perror("ERROR: Directory Change Failed\n");}
		}
		else //if there is an additional arg
		{

		  int result = chdir(ArgsArray[1]);
		  if (result == 1)
		  {printf("ERROR: Directory Change Failed\n");}
		}	
	}

	//if input is status call
        else if(strcmp(ArgsArray[0], "status") == 0)
	{ 
	  if(Signaled == 0)
	  {printf("exit value %d\n", curStat);}
	  else
	  {printf("terminated by signal %d\n", curStat);}	  

	}
	
	//if input has not been recognized, it will be passed to bash
	else
   	{passToBash(ArgsArray, numOfArgs, &curStat, &numBackProc, backProc, &Signaled);}	
   }//while loop	

   //Buffer Cleanup
	free( getBuffer );
   for (count = 0; count < MAXARGS; count++)
	{
	  free(ArgsArray[count]);
	}

   return 0;	
}




/******************************************************
 * SignalCaught()
 * Function to process SIGTSTP
 * ***************************************************/

void signalCaught(int sig)
{
   if(forgroundMode == 0)
   {	
      char* sigMsg = "Entering foreground-only mode (& is now ignored)\n";
      write(STDOUT_FILENO, sigMsg, 49);
      //variable is used to force background calls to be run in forground in passtobash function	
      forgroundMode = 1;
   }
   else
   {
      char* sigMsg = "Exiting foreground-only mode\n";
      write(STDOUT_FILENO, sigMsg, 29);
      forgroundMode = 0;
   }
   char colon = ':'; 	
   write(STDOUT_FILENO, &colon, 1);
}




/******************************************************
 * Function: determines if any of the background processes have finished, reaps zombies 
 * 
 * ***************************************************/

void checkBackground (int * curStat, int * numBackProc, int backProc[])
{
 	if (*numBackProc > 0)//if there is atleast one backgroud proc
	{	
		
		int count = 0;

		// cycle through array of background processes to check for zombies 
		for (count = 0; count < *numBackProc; count++)
		{
			int returnVal; 
			int childExitMethod;
			int results = waitpid(backProc[count], &childExitMethod, WNOHANG); 
			if(results != 0)//process completed
			{
			 printf("background pID %d is done: ", backProc[count]);
			 //background task finished
			  if(WIFEXITED(childExitMethod) == 0)
			   { //printf("bad exit\n");
				int termSignal = WTERMSIG(childExitMethod);
				printf("terminated by signal %d\n", termSignal);	
			   }
		   	   if(WIFEXITED(childExitMethod) != 0)
		   	   { 
		   	     returnVal = WEXITSTATUS(childExitMethod);  	
  		   	     printf("Exit Value %d\n", returnVal);	
			   }
			 //shift backproc array down to fill hole made by finished proc
			 int j = count;
			 for(j = count; j < (*numBackProc-1); j++)
			   { 
				backProc[j] = backProc[j+1];
   			   }
 
			 //drop count down to adjust for array shift
			 *numBackProc = *numBackProc - 1; 
			 count--;	
			}
		} 	
	}
}




/*********************************************
 * Name: Pass to Bash
 * Description: passes prompts to bash
 *	*******************************************/ 
void passToBash(char** ArgsArray, int numOfArgs, int * curStat, int * numBackProc, int backProc[], int * Signalled)
{
	//fork of new proc
	pid_t spawnPid = -5;
	int childExitStatus = -5;
	spawnPid=fork();

	
	switch (spawnPid)
	{
	case -1: { //fork failure
		   perror("ERROR: Fork Failure\n");
		   fflush(stderr);
		   exit(1);
	   	 }  
	case 0:	 {   //Child procedure
		     sigaction(SIGTSTP, &ignore_action, NULL);
	
		     //If forground process, make it so sig int will kill proc	
		     if (strcmp (ArgsArray[numOfArgs-1],"&") != 0)
			{
			   sigaction(SIGINT, &default_action, NULL);
			}

		     //Process marked for completeing in the background	
		     if (strcmp (ArgsArray[numOfArgs-1],"&") == 0)
		      {
			   numOfArgs--;
			   if(forgroundMode == 0)//don't redirect stdout/in if background process called but 
			     {			 //SIGTSTP was called to force forground processes 
				//open dev/null
				int targetFD = open("/dev/null", O_RDWR);
				if (targetFD == -1)
				   {perror("ERROR: Unable to open output file");
				    fflush(stderr);
				    exit(1);
				   }
	
				//redirect stdout
				int dupResult = dup2(targetFD, 1);
				if (dupResult == -1)
				   {perror("ERROR: Failed to redirect stdout");
				    fflush(stderr);
				    exit(1);
				   }
				//redirect stdin	 
				    dupResult = dup2(targetFD, 0);
				if (dupResult == -1)
				   {perror("ERROR: Failed to redirect stdin");
				    fflush(stderr);
				    exit(1);
				   }	 
			    }
		      }
		
		   int count;
		   for(count = 0; count < 2; count++);
		     {	
	 	    	//check for output redirect - for background process
			     if (strcmp (ArgsArray[numOfArgs-2],">") == 0)
			      {		
					//redirect stdout to input args
					int targetFD = open(ArgsArray[numOfArgs-1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (targetFD == -1)
					   {perror("ERROR: Unable to open output file");
					    fflush(stderr);
					    exit(1);
					   }
					 
					int dupResult = dup2(targetFD, 1);
					if (dupResult == -1)
					   {perror("ERROR: Failed to redirect stdout");
					    fflush(stderr);
					    exit(1);
					   }			
					numOfArgs--;
					numOfArgs--;
		     	      }

				//check for input redirect - for background process
		   	     if (strcmp (ArgsArray[numOfArgs-2],"<") == 0)
			      {
				 	int sourceFD = open(ArgsArray[numOfArgs-1], O_RDONLY);
					if (sourceFD == -1)
					   {perror("ERROR: Unable to open input file");
					    fflush(stderr);
					    exit(1);
					   }
					int dupResult = dup2(sourceFD, 0);
					if (dupResult == -1)
					   {perror("ERROR: Failed to redirect stdin");
					    fflush(stderr);
					    exit(1);
					   }
					numOfArgs--;
					numOfArgs--;
		    	      } 
		       }	  	
		   //set first empty arg to Null 
		   ArgsArray[numOfArgs] = NULL;	
	
		   //pass process 
		   int exeReturn = 100;	
		   exeReturn = execvp(ArgsArray[0], ArgsArray);
		     
		   if(exeReturn == -1)
		     {
		       perror("ERROR: Command Failure!");
		     }
		   fflush(stderr);	
		   exit(1);
		   break;	 
		 }
	default: {
		   //Parent thread execution
		   //If background process has been started
		   if (strcmp (ArgsArray[numOfArgs-1],"&") == 0 && forgroundMode == 0)
			{  
			  printf("Background pID is %d\n", spawnPid);
			  fflush(stdout);
			  backProc[*numBackProc] = spawnPid;
			  *numBackProc = *numBackProc + 1;
			}

		   //If foreground process is started
		   else
		        {
			   pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
			   //Process did not term regularly
			   
			   if(WIFSIGNALED(childExitStatus != 0))
			   {
			     //Proc was signal termed
				int termSignal = WTERMSIG(childExitStatus);
				if( termSignal == 2)	
				{printf("terminated by signal %d\n", termSignal);	
				*Signalled = 1;	}
			        else { *Signalled = 0;}
				*curStat = termSignal;
				
			   }
			   //Process did term normally
		   	   if(WIFEXITED(childExitStatus) != 0)
		   	   { 
		   	     *curStat = WEXITSTATUS(childExitStatus);
			     Signalled = 0;  	
  		   	   }
		   	}
			break;
		 }
	}
}



/*********************************************
 * Name: GetArgs
 * Description: prompts for arguments and tokens to array
 ********************************************/ 
void getArg (char* getBuffer, char ** ArgArray, int *ArgsFound, int MaxArgs, int MaxLength)
{
	//clear all buffers in args array to prepare for user input
	int count; 
	for(count = 0; count < MaxArgs; count++)
	{
	memset( ArgArray[count], '\0', MaxLength);
	}

	//clear buffer used by getline
	memset( getBuffer, '\0', sizeof(getBuffer));
	size_t  getSize = 2048 ;
	 
	//print prompt
	printf(":");
	fflush(stdout);
	getline(&getBuffer, &getSize, stdin);

	//split input buffer into tokens and place each token in arg array
	count = 0;
	*ArgsFound = 0; 
	char * token;

	//pull first token
	token = strtok(getBuffer, " ");

	//loop until no more tokens are found
	while(token != NULL)
	{
	 *ArgsFound = *ArgsFound+1; 
	 //test if token is longer than string length
	 if( (int)strlen(token) > MaxLength-1 )
		{
		 printf("WARNING!!! Arg Overflow\n");
		}
	 //copy each token to arguement array 
	 strcpy (ArgArray[count],token);

	 //pull next token
	 token = strtok (NULL, " ");
	 count++;
	}

	//remove newline from end of last argument in argument array
	char* strEnd6 = strchr(ArgArray[*ArgsFound-1], '\n');
	*strEnd6 = '\0';	

	//check for $$ and convert to PID
	for(count = 0; count < *ArgsFound; count++)
	 if( strcmp(ArgArray[count],"$$") == 0 )
	 {
		pid_t processID = getpid();
		//convert int to string
		sprintf(ArgArray[count], "%d", processID);
	 }
		
}
