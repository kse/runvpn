#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
//#include <sys/stat.h>
//#include <fcntl.h>

#define PID_FILE "openvpn.pid"
#define ARRAY_SIZE 50

struct vpn {
	char	*name;
	char	*path;
	struct vpn *next;
};

struct vpn* get_vpns(const char* root_folder);
void print_status(struct vpn* vpn);
void chomp (char *string);


int main(int argc, char **argv) {
	const char *root_folder = getenv("runvpn_root");
	struct vpn *vpn;

	(void) argv;

	if(root_folder == NULL) {
		printf("Environment variable \"runvpn_root\" is not defined.\n");
		return EXIT_FAILURE;
	}

	if(argc == 1) {
		vpn = get_vpns(root_folder);

		while (vpn != NULL) {
			print_status(vpn);
			vpn = vpn->next;
		}
		//print_status(root_folder);
		//get_vpns();
	}
	return EXIT_SUCCESS;
}

struct vpn* get_vpns(const char* root_folder) {
	struct dirent *dir;
	DIR *dfd_root;

	struct vpn *current = NULL;
	struct vpn *next = NULL;

	dfd_root = opendir(root_folder);

	if (dfd_root == NULL) {
		fprintf(stderr, "Cannot open directory %s.\n", root_folder);
		exit(EXIT_FAILURE);
	}

	while((dir = readdir(dfd_root)) != NULL) {

		// Skip if directory if '.' or ".."
		if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
			continue;
		}

		if(dir->d_type == DT_DIR) {
			current = malloc(sizeof(struct vpn));

			current->name = strdup(dir->d_name);

			current->path = malloc(strlen(root_folder) + strlen(dir->d_name) + 2);
			sprintf(current->path, "%s/%s", root_folder, dir->d_name);

			current->next = NULL;
			if(next != NULL) {
				current->next = next;
			}

			next = current;
		} 
	}

	closedir(dfd_root);

	return current;
}

void print_status(struct vpn* vpn) {
	FILE *pid_file;
	int pid;
	char line[10];

	if(chdir(vpn->path) == -1) {
		fprintf(stderr, "Error changing to folder '%s': %s", vpn->path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	pid_file = fopen(PID_FILE, "r");

	if(pid_file == NULL) {
		return;
	}

	if(fgets(line, 10, pid_file) == NULL) {
		return;
	}
	chomp(line);
	pid = atoi(line);

	
	if(kill(pid, 0) == -1) {
		fprintf(stderr, "Error killing %d: %s\n", pid, strerror(errno));
	}

	fprintf(stdout, "PID of %s is %i\n", vpn->path, pid);
}

void chomp (char *string) {
	int length = strlen(string);

	if(string[length - 1] == '\n') {
		string[length - 1] = '\0';
	}
}
