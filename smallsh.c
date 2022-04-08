#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
// setenv/unsetenv, sigemptyset/sigfillset/sigaddset/sigdelset, sigaction, strtok_r
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include "smallsh_funcs.h"

#define REDIRI_SYM "<"
#define REDIRO_SYM ">"
#define BG_SYM "&"
#define MAX_ARGS 512

#define DEBUGINPUT 0
#define DEBUG1 0
#define DEBUGCD 0
#define DEBUGPROMPT 0

/* GLOBAL VARIABLES */
int _fg_only_mode = 0;  // Specifies shell mode. 0: normal. !0: fg-only mode.

/*  SIGTSTP HANDLER
    Changes a global variable that toggles to the shell to a state where all
    commands are run in the foreground, regardless of an input background
    operator (&). A message is displayed and the new state takes affect once
    all foregroundd processes are terminated.
*/
void handle_SIGTSTP(int signum)
{
    if (_fg_only_mode == 0)
        _fg_only_mode = 1;
    else
        _fg_only_mode = 0;
}

/*  SIGTERM HANDLER
    Terminates the process if it is not a session leader.
*/
void handle_SIGTERM(int signum)
{
    if (getpid() != getpgrp())
        exit(EXIT_FAILURE);
}

/*  SIGCHLD HANDLER
    Unpauses execution
*/
void handle_SIGCHLD(int signum)
{
}

// Struct for holding parsed user input
struct user_input
{
    char *cmd;
    char bg_process;
    char *input_file, *output_file;
    int num_cmd_args;
    char **cmd_args;
};

