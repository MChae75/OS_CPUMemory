#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>


#define MAX 2000
#define MAX_LINE 256
#define USER_MEMORY 1000
#define USER_STACK 999
#define SYSTEM_STACK 1999


int main(int argc, char *argv[]) {
    // Check if the number of arguments is correct
    if (argc != 3) {
        printf("Error: Invalid number of arguments\n");
        exit(1);
    }

    // fork the process into CPU and Memory
    pid_t CPU_pid, Memory_pid;
    int C_exitcode, M_exitcode;
    
    // Create a pipe for communication between Memory and CPU
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        exit(1);
    }

    CPU_pid = fork();

    if (CPU_pid == -1) {
        perror("CPU fork failed");
        exit(1);
    } 
    else if (CPU_pid == 0) {
        // Child process (CPU)
        int memory[MAX];
        int PC, IR, AC, X, Y;
        int SP = USER_STACK + 1;
        int systemMode = 0;
        int systemStack = SYSTEM_STACK + 1;

        // You get timer from argument
        int Timer = atoi(argv[2]);
        int timer = 0;

        // Close write end of the pipe
        close(pipe_fd[1]); 

        // Read the memory array from the pipe
        for (int i = 0; i < MAX; i++) {
            char word[MAX_LINE];
            read(pipe_fd[0], word, sizeof(word));
            sscanf(word, "%d", &memory[i]);
        }

        PC = 0;
        while (1) {
            // Fetch
            IR = memory[PC];

            // If the instruction need an operand, save the operand in a local variable and increment PC
            int operand = 0;
            if ( IR == 1 || IR == 2 || IR == 3 || IR == 4 || IR == 5 || IR == 7 || IR == 9 || IR == 0 || IR == 20 || IR == 21 || IR == 22 || IR == 23) {
                operand = memory[++PC];
            }

            //Instruction set
            switch (IR) {
                case 1:
                    AC = operand;
                    break;
                case 2:
                    AC = memory[operand];
                    if (operand >= USER_MEMORY && systemMode == 0) {
                        printf("Memory violation: accessing system address %d in user mode\n", operand);
                        exit(1);
                    }
                    break;
                case 3:
                    AC = memory[memory[operand]];
                    if (memory[operand] >= USER_MEMORY && systemMode == 0) {
                        printf("Memory violation: accessing system address %d in user mode\n", memory[operand]);
                        exit(1);
                    }
                    break;
                case 4:
                    AC = memory[operand + X];
                    if (operand + X >= USER_MEMORY && systemMode == 0) {
                        printf("Memory violation: accessing system address %d in user modey\n", operand + X);
                        exit(1);
                    }
                    break;
                case 5:
                    AC = memory[operand + Y];
                    if (operand + Y >= USER_MEMORY && systemMode == 0) {
                        printf("Memory violation: accessing system address %d in user mode\n", operand + Y);
                        exit(1);
                    }
                    break;
                case 6:
                    AC = memory[SP + X];
                    if (SP + X >= USER_MEMORY && systemMode == 0) {
                        printf("Memory violation: accessing system address %d in user mode\n", SP + X);
                        exit(1);
                    }
                    break;
                case 7:
                    memory[operand] = AC;
                    break;
                case 8:
                    AC = rand() % 100 + 1;
                    break;
                case 9:
                    if (operand == 1) {
                        printf("%d", AC);
                    } else if (operand == 2) {
                        putchar(AC);
                    }
                    break;
                case 10:
                    AC += X;
                    break;
                case 11:
                    AC += Y;
                    break;
                case 12:
                    AC -= X;
                    break;
                case 13:
                    AC -= Y;
                    break;
                case 14:
                    X = AC;
                    break;
                case 15:
                    AC = X;
                    break;
                case 16:
                    Y = AC;
                    break;
                case 17:
                    AC = Y;
                    break;
                case 18:
                    SP = AC;
                    break;
                case 19:
                    AC = SP;
                    break;
                case 20:
                    PC = operand;
                    timer++;
                    continue;
                case 21:
                    if (AC == 0) {
                        PC = operand;
                        timer++;
                        continue;
                    }
                    break;
                case 22:
                    if (AC != 0) {
                        PC = operand;
                        timer++;
                        continue;
                    }
                    break;
                case 23:
                    memory[--SP] = PC;
                    PC = operand;
                    timer++;
                    continue;
                case 24:
                    PC = memory[SP++];
                    timer++;
                    continue;
                case 25:
                    X++;
                    break;
                case 26:
                    X--;
                    break;
                case 27:
                    memory[--SP] = AC;
                    break;
                case 28:
                    AC = memory[SP++];
                    break;
                case 29:
                    systemMode = 1;
                    memory[--systemStack] = PC + 1;
                    memory[--systemStack] = SP;
                    PC = 1500;
                    SP = systemStack;
                    timer++;
                    continue;
                case 30:
                    SP = memory[systemStack++];
                    PC = memory[systemStack++];
                    systemMode = 0;
                    timer++;
                    continue;
                case 50:
                    exit(0);
                    break;
            }

            timer++;
            // Timer interrupt
            // A timer interrupt should cause execution at address 1000.
            if (timer >= Timer && systemMode == 0) {
                systemMode = 1;
                memory[--systemStack] = PC + 1;
                memory[--systemStack] = SP;
                PC = 1000;
                SP = systemStack;
                timer = 0;
                continue;
            }
            PC++;
        }
        close(pipe_fd[0]);  // Close read end of the pipe
        exit(0);

        wait(&C_exitcode);
    } 
    else {
        // Parent process
        Memory_pid = fork();

        if (Memory_pid == -1) {
            perror("Memory fork failed");
            exit(1);
        } 
        else if (Memory_pid == 0) {
            // Child process (Memory)
           int memory[MAX];

            close(pipe_fd[0]);  // Close read end of the pipe

            FILE *file = fopen(argv[1], "r");
            if (file == NULL) {
                printf("Error: Cannot open file\n");
                exit(1);
            }

            char line[MAX_LINE];
            int address = 0;
            // Read the first word of each line from the input file
            while (fgets(line, MAX_LINE, file) != NULL) {
                // If the line is blank or not integer, put 0 in the memory array and continue
                if (line[0] == '\n' || line[0] == '/' || line[0] == ' ') {
                    continue;
                }

                char word[MAX_LINE];
                sscanf(line, "%s", word);
                // If the word is period followed by a number(ex: .1000), change the load address to the number followed by the period
                if (word[0] == '.') {
                    sscanf(word, ".%d", &address);
                    continue;
                }
                memory[address] = atoi(word);
                address++;
            }

           // Pipe memory array to CPU by pipe
            for (int i = 0; i < MAX; i++) {
                char word[MAX_LINE];
                sprintf(word, "%d", memory[i]);
                write(pipe_fd[1], word, sizeof(word));
            }

            fclose(file);
            
            close(pipe_fd[1]);  // Close write end of the pipe

            // Wait for the CPU process to finish
            wait(&M_exitcode);
        } 
        else {
            // Parent process
            close(pipe_fd[0]); // Close read end of the pipe
            close(pipe_fd[1]); // Close write end of the pipe

            // Wait for both Memory and CPU processes to finish
            waitpid(Memory_pid, &M_exitcode, 0);
            waitpid(CPU_pid, &C_exitcode, 0);
        }
    }
    return 0;
}




