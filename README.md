smallsh is a small shell that can execute a limited number of commands on its own and pass other command to bash. The shell allows for redirection of stdin and stdout and supports foreground and background processes. The built-in commands are *exit*, *cd*, & *status*. 

the *:* is used as an input prompt for smallsh

syntax: 

:**command** [arg1 arg2 arg3] [< input_file] [> output_file] {&}

The last word must be *&* to have command executed in background.
No quoting/arguments with spaces are supported.
Comments can be entered using "#".
No error checking is done on commandline syntax. max chars = 2048; max arguments = 512;

Whenever a non-built command is run, the parent process forks off a new child process. 
If that command is run in the foreground, the parent will wait for it to complete. Otherwise, the parent will continue after forking. 

Note:
SIGINT(ctrl-c) will not terminate parent process. It only terminates a forground child process if one exists.
SIGTSTP(ctrl-z) will toggle disabling all background processes from being able to be run.

built in commands:
exit: will close smallsh
cd: change dir
status: prints exit status or terminating signal of last foreground process

compile program using **gcc -o smallsh smallsh.c**
