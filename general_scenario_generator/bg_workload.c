#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<error.h>
#include	<getopt.h>
#include	<sys/time.h>
#include	<sys/types.h>
#include	<signal.h>

#include	"libpq-fe.h"

char	*def_hosts[] = {"ubuntu00", "ubuntu01", "ubuntu02", "ubuntu03", "ubuntu04", NULL};
#define DEF_NHOSTS 5
#define	CONN_PER_HOST	8
#define TXN_PER_CONN	16

#define	SQL_STMT "LOCK TABLE t1 IN ACCESS SHARE MODE;"

#define	DBNAME	"koichi"
#define	DBUSER	"koichi"

typedef int	bool;
#define true	1
#define	false	0

char	**hosts = NULL;
int		  n_hosts = 0;
int		  conn_per_host = 0;
int		  txn_per_conn = 0;
int		  interval = 0;
char	 *dbname = NULL;
char	 *dbuser = NULL;
char	 *query = NULL;
bool	  debug = false;

pid_t	 *backend_pid = NULL;

pid_t	  my_pid;

static struct option long_options[] =
{
	{"host",	required_argument,	0,	'h'	},
	{"conn",	required_argument,	0,	'c'	},
	{"txn",		required_argument,	0,	't'	},
	{"dbname",	required_argument,	0,	'd'	},
	{"user",	required_argument,	0,	'u'	},
	{"interval",required_argument,	0,	'i'	},
	{"query",	required_argument,	0,	'q'	},
	{"debug",	no_argument,		0,	'D'	},
	{0,			0,					0,	0	}
};

static void sighandler(int signal);
static void start_workload(int host_idx);
static void init_rand(void);
static void wait_random(void);
static void add_host(char	*host);
static void *my_realloc(void *ptr, size_t size);

int
main(int argc, char *argv[])
{
	int	c;
	int	option_index;
	int	ii, jj, kk;

	while(1)
	{
		c = getopt_long(argc, argv, "h:c:t:d:u:D",
						long_options, &option_index);
		if (c == -1)
			break;
		switch(c)
		{
			case 'h':
				add_host(optarg);
				break;
			case 'c':
				conn_per_host = atoi(optarg);
				break;
			case 't':
				txn_per_conn = atoi(optarg);
				break;
			case 'd':
				dbname = strdup(optarg);
				break;
			case 'u':
				dbuser = strdup(optarg);
				break;
			case 'i':
				interval = atoi(optarg);
				break;
			case 'q':
				query = strdup(optarg);
				break;
			case 'D':
				debug = true;
				break;
			case '?':
				break;
			default:
				fprintf(stderr, "?? invalid character code from getopt %o ??\n", c);
		}
	}
	if (hosts == NULL)
	{
		hosts = def_hosts;
		n_hosts = DEF_NHOSTS;
	}
	if (conn_per_host == 0)
		conn_per_host = CONN_PER_HOST;
	if (txn_per_conn == 0)
		txn_per_conn = TXN_PER_CONN;
	if (dbname == NULL)
		dbname = strdup(DBNAME);
	if (dbuser == NULL)
		dbuser = strdup(DBUSER);
	if (query == NULL)
		query = strdup(SQL_STMT);
	backend_pid = (pid_t *)malloc(sizeof(pid_t) * conn_per_host * n_hosts);

	printf("**** Backend workload generator for general global deadlock scenario test ****\n");
	printf("HOSTS: ");
	for (ii = 0; ii < n_hosts; ii++)
		printf("%s ", hosts[ii]);
	printf("\nCONN per host: %d, TXN per CONN: %d.\n", conn_per_host, txn_per_conn);
	printf("QUERY: %s.\n", query);

	kk = -1;
	if (!debug)
	{
		for (ii = 0; ii < n_hosts; ii++)
		{
			for (jj = 0; jj < conn_per_host; ii++)
			{
				kk++;
				if ((my_pid = fork()) == 0)
					start_workload(ii);
				else
					backend_pid[kk] = my_pid;
			}
		}
	}
	else
	{
		printf("Debug mode.   Start workload for the host %s\n", hosts[0]);
		start_workload(0);
	}
	printf("Now all the processes started.   Just waiting.   Type CTRL-C to stop the workload.\n");
	signal(SIGQUIT, &sighandler);
	signal(SIGINT, &sighandler);
	while(1)
	{
		sleep(1);
		putchar('.');
		fflush(stdout);
	}
}

static void
sighandler(int signal)
{
	int ii;

	printf("\nInterrupt received.  Stopping.\n");
	for(ii = 0; ii < (n_hosts * conn_per_host); ii++)
		kill(backend_pid[ii], SIGQUIT);
	exit(0);
}

#define LDSN	1024
static void
start_workload(int host_idx)
{
	PGconn		*conn;
	PGresult	*res;
	char		 dsn[LDSN];
	int			 ii;

	init_rand();
	snprintf(dsn, LDSN, "host=%s dbname=%s user=%s", hosts[host_idx], dbname, dbuser);
	while (1)
	{
		conn = PQconnectdb(dsn);
		if ((conn == NULL) || (PQstatus(conn) == CONNECTION_BAD))
			fprintf(stderr, "Failed to connect to remote database '%s' not reachable.", dsn);

		for (ii = 0; ii < txn_per_conn; ii++)
		{
			res = PQexec(conn, "BEGIN;");
			PQclear(res);
			res = PQexec(conn, query);
			PQclear(res);
			res = PQexec(conn, "ABORT;");
			PQclear(res);

			wait_random();
		}

		PQfinish(conn);
	}
}
#undef LDSN

static void
init_rand(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);
}

static void
wait_random(void)
{
	int	wait_usec;

	if (interval == 0)
		return;
	wait_usec = (int)((((double)rand()) * ((double)interval)) / (double)RAND_MAX);
	usleep((useconds_t)wait_usec);
}

static void
add_host(char	*host)
{
	n_hosts++;
	hosts = (char **)my_realloc(hosts, sizeof(char *) * n_hosts);
	hosts[n_hosts - 1] = strdup(host);
}

static void *
my_realloc(void *ptr, size_t size)
{
	void	*rv;

	if (ptr == NULL)
		rv = malloc(size);
	else
		rv = realloc(ptr, size);
	if (rv == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}
	return rv;
}
