#include "runvpn.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <glob.h>
//#include <sys/stat.h>
//#include <fcntl.h>


int main(int argc, char **argv) {
	const char *root_folder = getenv("runvpn_root");
	struct vpn *vpn;
	int status;

	(void) argv;

	if(root_folder == NULL) {
		printf("Environment variable \"runvpn_root\" is not defined.\n");
		return EXIT_FAILURE;
	}

	if(argc == 1) {
		fprintf(stdout, "%30s%s%s\n", BLUE_GRAY, "Listing VPNS", RESET);
		vpn = get_vpns(root_folder);

		while (vpn != NULL) {
			status = vpn_status(vpn);

			fprintf(stdout, "%25s - ", vpn->name);

			switch(status) {
				case VPN_DEAD :
					print_color("Down", YELLOW);
					break;
				case VPN_RUNNING :
					print_color("Up", GREEN);
					break;
				case VPN_PERM_DENIED :
					print_color("Permission denied", RED);
					break;
				case VPN_STALE_PID :
					print_color("Down", YELLOW);
					delete_pid_file(vpn);
					break;
				default :
					print_color("Unknown", RED);
					break;
			}

			vpn = vpn->next;
		}
	} else if (argc == 2) {
		struct vpn vpn;
		if (get_vpn(root_folder, argv[argc - 1], &vpn) == -1) {
			fprintf(stderr, "Problem finding vpn: %s\n", strerror(errno));
			return 1;
		}
		vpn_status(&vpn);
		start_vpn(&vpn, NO_DAEMON);

	} else if (argc == 3) {
		struct vpn vpn;
		char *name = *++argv;
		char *argument = *++argv;

		if(get_vpn(root_folder, name, &vpn) == -1) {
			fprintf(stderr, "Problem finding vpn: %s\n", strerror(errno));
		}
		vpn_status(&vpn);

		/* Do different things depending on 2nd argument pased. */
		if (strcmp(argument, "stop") == 0) {
			stop_vpn(&vpn);
		} else if (strcmp(argument, "reload") == 0) {
			reload_vpn(&vpn);
		} else if (strcmp(argument, "daemon") == 0) {
			start_vpn(&vpn, DAEMON);
		} else if (strcmp(argument, "drestart") == 0) {
			stop_vpn(&vpn);
			start_vpn(&vpn, DAEMON);
		} else if (strcmp(argument, "restart") == 0) {
			stop_vpn(&vpn);
			start_vpn(&vpn, NO_DAEMON);
		} else {
			fprintf(stderr, "Unknown argument '%s'\n", argument);
		}
	}
	return EXIT_SUCCESS;
}

int start_vpn (struct vpn *vpn, int as_daemon) {
	char root_prefix[] = "--cd";
	char conf_prefix[] = "--config";
	char conf_security[] = "--script-security";
	char security_val[] = "2";

	switch (vpn->status) {
		case VPN_RUNNING:
			printf("VPN %s is already running.\n", vpn->name);
			exit(EXIT_FAILURE);
			break;
	}

	printf("Starting VPN %s\n", vpn->name);

	if( as_daemon == DAEMON ) {
		execl("/usr/sbin/openvpn", "/usr/sbin/openvpn",
				root_prefix, vpn->path,
				conf_prefix, vpn->config,
				conf_security, security_val,
				"--log", LOG_FILE,
				"--writepid", PID_FILE,
				"--daemon",
				(char *)NULL);
	} else {
		execl("/usr/sbin/openvpn", "/usr/sbin/openvpn",
				root_prefix, vpn->path,
				conf_prefix, vpn->config,
				conf_security, security_val,
				"--writepid", PID_FILE,
				(char *)NULL);
	}

	return 0;
}

int stop_vpn (struct vpn *vpn) {
	int kill_status;

	if(vpn->status != VPN_RUNNING) {
		fprintf(stderr, "Error stopping VPN '%s', it is not currently running.\n", vpn->name);
		return -1;
	}

	kill_status = kill(vpn->pid, 15);

	switch (kill_status) {
		case EPERM:
			fprintf(stderr, "You don't have permission to kill pid %i\n", vpn->pid);
			exit(EXIT_FAILURE);
			break;
		case ESRCH:
			fprintf(stderr, "No such process %i\n", vpn->pid);
			break;
		default:
			print_color("VPN stopped successfully", GREEN);
			vpn->status = VPN_DEAD;
	}

	return 0;
}

