#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <glob.h>
#include <fcntl.h>

#include "runvpn.h"

#define log_error(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

void
vpn_free(struct vpn *vpn)
{
    free(vpn->name);
    free(vpn->path);
    free(vpn->config);
    free(vpn->log);
    free(vpn->pid_file);
    free(vpn);
}

static void
chomp(char *string)
{
	int length = strlen(string);

	if (string[length - 1] == '\n')
		string[length - 1] = '\0';
}

enum vpn_status
vpn_status(struct vpn *vpn)
{
	FILE *pid_file = NULL;
	int pid;
	char line[11];

	if (chdir(vpn->path) == -1) {
		log_error("Error changing to folder '%s': %s",
		          vpn->path, strerror(errno));
		vpn->status = VPN_ERROR;
		goto out;
	}

	pid_file = fopen(vpn->pid_file, "r");
	if (pid_file == NULL) {
		vpn->status = VPN_DEAD;
		goto out;
	}

	if (fgets(line, 10, pid_file) == NULL) {
		vpn->status = VPN_DEAD;
		goto out;
	}
	chomp(line);
	pid = atoi(line);

	if (kill(pid, 0) == -1) {
		switch (errno) {
		case ESRCH:
			vpn->status = VPN_STALE_PID;
			break;

		case EPERM:
			vpn->status = VPN_PERM_DENIED;
			break;

		default:
			log_error("Unknown return from kill");
			vpn->status = VPN_ERROR;
			break;
		}

		goto out;
	}

	vpn->status = VPN_RUNNING;
	vpn->pid = pid;

out:
	if (pid_file)
		fclose(pid_file);

	return vpn->status;
}

int
vpn_start(struct vpn *vpn, int as_daemon)
{
	char *argv[] = {
		"/usr/sbin/openvpn",
		"--cd", vpn->path,
		"--config", vpn->config,
		"--script-security", "2",
		"--writepid", vpn->pid_file,
		as_daemon == DAEMON ? "--daemon" : NULL,
		"--log", LOG_FILE,
		NULL
	};

	if (vpn->status == VPN_RUNNING) {
		log_error("VPN %s is already running", vpn->name);
		return -1;
	}

	printf("Starting VPN %s\n", vpn->name);

	if (execv("/usr/sbin/openvpn", argv)) {
		log_error("Error executing /usr/sbin/openvpn: %s",
		          strerror(errno));
		return -1;
	}

	return 0;
}

int
vpn_stop(struct vpn *vpn)
{
	if (vpn->status != VPN_RUNNING) {
		log_error("Error stopping VPN '%s', it is not currently running",
		          vpn->name);
		return -1;
	}

	if (kill(vpn->pid, 15)) {
		switch (errno) {
		case EPERM:
			log_error("You don't have permission to kill pid %i",
			          vpn->pid);
			break;

		case ESRCH:
			log_error("No such process %i", vpn->pid);
			break;

		default:
			log_error("Error killing pid %d: %s",
			          vpn->pid, strerror(errno));
			break;
		}
		return -1;
	}

	print_color("VPN stopped successfully", GREEN);
        vpn->status = VPN_DEAD;

	return 0;
}

int
vpn_reload(struct vpn *vpn)
{
	if (vpn->status != VPN_RUNNING) {
		log_error("Error stopping VPN '%s', it is not currently running",
		          vpn->name);
		return -1;
	}

	if (kill(vpn->pid, 10)) {
		switch (errno) {
		case EPERM:
			log_error("You don't have permission "
			          "to send signals to pid %i", vpn->pid);
			break;

		case ESRCH:
			log_error("Not such process %i", vpn->pid);
			break;

		default:
			log_error("Error killing pid %d: %s",
			          vpn->pid, strerror(errno));
			break;
		}

		return -1;
	}

	print_color("VPN reloaded successfully", GREEN);

	return 0;
}

void
vpn_dumplog(struct vpn *vpn)
{
	FILE *log;
	int c;

	log = fopen(vpn->log, "r");
	if (log == NULL) {
		switch (errno) {
		case EACCES:
			puts("Unable to open logfile.");
			break;

		default:
			log_error("Error opening log '%s': %s",
			          vpn->log, strerror(errno));
		}

		return;
	}

	while ((c = fgetc(log)) != EOF)
		putc(c, stdout);
}

