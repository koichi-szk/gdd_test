#include	<ctype.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<stdarg.h>
#include	<getopt.h>
#include	<string.h>
#include	<errno.h>

#include	"libpq-fe.h"

#define CMDMAX	4096
#define MAXWORDLEN	64
#define MAXSTRLEN	4096

typedef int	bool;
#define true	1
#define false	0


/*
 * Syntax
 *
 * connect 'dsn' {'sql'| connect 'dsn' ... return} .. return
 */

typedef enum StepCategory
{
	STEP_QUERY,
	STEP_COMMAND,
	STEP_REMOTE
} StepCategory;

typedef union SequenceStep
{
	char	*query;
	char	*command;
	struct RemoteSequence *remote;
} SequenceStep;

typedef struct RemoteSequence
{
	char			 *label;
	char			 *dsn;
	PGconn			 *conn;
	unsigned long	  system_id;		/* Database system ID */
	int				  pid;				/* Backend pid */
	int				  pgprocno;			/* Backend pgprocno */
	int				  lxid;				/* Backend logical TxNo */
	bool			  has_external_lock;	/* Waiting for the external lock */
	int				  nSequenceElement;
	StepCategory *sequenceCategory;
	SequenceStep	**sequenceStep;
} RemoteSequence;

int	auto_label = 0;

char	*inFileName = NULL;
char	*outFileName = NULL;
bool	 parse_only = false;
bool	 write_diagnose = false;
bool	 stay = false;
bool	 add_to_output = false;

FILE	*outF;
FILE	*inF;

static void connectDatabase(RemoteSequence *remoteSequence);
static void connectRemoteSequence(RemoteSequence *mySequence, RemoteSequence *parent);
static void errHandler(bool return_f, char *format, ...) __attribute__((format(printf, 2, 3)));
static void expandRemoteSequence(RemoteSequence *remoteSequence);
static char *getString(char *inata, char **str);
static char *getWord(char *indata, char **word);
static void *my_realloc(void *ptr, size_t size);
static char *parseRemoteSequence(char *indata, RemoteSequence **sequence);
static char *peekChar(char *indata, char *peeked);
static char *read_file(FILE *f);
static void printRemoteSequence(RemoteSequence *sequence, int level);
static void runCommand(RemoteSequence *sequence, char *command);
static void runRemoteSequence(RemoteSequence *sequence, RemoteSequence *parent);
static void runQuery(PGconn *conn, char *query);
static void setCommand(RemoteSequence *sequence, char *command);
static char *skip_sp(char *indata);
static void unWaitRemoteSequence(RemoteSequence *remote, RemoteSequence *parent);
static void waitRemoteSequence(RemoteSequence *remote, RemoteSequence *parent);
static bool PQexecErrCheck(PGresult *res, char *cmd, char *dsn, bool return_f);
/*
 * -f FILE, --input FILE: read sequence from the file.  '-' means stdin and it's the default..
 * -o FILE, --output FILE: write output to the file. '-' means stdout and it's the default.
 * -d, --diagnose: write parsed sequence.
 * -t, --test: Read and parse the sequence.   Do not run it.
 * -s, --stay: Do not terminate the sequence and keep this as outstanding transaction.
 */
int
main(int argc, char *argv[])
{
	int c;
	int	option_index;
	char	*input_sequence;

	RemoteSequence	*topSequence;

	static struct option long_options[] = 
	{
		{"input",	required_argument,	0,	'f' },
		{"output",	required_argument,	0,	'o' },
		{"add",		no_argument,		0,	'a' },
		{"diagnose",	no_argument,	0,	'd' },
		{"test",	no_argument,		0,	't' },
		{"stay",	no_argument,		0,	's' },
		{0,			0,					0,	0   }
	};

	while(1)
	{
		c = getopt_long(argc, argv, "f:o:adts",
					    long_options, &option_index);
		if (c == -1)
			break;
		switch(c)
		{
			case 'f':
				if (inFileName)
					free(inFileName);
				inFileName = strdup(optarg);
				break;
			case 'o':
				if (outFileName)
					free(outFileName);
				outFileName = strdup(optarg);
				break;
			case 'a':
				add_to_output = true;
				break;
			case 'd':
				write_diagnose = true;
				break;
			case 't':
				parse_only = true;
				break;
			case 's':
				stay = true;
				break;
			case '?':
				break;
			default:
				fprintf(stderr, "?? invalid character code from getopt %o ??\n", c);
		}
	}
	if (inFileName == NULL)
		inF = stdin;
	else
	{
		inF = fopen(inFileName, "r");
		if (inF == NULL)
			errHandler(false, "Cannot open input file '%s', %s\n", inFileName, strerror(errno));
	}
	if (outFileName == NULL)
		outF = stdout;
	else
	{
		if (add_to_output)
			outF = fopen(inFileName, "a");
		else
			outF = fopen(inFileName, "w");
		if (outF == NULL)
			errHandler(false, "Cannot open output file '%s', %s\n", outFileName, strerror(errno));
	}
	input_sequence = read_file(inF);
	fclose(inF);
	inF = NULL;

	/* Okay, every input parameter and sequence are good */

	parseRemoteSequence(input_sequence, &topSequence);

	if (write_diagnose)
		printRemoteSequence(topSequence, 0);

	if (parse_only)
		fprintf(outF, "--test option was specified. Exiting.\n");

	runRemoteSequence(topSequence, NULL);
}

