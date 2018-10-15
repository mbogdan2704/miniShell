/**
 * Operating Systems 2013-2017 - Assignment 2
 *
 * Maria Costin Bogdan, 333CB
 *
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 Nu a folosit asta
 */

static bool shell_cd(word_t *dir)
{
	
	return 0;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	exit(EXIT_FAILURE);
}
/*Functie care verifica daca level-ul este un numar pozitiv si daca exista comanda*/
int sanity_simple_check(simple_command_t *s, int level, command_t *father) {
	
	if(level < 0)
		return -1;
	if(s == NULL)
		return -1;
	return 0;

}
/*Functia care deschide(creeaza) un fisier si face Trunc sau Append, in functie de flag,
  dupa care duplica fd-ul in fd-ul fisierului respectiv*/
static int redirect(int fd, const char *name, int flag) {
	
	int new_fd;
	if(flag == 0) //trunct
		new_fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else if(flag ==1) //Append
		new_fd = open(name, O_WRONLY | O_APPEND | O_CREAT, 0644);
	else if(flag == 2) 
		new_fd = open(name, O_RDONLY| 0644);
	if(new_fd < 0)
		return -1;
	int rez = dup2(new_fd, fd);
	return rez;  
}
/*
*Functia care face parsarea unei comenzi simple

*/
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	
	if(strcmp(s->verb->string,"exit") == 0 || strcmp(s->verb->string,"quit") == 0)
		shell_exit();
 
	int countx = 0;
	int status;
	int i;
	char ** params = NULL;
	int flag;
	int ok = 0;
	if(level < 0)
		return -1;
	int fd = sanity_simple_check(s,level,father);
	if(fd < 0)
		return SHELL_EXIT;
	/*
	*Construiesc lista cu parametrii pe care o voi pasa mai tarziu lui execlp
	*/
	if(s->params != NULL) {
		params = get_argv(s, &countx);
	}
	/*	
		Setare variabile externe
	*/
	if(s->verb->next_part != NULL){
		char *word = get_word(s->verb);
		char *tok = strtok(word, "=");
		char *first = strdup(tok);
		tok = strtok(NULL, "=");
		char *second = strdup(tok);
		setenv(first,second,1);
		free(word);
		free(first);
		free(second);
		return EXIT_SUCCESS;
	}

	pid_t pid = fork();
	switch(pid) {
	case -1: //Eroare
		return -1;	
	case 0: //Procesul copil
		
		ok = 0;
		flag = s->io_flags;
		if(s-> out != NULL) { // pt <
			
			int rez = redirect(STDOUT_FILENO, get_word(s->out), flag);
			if(rez < 0)
				exit(EXIT_FAILURE);
			if(s -> err != NULL) { // pt &>
				ok = 1;
				int rez;
				if(strcmp(get_word(s->out),get_word(s->err)) == 0)
					rez = dup2(STDOUT_FILENO, STDERR_FILENO);
				else
					rez = redirect(STDERR_FILENO, get_word(s->err), 1);
				if(rez < 0)
					exit(EXIT_FAILURE);
			}
		}
		if(s -> in != NULL ) { // pt >
			int rez = redirect(STDIN_FILENO, s->in->string, 2);
			if(rez < 0)
				exit(EXIT_FAILURE);
		
		}
		if(s -> err != NULL && ok == 0 ){ // pt 2> 
			int rez;
			if(flag == 2)
				flag = 1;
			rez = redirect(STDERR_FILENO, s->err->string, flag);

			if(rez < 0)
				exit(EXIT_FAILURE);

		}

		fflush(stdout);
		if(s->params != NULL) {	
			execvp(s->verb->string, params);
		}
		else {
			execlp(s->verb->string, s->verb->string, NULL, NULL);
		}
		//Inseamna ca execvp esueaza, deci nu exista comanda respectiva
		fprintf(stderr, "Execution failed for '%s'\n", s->verb->string);
		exit(EXIT_FAILURE);//Daca ajung aici inseamna ca e o problema
	
	default:
		/* Dezaloc vectorul ince stochez parametrii */
		if(params != NULL) {
		for(i = 0; i < countx ; i++)
				free(params[i]);
		free(params);
		}
		//Aici schimb directorul
		if(strcmp(s->verb->string, "cd") == 0) {
		if(s->params != NULL)
			status = chdir(s->params->string);
		return status;
		}	
		//Astept sa se termine procesul copil si returnez statusul
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) 
				return WEXITSTATUS(status);
		else {
				return SHELL_EXIT;
			}

	}

}
/* Implementare "|" */
static bool do_on_pipe(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{

	int fd[2],status,status2;
	int pid1, pid2;
	pid1 = fork(); //Creez primul proces copil
	status = pipe(fd); //Creez pipe-ul
	switch (pid1){ 
		case -1: //Eroare
			return EXIT_FAILURE;
		case 0://Copil
		
			if(status < 0)
				return EXIT_FAILURE;
			pid2 = fork(); //Creez inca un proces copil, care va face citirea in pipe
			switch(pid2){
				case -1:
					
					return EXIT_FAILURE;
				case 0:
				
					status = close(fd[1]); //Inchidem scrierea, aici se face doar citire
					if(status < 0)
						return EXIT_FAILURE;
					status = dup2(fd[0], STDIN_FILENO);
					if(status < 0)
						return EXIT_FAILURE;
					status = close(fd[0]);
					if(status < 0)
						return EXIT_FAILURE;
					status = parse_command(cmd2, level + 1, father); //Executam comanda
					exit(status);
				default: //Parinte, aici se face scrierea
				
					status = close(fd[0]); //Inchide citirea, aici se face doar scriere
					if(status < 0)
						return EXIT_FAILURE;
				
					status = dup2(fd[1], STDOUT_FILENO);
					if(status < 0)
						return EXIT_FAILURE;
				
					status = close(fd[1]);
				
				
					status = parse_command(cmd1, level + 1, father); //Executam comanda
					if(status < 0)
						return EXIT_FAILURE;
					close(STDOUT_FILENO);
					

			
					status = waitpid(pid2, &status2, 0); //Asteptam procesul copil sa se termine
					if (WIFEXITED(status2)) {
						exit(WEXITSTATUS(status2));
					} else
						exit(EXIT_FAILURE);
			}
		default:
		
			status = waitpid(pid1, &status2, 0); //Asteptam primul proces copil sa se termine
			if (WIFEXITED(status2)) {
				return WEXITSTATUS(status2);
			} else
				return EXIT_FAILURE;
	}

}


/**
 *Implementare "&"
 */
static bool do_in_parallel(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{

    pid_t pid;
	int status;
    int status2;
	pid = fork();
    switch(pid) {
        case -1: //Eroare
        {
            exit(EXIT_FAILURE);
            break;
        
        }
        case 0: //Proces copil, executam parse_command
        {
    		status = parse_command(cmd1, level + 1, father);        
            exit(status);
        }
        default:
        {
          
			status = parse_command(cmd2, level + 1, father);
            if(status < 0)
            	exit(EXIT_FAILURE);
            status = waitpid(pid, &status2, 0);
          	if (WIFEXITED(status2))
                return WEXITSTATUS(status2);
        
        }
    }
    
    return 0;

}


int parse_command(command_t *c, int level, command_t *father)
{

	int status;
	if (c->op == OP_NONE) {
	
		if(strcmp(c->scmd->verb->string, "true") == 0)
			return 0;
		if(strcmp(c->scmd->verb->string, "false") == 0)
			return 1;
		status = parse_simple(c->scmd, level, father);
		return status;
	
	}

	switch (c->op) {

	case OP_SEQUENTIAL:
	
		status = parse_command(c->cmd1, level+1, c);
		if(status == -1)
			return -1;
		status = parse_command(c->cmd2, level+1, c);
		return status;
		break;

	case OP_PARALLEL:

			return do_in_parallel(c->cmd1, c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_NZERO:

		status = parse_command(c->cmd1, level + 1, c);
		if(status != 0)
			status = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_ZERO:
	
		status = parse_command(c->cmd1, level + 1, c);
		
		if(status == 0)
			status = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PIPE:

		status = do_on_pipe(c->cmd1, c->cmd2, level + 1, c);
		break;

	default:
	
		break;
	}
		
	return status; 
}
