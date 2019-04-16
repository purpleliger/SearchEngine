#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include "freq_list.h"
#include "worker.h"

//Create a struct with directory name, 2 read/ 2 write pipes, child process PID
struct dir_stat {
	int read_pipe[2];
	int write_pipe[2];
	pid_t child_pid;
	char* dirname;
};
typedef struct dir_stat dir_stat;

int main(int argc, char **argv) {
	
	char ch;
	char path[PATHLENGTH];
	char *startdir = ".";

	while((ch = getopt(argc, argv, "d:")) != -1) {
		switch (ch) {
			case 'd':
			startdir = optarg;
			break;
			default:
			fprintf(stderr, "Usage: queryone [-d DIRECTORY_NAME]\n");
			exit(1);
		}
	}
	// Open the directory provided by the user (or current working directory)
	
	DIR *dirp;
	if((dirp = opendir(startdir)) == NULL) {
		perror("opendir");
		exit(1);
	} 

	//count number of directories
	struct dirent *dp_counter;
	int number_of_dir = 0;
	while((dp_counter = readdir(dirp)) != NULL) {

		if(strcmp(dp_counter->d_name, ".") == 0 || 
		   strcmp(dp_counter->d_name, "..") == 0 ||
		   strcmp(dp_counter->d_name, ".svn") == 0){
			continue;
		}
		strncpy(path, startdir, PATHLENGTH);
		strncat(path, "/", PATHLENGTH - strlen(path) - 1);
		strncat(path, dp_counter->d_name, PATHLENGTH - strlen(path) - 1);

		struct stat sbuf;
		if(stat(path, &sbuf) == -1) {
			//This should only fail if we got the path wrong
			// or we don't have permissions on this entry.
			perror("stat");
			exit(1);
		} 
		
		// Only call run_worker if it is a directory
		// Otherwise ignore it.

		if(S_ISDIR(sbuf.st_mode)) {
			number_of_dir++;
		}
	}
	rewinddir(dirp);

	//Define array used for child processes
	dir_stat** array_dir_stat = calloc(number_of_dir, sizeof(dir_stat*));
	if (array_dir_stat == NULL) {
		perror("calloc");
		exit(1);
	}

	/* For each entry in the directory, eliminate . and .., and check
	* to make sure that the entry is a directory, then call run_worker
	* to process the index file contained in the directory.
 	* Note that this implementation of the query engine iterates
	* sequentially through the directories, and will expect to read
	* a word from standard input for each index it checks.
	*/
	
	int counter = 0;
	struct dirent *dp;
	while((dp = readdir(dirp)) != NULL) {
		if(strcmp(dp->d_name, ".") == 0 || 
		   strcmp(dp->d_name, "..") == 0 ||
		   strcmp(dp->d_name, ".svn") == 0){
			continue;
		}
		strncpy(path, startdir, PATHLENGTH);
		strncat(path, "/", PATHLENGTH - strlen(path) - 1);
		strncat(path, dp->d_name, PATHLENGTH - strlen(path) - 1);

		struct stat sbuf;
		if(stat(path, &sbuf) == -1) {
			//This should only fail if we got the path wrong
			// or we don't have permissions on this entry.
			perror("stat");
			exit(1);
		} 
		
		// Only call run_worker if it is a directory
		// Otherwise ignore it.
		
		if(S_ISDIR(sbuf.st_mode)) {
			//assign memory to dir_stat structs array and storing dir name(path)
			array_dir_stat[counter] = calloc(1,sizeof(dir_stat));
			if (array_dir_stat[counter] == NULL) {
				perror("calloc");
				exit(1);			
			}
			array_dir_stat[counter]->dirname = calloc(strlen(path),sizeof(char));
			if (array_dir_stat[counter]->dirname == NULL) {
				perror("calloc");
				exit(1);
			}
			strncpy(array_dir_stat[counter]->dirname, path, strlen(path));
			counter++;
		}	
	}

	//Create one process for each subdirectory
	for (counter = 0; counter != number_of_dir; counter++) {
		if (pipe(array_dir_stat[counter]->read_pipe) == -1) {
			perror("pipe");
			exit(1);
		}
		if (pipe(array_dir_stat[counter]->write_pipe) == -1) {
			perror("pipe");
			exit(1);
		}
		array_dir_stat[counter]->child_pid = fork();

		//Find the child/directory process
		if (array_dir_stat[counter]->child_pid == 0) { //Child
			if (close(array_dir_stat[counter]->read_pipe[1]) == -1) { //close pipes
				perror("close");
				exit(1);
			}
			if (close(array_dir_stat[counter]->write_pipe[0]) == -1) {
				perror("close");
				exit(1);
			}
			//run_worker with input from pipe and ouput to pipe
			run_worker(array_dir_stat[counter]->dirname,array_dir_stat[counter]->read_pipe[0], array_dir_stat[counter]->write_pipe[1]);
			exit(0);
		} else if (array_dir_stat[counter]->child_pid > 0) { //Parent
			if (close(array_dir_stat[counter]->write_pipe[1]) == -1) {
				perror("close");
				exit(1);
			}
			if (close(array_dir_stat[counter]->read_pipe[0]) == -1) {
				perror("close");
				exit(1);
			}
		} else {
			perror("fork");
			exit(1);
		}
	}

	while(1) {
		//prompt user for input
		char a_word[MAXWORD];
		printf("Enter a word:");
		if (fflush(stdout) != 0) {
			perror("fflush");
			exit(1);
		}
		memset(a_word, '\0', MAXWORD);
		//read from file descriptor "in", get_word from index list
		//and store in var

		if (read(STDIN_FILENO, a_word, MAXWORD) == -1) {
			perror("read");
			exit(1);
		}
		if (a_word[strlen(a_word) - 1] == '\n') {
			a_word[strlen(a_word) - 1] = 0;
		}

		//Create array of FreqRecord structs
		FreqRecord** master_array = calloc(MAXRECORDS, sizeof(FreqRecord*));
		if (master_array == NULL) {
			perror("calloc");
			exit(1);
		}

		//Write to pipe
		for (counter = 0; counter != number_of_dir; counter++) {
			if (write(array_dir_stat[counter]->read_pipe[1], a_word, MAXWORD) == -1) {
				perror("write");
				exit(1);
			}
		}

		//Check if we need to exit
		if (strlen(a_word) == 0) {
			break;		
		}

		//Read from pipe
		for (counter = 0; counter != number_of_dir; counter++) {
			char buf[500];
			memset(buf, '\0', 500);
			
			//Seperate freq and filenames from read input from pipe
			while (read(array_dir_stat[counter]->write_pipe[0], buf, 500) > 0) {
				int freq = atoi(strtok(buf, "\t"));
				if (freq == 0) {
					break;				
				}
				char* filename = strtok(NULL, "\t");
				
				FreqRecord* record= calloc(1, sizeof(FreqRecord*));
				if (record == NULL) {
					perror("calloc");
					exit(1);
				}
				record->freq = freq;
				strncpy(record->filename, filename, strlen(filename));
				
				//Add to master_array
				int add_index = 0;
				for (; add_index != MAXRECORDS; add_index++) {
					if (master_array[add_index] == NULL) {
						break;
					}
					if (master_array[add_index]->freq <= record->freq) {
						break;
					}
				}
				//If index is empty, add
				if (master_array[add_index] == NULL) {
					master_array[add_index] = record;
				}
				//If index not empty, push record one by one
				else { //remove last value if needed
					if (master_array[MAXRECORDS - 1] != NULL) {
						free(master_array[MAXRECORDS - 1]);
					}
					//push everything down one by one
					FreqRecord* prev = master_array[add_index];
					master_array[add_index] = record;
					for (int curr_index = add_index + 1; curr_index != MAXRECORDS; curr_index++) {
						FreqRecord* temp = master_array[curr_index];
						master_array[curr_index] = prev;
						prev = temp;

						if (prev == NULL) {
							break;						
						}
					}
				}

				memset(buf, '\0', 500);
			}
		}
		//Print out FreqRecords
		for (int i = 0; i != MAXRECORDS; i++) {
			if (master_array[i] == NULL) {
				break;
			}
			
			printf("%d\t%s\n", master_array[i]->freq, master_array[i]->filename);
		}
		//Clean up memory
		for (int i = 0; i != MAXRECORDS; i++) {
			free(master_array[i]);
		}
		free(master_array);
	}

	//Exit - wait for child processes and cleanup
	for (counter = 0; counter != number_of_dir; counter++) {
		wait(&array_dir_stat[counter]->child_pid);
	}

	for (counter = 0; counter != number_of_dir; counter++) {
		free(array_dir_stat[counter]->dirname);
		free(array_dir_stat[counter]);
	}

	free(array_dir_stat);
	return 0;
}