#define MSGLEN 1024
static void
errHandler(bool return_f, char *format, ...)
{
	va_list	 ap;
	char	 msg[MSGLEN+1];

	va_start(ap, format);
	vsnprintf(msg, MSGLEN, format, ap);
	va_end(ap);
	fprintf(stderr, "%s\n", msg);
	if (return_f)
		return;
	exit(1);
}
#undef MSGLEN

static int
hex2int(char hex)
{
	if (hex >= '0' && hex <= '9')
		return hex - '0';
	if (hex >= 'a' && hex <= 'f')
		return (hex - 'a') + 10;
	if (hex >= 'A' && hex <= 'F')
		return (hex - 'A') + 10;
	return 0;
}

static long
hex2int64(char *hex)
{
	int		ii;
	long	rv;
	
	rv = 0;
	for (ii = 0; ii < 16; ii += 2)
	{
		int xx;
	
		xx = (hex2int(hex[ii]) << 4) | hex2int(hex[ii+1]);
		rv = (rv << 8) | xx;
	}
	return rv;
}

/*
 * Connect to the remote database, acquire EXTERNAL LOCK at the parent,
 * obtain remote transaction information and set this as EXTERNAL LOCk
 * property and wait on it.
 *
 * If parent already has exernal lock, unwait the old one, then set new
 * external lock property and wait on the new one.
 */
static void
connectRemoteSequence(RemoteSequence *mySequence, RemoteSequence *parent)
{
	PGresult	*res;
	int			 nn;
#define	BEGIN_STMT			"begin;"
#define SHOW_MYSELF_STMT	"select * from gdd_test_show_myself();"


	/* Connect to the database for mySequence */
	fprintf(outF, "Connecting to database '%s'\n", mySequence->dsn);
	if (parent && parent->conn == NULL)
		errHandler(false, "Parent is not connected to the databae '%s'.", parent->dsn);
	if (mySequence->conn)
		errHandler(false, "Already connected to the database, '%s'.", mySequence->dsn);
	connectDatabase(mySequence);

	/* BEGIN */
	res = PQexec(mySequence->conn, BEGIN_STMT);
	PQexecErrCheck(res, BEGIN_STMT, mySequence->dsn, false);
	PQclear(res);

	/* Get target database transaction status */
	res = PQexec(mySequence->conn, SHOW_MYSELF_STMT);
	PQexecErrCheck(res, SHOW_MYSELF_STMT, mySequence->dsn, false);


	nn = 0;
	mySequence->system_id = hex2int64(PQgetvalue(res, 0, nn++));
	mySequence->pid = atoi(PQgetvalue(res, 0, nn++));
	mySequence->pgprocno = atoi(PQgetvalue(res, 0, nn++));
	mySequence->lxid = atoi(PQgetvalue(res, 0, nn++));
	PQclear(res);

	fprintf(outF, "Target database: %016lx, pid: %d, pgorocno: %d, lxid: %d\n",
			mySequence->system_id,
			mySequence->pid,
			mySequence->pgprocno,
			mySequence->lxid);

	if (parent)
		waitRemoteSequence(mySequence, parent);
#undef BEGIN_STMT
#undef SHOW_MYSELF_STMT
}

static bool
PQexecErrCheck(PGresult *res, char *cmd, char *dsn, bool return_f)
{
	ExecStatusType	status;

	if (res == NULL)
	{
		errHandler(return_f, "Serious error occurred in execute '%s' for database '%s'.", cmd, dsn);
		return false;
	}
	status = PQresultStatus(res);
	if ((status != PGRES_COMMAND_OK) && (status != PGRES_TUPLES_OK))
	{
		errHandler(return_f, "Query execution error: '%s', at '%s', %s.", cmd, dsn, PQresultErrorMessage(res));
		return false;
	}
	return true;
}

