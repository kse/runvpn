#define GREEN         "\e[0;32m"
#define RESET         "\e[0m"
#define RED           "\e[0;31m"
#define YELLOW        "\e[0;33m"
#define PURPLE        "\e[0;35m"
#define BLUE_GRAY     "\e[40;1;34m"

#define PID_FILE      "openvpn.pid"
#define LOG_FILE      "openvpn.log"
#define CONF_PATTERN  "*.conf"

#define DAEMON        1
#define NO_DAEMON     2

/* Defines for vpn status */
enum vpn_status {
	VPN_RUNNING     = 1,
	VPN_DEAD        = 2,
	VPN_PERM_DENIED	= 3,
	VPN_STALE_PID   = 4
};

struct vpn {
	char            *name;
	char            *path;
	char            *config;
	char            *log;
	enum vpn_status status;
	int             pid;
	struct vpn      *next;
};

int vpn_init(struct vpn *vpn, const char *folder, const char *name);
enum vpn_status vpn_status(struct vpn *vpn);
int vpn_start(struct vpn *vpn, int daemon);
int vpn_stop(struct vpn *vpn);
void vpn_reload(struct vpn *vpn);
void vpn_dumplog(struct vpn *vpn);
int vpn_delete_pidfile(struct vpn *vpn);

struct vpn *get_vpns(const char *root_folder);
struct vpn * vpns_sort(struct vpn *vpn);
void print_color(const char *text, char *color);

void *xmalloc(size_t size);
char *xstrdup(const char *str);
void vpn_delete_logfile(struct vpn *vpn);
