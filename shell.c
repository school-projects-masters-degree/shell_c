#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

/*
 * Aldin Cimpo
 * How to compile
 * gcc main.c -o main
 */

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 16

typedef struct
{
    char **command;
    char *outputFile;
} ParsedCommand;

// Handling cd command
// https://unix.stackexchange.com/questions/38808/why-is-cd-not-a-program
// https://www.bell-labs.com/usr/dmr/www/hist.html
int cd(char *path)
{
    return chdir(path);
}

void signal_handler(int signo)
{

    if (signo == SIGINT)
    {
        printf("\n$> ");
        // making sure the prompt
        // is printend instantly
        fflush(stdout);
    }
}
ParsedCommand parse_user_input(char *user_input)
{
    char **command = malloc(MAX_ARGS * sizeof(char *));
    if (command == NULL)
    {
        perror("Malloc failed");
        exit(1);
    }

    ParsedCommand parsedCommand;
    parsedCommand.outputFile = NULL;
    parsedCommand.command = command;

    int index = 0;
    char *parsed = user_input;
    char *next_space;
    char *redirect_pos;

    while (parsed != NULL && index < MAX_ARGS - 1)
    {
        redirect_pos = strchr(parsed, '>');
        if (redirect_pos != NULL)
        {
            // Handle redirection
            // Split the command and redirection part
            *redirect_pos = '\0';

            char *filename = redirect_pos + 1;
            while (*filename == ' ')
                filename++;

            parsedCommand.outputFile = strdup(filename);

            // Remove spaces before '>' (if there)
            char *command_end = redirect_pos - 1;
            while (*command_end == ' ' && command_end > parsed)
            {
                *command_end = '\0';
                command_end--;
            }

            if (*parsed != '\0')
            {
                command[index++] = strdup(parsed);
            }
            break;
        }
        else
        {
            next_space = strchr(parsed, ' ');
            if (next_space != NULL)
            {
                // Replace space with null terminator
                *next_space = '\0';
                // Move to the character after the space
                next_space++;
            }

            if (*parsed != '\0')
            { // Ignore empty tokens
                command[index++] = strdup(parsed);
            }

            parsed = next_space;
        }
    }
    // Null-terminate the command array
    command[index] = NULL;
    return parsedCommand;
}
int main()
{
    char user_input[MAX_INPUT_SIZE];
    pid_t child_pid;
    int stat_loc;

    pid_t shell_pgid = getpgrp();

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);

    printf("Welcome to the shell!\n");
    printf("Newly added shell features: cd Command, ls>test.txt\n");
    for (;;)
    {
        struct sigaction ignore_action;
        ignore_action.sa_handler = SIG_IGN;
        sigemptyset(&ignore_action.sa_mask);
        ignore_action.sa_flags = 0;
        // SIGTTIN ist wenn background process
        // Vom Controlling terminal lesen will
        sigaction(SIGTTIN, &ignore_action, NULL);
        // SIGTTOU wenn man als background process group
        // Auf das controlling terminal zugreifen will und schreiben will
        sigaction(SIGTTOU, &ignore_action, NULL);

        printf("$> ");

        if (fgets(user_input, MAX_INPUT_SIZE, stdin) == NULL)
        {
            printf("\n");
            break;
        }

        if (strcmp(user_input, "\n") == 0)
        {
            continue;
        }

        user_input[strcspn(user_input, "\n")] = 0;

        ParsedCommand parsedCommand = parse_user_input(user_input);

        if (strcmp(parsedCommand.command[0], "cd") == 0)
        {
            if (cd(parsedCommand.command[1]) < 0)
            {
                perror(parsedCommand.command[1]);
            }
            continue;
        }

        child_pid = fork();

        if (child_pid < 0)
        {
            perror("Fork Failed.");
            exit(1);
        }

        if (child_pid == 0)
        {
            // Child
            setpgid(0, 0);

            if (parsedCommand.outputFile != NULL)
            {
                int fd = open(parsedCommand.outputFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (fd == -1)
                {
                    perror("open");
                    exit(1);
                }
                if (dup2(fd, STDOUT_FILENO) == -1)
                {
                    perror("dup2");
                    exit(1);
                }
                close(fd);
            }

            if (execvp(parsedCommand.command[0], parsedCommand.command) < 0)
            {
                perror(parsedCommand.command[0]);
                exit(1);
            }
        }
        else
        {
            // PARENT
            // Set the child process group explicitely
            setpgid(child_pid, child_pid);

            tcsetpgrp(STDIN_FILENO, getpgid(child_pid));

            // Use -child_pid to wait
            // for the specific child
            // process group
            waitpid(-child_pid, &stat_loc, WUNTRACED);

            // Give terminal control
            // back to shell
            // process group
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }

        for (int i = 0; parsedCommand.command[i] != NULL; i++)
        {
            free(parsedCommand.command[i]);
        }
        free(parsedCommand.command);
        if (parsedCommand.outputFile != NULL)
        {
            free(parsedCommand.outputFile);
        }
    }

    return 0;
}