static void
waitRemoteSequence(RemoteSequence *remote, RemoteSequence *parent)
{
	char		 cmd[CMDMAX+1];
	PGresult	*res;

	/* Acquire external lock */
	snprintf(cmd, CMDMAX, "select * from gdd_test_external_lock_acquire_myself('%s');", parent->label);

	res = PQexec(parent->conn, cmd);
	PQexecErrCheck(res, cmd, parent->dsn, false);
	PQclear(res);

	/* Setup lock property */
	snprintf(cmd, CMDMAX, "select gdd_test_external_lock_set_properties_myself"
						  "('%s', '%s', %d, %d, %d, true);",
						  parent->label,
						  remote->dsn,
						  remote->pgprocno,
						  remote->pid,
						  remote->lxid);
	res = PQexec(parent->conn, cmd);
	PQexecErrCheck(res, cmd, parent->dsn, false);
	PQclear(res);

	/* Wait external lock */
	snprintf(cmd, CMDMAX, "select gdd_test_external_lock_wait_myself('%s');", parent->label);
	res = PQexec(parent->conn, cmd);
	PQexecErrCheck(res, cmd, parent->dsn, false);
	PQclear(res);

	parent->has_external_lock = true;
}

static void
unWaitRemoteSequence(RemoteSequence *remote, RemoteSequence *parent)
{
	char		 cmd[CMDMAX+1];
	PGresult	*res;

	snprintf(cmd, CMDMAX, "gdd_test_external_lock_unwait_pgprocno"
						  "('%s', %d);",
						  parent->label,
						  remote->pgprocno);
	res = PQexec(parent->conn, cmd);
	PQexecErrCheck(res, cmd, parent->dsn, false);
	PQclear(res);

	parent->has_external_lock = false;
}

static void
runRemoteSequence(RemoteSequence *sequence, RemoteSequence *parent)
{
	int ii;

	connectRemoteSequence(sequence, parent);

	for (ii = 0; ii < sequence->nSequenceElement; ii++)
	{
		if (sequence->sequenceCategory[ii] == STEP_REMOTE)
			runRemoteSequence(sequence->sequenceStep[ii]->remote, sequence);
		else if (sequence->sequenceCategory[ii] == STEP_COMMAND)
			runCommand(sequence, sequence->sequenceStep[ii]->command);
		else
			runQuery(sequence->conn, sequence->sequenceStep[ii]->query);
	}

	/*
	 * K.Suzuki: The following should not be mandatory.
	 * May need to keep the connecton and the transaction open.
	 */
	unWaitRemoteSequence(sequence, parent);
}

static void
runCommand(RemoteSequence *sequence, char *command)
{
	if (strcmp(command, "return") == 0)
	{
		PQfinish(sequence->conn);
		sequence->conn = NULL;
		return;
	}
	else if (strcmp(command, "hold") == 0)
		return;
	else
	{
		fprintf(stderr, "Invalid command found: '%s'\n", command);
		return;
	}
}

static void
runQuery(PGconn *conn, char *query)
{
	/*
	 * Note that this geenrator does not display output from the query.
	 * It display only if it is successful or not.
	 */
	PGresult		*res;
	ExecStatusType	 status;

	res = PQexec(conn, query);
	if (res == NULL)
		errHandler(false, "Serious error in executing '%s'", query);
	status = PQresultStatus(res);
	if ((status != PGRES_COMMAND_OK) && (status != PGRES_TUPLES_OK))
	{
		fprintf(outF, "Query '%s' failed.  %s\n", query, PQresultErrorMessage(res));
		PQclear(res);
		res = PQexec(conn, "abort;");
		PQclear(res);
		return;
	}
	else
		fprintf(outF, "Query '%s' done successfully.\n", query);
	PQclear(res);
	return;
}

static void
connectDatabase(RemoteSequence *remoteSequence)
{
	PGconn *conn;

	if (remoteSequence->conn == NULL)
	{
		conn = PQconnectdb(remoteSequence->dsn);
		if ((conn == NULL) || (PQstatus(conn) == CONNECTION_BAD))
			errHandler(false, "Failed to connect to remote database '%s' not reachable.",
					remoteSequence->dsn);
	}
	remoteSequence->conn = conn;
	return;
}

