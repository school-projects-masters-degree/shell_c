#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024

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

char **parse_user_input(char *user_input)
{
    // Command maximum of 16 words
    char **command = malloc(16 * sizeof(char *));
    if (command == NULL)
    {
        perror("Malloc for command failed");
        exit(1);
    }

    char *separator = " ";
    char *parsed;
    int index = 0;

    parsed = strtok(user_input, separator);

    while (parsed != NULL)
    {
        command[index] = parsed;
        index++;
        parsed = strtok(NULL, separator);
    }

    command[index] = NULL;
    return command;
}

int main()
{
    char **command;
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

    for (;;)
    {
        // Ignore SIGTTIN and SIGTTOU to prevent shell from being suspended
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

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

        // After cleaning user input
        // Call method
        command = parse_user_input(user_input);

        if (strcmp(command[0], "cd") == 0)
        {
            if (cd(command[1]) < 0)
            {
                perror(command[1]);
            }
            // Skip the fork()
            continue;
        }

        child_pid = fork();

        // Error Handling:
        // So that we dont run out of memory

        if (child_pid < 0)
        {
            perror("Fork Failed.");
            exit(1);
        }

        if (child_pid == 0)
        {
            // Child
            setpgid(0, 0);

            // if it returns -1 it failed
            if (execvp(command[0], command) < 0)
            {
                perror(command[0]);
                exit(1);
            }

            // Exit after fork will terminate the entire program
            // And the exit after execvp will only terminate the child
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

        free(command);
    }

    return 0;
}