void
vpn_delete_logfile(struct vpn *vpn)
{
	switch(unlink(vpn->log)) {
		case EPERM:
			log_error("Error deleting logfile: %s",
			          strerror(errno));
			break;
	}
}

int
vpn_delete_pidfile(struct vpn *vpn)
{
	if (unlink(vpn->pid_file) == -1) {
		switch (errno) {
		default:
			log_error("Error trying to remove file '%s': %s",
			          vpn->pid_file, strerror(errno));
			break;
		}
	}

	return 0;
}

int
vpn_init(struct vpn *vpn, const char *folder, const char *name)
{
	char *path   = NULL;
	char *log    = NULL;
	char *config = NULL;
	glob_t wcard;

	path = xmalloc(strlen(folder) + 1 + strlen(name) + 1);
	sprintf(path, "%s/%s", folder, name);

	if (access(path, F_OK) == -1)
		goto error;

	if (chdir(path) == -1) {
		log_error("Error changing to folder '%s': %s",
		          path, strerror(errno));
		goto error;
	}

	log = xmalloc(strlen(path) + 1 + strlen(LOG_FILE) + 1);
	sprintf(log, "%s/%s", path, LOG_FILE);

	switch (glob(CONF_PATTERN, GLOB_NOESCAPE, NULL, &wcard)) {
	case 0:
		break;

	case GLOB_NOMATCH:
		log_error("VPN not found: '%s'", name);
		globfree(&wcard);
		goto error;

	default:
		log_error("Failure finding config: %s",
		          strerror(errno));
		globfree(&wcard);
		goto error;
	}

	if (wcard.gl_pathc > 2)
		log_error("Warning, there are several .conf files. Using the first");

	config = xmalloc(strlen(path) + 1 + strlen(wcard.gl_pathv[0]) + 1);
	sprintf(config, "%s/%s", path, wcard.gl_pathv[0]);

	vpn->pid_file = xmalloc(strlen(name) +
	                        strlen(PID_PREFIX) +
	                        strlen(PID_SUFFIX) + 2);

	sprintf(vpn->pid_file, "%s/%s%s", PID_PREFIX, name, PID_SUFFIX);

	globfree(&wcard);

	vpn->name   = xstrdup(name);
	vpn->path   = path;
	vpn->log    = log;
	vpn->config = config;
	vpn->next	= NULL;

	return 0;

error:
	if (path)
		free(path);
	if (log)
		free(log);
	if (config)
		free(config);

	return -1;
}

struct vpn *
get_vpns(const char *root_folder)
{
	DIR 	*dfd_root;
	struct 	dirent 	*dir;
	struct 	vpn 	*head = NULL;
	struct 	vpn 	*t_vpn = NULL;
	struct	vpn		*prev = NULL;
	int 			cmp;

	dfd_root = opendir(root_folder);
	if (dfd_root == NULL) {
		log_error("Cannot open directory '%s'", root_folder);
		exit(EXIT_FAILURE);
	}

	while ((dir = readdir(dfd_root)) != NULL) {
		/* Skip if directory if '.' or ".." */
		if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		if (dir->d_type == DT_DIR) {
			struct vpn *new = xmalloc(sizeof(struct vpn));

			if (vpn_init(new, root_folder, dir->d_name))
				return NULL;


			if(head == NULL) {
				head = new;
				t_vpn = head;
			} else {
				// Reset loop variables.
				t_vpn = head;
				prev = NULL;

				while(1) {
					cmp = strcasecmp(t_vpn->name, new->name);

					// If t_vpn is greater than new.
					if(cmp > 0) {
						//We add t_vpn ahead of new.
						new->next = t_vpn;

						// If we had a previous, we correct the adressing.
						if(prev != NULL) {
							prev->next = new;
						}
						
						// If t_vpn is head, replace the link to head.
						if(t_vpn == head) {
							head = new;
						}

						break;

					} else if(t_vpn->next == NULL) {
						// If this is the case we've reached the end of the linked list.
						// Therefore we just append
						t_vpn->next = new;
						break;
					}

					// Remember the previous VPN
					prev = t_vpn;

					// Loop over the next point in our list.
					t_vpn = t_vpn->next;
				}
			}
		}
	}

	closedir(dfd_root);

	return head;
}