static char *
parseRemoteSequence(char *indata, RemoteSequence **sequence)
{
	RemoteSequence	*mySequence;
	char	*word;
	char	*dsn;
	char	*label;

	mySequence = *sequence = (RemoteSequence *)malloc(sizeof(RemoteSequence));
	memset(mySequence, 0, sizeof(RemoteSequence));

	indata = getWord(indata, &word);
	if (indata == NULL)
	{
		*sequence = NULL;
		return NULL;
	}
	if (strcmp(word, "connect") != 0)
	{
		fprintf(stderr, "First word of local sequence must be \"connect\".\n");
		exit(1);
	}
	indata = getString(indata, &label);
	if (indata == NULL)
	{
		fprintf(stderr, "Label is missing after the connect.\n");
		exit(1);
	}
	if ((label == NULL) || *label == '\0')
	{
		label = (char *)malloc(sizeof(char) * 5);
		sprintf(label, "%04d", auto_label++);
	}
	mySequence->label = label;

	indata = getString(indata, &dsn);
	if (indata == NULL)
	{
		fprintf(stderr, "DNS is missing after the label.\n");
		exit(1);
	}
	mySequence->dsn = dsn;
	/*
	 * Sequence of SQL statement or local squence
	 */
	while(1)
	{
		char	 firstchar;

		if (indata == NULL)
			return NULL;
		indata = peekChar(indata, &firstchar);
		if (firstchar == '\'')
		{
			char	*query;
			int		 nn;

			indata = getString(indata, &query);
			/* It's query */
			nn = mySequence->nSequenceElement;
			expandRemoteSequence(mySequence);
			mySequence->sequenceCategory[nn] = STEP_QUERY;
			mySequence->sequenceStep[nn] = (SequenceStep *)malloc(sizeof(SequenceStep));
			mySequence->sequenceStep[nn]->query = query;
		}
		else
		{
			char	*word;
			char	*old_ptr = indata;
			int		 nn;
			RemoteSequence *sub_sequence;

			indata = getWord(indata, &word);
			if (word == NULL)
				errHandler(false, "Input syntax error.");
			if (strcmp(word, "connect") == 0)
			{
				indata = parseRemoteSequence(old_ptr, &sub_sequence);
				nn = mySequence->nSequenceElement;
				expandRemoteSequence(mySequence);
				mySequence->sequenceCategory[nn] = STEP_REMOTE;
				mySequence->sequenceStep[nn] = (SequenceStep *)malloc(sizeof(SequenceStep));
				mySequence->sequenceStep[nn]->remote = sub_sequence;
			}
			/* K.Suzuki: Add parse of commands */
			else if ((strcmp(word, "return") == 0) || (strcmp(word, "finish") == 0))
			{
				setCommand(mySequence, "return");
				return indata;
			}
			else if (strcmp(word, "hold") == 0)
			{
				/* K.Suzuki: Add parse for "hold" command */
				setCommand(mySequence, "hold");
				return indata;
			}
			else
				errHandler(false, "Input syntax error.");
		}
	}
	return NULL;
}



static void
setCommand(RemoteSequence *sequence, char *command)
{
	int	nn;

	nn = sequence->nSequenceElement;
	expandRemoteSequence(sequence);
	sequence->sequenceCategory[nn] = STEP_COMMAND;
	sequence->sequenceStep[nn] = (SequenceStep *)malloc(sizeof(SequenceStep));
	sequence->sequenceStep[nn]->command = strdup(command);
}

/*
 * Enlarge RemoteSequence members
 */
static void
expandRemoteSequence(RemoteSequence *remoteSequence)
{
	remoteSequence->nSequenceElement++;
	remoteSequence->sequenceCategory = (StepCategory *)my_realloc(remoteSequence->sequenceCategory,
												   sizeof(StepCategory) * remoteSequence->nSequenceElement);
	remoteSequence->sequenceStep = (SequenceStep **)my_realloc(remoteSequence->sequenceStep,
																 sizeof(SequenceStep *) * remoteSequence->nSequenceElement);
}

static char *
getWord(char *indata, char **word)
{
	char	word_buf[MAXWORDLEN + 1];
	int		ii;

	if (indata == NULL)
	{
		*word = NULL;
		return NULL;
	}
	indata = skip_sp(indata);
	if (indata == NULL)
	{
		*word = NULL;
		return NULL;
	}
	word_buf[0] = '\0';
	for (ii = 0; (ii < MAXWORDLEN) && *indata; indata++, ii++)
	{
		if (isspace(*indata))
		{
			word_buf[ii] = '\0';
			*word = strdup(word_buf);
			return indata;
		}
		else
		{
			word_buf[ii] = *indata;
			continue;
		}
	}
	word_buf[ii + 1] = '\0';
	*word = strdup(word_buf);
	return NULL;
}

