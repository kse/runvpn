#include <stdio.h>
#include <stdlib.h>
//#include <dirent.h>
#include <string.h>
//#include <signal.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <errno.h>
//#include <unistd.h>
//#include <glob.h>
//#include <fcntl.h>

#include "runvpn.h"

static void
oom()
{
	static const char e[] = "Out of memory\n";

	fprintf(stderr, e);
#ifdef SIGQUIT
	raise(SIGQUIT);
#endif
	_Exit(EXIT_FAILURE);
}

void *
xmalloc(size_t size)
{
	void *p = malloc(size);

	if (p == NULL)
		oom();

	return p;
}

char *
xstrdup(const char *str)
{
	char *dup = strdup(str);

	if (dup == NULL)
		oom();

	return dup;
}


int
main(int argc, char *argv[])
{
	const char *root_folder = getenv("runvpn_root");
	struct vpn *vpn = NULL;
	struct vpn *prev = NULL;

	if (root_folder == NULL) {
		printf("Environment variable \"runvpn_root\" is not defined.\n");
		return EXIT_FAILURE;
	}

	if (argc == 1) {
		/* fprintf(stdout, "%30s%s%s\n", BLUE_GRAY, "Listing VPNS", RESET); */
		puts("                    " BLUE_GRAY "Listing VPNS" RESET);

		vpn = get_vpns(root_folder);
		while(vpn != NULL)
		{
			//for (vpn = get_vpns(root_folder); vpn; vpn = vpn->next) {
			fprintf(stdout, "%25s - ", vpn->name);

			switch (vpn_status(vpn)) {
			case VPN_ERROR:
				return EXIT_FAILURE;

			case VPN_DEAD:
				print_color("Down", YELLOW);
				break;

			case VPN_RUNNING:
				print_color("Up", GREEN);
				break;

			case VPN_PERM_DENIED:
				print_color("Permission denied", RED);
				break;

			case VPN_STALE_PID:
				print_color("Down", YELLOW);
				vpn_delete_logfile(vpn);
				vpn_delete_pidfile(vpn);
				break;

			default:
				print_color("Unknown", RED);
				break;
			}
			
			prev = vpn;
			vpn = prev->next;

			vpn_free(prev);
			free(prev);
		}

	} else if (argc == 2) {
		struct vpn vpn;

		if (vpn_init(&vpn, root_folder, argv[argc - 1])) {
			fprintf(stderr, "Problem finding vpn: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}

		if (vpn_status(&vpn) == VPN_ERROR ||
				vpn_start(&vpn, NO_DAEMON))
			return EXIT_FAILURE;

		vpn_start(&vpn, NO_DAEMON);
        vpn_free(&vpn);

	} else if (argc == 3) {
		struct vpn vpn;
		char *name = *++argv;
		char *argument = *++argv;

		if (vpn_init(&vpn, root_folder, name) == -1) {
			fprintf(stderr, "Problem finding vpn: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}

		if (vpn_status(&vpn) == VPN_ERROR)
			return EXIT_FAILURE;

		/* Do different things depending on 2nd argument pased. */
		if (strcmp(argument, "stop") == 0) {
			if (vpn_stop(&vpn))
				return EXIT_FAILURE;
		} else if (strcmp(argument, "reload") == 0) {
			if (vpn_reload(&vpn))
				return EXIT_FAILURE;
		} else if (strcmp(argument, "daemon") == 0) {
			if (vpn_start(&vpn, DAEMON))
				return EXIT_FAILURE;
		} else if (strcmp(argument, "drestart") == 0) {
			if (vpn_stop(&vpn) || vpn_start(&vpn, DAEMON))
				return EXIT_FAILURE;
		} else if (strcmp(argument, "restart") == 0) {
			if (vpn_stop(&vpn) || vpn_start(&vpn, NO_DAEMON))
				return EXIT_FAILURE;
		} else if (strcmp(argument, "log") == 0) {
			vpn_dumplog(&vpn);
		} else {
			fprintf(stderr, "Unknown action '%s'\n", argument);
		}

        vpn_free(&vpn);
	}

	return EXIT_SUCCESS;
}

void
print_color(const char *text, char *color)
{
	fprintf(stdout, "%s%s%s\n", color, text, RESET);
}
