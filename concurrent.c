#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int i;
    pid_t pid;

    for (i = 0; i < 3; i++) {
        pid = fork();

        if (pid < 0) {
            // Fork failed
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {

            printf("Child process %d executing: \n", i + 1);


            int status = system("./client cobalt 18100 5000 1000");

            if (status == -1) {
                perror("system() call failed");
            }

            // Exit child process after executing the command
            exit(0);
        }
        // Parent process continues to next iteration (creating more children)
    }

    // Parent process waits for all child processes to complete
    for (i = 0; i < 10; i++) {
        wait(NULL);  // Wait for each child to finish
    }

    printf("All child processes finished.\n");

    return 0;
}