static char *
getString(char *indata, char **str)
{
	char	str_buf[MAXSTRLEN + 1];
	int		ii;

	if (indata == NULL)
	{
		*str = NULL;
		return NULL;
	}
	indata = skip_sp(indata);
	if (*indata != '\'')
	{
		*str = NULL;
		return indata;
	}
	indata++;
	for (ii = 0; (ii < MAXSTRLEN) && *indata; ii++, indata++)
	{
		if (*indata == '\'')
		{
			if (*(indata + 1) == '\'')
			{
				str_buf[ii] = '\'';
				indata++;
				continue;
			}
			else
			{
				str_buf[ii] = '\0';
				*str = strdup(str_buf);
				return (indata + 1);
			}
		}
		else
		{
			str_buf[ii] = *indata;
			continue;
		}
	}
	str_buf[ii] = '\0';
	*str = strdup(str_buf);
	return NULL;
}

static char *
peekChar(char *indata, char *peeked)
{
	if (indata == NULL)
	{
		*peeked = '\0';
		return NULL;
	}
	indata = skip_sp(indata);
	*peeked = *indata;
	return indata;
}

static char *
skip_sp(char *indata)
{
	if (indata == NULL)
		return NULL;

	while(1)
	{
		for (; *indata; indata++)
		{
			if (isspace(*indata))
				continue;
			else
				break;
		}
		if (*indata == '#')
		{
			/* Comment */
			while (*indata != '\n')
				indata++;
			continue;
		}
		else if (*indata == '-' && *(indata + 1) == '-')
		{
			/* Comment */
			indata += 2;
			while (*indata != '\n')
				indata++;
			continue;
		}
		else if (*indata == '/' && *(indata + 1) == '*')
		{
			/* Comment */
			indata += 2;
			while (1)
			{
				while (*indata != '*')
					indata++;
				indata++;
				if (*indata == '/')
				{
					indata +=2;
					break;
				}
			}
			continue;
		}
		else if (*indata == '\0')
			return NULL;
		else
			break;
	}
	return indata;
}

#define BUF_INCREMENT	4096
static char *
read_file(FILE *f)
{
	char	*buf = NULL;
	int		 cur_size = 0;
	int		 cur_used = 0;
	char	 c;

	while(1)
	{
		if (cur_size <= cur_used)
		{
			buf = (char *)my_realloc(buf, (size_t)(cur_size + BUF_INCREMENT));
			cur_size += BUF_INCREMENT;
		}
		c = fgetc(f);
		if (c == EOF)
			break;
		if (c == '\0')
			continue;
		buf[cur_used++] = c;
	}
	buf[cur_used] = '\0';
	return buf;
}
#undef BUF_INCREMENT

static void *
my_realloc(void *ptr, size_t size)
{
	void	*rv;

	if (ptr == NULL)
		rv = malloc(size);
	else
		rv = realloc(ptr, size);
	if (rv == NULL)
		errHandler(false, "Out of memory.");
	return rv;
}

static void
printRemoteSequence(RemoteSequence *sequence, int level)
{
	int		ii;
	int		lHeading = level * 4;
	char   *heading;

	heading = (char *)malloc((sizeof(char) * lHeading) + 1);
	for (ii = 0; ii < lHeading; ii++)
		heading[ii] = ' ';
	heading[ii] = '\0';
	fprintf(outF, "%sRemoteSequence, label: %s, dsn: %s, system_id: %016lx, pid: %d, pgprocno: %d, lxid: %d, external_lock: %s\n",
			heading, sequence->label, sequence->dsn, sequence->system_id, sequence->pid, sequence->pgprocno, sequence->lxid,
			sequence->has_external_lock ? "yes" : "no");
	for (ii = 0; ii < sequence->nSequenceElement; ii++)
	{
		SequenceStep	*seq_step = sequence->sequenceStep[ii];
		switch(sequence->sequenceCategory[ii])
		{
			case STEP_QUERY:
				fprintf(outF, "%s    Query: '%s'\n", heading, seq_step->query);
				break;
			case STEP_COMMAND:
				fprintf(outF, "%s    Command: '%s'\n", heading, seq_step->command);
				break;
			case STEP_REMOTE:
				printRemoteSequence(seq_step->remote, level + 1);
				break;
			default:
				fprintf(stderr, "Invalid sequence category, %d\n", sequence->sequenceCategory[ii]);
				exit(1);
		}
	}
}