void reload_vpn (struct vpn *vpn) {
	int kill_status;

	if(vpn->status != VPN_RUNNING) {
		fprintf(stderr, "Error stopping VPN '%s', it is not currently running.\n", vpn->name);
		exit(EXIT_FAILURE);
	}

	kill_status = kill(vpn->pid, 10);

	switch (kill_status) {
		case EPERM:
			fprintf(stderr, "You don't have permission to send signals to pid %i\n", vpn->pid);
			break;
		case ESRCH:
			fprintf(stderr, "Not such process %i\n", vpn->pid);
			break;
		default:
			print_color("VPN reloaded successfully", GREEN);
	}
}

void print_color(const char* text, char* color) {
	fprintf(stdout, "%s%s%s\n", color, text, RESET);
}

int delete_pid_file(struct vpn *vpn) {
	char *path = malloc(strlen(vpn->path) + strlen(PID_FILE) + 2);

	strcpy(path, vpn->path);
	strcat(path, "/");
	strcat(path, PID_FILE);

	if (unlink(path) == -1) {
		switch (errno) {
			default :
				fprintf(stderr, "Error trying to remove file '%s': %s\n", path, strerror(errno));
			break;
		}
	}

	free(path);

	return 0;
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

			get_vpn(root_folder, dir->d_name, current);

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

int get_vpn(const char *root_folder, char *name, struct vpn *vpn) {
	char *path = malloc (strlen(root_folder) + 2 + strlen(name));
	int glob_rtrn;
	glob_t wcard;

	strcpy(path, root_folder);
	strcat(path, "/");
	strcat(path, name);

	if(access(path, F_OK) == -1) {
		return -1;
	}
	vpn->name = name;
	vpn->path = path;


	if (chdir(vpn->path) == -1) {
		fprintf(stderr, "Error changing to folder '%s': %m\n", vpn->path);
	}
	glob_rtrn = glob(CONF_PATTERN, GLOB_NOESCAPE, NULL, &wcard);

	switch (glob_rtrn) {
		case 0:
			if (wcard.gl_pathc > 2) {
				fprintf(stderr, "Warning, there are several .conf files. using the first.\n");
			}
			vpn->config = malloc(strlen(vpn->path) + strlen(wcard.gl_pathv[0]) + 2);

			strcpy(vpn->config, vpn->path);
			strcat(vpn->config, "/");
			strcat(vpn->config, wcard.gl_pathv[0]);
			break;
		case GLOB_NOMATCH:
			fprintf(stderr, "VPN not found: '%s'\n", name);
			exit(EXIT_FAILURE);
			break;
		default:
			fprintf(stderr, "Failure finding config: %m\n");
			exit(EXIT_FAILURE);
			break;
	}

	return 0;
}

int vpn_status(struct vpn* vpn) {
	FILE *pid_file;
	int pid;
	char line[11];

	if(chdir(vpn->path) == -1) {
		fprintf(stderr, "Error changing to folder '%s': %s\n", vpn->path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	pid_file = fopen(PID_FILE, "r");

	if(pid_file == NULL) {
		vpn->status = VPN_DEAD;
		return VPN_DEAD;
	}

	if(fgets(line, 10, pid_file) == NULL) {
		vpn->status = VPN_DEAD;
		return VPN_DEAD;
	}
	chomp(line);
	pid = atoi(line);
	
	if(kill(pid, 0) == -1) {
		if (errno == ESRCH) {
			vpn->status = VPN_STALE_PID;
			return VPN_STALE_PID;
		}
		else if (errno == EPERM) {
			vpn->status = VPN_PERM_DENIED;
			return VPN_PERM_DENIED;
		}
	}

	vpn->status = VPN_RUNNING;
	vpn->pid = pid;
	return VPN_RUNNING;
}

void chomp (char *string) {
	int length = strlen(string);

	if(string[length - 1] == '\n') {
		string[length - 1] = '\0';
	}
}