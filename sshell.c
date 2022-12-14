#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "string_vector.h"

#define CMDLINE_MAX 512
# define ARGS_MAX 16
# define FAILED 1


typedef struct
{
        char* command;
        char* args[ARGS_MAX];
} program;


char cmd_parser(char* , string_vector*); // returns an arrya of tokens
int execute(program * , unsigned int );

int main(void)
{
        char cmd[CMDLINE_MAX];
        string_vector dir_stack; // to store directory stack
        vec_init(&dir_stack);
        
        char init_cwd[CMDLINE_MAX];
        getcwd(init_cwd, sizeof(init_cwd));
        vec_add(&dir_stack, init_cwd); // add current dir to dirrectoy stack
        pid_t pid;
        int terminal_stdout; // for storing terminal stdout

        while (1) {
                char *nl;
                int retval;

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';
                
                char* is_cd_command = strstr(cmd, "cd"); // check if it's cd command
                
                /* ======== Builtin command ===============*/
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                        return EXIT_SUCCESS;
                        //break;
                }
                else if (!strcmp(cmd, "pwd"))
                {
                        char cwd[CMDLINE_MAX];
                        if (getcwd(cwd, sizeof(cwd)) != NULL) 
                        {
                                // executed sucessfully
                                printf("%s\n", cwd);
                                fprintf(stderr, "+ completed 'pwd' [0]\n");
                        } 
                        else
                        {
                                //perror("Error in getcwd()");
                                fprintf(stderr,"Error in getcwd()");
                                fprintf(stderr, "+ completed 'pwd' [1]\n");
                        }
                }

                else if (is_cd_command)
                {
                        char cwd[CMDLINE_MAX];
                        /* extract the new directory from cmd*/
                        char* cmd_copy = (char*) malloc(strlen(cmd)+1);
                        strcpy(cmd_copy, cmd);
                        char * new_dir = strtok(cmd_copy, " ");
                        new_dir = strtok(NULL, " ");
                        if (!strcmp(new_dir, "~"))
                                // since chdir doesn't recognize ~, we have to use getenv
                                new_dir = getenv("HOME");
                        if (chdir(new_dir) != 0)
                        {
                                fprintf(stderr,"Error: cannot cd into directory\n");
                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 1);
                        }
                        else
                        {
                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                                getcwd(cwd, sizeof(cwd));
                                dir_stack.items[dir_stack.total-1] = cwd;
                        }
                }
                /* ============ Regular command  =====================================================*/
                
                else{
                        program prog; // to store a command + it's arguments
                        string_vector v; // to store all tokens
                        vec_init(&v); //initializes a vector
                        char* cmd_copy = (char*) malloc(strlen(cmd)+1);
                        strcpy(cmd_copy, cmd);
                        char failed = cmd_parser(cmd_copy, &v); // stores tokens in &v
                        if (failed)
                                continue;
                        char is_command = 1; // to distinguish command from argument
                        char is_out_redirection = 0; // boolean
                        char is_pushd = 0; // boolean
                        char finished_input_redirection = 0; // boolean
                        unsigned int args_count = 0; // to keep track of arguments
                        int pipes_count = 0; // to keep track of how many pipes we encounter so far
                        int total_pipes = 0; // total number of pipes in cmd

                        for (int i = 0; i < v.total; i++)
                        {
                                if (!strcmp(v.items[i], "|"))
                                        total_pipes++;
                        }
                        // added extra element, since we are not using zero-indexing
                        int fd[total_pipes+1][2];
                        pid_t pids[total_pipes+2];
                        int pip_ret_status[total_pipes+2]; //output status for each fork that is result of pipe
                        
                        for (int i = 0; i < v.total; i++)
                        {
                                if (args_count >= ARGS_MAX)
                                {
                                        fprintf(stderr, "Error: too many arguments\n");
                                        break;
                                }

                                if ((total_pipes != 0) && (pipes_count == total_pipes) && (i == (v.total-1)))
                                {
                                        // we reached end of cmd, and we did encounter pipe
                                        if (is_command)
                                        {
                                                prog.command= v.items[i];
                                                args_count = 0; 
                                        }

                                        prog.args[args_count++]=v.items[i];
                                        pid = fork();
                                        if (!pid) //child
                                        {
                                                dup2(fd[pipes_count][0], STDIN_FILENO);
                                                // close current pipe
                                                close(fd[pipes_count][0]);
                                                close(fd[pipes_count][1]);
                                                // close previouse pipe
                                                close(fd[pipes_count-1][0]); 
                                                close(fd[pipes_count-1][1]);
                                                char* args_list[args_count+1];
                                                for (unsigned int i = 0; i < args_count; i++)
                                                {
                                                        args_list[i] = prog.args[i];
                                                        //printf("%s\n", args_list[i]);
                                                }
                                                if (is_out_redirection)
                                                {       int fd = open(v.items[i], O_RDWR);
                                                        terminal_stdout = dup(STDOUT_FILENO);
                                                        dup2(fd, STDOUT_FILENO);
                                                        args_list[args_count-1]= NULL;
                                                }
                                                else
                                                    args_list[args_count]= NULL;
                                                execvp(prog.command,  args_list);
                                        }
                                        else // parent
                                        {
                                                pids[pipes_count+1] = pid;
                                                // close current pipe
                                                close(fd[pipes_count][0]);
                                                close(fd[pipes_count][1]);
                                                // close previouse pipe
                                                close(fd[pipes_count-1][0]); 
                                                close(fd[pipes_count-1][1]);
                                                
                                                //int status;
                                                for (int j = 1; j < total_pipes+2; j++)
                                                        waitpid(pids[j], &pip_ret_status[j], 0);
                                                        
                                                fprintf(stderr, "+ completed '%s' ", cmd);
                                                for (int j = 1; j < total_pipes+2; j++)
                                                        fprintf(stderr, "[%d]", WEXITSTATUS(pip_ret_status[j]));
                                                fprintf(stderr, "\n");
                                        }
                                }

                                if (!strcmp(v.items[i], "pushd"))
                                {
                                        is_pushd = 1;
                                        char* filename = (char*)v.items[i+1];
                                        char * abs_path = realpath(filename, NULL);
                                        if (chdir(abs_path) != 0)
                                        {
                                                fprintf(stderr, "Error: no such directory\n");
                                                fprintf(stderr, "+ completed 'pushd %s' [1]\n", (char*) v.items[i+1]);
                                        }
                                        else
                                        {
                                                vec_add(&dir_stack, abs_path);
                                                fprintf(stderr, "+ completed 'pushd %s' [0]\n", (char*)v.items[i+1]);
                                        }
                                        
                                }
                                else if (is_pushd)
                                {
                                        is_pushd = 0;
                                        continue;
                                }
                                 else if (!strcmp(v.items[i], "popd"))
                                 {
                                        if (dir_stack.total == 1)
                                        {
                                                fprintf(stderr, "Error: directory stack empty\n");
                                                continue;
                                        }
                                        //char * popd_dir = vec_pop(&dir_stack);
                                        vec_pop(&dir_stack);
                                        chdir(dir_stack.items[dir_stack.total-1]);
                                        fprintf(stderr, "+ completed 'popd' [0]\n");

                                 }
                                else if (!strcmp(v.items[i], "dirs"))
                                {
                                        for (int i = dir_stack.total-1; i >= 0; --i)
                                        {
                                                printf("%s\n", (char*)dir_stack.items[i]);
                                        }
                                        fprintf(stderr, "+ completed 'dirs' [0]\n");
                                }
                                else if (is_command)
                                {
                                        args_count = 0;
                                        prog.command = v.items[i];
                                        prog.args[args_count++]=v.items[i];
                                        is_command = 0; 
                                        /* if we reach end of cmd, then it means
                                         it's command without any arguments */
                                        if (i == (v.total-1) && (total_pipes == 0))
                                        {
                                                retval = execute(&prog, args_count);
                                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
                                        }
                                        
                                }
                                else if (!strcmp(v.items[i], ">"))
                                {
                                        is_out_redirection = 1;
                                        continue;
                                }
                                
                                else if (!strcmp(v.items[i], "<"))
                                {
                                        finished_input_redirection = 1;
                                        prog.args[args_count++]=v.items[i+1];
                                        retval = execute(&prog, args_count);
                                        
                                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
                                        continue;

                                }
                                else if (finished_input_redirection)
                                {
                                        finished_input_redirection = 0;
                                        is_command = 1;
                                        continue;
                                }
                                else if (!strcmp(v.items[i], "|"))
                                {
                                        pipes_count++;
                                        pipe(fd[pipes_count]);
                                        pid = fork();
                                        if (!pid) //child
                                        {
                                                if (pipes_count == 1) // if this is the first pipe
                                                {
                                                        close(fd[pipes_count][0]); // no need for read access 
                                                        dup2(fd[pipes_count][1], STDOUT_FILENO); // redirect output to pipe
                                                        close(fd[pipes_count][1]);
                                                        char* args_list[args_count+1];
                                                        for (unsigned int i = 0; i < args_count; i++)
                                                        {
                                                                args_list[i] = prog.args[i];
                                                                //fprintf(stderr, "%s\n", args_list[i]);
                                                        }
                                                        //fprintf(stderr, "end\n");
                                                        args_list[args_count]= NULL;
                                                       execvp(prog.command,  args_list);                                                         
                                                }
                                                //else if (pipes_count > 1 && (pipes_count < total_pipes))
                                                else
                                                {
                                                        dup2(fd[pipes_count-1][0], STDIN_FILENO);
                                                        dup2(fd[pipes_count][1], STDOUT_FILENO);
                                                         // close current pipe
                                                        close(fd[pipes_count][0]);
                                                        close(fd[pipes_count][1]);
                                                        // close previouse pipe
                                                        close(fd[pipes_count-1][0]); 
                                                        close(fd[pipes_count-1][1]);
                                                        char* args_list[args_count+1];
                                                        for (unsigned int i = 0; i < args_count; i++)
                                                        {
                                                                args_list[i] = prog.args[i];
                                                                //fprintf(stderr, "%s\n", args_list[i]);
                                                        }
                                                        //fprintf(stderr, "end\n");
                                                        args_list[args_count]= NULL;
                                                       execvp(prog.command,  args_list);      
                                                }
                                                
                                        }
                                        else // parent
                                        {
                                                pids[pipes_count] = pid;
                                                is_command = 1; // next token is new command
                                                prog.command = NULL;
                                                continue;
                                        }    
                                } 
                                /*  == end of pipe handling block ==  */
                                
                                else if ((i == (v.total-1)) && (total_pipes == 0)) 
                                {                                        
                                        // reached end of cmd and didn't encounter pipes
                                        if (is_out_redirection)
                                        {       int fd = open(v.items[i], O_RDWR);
                                                terminal_stdout = dup(STDOUT_FILENO);
                                                dup2(fd, STDOUT_FILENO);
                                                retval = execute(&prog, args_count);
                                                // restore stdout back to terminal
                                                dup2(terminal_stdout, STDOUT_FILENO);
                                                close(terminal_stdout);
                                                close(fd);
                                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);   
                                        }
                                        
                                        else
                                        {
                                                prog.args[args_count++]=v.items[i];
                                                retval = execute(&prog, args_count);
                                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
                                        }
                                        break;

                                }
                                else
                                {
                                        prog.args[args_count++]=v.items[i];
                                }
                                
                        }

                        free(v.items);
                }


                //retval = system(cmd);
                //fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
        }

        return EXIT_SUCCESS;
}

