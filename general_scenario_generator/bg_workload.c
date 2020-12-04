#include	<errno.h>
#include	<getopt.h>
#include	<signal.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/time.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<unistd.h>

#include	"libpq-fe.h"

char	*def_hosts[] = {"ubuntu00", "ubuntu01", "ubuntu02", "ubuntu03", "ubuntu04", NULL};
#define DEF_NHOSTS 5
#define	CONN_PER_HOST	8
#define TXN_PER_CONN	16
#define	DEF_INTVL		1000		/* in millisecond */

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
int		  interval = -1;
char	 *dbname = NULL;
char	 *dbuser = NULL;
char	 *query = NULL;
bool	  verbose = false;
bool	  debug = false;
bool	  interactive = false;
FILE	 *logF = NULL;
char	 *logFname = NULL;

pid_t	 *backend_pid = NULL;

pid_t	  my_pid;

static struct option long_options[] =
{
	{"host",	required_argument,	0,	'h'	},
	{"conn",	required_argument,	0,	'c'	},
	{"txn",		required_argument,	0,	't'	},
	{"dbname",	required_argument,	0,	'd'	},
	{"user",	required_argument,	0,	'u'	},
	{"interval",required_argument,	0,	'i'	},	/* Interval in millisecond */
	{"query",	required_argument,	0,	'q'	},
	{"verbose",	no_argument,		0,	'v'	},
	{"debug",	no_argument,		0,	'D'	},
	{"log",		required_argument,	0,	'l'	},
	{"help",	no_argument,		0,	'H'	},
	{0,			0,					0,	0	}
};

static void sighandler(int signal);
static void start_workload(int host_idx, int idx_in_host);
static void init_rand(void);
static void wait_random(void);
static void add_host(char	*host);
static void *my_realloc(void *ptr, size_t size);
static bool PQexecErrCheck(PGresult *res, char *cmd, char *host, int idx_in_host, bool flush_f);
static void enterKey(char *prompt);
static void print(bool logWrite, char *format, ...) __attribute__((format(printf, 2, 3)));
static void flush(void);
static void print_help(void);
#define BOOL(b) ((b) ? "true" : "false")

static void
print_help(void)
{
	printf(
	"--host host | -h host: add host.  Repeat this for more than one host.\n"
	"--conn n | -c n: number of connection per host\n"
	"--txn n | -t n:  number of transactions per connection.  Zero means keep connected to the host and repeat transations.\n"
	"--dbname name | -d name: target database name.  Default is koichi.\n"
	"--user name | -u name: database user name.  Default is koichi.\n"
	"--interval n | -i n: maximum interval between query and transaction end, and before new transaction.\n"
	"--query q | -q q: Query to issue.\n"
	"--verbose | -v : verbose.   Print when each query is issued.\n"
	"--debug | -d: debug mode.\n"
	"--log name | -l name: specify log file to write a log.   Default is stdout.\n"
	"--help : -H : print this message and exit.\n");
}

int
main(int argc, char *argv[])
{
	int	c;
	int	option_index;
	int	ii, jj, kk;

	while(1)
	{
		c = getopt_long(argc, argv, "q:l:i:h:c:t:d:u:DHv",
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
				interval = atoi(optarg);		/* millisecond */
				break;
			case 'q':
				query = strdup(optarg);
				break;
			case 'D':
				debug = true;
				break;
			case 'l':
				if (strcmp(optarg, "-") == 0)
				{
					logF = stdout;
					logFname = NULL;
				}
				else
				{
					logF = fopen(optarg, "w");
					if (logF == NULL)
					{
						printf("Cannot open log file, %s\n", strerror(errno));
						logF = stdout;
						logFname = NULL;
					}
					else
						logFname = strdup(optarg);
				}
				break;
			case 'v':
				verbose = true;
				break;
			case 'H':
				print_help();
				exit(0);
			case '?':
				break;
			default:
				printf("?? invalid character code from getopt %o ??\n", c);
		}
	}
	if (hosts == NULL)
	{
		hosts = def_hosts;
		n_hosts = DEF_NHOSTS;
	}
	if (conn_per_host == 0)
		conn_per_host = CONN_PER_HOST;
	if (txn_per_conn < 0)
		txn_per_conn = TXN_PER_CONN;
	if (dbname == NULL)
		dbname = strdup(DBNAME);
	if (dbuser == NULL)
		dbuser = strdup(DBUSER);
	if (query == NULL)
		query = strdup(SQL_STMT);
	if (interval < 0)
		interval = DEF_INTVL;
	if (isatty(fileno(stdin)) && isatty(fileno(stdout)))
		interactive = true;
	if (logF == NULL)
		logF = stdout;
	backend_pid = (pid_t *)malloc(sizeof(pid_t) * conn_per_host * n_hosts);

	print(true, "**** Backend workload generator for general global deadlock scenario test ****\n");
	print(true, "HOSTS: ");
	for (ii = 0; ii < n_hosts; ii++)
	{
		print(true, "%s ", hosts[ii]);
	}
	print(true,
		  "\nCONN per host: %d, TXN per CONN: %d, optional wait millisec: %d.\n",
		  conn_per_host, txn_per_conn, interval);
	print(true, "Verbosity: %s, logF: %s, debug: %s\n", BOOL(verbose), logFname ? logFname : "stdout", BOOL(debug));
	print(true, "QUERY: %s.\n", query);

	enterKey("Type Enter key to proceed: ");

	kk = -1;
	if (!debug)
	{
		for (ii = 0; ii < n_hosts; ii++)
		{
			for (jj = 0; jj < conn_per_host; jj++)
			{
				flush();
				kk++;
				if ((my_pid = fork()) == 0)
				{
					start_workload(ii, jj);
					/* The function should not return unless there's an error */
					print(true, "Workload generator process returned unexpectedly.  Host: %s\n", hosts[ii]);
					flush();
					exit(1);
				}
				else
					backend_pid[kk] = my_pid;
			}
		}
	}
	else
	{
		printf("Debug mode.   Start workload for the host %s\n", hosts[0]);
		start_workload(0, 0);
	}
	signal(SIGQUIT, &sighandler);
	signal(SIGINT, &sighandler);
	printf("Now all the processes started.   Just waiting.   Type CTRL-C to stop the workload.\n");
	while(1)
	{
		sleep(1);
		putchar('.');
		fflush(stdout);
	}
}

