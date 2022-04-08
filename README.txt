Name: Jack Gallivan
OSU ID: gallivaj
Course: CS344
Assignment: 3

Compilation instructions:
1. Make the shell script executable with the command:
  chmod +x compile.sh
2. Run the script to compile the executables:
  ./compile.sh

--------------------------------------------------------------------------------

Description:
The program, "smallsh", is a limited-functionality shell.
See the assignment details online for full functionality.

Command-line syntax:
  command [arg1 arg2 ...] [< input_file] [> output_file] [&]

  Blank lines and comments (lines starting with '#') are ignored.
  Any instances of '$$' entered in the command line are expanded to the shell's
    pid.


I/O Redirection:
  Input and/or output for a command can be redirected by entering either '<' or
  '>' after the command arguments, followed immediately by the file location to
  redirect input from/output to.

Background processes:
  To run a command in the background, the last argument in the command must be
  '&'.

Built-in commands:
  cd [pathname]
    Changes current directory to path specified by PATHNAME, or to the home
    directory if unspecified.
  status
    Prints the exit status of the most recently run foreground process. If no
    foreground processes have been run yet, exit status returned is 0.
  exit
    Exits the shell.

Custom signal handlers:
  SIGINT (CTRL+C) terminates any running foreground process.
  SIGTSTP (CTRL+Z) toggles shell mode to foreground-only once any running
    foreground processes have terminated or the shell is sitting at the command
    line. Foreground-only mode ignores the '&' operator that runs a process in
    the background, forcing all commands to run in the foreground.

--------------------------------------------------------------------------------
