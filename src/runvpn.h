#define	GREEN			"\e[0;32m"
#define RESET			"\e[0m"
#define RED 			"\e[0;31m"
#define YELLOW			"\e[0;33m"
#define PURPLE			"\e[0;35m"
#define BLUE_GRAY		"\e[40;1;34m"

#define PID_FILE 		"openvpn.pid"
#define LOG_FILE		"openvpn.log"
#define CONF_PATTERN	"*.conf"

/* Defines for vpn status */
#define VPN_RUNNING		1
#define VPN_DEAD		2
#define VPN_PERM_DENIED	3
#define VPN_STALE_PID	4

#define DAEMON			1
#define NO_DAEMON		2

struct vpn {
	char	*name;
	char	*path;
	char	*config;
	char	*log;
	int		status;
	int		pid;
	struct vpn *next;
};

void print_log(struct vpn *vpn);
int stop_vpn (struct vpn *vpn);
int get_vpn(const char *root_folder, char *name, struct vpn *vpn);
int start_vpn (struct vpn *vpn, int daemon);
int delete_pid_file(struct vpn *vpn);
struct vpn* get_vpns(const char* root_folder);
int vpn_status(struct vpn* vpn);
void print_color(const char* text, char* color);
void reload_vpn (struct vpn *vpn);
void chomp (char *string);
