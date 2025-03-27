# Shell

- Developing a shell interface that supports input/output redirection, piping, background processing, and a set of built-in functions.
- Learning about process control mechanics (parent-child, process creation, user-input parsing).
- Understanding command line parsing
- Design and implement error handling mechanisms for shell execution.

## Group Members
- Miles Brosz: mjb21h@fsu.edu
- Gerrell Cornelius: gc21a@fsu.edu
- Gavin Mcdavitt: grm21b@fsu.edu
## Division of Labor

### Part 1: Prompt
- **Responsibilities**: Indicate absolute working directory, Expand environment variables, User will type on same line as prompt.
- **Assigned to**: Gavin Mcdavitt

### Part 2: Environment Variables
- **Responsibilities**: Token expanison of environment variables, utilize getenv function, remove $ from token to do this.
- **Assigned to**: Miles Brosz

### Part 3: Tilde Expansion
- **Responsibilities**: Expand tokens beginning in '~', and replace with absolute path, to appear at the beginnig of a path or by itself.
- **Assigned to**: Gavin Mcdavitt, Miles Brosz

### Part 4: $PATH Search
- **Responsibilities**: Shell is able to execute a corresponding program/executable through path searching, looking through predefined directories for built in functions.
- **Assigned to**: Gerrell Cornelius

### Part 5: External Command Execution
- **Responsibilities**: Once obtaining the path of a program, execute this external command with execv(), use fork to create child process and execute this command.
- **Assigned to**: Miles Brosz

### Part 6: I/O Redirection
- **Responsibilities**: redirecting the output or input of external commands, creating files if outputed to a file that does not exist, doing multiple redirects in one command.
- **Assigned to**: Miles Brosz

### Part 7: Piping
- **Responsibilities**: Expanding on I/O Redirection to simultaneous execution of multiple commands, input and output.
- **Assigned to**: Gavin Mcdavitt

### Part 8: Background Processing
- **Responsibilities**: Allow for shell to execute commands without waiting for their completion, keeping track of completion status periodically.
- **Assigned to**: Gerrell Cornelius

### Part 9: Internal Command Execution
- **Responsibilities**: Built-in functions that are natively supported by the shell, implementing: Exit, CD, and Jobs.
- **Assigned to**: Gavin Mcdavitt

### Extra Credit
N/A

## File Listing
```
starter/
│
├── src/
│ └── lexer.c
├── obj/
│ └── lexer.o
├── bin/
│ └── (Where the "shell" executable will be located when compiled)
├── include/
│ └── lexer.h
│
├── README.md
└── Makefile
```
## How to Compile & Execute
- Type make in the directory of the "Makefile" (/starter/).
- Change directory to /bin, and enter command: "shell" to run


### Requirements
- **Compiler**: `gcc` for C/C++.
- **Dependencies**: N/A, all native.

### Compilation
For a C/C++ example:
```bash
cd ~/starter/
make
cd /bin
shell
```
This will build the executable in ...
/starter/bin

### Execution
"shell" in /starter/bin will run the program.

## Bugs
- **Bug 1**: I/O redirection does not work for cmd > file < file
- **Bug 2**: This is bug 2.
- **Bug 3**: This is bug 3.

## Extra Credit
N/A

## Considerations
All assumptions are included in the project writeup.
