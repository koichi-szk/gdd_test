#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>

#if 0
static int skipword(void);
#endif
static int getint(void);
static int skipline(void);
static int skipto(char to);



int main(int argc, char *argv[])
{
	char	*label;
	int		 pid;
	int		 pgprocno;
	int		 lxn;
	char	 connstr[1024];

	label = argv[1];
	skipline();
	skipline();
	skipto('|');
	pid = getint();
	skipline();
	skipto('|');
	pgprocno = getint();
	skipline();
	skipto('|');
	lxn = getint();

	snprintf(connstr, 1024, "host=%s dbname=koichi user=koichi", strcmp(label, "a") == 0 ? "ubuntu00" : "ksubuntu");
	printf("select gdd_test_external_lock_set_properties_myself('%s', '%s', %d, %d, %d, true);\n",
			label,
			connstr,
			pgprocno,
			pid,
			lxn);
}

#if 0
static int skipword(void)
{
	int	cc;

	while((cc = getchar()) >= 0 && (cc == ' ' || cc == '\t'));
	if (cc == '\n')
		return '\n';
	if (cc < 0)
		return EOF;
	unegtc(cc);
	while((cc = getchar()) >= 0 && (cc != ' ' && cc != '\t' && cc != '\n'));
	if (cc < 0)
		return EOF;
	ungetc(cc, stdin);
	if (cc = '\n')
		return '\n';
	return 0;
}
#endif

static int	getint(void)
{
	int	vv;
	int cc;

	while ((cc = getchar()) >= 0 && (cc == ' ' || cc == '\t'));
	if (cc < 0 || cc == '\n')
		return -1;
	ungetc(cc, stdin);
	while((cc = getchar()) >= 0 && (cc < '0' || cc > '9'));
	if (cc < 0)
		return -1;
	vv = cc - '0';
	while((cc = getchar()) > 0 && (cc >= '0' && cc <= '9'))
		vv = vv * 10 + cc - '0';
	if (cc >= 0)
		ungetc(cc, stdin);
	return vv;
}

static int skipline(void)
{
	int	cc;

	while((cc = getchar()) >= 0 && cc != '\n');
	if (cc < 0)
		return -1;
	return 0;
}

static int skipto(char to)
{
	int	cc;

	while ((cc = getchar()) >= 0 && cc != to);
	if (cc < 0)
		return EOF;
	return 0;
}

