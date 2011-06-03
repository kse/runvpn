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
		fprintf(stderr, "Error changing to folder '%s': %s\n", vpn->path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	pid_file = fopen(PID_FILE, "r");
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
			goto out;

		case EPERM:
			vpn->status = VPN_PERM_DENIED;
			goto out;
		}
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
		"--writepid", PID_FILE,
		as_daemon == DAEMON ? "--daemon" : NULL,
		"--log", LOG_FILE,
		NULL
	};

	switch (vpn->status) {
	case VPN_RUNNING:
		printf("VPN %s is already running.\n", vpn->name);
		exit(EXIT_FAILURE);

	default:
		break;
	}

	printf("Starting VPN %s\n", vpn->name);

	if (execv("/usr/sbin/openvpn", argv)) {
		fprintf(stderr, "Error executing /usr/sbin/openvpn: %s\n",
		        strerror(errno));
		exit(EXIT_FAILURE);
	}

	return 0;
}

int
vpn_stop(struct vpn *vpn)
{
	if (vpn->status != VPN_RUNNING) {
		fprintf(stderr, "Error stopping VPN '%s', it is not currently running.\n", vpn->name);
		return -1;
	}

	switch (kill(vpn->pid, 15)) {
	case EPERM:
		fprintf(stderr, "You don't have permission to kill pid %i\n", vpn->pid);
		exit(EXIT_FAILURE);

	case ESRCH:
		fprintf(stderr, "No such process %i\n", vpn->pid);
		break;

	default:
		print_color("VPN stopped successfully", GREEN);
		vpn->status = VPN_DEAD;
	}

	return 0;
}

void
vpn_reload(struct vpn *vpn)
{
	if (vpn->status != VPN_RUNNING) {
		fprintf(stderr, "Error stopping VPN '%s', it is not currently running.\n", vpn->name);
		exit(EXIT_FAILURE);
	}

	switch (kill(vpn->pid, 10)) {
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
			fprintf(stderr, "Error opening log %s: %m\n", vpn->log);
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
			fprintf(stderr, "Error deleting logfile %m");
			break;
	}
}

int
vpn_delete_pidfile(struct vpn *vpn)
{
	char *path = xmalloc(strlen(vpn->path) + strlen(PID_FILE) + 2);

	sprintf(path, "%s/%s", vpn->path, PID_FILE);

	if (unlink(path) == -1) {
		switch (errno) {
		default:
			fprintf(stderr, "Error trying to remove file '%s': %s\n", path, strerror(errno));
			break;
		}
	}

	free(path);

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
		fprintf(stderr, "Error changing to folder '%s': %m\n", path);
		goto error;
	}

	log = xmalloc(strlen(path) + 1 + strlen(LOG_FILE) + 1);
	sprintf(log, "%s/%s", path, LOG_FILE);

	switch (glob(CONF_PATTERN, GLOB_NOESCAPE, NULL, &wcard)) {
	case 0:
		break;

	case GLOB_NOMATCH:
		fprintf(stderr, "VPN not found: '%s'\n", name);
		globfree(&wcard);
		goto error;

	default:
		fprintf(stderr, "Failure finding config: %s\n",
		        strerror(errno));
		globfree(&wcard);
		goto error;
	}

	if (wcard.gl_pathc > 2)
		fprintf(stderr, "Warning, there are several .conf files. using the first.\n");

	config = xmalloc(strlen(path) + 1 + strlen(wcard.gl_pathv[0]) + 1);
	sprintf(config, "%s/%s", path, wcard.gl_pathv[0]);

	globfree(&wcard);

	vpn->name   = xstrdup(name);
	vpn->path   = path;
	vpn->log    = log;
	vpn->config = config;

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
	DIR *dfd_root;
	struct dirent *dir;
	struct vpn *head = NULL;

	dfd_root = opendir(root_folder);
	if (dfd_root == NULL) {
		fprintf(stderr, "Cannot open directory %s.\n", root_folder);
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

			new->next = head;
			head = new;
		}
	}

	closedir(dfd_root);

	return head;
}