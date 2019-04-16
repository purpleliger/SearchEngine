#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "freq_list.h"
#include "worker.h"

FreqRecord** get_word(char *given_word, Node* node, char **filenames);

int main() {
	char *listfile = "./testing/big/dir1/index";
	char *namefile = "./testing/big/dir1/filenames";
	Node *head;
	char **filenames = init_filenames();
	
	read_list(listfile, namefile, &head, filenames);

	//display_list(head, filenames);
	char *given_word = "zeal";
	FreqRecord** result = get_word(given_word, head, filenames);
	print_freq_records(result[0]);
	print_freq_records(result[1]);
	print_freq_records(result[2]);
	return 0;
}
