# myShell

myShell is a basic implementation of a Shell, written in C, demonstrating the basics of how a shell works.

# Compilation

> $ make

# Execution

> $ ./myshell

# Program Details

This program can work in two simple ways. These are foreground and background states and these can be determined according to the input entered by the user. 
If ampersand is used, it is working on foreground if it is not used in background. After the command is received from the user, the process starts.First, child processes are created with the fork operation. If the program is wanted to run in the background, it continues in the 
background without waiting. If it wants to work in the foreground, the child process waits for its parent. 
After the processes are created, execute processes start. Program searched the folders in 
the absolute path to execute the processes. Thus, it is not necessary to write the entire path to be able to execute the program.

# Functions We Wrote

These functions are not ready-made functions that we write ourselves.

* Ps_all Thanks to this function, running and finished operations are listed and displayed.

* Search This function works either recursive or normally. If it works normally, it searches for the word that the user entered as input in the files in the current directory..
If it works in a recursive way, it searches the current directory and searches the directories in the current directory. If there are directories in these directories, the search continues there.
This process continues until there are no unchecked files.After the search, the word, the whole line and the number of the line, regardless of whether it is recursive or not, is printed. These operations are valid for .c and .h files.

* Bookmark By using this function, we can list system functions or functions that we have written. Adding, removing and finding any element of the list can be done by typing the index value.