char cmd_parser(char* cmd, string_vector* v)
{
        char * token = strtok(cmd, " ");
        char found_sub_token = 0; // boolean
        
        while (token != NULL)
        {
                // check if we have > or < but without whitespace
                if (strlen(token) > 1)
                        for (unsigned int i = 0; i < strlen(token); ++i)
                        {
                                if (token[i] == '>' || token[i] == '<')
                                {
                                        char delimeter[2] = {token[i], '\0'}; //since strtok doesn't accpet char
                                        found_sub_token = 1; // true if we find > or <  without whitespaces
                                        char* token_copy;
                                        token_copy = (char*) malloc(strlen(token)+1);
                                        //memcpy(token_copy, token, strlen(token));
                                        strcpy(token_copy, token);
                                        char * sub_token = strtok(token_copy, delimeter);
                                        if (i > 0) // it means there was no whitespace before and after
                                        { 
                                                vec_add(v, sub_token);
                                                vec_add(v,delimeter);
                                                sub_token = strtok(NULL, delimeter);
                                                vec_add(v, sub_token);
                                        }
                                        else // it means there was no whitespace only after
                                        {
                                                vec_add(v, delimeter);
                                                vec_add(v, sub_token);

                                        }
                                }
                        }
                
                if (!found_sub_token)
                        vec_add(v, token);
                token = strtok(NULL, " ");
                found_sub_token = 0;
        }
        if (!strcmp(v->items[0], "|") || !strcmp(v->items[0], ">") || !strcmp(v->items[0], "<"))
        {
                fprintf(stderr, "Error: missing command\n");
                return FAILED; 
        }
        if (!strcmp(v->items[v->total-1], "|"))
        {
                fprintf(stderr, "Error: missing command\n");
                return FAILED; 
        }
        if (!strcmp(v->items[v->total-1], ">"))
        {
                fprintf(stderr, "Error: no output file\n");
                return FAILED; 
        }
        if (!strcmp(v->items[v->total-1], "<"))
        {
                fprintf(stderr, "Error: no input file\n");
                return FAILED; 
        }
        for (int i = 0; i < v->total; i++)
        {
                //printf("%s\n", v->items[i]);
                if (!strcmp(v->items[i], ">"))
                {
                        for (int j = i+1; j < v->total; j++)
                        {
                                if (!strcmp(v->items[j], "|"))
                                {
                                        fprintf(stderr, "Error: mislocated output redirection\n");
                                        return FAILED;
                                }
                        }
                        int fd = open(v->items[i+1] , O_WRONLY | O_CREAT | O_TRUNC, 0777);
                        if (fd == -1)
                        {
                                fprintf(stderr,"Error: cannot open output file\n");
                                close(fd);
                                return FAILED;
                        }
                        close(fd);
                }
                if (!strcmp(v->items[i], "<"))
                {
                       for (int j = i-1; j > 0; j--)
                        {
                                if (!strcmp(v->items[j], "|"))
                                {
                                        fprintf(stderr, "Error: mislocated input redirection\n");
                                        return FAILED;
                                }
                        }
                        int fd = open(v->items[i+1] , O_RDONLY);
                        
                        if (fd == -1)
                        {
                                fprintf(stderr,"Error: cannot open input file\n");
                                close(fd);
                                return FAILED;
                        }
                        close(fd);
                }
        }
        // for (int i = 0; i < v->total; i++)
        //         printf("%s\n", v->items[i]);
        return 0;
}


int execute(program * prog, unsigned int num_args)
{
        char* args_list[num_args+1];
        for (unsigned int i = 0; i < num_args; i++)
        {
                args_list[i] = prog->args[i];
                //printf("%s\n", prog->args[i]);
        }
        args_list[num_args]= NULL;
        pid_t pid;
        pid = fork();


        if (pid == 0) {
                // child
                int out = execvp(prog->command,  args_list); // if fails, returns -1
                if (out == -1)
                {
                        printf("Error: command not found\n");
                        return 1;
                }
                
        }
        else if (pid != 0)
        {
                //parent
                int status;
                wait(&status); // wait for child to finish
                // fprintf(stderr, "+ completed '%s' [%d]\n", prog->command, WEXITSTATUS(status));
                return WEXITSTATUS(status);

        }
        else
        {
                perror("Error while forking\n");
                //free(v->items);
                exit(1);
        }
        return 0;
}