/* MAIN PROGRAM */
int main(int argc, char **argv)
{
    // Create a session, which initializes a new process group ID
    setsid();


    /* SIGNAL HANDLING */

    struct sigaction SIGTSTP_action = {0}, SIGTERM_action = {0},
        SIGCHLD_action = {0}, ignore_action = {0}, default_action = {0};

    // Default actions
    default_action.sa_handler = SIG_DFL;
    sigemptyset(&default_action.sa_mask);
    default_action.sa_flags = 0;

    // Set signals to ignore
    ignore_action.sa_handler = SIG_IGN;
    sigemptyset(&ignore_action.sa_mask);
    ignore_action.sa_flags = 0;
    sigaction(SIGINT, &ignore_action, NULL);

    // Set handler for SIGTSTP
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Set handler for SIGTERM
    SIGTERM_action.sa_handler = handle_SIGTERM;
    sigfillset(&SIGTERM_action.sa_mask);
    SIGTERM_action.sa_flags = 0;
    sigaction(SIGTERM, &SIGTERM_action, NULL);

    // Set handler for SIGCHLD
    SIGCHLD_action.sa_handler = handle_SIGCHLD;
    sigfillset(&SIGCHLD_action.sa_mask);
    SIGCHLD_action.sa_flags = 0;

    // Local shell mode. 0 = normal mode, !0 = foreground-only mode.
    // Comparison made to _fg_only_mode for mode change whenever the command
    // prompt is about to be displayed
    int fg_only_mode = 0;

    // Hold exit status of most recent foreground process termination
    int fg_status = 0;

    // Initialize variables that hold background process information
    int num_bg_procs = 0;
    int bg_pids_arr_size = 8;
    int *bg_pids_arr = calloc(bg_pids_arr_size, sizeof(int));

    // Initialize user_input struct for holding parsed user input
    struct user_input *user_input = calloc(1, sizeof(struct user_input));
    user_input->cmd_args = calloc(MAX_ARGS, sizeof(char *));

    // Initialize input string buffer input_buf
    size_t input_buf_size = 2052;
    char *input_buf = calloc(input_buf_size, sizeof(char));


    /* MAIN LOOP */

    while (1)
    {
        /* Clear any data from user_input struct */
        if (user_input->cmd)
            free(user_input->cmd);
        if (user_input->input_file)
            free(user_input->input_file);
        if (user_input->output_file)
            free(user_input->output_file);
        if (user_input->cmd_args[0])
        {
            for (int i = 0; i < user_input->num_cmd_args; i++)
            {
                free(user_input->cmd_args[i]);
            }
        }
        memset(user_input, '\0', sizeof(struct user_input) - sizeof(char **));
        memset(user_input->cmd_args, '\0', sizeof(char *) * MAX_ARGS);


        /* Display command-line prompt until user enters a valid string */
        do
        {
            if (DEBUGPROMPT)
                printf("Checking for shell mode change...\n");

            /* Check if foreground-only mode has changed settings */
            if (fg_only_mode != _fg_only_mode)
            {
                if (_fg_only_mode == 0)
                {
                    printf("\nExiting foreground-only mode\n");
                }
                else
                {
                    printf("\nEntering foreground-only mode (& is now ignored)\n");
                }
                fg_only_mode = _fg_only_mode;
            }

            if (DEBUGPROMPT)
                printf("Checking for background process termination...\n");

            /* Check for termination of background processes */
            int bg_status;
            for (int i = 0; i < num_bg_procs; i++)
            {
                // Iterate through the bg processes
                if (waitpid(bg_pids_arr[i], &bg_status, WNOHANG) == bg_pids_arr[i])
                {
                    printf("background pid %d is done: ", bg_pids_arr[i]);
                    // Process changed state. See if it terminated.
                    if (WIFEXITED(bg_status))
                    {
                        printf("exit value %d\n", WEXITSTATUS(bg_status));
                    }
                    else if (WIFSIGNALED(bg_status))
                    {
                        printf("terminated by signal %d\n", WTERMSIG(bg_status));
                    }
                    fflush(stdout);
                    if (arr_del(&bg_pids_arr, &bg_pids_arr_size, i, &num_bg_procs) != 0)
                        exit(1);
                    i--;
                }
            }

            if (DEBUGPROMPT)
                printf("Clearing input buffer and displaying prompt...\n");

            /* Clear input buffer, display prompt, then get input */
            memset(input_buf, '\0', input_buf_size);
            printf(": ");
            fflush(stdout);
            char *input_ptr = NULL;

            input_ptr = fgets(input_buf, input_buf_size, stdin);
            // Check for read error
            if (input_ptr == NULL)
            {
                // Read error. Clear error then display prompt again.
                clearerr(stdin);
                continue;
            }
            // Remove trailing newline char
            size_t input_len = strlen(input_buf);
            if (input_buf[input_len-1] == '\n')
                input_buf[input_len-1] = '\0';
        } while (strlen(input_buf) == 0 || input_buf[0] == '#');
        // End prompt

        if (DEBUGINPUT)
            printf("string_buf: %s\n", input_buf);

        /*  PARSE USER INPUT
            Iterate through space-delimited arguments in input_buf, expanding
            every substring of "$$" to the current process pid, then copying
            the expanded argument to the appropriate struct members of
            user_input.

            Since the max pid length is 5 digits, each "$$" substring of 2
            characters may need to expand up to 5 characters, so memory
            allocated for each argument will be 2.5x the length of each input
            argument.
        */
        char *saveptr = NULL;
        char *token = strtok_r(input_buf, " ", &saveptr);

        // First argument is the command
        user_input->cmd = calloc(1, (size_t)(2.5 * strlen(token) + 1));
        copy_and_expand_dollar(user_input->cmd, token);

        user_input->num_cmd_args = 0;
        user_input->cmd_args[user_input->num_cmd_args] = calloc(1, strlen(user_input->cmd) + 1);
        strcpy(user_input->cmd_args[user_input->num_cmd_args], user_input->cmd);
        user_input->num_cmd_args++;

        token = strtok_r(NULL, " ", &saveptr);
        while (token)
        {
            // Input redirection
            if (strcmp(token, "<") == 0)
            {
                // Next token is input file
                token = strtok_r(NULL, " ", &saveptr);
                user_input->input_file = calloc(1, (size_t)(2.5 * strlen(token) + 1));
                copy_and_expand_dollar(user_input->input_file, token);
            }
            // Output redirection
            else if (strcmp(token, ">") == 0)
            {
                // Next token is output file
                token = strtok_r(NULL, " ", &saveptr);
                user_input->output_file = calloc(1, (size_t)(2.5 * strlen(token) + 1));
                copy_and_expand_dollar(user_input->output_file, token);
            }
            // Background process
            else if (strcmp(token, "&") == 0)
            {
                // See if token is last arg in string
                token = strtok_r(NULL, " ", &saveptr);
                if (!token)
                {
                    // Run process in background. Set bg_process to non-NULL
                    if (!fg_only_mode)
                        user_input->bg_process = 'T';
                    break;
                }
                else
                {
                    // "&" is part of command arguments. Add to cmd_args.
                    if (user_input->num_cmd_args >= MAX_ARGS)
                    {
                        // Too many arguments
                        user_input->num_cmd_args++;
                        break;
                    }
                    user_input->cmd_args[user_input->num_cmd_args] = malloc(2 * sizeof(char));
                    strcpy(user_input->cmd_args[user_input->num_cmd_args], "&");
                    user_input->num_cmd_args++;
                    continue;
                }
            }
            // Command argument
            else
            {
                // Check that we are below the maximum number of arguments
                if (user_input->num_cmd_args >= MAX_ARGS)
                {
                    // Too many arguments
                    user_input->num_cmd_args++;
                    break;
                }

                user_input->cmd_args[user_input->num_cmd_args] = calloc(1, (size_t)(2.5 * strlen(token) + 1));
                copy_and_expand_dollar(user_input->cmd_args[user_input->num_cmd_args], token);
                user_input->num_cmd_args++;
            }

            // Get next token in string input_buf
            token = strtok_r(NULL, " ", &saveptr);
        }

        // Check for any overflow of arguments
        if (user_input->num_cmd_args >= MAX_ARGS)
        {
            // Too many arguments. Print error
            fprintf(stderr, "Error: arguments entered exceeds %d\n", MAX_ARGS);
            fflush(stderr);
            continue;
        }

        // Explicitly initialize a pointer to NULL after the last cmd_args
        user_input->cmd_args[user_input->num_cmd_args] = NULL;

        if (DEBUGINPUT)
        {
            printf("cmd: '%s'\n", user_input->cmd);
            if (user_input->input_file)
                printf("input: '%s'\n", user_input->input_file);
            if (user_input->output_file)
                printf("output: '%s'\n", user_input->output_file);
            printf("background process: '%c'\n", user_input->bg_process);
            printf("num cmd args: '%d'\n", user_input->num_cmd_args);
            for (int i = 0; i < user_input->num_cmd_args; i++)
            {
                if (user_input->cmd_args[i])
                    printf("arg%d: '%s'\n", i, user_input->cmd_args[i]);
            }
        }

        /* BUILT-IN COMMANDS */

        if (DEBUG1)
            printf("Checking for built-in command...\n");

        /* EXIT COMMAND */
        if (strcmp(user_input->cmd, "exit") == 0)
        {
            // Exit all processes and jobs running then terminate
            int kill_result = killpg(0, SIGTERM);
            if (kill_result == -1)
                killpg(0, SIGKILL);
            exit(EXIT_SUCCESS);
        }

        /* CD COMMAND */
        if (strcmp(user_input->cmd, "cd") == 0)
        {
            // Change directory. Takes one optional argument.
            // Ignores redirect of input/output and background process requests.
            if (user_input->num_cmd_args == 1)
            {
                if (DEBUGCD)
                    printf("cd to home dir...\n");
                // Change to home directory
                char *dest_dir = getenv("HOME");
                if (chdir(dest_dir) == -1)
                {
                    fprintf(stderr, "chdir to %s: ", dest_dir);
                    perror("");
                }
                else
                {
                    if (DEBUGCD)
                        printf("setting pwd...\n");
                    setenv("PWD", dest_dir, 1);
                }
            }
            else if (user_input->num_cmd_args == 2)
            {
                if (DEBUGCD)
                    printf("cd to user argument...\n");
                // Change directory to path given in argument.
                // chdir resolves relative pathnames
                if (chdir(user_input->cmd_args[1]) == -1)
                {
                    fprintf(stderr, "chdir to %s: ", user_input->cmd_args[1]);
                    perror("");
                }
                else
                {
                    if (DEBUGCD)
                        printf("setting pwd...\n");
                    char *cur_dir = getcwd(NULL, 0);
                    if (cur_dir)
                    {
                        setenv("PWD", cur_dir, 1);
                        free(cur_dir);
                    }
                    else
                        perror("getcwd()");
                }
            }
            else
            {
                // Too many arguments. Print error
                fprintf(stderr, "%s: Too many arguments\n", user_input->cmd);
            }
            fflush(stderr);
            // Reset prompt
            continue;
        }

        /* STATUS COMMAND */
        if (strcmp(user_input->cmd, "status") == 0)
        {
            // Prints out either the exit status or terminating signal of the
            // last foreground process ran.
            if (fg_status == 0)
                printf("exit value %d\n", EXIT_SUCCESS);
            else if (WIFEXITED(fg_status))
                printf("exit value %d\n", WEXITSTATUS(fg_status));
            else if (WIFSIGNALED(fg_status))
                printf("terminated by signal %d\n", WTERMSIG(fg_status));
            fflush(stdout);
            // Reset prompt
            continue;
        }


        /* NON BUILT-IN COMMANDS */

        if (DEBUG1)
            printf("Not a built-in command...\n");

        // Fork process
        pid_t spawnpid = fork();
        switch (spawnpid)
        {
        case -1:
            /* FORK ERROR */

            // Print error, then break to get back to command prompt.
            perror("fork()");
            break;

        case 0:
            /* CHILD PROCESS BRANCH */

            // All children processes ignore SIGTSTP
            sigaction(SIGTSTP, &ignore_action, NULL);

            // Foreground child processes terminate on SIGINT
            if (user_input->bg_process == '\0')
                sigaction(SIGINT, &default_action, NULL);

            // Redirect input if specified, or a background process
            if (user_input->input_file || user_input->bg_process)
            {
                // Get the input file name, if specified
                char *input_file = "/dev/null";
                if (user_input->input_file)
                    input_file = user_input->input_file;

                // Open the input file
                int input_fd = open(input_file, O_RDONLY);
                if (input_fd == -1)
                {
                    fprintf(stderr, "error redirecting input to %s: open(): ", input_file);
                    perror("");
                    fflush(stderr);
                    exit(EXIT_FAILURE);
                }

                // Point stdin to input_fd
                int input_result = dup2(input_fd, STDIN_FILENO);
                if (input_result == -1)
                {
                    fprintf(stderr, "error redirecting input to %s: dup2(): ", input_file);
                    perror("");
                    fflush(stderr);
                    exit(EXIT_FAILURE);
                }

                // Set file descriptor flag for input_fd to close on exec
                fcntl(input_fd, F_SETFD, FD_CLOEXEC);
            }

            // Redirect output if specified, or a background process
            if (user_input->output_file || user_input->bg_process)
            {
                // Get the output file name, if specified
                char *output_file = "/dev/null";
                if (user_input->output_file)
                    output_file = user_input->output_file;

                // Open the output file
                int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd == -1)
                {
                    fprintf(stderr, "error redirecting output to %s: open(): ", output_file);
                    perror("");
                    fflush(stderr);
                    exit(EXIT_FAILURE);
                }

                // Point stdout to output_fd
                int output_result = dup2(output_fd, STDOUT_FILENO);
                if (output_result == -1)
                {
                    fprintf(stderr, "error redirecting output to %s: dup2(): ", output_file);
                    perror("");
                    fflush(stderr);
                    exit(EXIT_FAILURE);
                }

                // Set file descriptor flag for output_fd to close on exec
                fcntl(output_fd, F_SETFD, FD_CLOEXEC);
            }

            // Call exec function to replace process
            execvp(user_input->cmd, user_input->cmd_args);

            // If function returned, error occurred. Print error and exit.
            fprintf(stderr, "execvp(): %s: ", user_input->cmd);
            perror("");
            fflush(stderr);
            exit(EXIT_FAILURE);

        default:
            /* PARENT PROCESS BRANCH */

            // Child is a foreground process
            if (user_input->bg_process == '\0')
            {
                // Wait here until child process completes.
                sigaction(SIGCHLD, &SIGCHLD_action, NULL);
                while(waitpid(spawnpid, &fg_status, WNOHANG) != spawnpid)
                    pause();
                sigaction(SIGCHLD, &default_action, NULL);
                if (WIFSIGNALED(fg_status))
                    printf("terminated by signal %d\n", WTERMSIG(fg_status));
            }

            // Child is a background process
            else
            {
                // Print pid of background process
                printf("background pid is %d\n", spawnpid);

                // Store child's pid in bg_pids_arr.
                if (arr_add(&bg_pids_arr, &bg_pids_arr_size, spawnpid, &num_bg_procs) != 0)
                    exit(1);
            }
        }
    } // Main loop

    // Free memory. Won't be reached anyway.
    if (user_input->cmd)
        free(user_input->cmd);
    if (user_input->input_file)
        free(user_input->input_file);
    if (user_input->output_file)
        free(user_input->output_file);
    if (user_input->cmd_args[0])
    {
        for (int i = 0; i < user_input->num_cmd_args; i++)
            free(user_input->cmd_args[i]);
    }
    free(user_input->cmd_args);
    free(user_input);
    free(input_buf);
    free(bg_pids_arr);

    return EXIT_SUCCESS;
}

/*  If a command fails because the shell could not find the command to run,
    then the shell will print an error message and set the exit status to 1

    An input file redirected via stdin should be opened for reading only; if your
    shell cannot open the file for reading, it should print an error message and
    set the exit status to 1 (but don't exit the shell).

    Similarly, an output file redirected via stdout should be opened for writing
    only; it should be truncated if it already exists or created if it does not
    exist. If your shell cannot open the output file it should print an error
    message and set the exit status to 1 (but don't exit the shell).
*/
