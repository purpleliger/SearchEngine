#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "freq_list.h"
#include "worker.h"

/* The function get_word should be added to this file */
FreqRecord** get_word(char *given_word, Node* node, char **filenames) {
	//Find the given word in the linked list
	while (node != NULL) {
		if (strncmp(node->word, given_word, strlen(given_word)) == 0) {
			break;
		}
		node = node->next;
	}
	//No word found, create a freqRecord array of length 1 and freq value is 0
	if (node == NULL) {
		//Pointer memory
		FreqRecord** array_FreqRecord = calloc(1, sizeof(FreqRecord*));
		if (array_FreqRecord == NULL) {
			perror("calloc");
			exit(1);
		}
		//Struct memory
		array_FreqRecord[0] = calloc(1, sizeof(FreqRecord));
		if (array_FreqRecord[0] == NULL) {
			perror("calloc");
			exit(1);
		}
		array_FreqRecord[0]->freq = 0;
		return array_FreqRecord;
	}
	//***Word is found!
	int num_freq = 0;
	for (int i = 0; i != MAXFILES; i++) {
		if (node->freq[i] != 0) {
			num_freq++;
		}
	}
	//Create array from the num_freq calculated
	//Pointer memory
	FreqRecord** array_FreqRecord = calloc(num_freq+1,sizeof(FreqRecord*));
	if (array_FreqRecord == NULL) {
		perror("calloc");
		exit(1);
	}
	
	for (int i = 0, j = 0; i != MAXFILES; i++) {
		if (node->freq[i] == 0) {
			continue;
		}
		//Struct memory
		array_FreqRecord[j] = calloc(1,sizeof(FreqRecord));
		if (array_FreqRecord[j] == NULL) {
			perror("calloc");
			exit(1);
		}
		array_FreqRecord[j]->freq = node->freq[i];
		strncpy(array_FreqRecord[j]->filename, filenames[i], strlen(filenames[i]));
		j++;
	}
	//End Node
	array_FreqRecord[num_freq] = calloc(1,sizeof(FreqRecord));
	if (array_FreqRecord[num_freq] == NULL) {
		perror("calloc");
		exit(1);
	}
	array_FreqRecord[num_freq]->freq = 0;
	return array_FreqRecord;
}

/* Print to standard output the frequency records for a word.
* Used for testing.
*/
void print_freq_records(FreqRecord *frp) {
	int i = 0;
	while(frp != NULL && frp[i].freq != 0) {
		printf("%d    %s\n", frp[i].freq, frp[i].filename);
		i++;
	}
}

/* run_worker
* - load the index found in dirname
* - read a word from the file descriptor "in"
* - find the word in the index list
* - write the frequency records to the file descriptor "out"
*/
void run_worker(char *dirname, int in, int out){
	//set up pathname for index
	char index_pathname[strlen(dirname)+7];
	strcpy(index_pathname,dirname);
	strcat(index_pathname,"/index");
	
	//set up pathname for filenames
	char filenames_pathname[strlen(dirname)+11];
	strcpy(filenames_pathname,dirname);
	strcat(filenames_pathname,"/filenames");
	
	//set up head
	Node *head;
	
	//set up filenames
	char **filenames = init_filenames();
	read_list(index_pathname,filenames_pathname, &head, filenames);
	
	//specifies the size of string that will be stored
	char buf[MAXWORD];
	while (1) {
		memset(buf, '\0', MAXWORD);
		//read from file descriptor "in", get_word from index list
		//and store in var

		if (read(in, buf, MAXWORD) == -1) {
			perror("read");
			exit(1);
		}
		if (strlen(buf) == 0) {
			break;		
		}

		if (buf[strlen(buf) - 1] == '\n') {
			buf[strlen(buf) - 1] = 0;
		}

		FreqRecord** result = get_word(buf, head, filenames);

		//write FreqRecords to stdout from results array
		int index = 0;
		while (1) {
			//write FreqRecords to fd out
			char tmp[500];
			memset(tmp, '\0', 500);
			sprintf(tmp, "%d\t%s", result[index]->freq, result[index]->filename);
			if (write(out, tmp, 500) == -1) {
				perror("write");
				exit(1);
			}
			
			if (result[index]->freq == 0) {
				break;
			}
			index++;
		}

		if (in == STDIN_FILENO) {
			break;		
		}
	}
}
