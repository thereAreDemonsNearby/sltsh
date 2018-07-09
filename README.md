# sltsh
a toy shell (no programming feature)

## Features Supported:
* subshell : ()  
* pipeline : |  
* background : &  
* exec in order : ;  
* built-in commands  
* redirection  
* expansion

### Built-in:
* jobs  
* fg <job id>  
* bg <job id>  
* umask (no arg to see the current value, or 0000~0777 to set)  
* cd <path>(no arg to go to home directory)  

### Redirection: 
* \>file, >>file, fd>file, fd>>file, fd>&fd, >&file, <file

### Expansion:
* command substitution:  
  * cd /usr/src/kernels/$(uname -r)
* arithmetic expansion:  
  * echo $((3+4*5))
* tilde expansion:  
  * cd ~/bin

## Grammar:


	CmdLine = CmdList ENDOFLINE
	CmdList = Pipe
			| Pipe Delim CmdList
			| Pipe Delim
	Delim = ; | &
	Pipe = Redirected
		 | Redirected '|' Pipe
	Redirected = Primary
			   | Primary [redirectflags]
	Primary = Exec
			| (CmdList)

## Installation
make