#define MSGLEN	4096
static void
print(bool logWrite, char *format, ...)
{
	va_list	ap;
	char	msg[MSGLEN+1];

	va_start(ap, format);
	vsnprintf(msg, MSGLEN, format, ap);
	va_end(ap);
	printf("%s", msg);
	fflush(stdout);
	if ((logF != stdout) && logWrite)
	{
		fprintf(logF, "%s", msg);
		fflush(logF);
	}
}
#undef MSGLEN

static void
flush(void)
{
	fflush(stdout);
	if (logF != stdout)
		fflush(logF);
}

#define INBUFSZ 512
static void
enterKey(char *prompt)
{
	char	inbuf[INBUFSZ];

	if (interactive)
	{
		if (prompt)
			printf("%s", prompt);
		else
			printf("%s", "Type Enter key: ");
		fgets(inbuf, INBUFSZ, stdin);
	}
}
#undef INBUFSZ

static void
sighandler(int signal)
{
	int ii;
	int	wstatus;

	print(true, "\nInterrupt received.  Terminating worker processes .......\n");
	for(ii = 0; ii < (n_hosts * conn_per_host); ii++)
		kill(backend_pid[ii], SIGQUIT);
	for(ii = 0; ii < (n_hosts * conn_per_host); ii++)
		waitpid(backend_pid[ii], &wstatus, 0);
	print(true, "Worker processes terminated.  Exiting.\n");
	exit(0);
}

#define PQERRCHECK(r, q, h, i, f) do{PQexecErrCheck(r,q,h,i,f); if((r)==NULL)return;}while(0)
#define LDSN	1024
#define PG_BACKEND_PID	"select * from pg_backend_pid();"
static void
start_workload(int host_idx, int idx_in_host)
{
	PGconn		*conn;
	PGresult	*res;
	char		 dsn[LDSN];
	int			 ii;
	char		*host;
	int			 backend_pid;

	host = hosts[host_idx];
	init_rand();
	snprintf(dsn, LDSN, "host=%s dbname=%s user=%s", host, dbname, dbuser);
	while (1)
	{
		conn = PQconnectdb(dsn);
		if ((conn == NULL) || (PQstatus(conn) == CONNECTION_BAD))
		{
			print(true, "Failed to connect to remote database %s not reachable. dsn: '%s'", host, dsn);
			return;
		}
		if (verbose)
		{
			fprintf(logF, "Connected to the database %s, dsn: '%s', idx: %d\n", host, dsn, idx_in_host);
			fflush(logF);
		}
		res = PQexec(conn, PG_BACKEND_PID);
		PQERRCHECK(res, PG_BACKEND_PID, host, idx_in_host, false);
		backend_pid = atoi(PQgetvalue(res, 0, 0));
		PQclear(res);
		print(true, "Host: %s, idx_in_host: %d, Backend PID: %d\n", host, idx_in_host, backend_pid);

		for (ii = 0; (txn_per_conn == 0) || (ii < txn_per_conn); ii++)
		{
			res = PQexec(conn, "BEGIN;");
			PQERRCHECK(res, "BEGIN;", host, idx_in_host, false);
			PQclear(res);
			res = PQexec(conn, query);
			PQERRCHECK(res, query, host, idx_in_host, false);
			PQclear(res);

			wait_random();

			res = PQexec(conn, "ABORT;");
			PQERRCHECK(res, "ABORT;", host, idx_in_host, true);
			PQclear(res);

			if (txn_per_conn == 0)
				ii = 0;
			wait_random();
		}

		PQfinish(conn);
		if (verbose)
		{
			fprintf(logF, "Disconnected from the database %s, dsn: '%s', idx: %d\n", host, dsn, idx_in_host);
			fflush(logF);
		}
	}
}
#undef LDSN

static bool
PQexecErrCheck(PGresult *res, char *cmd, char *host, int idx_in_host, bool flush_f)
{
	ExecStatusType	status;

	if (res == NULL)
	{
		print(true, "Serious error occurred in execute '%s' for host %s, idx %d.\n", cmd, host, idx_in_host);
		flush();
		return false;
	}
	status = PQresultStatus(res);
	if ((status != PGRES_COMMAND_OK) && (status != PGRES_TUPLES_OK))
	{
		print(true, "Query execution error: '%s', at host %s, idx: %d, %s.",
				cmd, host, idx_in_host, PQresultErrorMessage(res));
		flush();
		return false;
	}
	else if (verbose)
	{
		fprintf(logF, "Query executed.  host: %s, idx: %d, query: '%s'\n", host, idx_in_host, cmd);
		if (flush_f)
			fflush(logF);
	}
	return true;
}

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
	wait_usec = (int)((((double)rand()) * ((double)interval) * 1000.0) / (double)RAND_MAX);	/* Microsecond */
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
		printf("\n**** Out of memory. Exiting. ****\n\n");
		exit(1);
	}
	return rv;
}
