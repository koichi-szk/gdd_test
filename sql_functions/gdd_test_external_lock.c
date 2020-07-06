/*-------------------------------------------------------------------------
 *
 * gdd_test_external_lock.c
 *
 *	  POSTGRES global deadlock detection test functions
 *
 * See src/backend/storage/lmgr/README for a description of the deadlock
 * detection and resolution algorithms.
 *
 * Portions Copyright (c) 2020, 2ndQuadrant Ltd.,
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/lmgr/gdd_test_external_lock.c
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "catalog/pg_control.h"
#include "catalog/pg_type_d.h"
#include "common/controldata_utils.h"
#include "fmgr.h"
#include "funcapi.h"
#include "../interfaces/libpq/libpq-fe.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "postmaster/postmaster.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "utils/memutils.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

typedef struct LOCKTAG_DICT
{
	struct	LOCKTAG_DICT	*next;
	char	*label;
	LOCKTAG	 tag;
} LOCKTAG_DICT;

static LOCKTAG_DICT	*locktag_dict_head = NULL;
static LOCKTAG_DICT	*locktag_dict_tail = NULL;
static char		*outfilename = NULL;
static FILE		*outf = NULL;
static PGPROC	*pgproc;
static int64	 database_system_id;

static Datum gdd_test_external_lock_acquire_int(char *label, PGPROC *proc);
static Datum gdd_test_set_locktag_external_int(char *label, PGPROC *target_proc, bool increment);
static Datum gdd_test_show_myself_int(PGPROC *proc);
static PGPROC *find_pgproc(int pid);
static bool register_locktag_label(char *label, LOCKTAG *locktag);
static bool unregister_locktag_label(char *label);
static const char *lockAcquireResultName(LockAcquireResult res);
static const char * deadLockCheckResultName(DeadLockState res);
static const char *lockModeName(LOCKMODE lockmode);
static const char *waitStatusName(int status);
static void hold_all_lockline(void);
static void release_all_lockline(void);
static bool add_locktag_dict(LOCKTAG_DICT *locktag_dict);
static void free_locktag_dict(LOCKTAG_DICT *locktag_dict);
static bool remove_locktag_dict(LOCKTAG_DICT *locktag_dict);
static LOCKTAG_DICT *find_locktag_dict(char *label);
static Datum gdd_describe_int(PGPROC *proc);
static Datum gdd_if_has_external_lock_int(PGPROC *proc);
static PGPROC *find_pgproc_pgprocno(int pgprocno);
static Datum gdd_test_show_waiting_external_lock_int(PGPROC *proc);
static bool gdd_test_external_lock_set_properties_int(char *label, PGPROC *proc, char *dsn,
										  int target_pgprocno, int target_pid, int target_xid, bool update_flag);
static Datum gdd_describe_int(PGPROC *proc);

#if 0
/*
 * CREATE FUNCTION gdd_external_lock_test_begin(outfname text)
 *		VOLATILE
 *		RETURNS int
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_test_init_test';
 */
PG_FUNCTION_INFO_V1(gdd_test_init_test);

Datum
gdd_test_init_test(PG_FUNCTION_ARGS)
{
	if (outf != NULL)
	{
		fprintf(outf, "Opened log file exists  '%s'.  Closing.\n", outfilename ? outfilename : "Unknown");
		free(outfilename);
		elog(LOG, "Opened log file exists.  Closing.\n");
		fclose(outf);
	}
	outfilename = strdup(PG_GETARG_CSTRING(0));
	outf = fopen(outfilename, "w");
	if (outf == NULL)
		elog(INFO, "Could not open the output file.");
	database_system_id = get_database_system_id();
	fprintf(outf, "%s: beginning global deadlock detection test.\n", __func__);
	PG_RETURN_INT32(0);
}
#endif



#if 0
/*
 * CREATE FUNCTION gdd_external_lock_test_finish()
 *		VOLATILE
 *		RETURNS int
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_test_finish_test';
 */
PG_FUNCTION_INFO_V1(gdd_test_finish_test);

Datum
gdd_test_finish_test(PG_FUNCTION_ARGS)
{
	if (outf == NULL)
	{
		elog(LOG, "No log file opened.");
		PG_RETURN_INT32(-1);
	}
	fprintf(outf, "%s: finishing global deadlock detection test.\n", __func__);
	fclose(outf);
	outf = NULL;
	PG_RETURN_INT32(0);
}
#endif


/*
 * CREATE FUNCTION gdd_show_myself()
 *		IMMUTABLE
 *		RETURNS TABLE(system_id bigint, pid int, pgprocno int, lxn int)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_test_show_myself';
 */

PG_FUNCTION_INFO_V1(gdd_test_show_myself);

Datum
gdd_test_show_myself(PG_FUNCTION_ARGS)
{
	Datum	result;

	result = gdd_test_show_myself_int(MyProc);
	PG_RETURN_DATUM(result);
}


/*
 * CREATE FUNCTION gdd_show_proc(pid int)
 *		IMMUTABLE
 *		RETURNS TABLE(system_id bigint, pid int, pgprocno int, lxn int)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_test_show_proc';
 */

PG_FUNCTION_INFO_V1(gdd_test_show_proc);

Datum
gdd_test_show_proc(PG_FUNCTION_ARGS)
{
	Datum	result;
	PGPROC *pgproc;

	pgproc = find_pgproc(PG_GETARG_INT32(0));
	if (pgproc == NULL)
		PG_RETURN_NULL();
	result = gdd_test_show_myself_int(pgproc);
	PG_RETURN_DATUM(result);
}


static Datum
gdd_test_show_myself_int(PGPROC *proc)
{
#define CHARLEN 64
	TupleDesc        tupd;
	HeapTupleData    tupleData;
	HeapTuple        tuple = &tupleData;
	char             values[4][CHARLEN];
	char			*Values[4];
	Datum            result;
	int64			 database_system_id;
	int				 ii;

	for(ii = 0; ii < 4; ii++)
		Values[ii] = &values[ii][0];
	database_system_id = get_database_system_id();
	ii = 1; 
	tupd = CreateTemplateTupleDesc(4);
	TupleDescInitEntry(tupd, ii++, "system_id", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "pid", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "pgprocno", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "lxid", INT4OID, -1, 0);
	ii = 0;
	snprintf(values[ii++], CHARLEN, "%016lx", database_system_id);
	snprintf(values[ii++], CHARLEN, "%d", proc->pid);
	snprintf(values[ii++], CHARLEN, "%d", proc->pgprocno);
	snprintf(values[ii++], CHARLEN, "%d", proc->lxid);
	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupd), Values);
	result = TupleGetDatum(TupleDescGetSlot(tupd), tuple);
	return result;
#undef CHARLEN
}

/*
 * CREATE FUNCTION gdd_set_locktag_external_pgprocno(label text, pgprocno int, increment bool)
 *		VOLATILE
 *		RETURNS TABLE (label text, field1 text, field2 text, field3 text, field4 text, locktype text, lockmethod text)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_test_set_locktag_external_pgprocno';
 * 
 * K.Suzuki: 引数は pgprocno? show self でできればいいか？定義を変更すること
 *			 注意：このロックはロックタグを作成しただけで、まだロックは取得していないことに注意。
 */
PG_FUNCTION_INFO_V1(gdd_test_set_locktag_external_myself);

Datum
gdd_test_set_locktag_external_myself(PG_FUNCTION_ARGS)
{
	char	*label;
	bool	 increment;
	Datum	 result;

	label = PG_GETARG_CSTRING(0);
	increment = PG_GETARG_BOOL(1);
	result = gdd_test_set_locktag_external_int(label, MyProc, increment);
	PG_RETURN_DATUM(result);
}



PG_FUNCTION_INFO_V1(gdd_test_set_locktag_external_pgprocno);

Datum
gdd_test_set_locktag_external_pgprocno(PG_FUNCTION_ARGS)
{
	int		 pgprocno;
	char	*label;
	bool	 increment;
	PGPROC	*proc;
	Datum	 result;

	label = PG_GETARG_CSTRING(0);
	pgprocno = PG_GETARG_INT32(1);
	increment = PG_GETARG_BOOL(2);

	proc = find_pgproc_pgprocno(pgprocno);
	result = gdd_test_set_locktag_external_int(label, proc, increment);
	PG_RETURN_DATUM(result);
}

/*
 * CREATE FUNCTION gdd_set_locktag_external_pid(label text, pid int, increment bool)
 *		VOLATILE
 *		RETURNS TABLE (label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_test_set_locktag_external_pid';
 * 
 * K.Suzuki: 引数は pgprocno? show self でできればいいか？定義を変更すること
 *			 注意：このロックはロックタグを作成しただけで、まだロックは取得していないことに注意。
 */
PG_FUNCTION_INFO_V1(gdd_test_set_locktag_external_pid);

Datum
gdd_test_set_locktag_external_pid(PG_FUNCTION_ARGS)
{
	int		 pid;
	char	*label;
	bool	 increment;
	PGPROC	*proc;
	Datum	 result;

	label = PG_GETARG_CSTRING(0);
	pid = PG_GETARG_INT32(1);
	increment = PG_GETARG_BOOL(2);

	proc = find_pgproc(pid);
	if (proc == NULL)
		elog (ERROR, "No PGPROC found for pid = %d.", pid);
	result = gdd_test_set_locktag_external_int(label, proc, increment);
	PG_RETURN_DATUM(result);
}

static Datum
gdd_test_set_locktag_external_int(char *label, PGPROC *target_proc, bool increment)
{
#define CHARLEN 64
#define NCOLNUM 7
	LOCKTAG	 locktag;
	/* outoput */
	TupleDesc        tupd;
	HeapTupleData    tupleData;
	HeapTuple        tuple = &tupleData;
	char             values[NCOLNUM][CHARLEN];
	char            *Values[NCOLNUM];
	Datum            result;
	int				 ii;


	if (find_locktag_dict(label))
		elog(ERROR, "Specified label '%s' has already been used.", label);
	set_locktag_external(&locktag, target_proc, increment);
	register_locktag_label(label, &locktag);

	for (ii = 0; ii < NCOLNUM; ii++)
		Values[ii] = &values[ii][0];
	tupd = CreateTemplateTupleDesc(7);
	ii = 1;
	TupleDescInitEntry(tupd, ii++, "label", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field1", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field2", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field3", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field4", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "locktype", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "lockmethod", INT4OID, -1, 0);
	ii = 0;
	strncpy(Values[ii++], label, CHARLEN);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field1);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field2);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field3);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field4);
	strncpy(Values[ii++], locktagTypeName(locktag.locktag_type), CHARLEN);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_lockmethodid);
	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupd), Values);
	result = TupleGetDatum(TupleDescGetSlot(tupd), tuple);
	PG_RETURN_DATUM(result);
#undef NCOLNUM
#undef CHARLEN
}


/*
 * CREATE FUNCTION gdd_external_lock_acquire_myself()
 *		VOLATILE
 *		RETURNS TABLE (result text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod text)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_external_lock_acquire_myself';
 */

PG_FUNCTION_INFO_V1(gdd_test_external_lock_acquire_myself);

Datum
gdd_test_external_lock_acquire_myself(PG_FUNCTION_ARGS)
{
	Datum 	 result;
	char	*label;

	label = PG_GETARG_CSTRING(0);
	result = gdd_test_external_lock_acquire_int(label, MyProc);
	PG_RETURN_DATUM(result);
}

/*
 * CREATE FUNCTION gdd_external_lock_acquire_pid(pid int)
 *		VOLATILE
 *		RETURNS TABLE (result text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod text)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_external_lock_acquire_pid';
 *
 * If pid == 0, then backed pid will be taken.
 */
PG_FUNCTION_INFO_V1(gdd_test_external_lock_acquire_pid);

Datum
gdd_test_external_lock_acquire_pid(PG_FUNCTION_ARGS)
{
	Datum 	 result;
	int	  	 pid;
	char	*label;
	PGPROC	*proc;

	label = PG_GETARG_CSTRING(0);
	pid = PG_GETARG_INT32(1);
	proc = find_pgproc(pid);
	if (proc == NULL)
		elog(ERROR, "Could not find PGPROC entry for pid = %d.", pid);
	result = gdd_test_external_lock_acquire_int(label, proc);
	PG_RETURN_DATUM(result);
}


PG_FUNCTION_INFO_V1(gdd_test_external_lock_acquire_pgprocno);

Datum
gdd_test_external_lock_acquire_pgprocno(PG_FUNCTION_ARGS)
{
	Datum	 result;
	int		 pgprocno;
	char	*label;
	PGPROC	*proc;

	label = PG_GETARG_CSTRING(0);
	pgprocno = PG_GETARG_INT32(1);
	proc = find_pgproc_pgprocno(pgprocno);
	result = gdd_test_external_lock_acquire_int(label, proc);
	PG_RETURN_DATUM(result);
}

static Datum
gdd_test_external_lock_acquire_int(char *label, PGPROC *proc)
{
#define CHARLEN 32
#define NCOLUMN 8
	LOCKTAG				locktag;
	LockAcquireResult	lock_result;
	/* outoput */
	TupleDesc        tupd;
	HeapTupleData    tupleData;
	HeapTuple        tuple = &tupleData;
	char             values[NCOLUMN][CHARLEN];
	char			*Values[NCOLUMN];
	Datum            result;
	int				 ii;

	if (find_locktag_dict(label))
		elog(ERROR, "Specified label '%s' has already been used.", label);
	for (ii = 0; ii < NCOLUMN; ii++)
		Values[ii] = &values[ii][0];
	lock_result = ExternalLockAcquire(proc, &locktag);
	register_locktag_label(label, &locktag);
	tupd = CreateTemplateTupleDesc(NCOLUMN);
	ii = 1;
	TupleDescInitEntry(tupd, ii++, "label", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "result", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field1", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field2", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field3", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "field4", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "locktype", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "lockmethd", INT4OID, -1, 0);
	ii = 0;
	strncpy(Values[ii++], label, CHARLEN);
	strncpy(Values[ii++], lockAcquireResultName(lock_result), CHARLEN);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field1);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field2);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field3);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_field4);
	strncpy(Values[ii++], locktagTypeName(locktag.locktag_type), CHARLEN);
	snprintf(Values[ii++], CHARLEN, "%d", locktag.locktag_lockmethodid);
	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupd), Values);
	result = TupleGetDatum(TupleDescGetSlot(tupd), tuple);
	return(result);

#undef NCOLUMN
#undef CHARLEN
}

/*
 * CREATE FUNCTION gdd_describe_pid(pid int)
 *		VOLATILE
 *		RETURNS TABLE
 *			(
 *				pgprocno int, waitstaus text, lxid int,
 *				wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int, wlocktype text, wlockmethod int,
 *				lockleaderpid int	-- pid of the leader
 *			)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_external_lock_acquire_pid';
 */
PG_FUNCTION_INFO_V1(gdd_describe_pid);

Datum
gdd_describe_pid(PG_FUNCTION_ARGS)
{
	PGPROC	*proc;
	int		 pid;
	Datum	 result;

	pid = PG_GETARG_INT32(0);
	proc = find_pgproc(pid);
	if (proc == NULL)
		elog(ERROR, "Could not find PGPROC entry for pid = %d.", pid);
	result = gdd_describe_int(proc);
	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(gdd_describe_pgprocno);

Datum
gdd_describe_pgprocno(PG_FUNCTION_ARGS)
{
	PGPROC *proc;
	int		pgprocno;
	Datum	result;

	pgprocno = PG_GETARG_INT32(0);
	proc = find_pgproc_pgprocno(pgprocno);
	if (proc == NULL)
		elog(ERROR, "Could not find PGPROC entry for pgprocno = %d.", pgprocno);
	result = gdd_describe_int(proc);
	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(gdd_describe_myself);

Datum
gdd_describe_myself(PG_FUNCTION_ARGS)
{
	Datum	 result;

	result = gdd_describe_int(MyProc);
	PG_RETURN_DATUM(result);
}
static Datum
gdd_describe_int(PGPROC *proc)
{
#define CHARLEN 32
#define NCOLUMN 10
	/* outoput */
	TupleDesc        tupd;
	HeapTupleData    tupleData;
	HeapTuple        tuple = &tupleData;
	char             values[NCOLUMN][CHARLEN];
	char			*Values[NCOLUMN];
	Datum            result;
	int				 ii;

	for (ii = 0; ii < NCOLUMN; ii++)
		Values[ii] = &values[ii][0];
	tupd = CreateTemplateTupleDesc(10);
	ii = 1;

	TupleDescInitEntry(tupd, ii++, "pid", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "pgprocno", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "waitstatus", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag1", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag2", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag3", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag4", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktype", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlockmethod", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "lockleaderpid", INT4OID, -1, 0);
	ii = 0;
	snprintf(values[ii++], CHARLEN, "%d", proc->pid);
	snprintf(values[ii++], CHARLEN, "%d", proc->pgprocno);
	strncpy(values[ii++], waitStatusName(proc->waitStatus), CHARLEN);
	snprintf(values[ii++], CHARLEN, "%d", proc->waitLock ? proc->waitLock->tag.locktag_field1 : -1);
	snprintf(values[ii++], CHARLEN, "%d", proc->waitLock ? proc->waitLock->tag.locktag_field2 : -1);
	snprintf(values[ii++], CHARLEN, "%d", proc->waitLock ? proc->waitLock->tag.locktag_field3 : -1);
	snprintf(values[ii++], CHARLEN, "%d", proc->waitLock ? proc->waitLock->tag.locktag_field4 : -1);
	strncpy(values[ii++], proc->waitLock ? locktagTypeName(proc->waitLock->tag.locktag_type) : "-1", CHARLEN);
	snprintf(values[ii++], CHARLEN, "%d", proc->waitLock ? proc->waitLock->tag.locktag_lockmethodid : -1);
	snprintf(values[ii++], CHARLEN, "%d", proc->lockGroupLeader ? proc->lockGroupLeader->pid : -1);
	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupd), Values);
	result = TupleGetDatum(TupleDescGetSlot(tupd), tuple);
	PG_RETURN_DATUM(result);
#undef NCOLUMN
#undef CHARLEN
}

/*
 * CREATE FUNCTION gdd_if_has_external_lock(pid int)
 *		STABLE
 *		RETURNS TABLE
 *			(
 *				has_external_lock	bool,
 *				has_external_prop	bool
 *			)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_if_has_external_lock';
 */
PG_FUNCTION_INFO_V1(gdd_if_has_external_lock_pid);

Datum
gdd_if_has_external_lock_pid(PG_FUNCTION_ARGS)
{
	int		 pid;
	PGPROC	*proc;
	Datum	 result;

	pid = PG_GETARG_INT32(0);
	proc = find_pgproc(pid);
	if (proc == NULL)
	{
		elog(WARNING, "Could not find PGPROC entry for pid = %d.", pid);
		PG_RETURN_NULL();
	}
	result = gdd_if_has_external_lock_int(proc);
	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(gdd_if_has_external_lock_pgprocno);

Datum
gdd_if_has_external_lock_pgprocno(PG_FUNCTION_ARGS)
{
	int		 pgprocno;
	PGPROC	*proc;
	Datum	 result;

	pgprocno = PG_GETARG_INT32(0);
	proc = find_pgproc_pgprocno(pgprocno);
	result = gdd_if_has_external_lock_int(proc);
	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(gdd_if_has_external_lock_myself);

Datum
gdd_if_has_external_lock_myself(PG_FUNCTION_ARGS)
{
	Datum	result;

	result = gdd_if_has_external_lock_int(MyProc);
	PG_RETURN_DATUM(result);
}

static Datum
gdd_if_has_external_lock_int(PGPROC *proc)
{
#define CHARLEN 32
#define NCOLUMN 2
	bool	 has_external_lock;
	bool	 has_external_lock_property;
	ExternalLockInfo	*external_lock_info;
	/* outoput */
	TupleDesc        tupd;
	HeapTupleData    tupleData;
	HeapTuple        tuple = &tupleData;
	char             values[NCOLUMN][CHARLEN];
	char			*Values[NCOLUMN];
	Datum            result;
	int				 ii;

	for (ii = 0; ii < NCOLUMN; ii++)
		Values[ii] = &values[ii][0];
	has_external_lock
		= (proc->waitLock && proc->waitLock->tag.locktag_type == LOCKTAG_EXTERNAL) ? true : false;
	if (has_external_lock)
	{
		external_lock_info = GetExternalLockProperties(&(proc->waitLock->tag));
		has_external_lock_property = external_lock_info ? true : false;
	}
	else
		has_external_lock_property = false;
	tupd = CreateTemplateTupleDesc(NCOLUMN);
	ii = 1;
	TupleDescInitEntry(tupd, ii++, "has_external_lock", BOOLOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "has_external_lock_property", BOOLOID, -1, 0);
	ii = 0;
	strncpy(values[ii++], has_external_lock ? "true" : "false", CHARLEN);
	strncpy(values[ii++], has_external_lock_property ? "true" : "false", CHARLEN);
	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupd), Values);
	result = TupleGetDatum(TupleDescGetSlot(tupd), tuple);
	PG_RETURN_DATUM(result);
#undef NCOLUMN
#undef CHARLEN
}


/*
 * CREATE FUNCTION gdd_show_external_lock(pid int)
 *		VOLATILE
 *		RETURNS TABLE
 *			(
 *				wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int, wlocktype text, wlockmethod int,
 *				dsn text, target_pid int, target_pgprocno int, target_txn
 *			)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_show_external_lock';
 */

PG_FUNCTION_INFO_V1(gdd_test_show_waiting_external_lock_pid);

Datum
gdd_test_show_waiting_external_lock_pid(PG_FUNCTION_ARGS)
{
	int		 pid;
	PGPROC	*proc;
	Datum	 result;

	pid = PG_GETARG_INT32(0);
	proc = find_pgproc(pid);
	if (proc == NULL)
		elog(ERROR, "Could not find PGPROC entry for pid = %d.", pid);
	result = gdd_test_show_waiting_external_lock_int(proc);
	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(gdd_test_show_waiting_external_lock_pgprocno);

Datum
gdd_test_show_waiting_external_lock_pgprocno(PG_FUNCTION_ARGS)
{
	int		 pgprocno;
	PGPROC	*proc;
	Datum	 result;

	pgprocno = PG_GETARG_INT32(0);
	proc = find_pgproc_pgprocno(pgprocno);
	result = gdd_test_show_waiting_external_lock_int(proc);
	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(gdd_test_show_waiting_external_lock_myself);

Datum
gdd_test_show_waiting_external_lock_myself(PG_FUNCTION_ARGS)
{
	Datum	 result;

	result = gdd_test_show_waiting_external_lock_int(MyProc);
	PG_RETURN_DATUM(result);
}

static Datum
gdd_test_show_waiting_external_lock_int(PGPROC *proc)
{
#define CHARLEN 1024
#define	NCOLUMN	11
	LOCK	*waitLock;
	ExternalLockInfo	*external_lock_info;
	/* outoput */
	TupleDesc        tupd;
	HeapTupleData    tupleData;
	HeapTuple        tuple = &tupleData;
	char             values[NCOLUMN][CHARLEN];
	char			*Values[NCOLUMN];
	Datum            result;
	int				 ii;
	bool			 has_waitlock;

	for (ii = 0; ii < NCOLUMN; ii++)
		Values[ii] = &values[ii][0];
	waitLock = proc->waitLock;
	if (!waitLock || waitLock->tag.locktag_type != LOCKTAG_EXTERNAL)
		has_waitlock = false;
	else
		has_waitlock = true;
	if (has_waitlock)
		external_lock_info = GetExternalLockProperties(&(waitLock->tag));
	else
		external_lock_info = NULL;

	tupd = CreateTemplateTupleDesc(9);
	ii = 1;
	TupleDescInitEntry(tupd, ii++, "pid", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag1", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag2", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag3", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktag4", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlocktype", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "wlockmethod", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "dsn", TEXTOID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "target_pid", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "target_pgprocno", INT4OID, -1, 0);
	TupleDescInitEntry(tupd, ii++, "target_txn", INT4OID, -1, 0);
	ii = 0;
	snprintf(values[ii++], CHARLEN, "%d", proc->pid);
	snprintf(values[ii++], CHARLEN, "%d", has_waitlock ? waitLock->tag.locktag_field1 : -1);
	snprintf(values[ii++], CHARLEN, "%d", has_waitlock ? waitLock->tag.locktag_field2 : -1);
	snprintf(values[ii++], CHARLEN, "%d", has_waitlock ? waitLock->tag.locktag_field3 : -1);
	snprintf(values[ii++], CHARLEN, "%d", has_waitlock ? waitLock->tag.locktag_field4 : -1);
	strncpy(values[ii++], has_waitlock ? locktagTypeName(waitLock->tag.locktag_type) : "NO_LOCK_FOUND", CHARLEN);
	snprintf(values[ii++], CHARLEN, "%d", has_waitlock ? waitLock->tag.locktag_lockmethodid : -1);
	strncpy(values[ii++], external_lock_info ? external_lock_info->dsn : "N/A", CHARLEN);
	snprintf(values[ii++], CHARLEN, "%d", external_lock_info ? external_lock_info->target_pid : -1);
	snprintf(values[ii++], CHARLEN, "%d", external_lock_info ? external_lock_info->target_pgprocno : -1);
	snprintf(values[ii++], CHARLEN, "%d", external_lock_info ? external_lock_info->target_txn : -1);
	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupd), Values);
	result = TupleGetDatum(TupleDescGetSlot(tupd), tuple);
	return result;
#undef NCOLUMN
#undef CHARLEN
}

/*
 * Argument: nothing
 * RETURNS seto of TABLE
 * 	(label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int,
 *   dsn text, target_pid int, target_pgprocno int, target_lxn int);
 */


PG_FUNCTION_INFO_V1(gdd_test_show_registered_external_lock);

Datum
gdd_test_show_registered_external_lock(PG_FUNCTION_ARGS)
{
#define CHARLEN 1024
#define NCOLUMN 11
	FuncCallContext	*funcctx;
	TupleDesc		 tupdesc;
	AttInMetadata	*attinmeta;
	HeapTupleData	 tupleData;
	HeapTuple		 tuple = &tupleData;
	LOCKTAG_DICT	*curr_dict;
	char			 values[NCOLUMN][CHARLEN];
	char			*Values[NCOLUMN];
	int				 ii;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext	oldcontext;

		/* Initialize itelating function */
		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			ereport(ERROR,
					  (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                       errmsg("function returning record called in context "
							  "that cannot accept type record")));
		attinmeta = TupleDescGetAttInMetadata(tupdesc);
		funcctx->attinmeta = attinmeta;
		funcctx->user_fctx = locktag_dict_head;

		MemoryContextSwitchTo(oldcontext);
	}
	funcctx = SRF_PERCALL_SETUP();
	for (ii = 0; ii < NCOLUMN; ii++)
		Values[ii] = &values[ii][0];
	curr_dict = (LOCKTAG_DICT *)funcctx->user_fctx;
	if (curr_dict)
	{
		Datum				 result;
		ExternalLockInfo	*external_lock_info;

		attinmeta = funcctx->attinmeta;
		external_lock_info = GetExternalLockProperties(&(curr_dict->tag));
		ii = 0;
		strncpy(Values[ii++], curr_dict->label, CHARLEN);
		snprintf(Values[ii++], CHARLEN, "%d", curr_dict->tag.locktag_field1);
		snprintf(Values[ii++], CHARLEN, "%d", curr_dict->tag.locktag_field2);
		snprintf(Values[ii++], CHARLEN, "%d", curr_dict->tag.locktag_field3);
		snprintf(Values[ii++], CHARLEN, "%d", curr_dict->tag.locktag_field4);
		strncpy(Values[ii++], locktagTypeName(curr_dict->tag.locktag_type), CHARLEN);
		snprintf(Values[ii++], CHARLEN, "%d", curr_dict->tag.locktag_lockmethodid);
		strncpy(Values[ii++], external_lock_info ? external_lock_info->dsn : "N/A", CHARLEN);
		snprintf(Values[ii++], CHARLEN, "%d", external_lock_info ? external_lock_info->target_pid : -1);
		snprintf(Values[ii++], CHARLEN, "%d", external_lock_info ? external_lock_info->target_pgprocno : -1);
		snprintf(Values[ii++], CHARLEN, "%d", external_lock_info ? external_lock_info->target_txn : -1);

		tuple = BuildTupleFromCStrings(attinmeta, Values);
		result = HeapTupleGetDatum(tuple);
		funcctx->user_fctx = curr_dict->next;
		SRF_RETURN_NEXT(funcctx, result);
	}
	SRF_RETURN_DONE(funcctx);

#undef NCOLUMN
#undef CHARLEN
}


/*
 * CREATE FUNCTION gdd_show_deadlock_info()
 *		VOLATILE
 *		RETURNS SET OF RECORD
 *			(
 *				no int, locktag1 int, locktag2 int, locktag3 int, locktag4 int, locktype text, lockmethod int,
 *				lockmode text, pid int, pgprocno int, txid int
 *			)
 *		LANGUAGE c
 *		AS 'gdd_test.so', 'gdd_show_deadlock_info';
 */

PG_FUNCTION_INFO_V1(gdd_show_deadlock_info);

typedef struct myDeadlockInfo
{
	int				 nDeadlockInfo;
	DEADLOCK_INFO	*deadlock_info;
} myDeadlockInfo;

Datum
gdd_show_deadlock_info(PG_FUNCTION_ARGS)
{
#define CHARLEN 64
#define NCOLUMN 11
	FuncCallContext	*funcctx;
	TupleDesc		 tupdesc;
	AttInMetadata	*attinmeta;
	myDeadlockInfo	*my_info;
	HeapTupleData    tupleData;
	HeapTuple        tuple = &tupleData;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext	oldcontext;

		/* Initialize itelating function */
		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		my_info = malloc(sizeof(myDeadlockInfo));
		my_info->deadlock_info = GetDeadLockInfo(&my_info->nDeadlockInfo);
		if (my_info->deadlock_info == NULL)
			elog(ERROR, "No deadlock infor found in this proc.");
		funcctx->user_fctx = my_info;
		funcctx->max_calls = my_info->nDeadlockInfo;
		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			ereport(ERROR,
					  (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                       errmsg("function returning record called in context "
							  "that cannot accept type record")));
		attinmeta = TupleDescGetAttInMetadata(tupdesc);
		funcctx->attinmeta = attinmeta;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		Datum			 result;
		int				 ii;
		char			 values[NCOLUMN][CHARLEN];
		char			*Values[NCOLUMN];

		for (ii = 0; ii < NCOLUMN; ii++)
			Values[ii] = &values[ii][0];
		attinmeta = funcctx->attinmeta;
		my_info = funcctx->user_fctx;
		ii = 0;
		snprintf(Values[ii++], CHARLEN, "%ld", funcctx->call_cntr);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].locktag.locktag_field1);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].locktag.locktag_field2);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].locktag.locktag_field3);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].locktag.locktag_field4);
		strncpy(Values[ii++], locktagTypeName(my_info->deadlock_info[funcctx->call_cntr].locktag.locktag_type), CHARLEN);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].locktag.locktag_lockmethodid);
		strncpy(Values[ii++], lockModeName(my_info->deadlock_info[funcctx->call_cntr].lockmode), CHARLEN);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].pid);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].pgprocno);
		snprintf(Values[ii++], CHARLEN, "%d", my_info->deadlock_info[funcctx->call_cntr].txid);

		tuple = BuildTupleFromCStrings(attinmeta, Values);

		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
#undef NCOLUMN
#undef CHARLEN
}


PG_FUNCTION_INFO_V1(gdd_test_external_lock_set_properties_pid);

Datum
gdd_test_external_lock_set_properties_pid(PG_FUNCTION_ARGS)
{
	char	*label;
	int		 pid;
	char	*dsn;
	int		 target_pgprocno;
	int		 target_pid;
	int		 target_xid;
	bool	 update_flag;
	PGPROC	*proc;
	bool	 result;


	label = PG_GETARG_CSTRING(0);
	pid = PG_GETARG_INT32(1);
	dsn = PG_GETARG_CSTRING(2);
	target_pgprocno = PG_GETARG_INT32(3);
	target_pid = PG_GETARG_INT32(4);
	target_xid = PG_GETARG_INT32(5);
	update_flag = PG_GETARG_BOOL(6);

	proc = find_pgproc(pid);
	if (proc == NULL)
		elog(ERROR, "PGPROC not found for pid: %d.", pid);
	result = gdd_test_external_lock_set_properties_int(label, proc,
				dsn, target_pgprocno, target_pid, target_xid, update_flag);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_set_properties_pgprocno);

Datum
gdd_test_external_lock_set_properties_pgprocno(PG_FUNCTION_ARGS)
{
	char	*label;
	int		 pgprocno;
	char	*dsn;
	int		 target_pgprocno;
	int		 target_pid;
	int		 target_xid;
	bool	 update_flag;
	PGPROC	*proc;
	bool	 result;


	label = PG_GETARG_CSTRING(0);
	pgprocno = PG_GETARG_INT32(1);
	dsn = PG_GETARG_CSTRING(2);
	target_pgprocno = PG_GETARG_INT32(3);
	target_pid = PG_GETARG_INT32(4);
	target_xid = PG_GETARG_INT32(5);
	update_flag = PG_GETARG_BOOL(6);

	proc = find_pgproc_pgprocno(pgprocno);
	if (proc == NULL)
		elog(ERROR, "PGPROC not found for pgprocno: %d.", pgprocno);
	result = gdd_test_external_lock_set_properties_int(label, proc,
				dsn, target_pgprocno, target_pid, target_xid, update_flag);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_set_properties_myself);

Datum
gdd_test_external_lock_set_properties_myself(PG_FUNCTION_ARGS)
{
	char	*label;
	char	*dsn;
	int		 target_pgprocno;
	int		 target_pid;
	int		 target_xid;
	bool	 update_flag;
	bool	 result;


	label = PG_GETARG_CSTRING(0);
	dsn = PG_GETARG_CSTRING(1);
	target_pgprocno = PG_GETARG_INT32(2);
	target_pid = PG_GETARG_INT32(3);
	target_xid = PG_GETARG_INT32(4);
	update_flag = PG_GETARG_BOOL(5);

	result = gdd_test_external_lock_set_properties_int(label, MyProc,
				dsn, target_pgprocno, target_pid, target_xid, update_flag);
	PG_RETURN_BOOL(result);
}

static bool
gdd_test_external_lock_set_properties_int(char		*label,
										  PGPROC	*proc,
										  char		*dsn,
										  int		 target_pgprocno,
										  int		 target_pid,
										  int		 target_xid,
										  bool		 update_flag)
{
	LOCKTAG_DICT	*locktag_dict;
	bool			 result;

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag not found with label '%s'.", label);
	result = ExternalLockSetProperties(&locktag_dict->tag, proc, dsn, target_pgprocno, target_pid, target_xid, update_flag);
	return(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_wait_myself);

Datum
gdd_test_external_lock_wait_myself(PG_FUNCTION_ARGS)
{
	char	*label;
	bool	 result;
	LOCKTAG_DICT	*locktag_dict;


	label = PG_GETARG_CSTRING(0);
	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag for label '%s' not found.", label);

	result = ExternalLockWaitProc(&locktag_dict->tag, MyProc);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_wait_pid);

Datum
gdd_test_external_lock_wait_pid(PG_FUNCTION_ARGS)
{
	char	*label;
	int		 pid;
	PGPROC	*proc;
	bool	 result;
	LOCKTAG_DICT	*locktag_dict;


	label = PG_GETARG_CSTRING(0);
	pid = PG_GETARG_INT32(1);

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag for label '%s' not found.", label);
	proc = find_pgproc(pid);
	if (proc == NULL)
		elog(ERROR, "PGPROC does not exist for process: %d.", pid);

	result = ExternalLockWaitProc(&locktag_dict->tag, proc);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_wait_pgprocno);

Datum
gdd_test_external_lock_wait_pgprocno(PG_FUNCTION_ARGS)
{
	char	*label;
	int		 pgprocno;
	PGPROC	*proc;
	bool	 result;
	LOCKTAG_DICT	*locktag_dict;


	label = PG_GETARG_CSTRING(0);
	pgprocno = PG_GETARG_INT32(1);

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag for label '%s' not found.", label);
	proc = find_pgproc_pgprocno(pgprocno);
	if (proc == NULL)
		elog(ERROR, "PGPROC does not exist for pgprocno: %d.", pgprocno);

	result = ExternalLockWaitProc(&locktag_dict->tag, proc);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_unwait_pid);

Datum
gdd_test_external_lock_unwait_pid(PG_FUNCTION_ARGS)
{
	char	*label;
	int		 pid;
	PGPROC	*proc;
	bool	 result;
	LOCKTAG_DICT	*locktag_dict;


	label = PG_GETARG_CSTRING(0);
	pid = PG_GETARG_INT32(1);

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag for label '%s' not found.", label);
	proc = find_pgproc(pid);
	if (proc == NULL)
		elog(ERROR, "PGPROC does not exist for process: %d.", pid);

	result = ExternalLockUnWaitProc(&locktag_dict->tag, proc);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_unwait_pgprocno);

Datum
gdd_test_external_lock_unwait_pgprocno(PG_FUNCTION_ARGS)
{
	char	*label;
	int		 pgprocno;
	PGPROC	*proc;
	bool	 result;
	LOCKTAG_DICT	*locktag_dict;


	label = PG_GETARG_CSTRING(0);
	pgprocno = PG_GETARG_INT32(1);

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag for label '%s' not found.", label);
	proc = find_pgproc_pgprocno(pgprocno);
	if (proc == NULL)
		elog(ERROR, "PGPROC does not exist for pgprocno: %d.", pgprocno);

	result = ExternalLockUnWaitProc(&locktag_dict->tag, proc);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_external_lock_unwait_myself);

Datum
gdd_test_external_lock_unwait_myself(PG_FUNCTION_ARGS)
{
	char			*label;
	bool			 result;
	LOCKTAG_DICT	*locktag_dict;


	label = PG_GETARG_CSTRING(0);

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag for label '%s' not found.", label);

	result = ExternalLockUnWaitProc(&locktag_dict->tag, MyProc);
	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(gdd_test_external_lock_release);

Datum
gdd_test_external_lock_release(PG_FUNCTION_ARGS)
{
	char			*label;
	bool	 		 result;
	LOCKTAG_DICT	*locktag_dict;


	label = PG_GETARG_CSTRING(0);

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		elog(ERROR, "Locktag for label '%s' not found.", label);

	result = ExternalLockRelease(&locktag_dict->tag);
	unregister_locktag_label(label);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(gdd_test_deadlock_check_pid);

Datum
gdd_test_deadlock_check_pid(PG_FUNCTION_ARGS)
{
	int				 pid;
	PGPROC			*proc;
	DeadLockState	 state;


	pid = PG_GETARG_INT32(0);
	proc = find_pgproc(pid);
	if (proc == NULL)
		elog(ERROR, "PGPROC not found for pid = %d.", pid);
	
	hold_all_lockline();
	state = DeadLockCheck(proc);
	release_all_lockline();

	PG_RETURN_CSTRING(deadLockCheckResultName(state));
}

PG_FUNCTION_INFO_V1(gdd_test_deadlock_check_myself);

Datum
gdd_test_deadlock_check_myself(PG_FUNCTION_ARGS)
{
	DeadLockState	 state;

	hold_all_lockline();
	state = DeadLockCheck(MyProc);
	release_all_lockline();

	PG_RETURN_CSTRING(deadLockCheckResultName(state));
}




/*****************************************************************************************************************/

static PGPROC *
find_pgproc(int pid)
{
	int	ii;
	PGPROC *proc = NULL;

	LWLockAcquire(ProcArrayLock, LW_SHARED);
	for (ii = 0; ii < ProcGlobal->allProcCount; ii++)
	{
		if (ProcGlobal->allProcs[ii].pid == pid)
		{
			proc = &ProcGlobal->allProcs[ii];
			break;
		}
	}
	for (ii++; ii < ProcGlobal->allProcCount; ii++)
	{
		if (ProcGlobal->allProcs[ii].pid == pid)
		{
			elog(WARNING, "%s(): found duplcate pgproc, pid = %d, pgprocno = %d, why?\n",
					   __func__, pid, ii);
			break;
		}
	}
	LWLockRelease(ProcArrayLock);
	if (pgproc == NULL)
		elog(WARNING, "%s(): failed to find PGPROC, pid=%d.\n", __func__, pid);
	return proc;
}

static void
hold_all_lockline(void)
{
	int ii;

	for (ii = 0; ii < NUM_LOCK_PARTITIONS; ii++)
	LWLockAcquire(LockHashPartitionLockByIndex(ii), LW_EXCLUSIVE);
}

static void
release_all_lockline(void)
{
	int ii;

	for (ii = 0; ii < NUM_LOCK_PARTITIONS; ii++)
	LWLockRelease(LockHashPartitionLockByIndex(ii));
}

static const char *
lockAcquireResultName(LockAcquireResult res)
{
	switch(res)
	{
		case LOCKACQUIRE_NOT_AVAIL:      /* lock not available, and dontWait=true */
			return "LOCKACQUIRE_NOT_AVAIL";
		case LOCKACQUIRE_OK:             /* lock successfully acquired */
			return "LOCKACQUIRE_OK";
		case LOCKACQUIRE_ALREADY_HELD:   /* incremented count for lock already held */
			return "LOCKACQUIRE_ALREADY_HELD";
		case LOCKACQUIRE_ALREADY_CLEAR:  /* incremented count for lock already clear */
			return "LOCKACQUIRE_ALREADY_CLEAR";
	}
	return "LOCKACQUIRE_ERROR_VALUE";
}

static const char *
deadLockCheckResultName(DeadLockState res)
{
	switch (res)
	{
		case DS_NOT_YET_CHECKED:         /* no deadlock check has run yet */
			return "DS_NOT_YET_CHECKED";
		case DS_NO_DEADLOCK:             /* no deadlock detected */
			return "DS_NO_DEADLOCK";
		case DS_SOFT_DEADLOCK:           /* deadlock avoided by queue rearrangement */
			return "DS_SOFT_DEADLOCK";
		case DS_HARD_DEADLOCK:           /* deadlock, no way out but ERROR */
			return "DS_HARD_DEADLOCK";
		case DS_BLOCKED_BY_AUTOVACUUM:   /* no deadlock; queue blocked by autovacuum worker */
			return "DS_BLOCKED_BY_AUTOVACUUM";
		case DS_EXTERNAL_LOCK:           /* waiting for remote transaction, need global deadlock detection */
			return "DS_EXTERNAL_LOCK";
		case DS_DEADLOCK_INFO:           /* waiting for remote transaction, need global deadlock detection */
			return "DS_DEADLOCK_INFO";
		case DS_GLOBAL_ERROR:            /* Used only to report internal error in global deadlock detection */
			return "DS_GLOBAL_ERROR";
	}
	return "DS_ERROR_VALUE";
}

static const char *
waitStatusName(int status)
{
	switch (status)
	{
		case STATUS_WAITING:
			return "STATUS_WAITING";
		case STATUS_OK:
			return "STATUS_OK";
		case STATUS_ERROR:
			return "STATUS_ERROR";
	}
	return "STATUS_NO_SUCH_VALUE";
}

static const char *
lockModeName(LOCKMODE lockmode)
{
	switch(lockmode)
	{
		case NoLock:
			return "NoLock";
		case AccessShareLock:
			return "AccessShareLock";
		case RowShareLock:
			return "RowShareLock";
		case RowExclusiveLock:
			return "RowExclusiveLock";
		case ShareUpdateExclusiveLock:
			return "ShareUpdateExclusiveLock";
		case ShareLock:
			return "ShareLock";
		case ShareRowExclusiveLock:
			return "ShareRowExclusiveLock";
		case ExclusiveLock:
			return "ExclusiveLock";
		case AccessExclusiveLock:
			return "AccessExclusiveLock";
	}
	return "NoSuchLockmode";
}

static bool
register_locktag_label(char *label, LOCKTAG *locktag)
{
	LOCKTAG_DICT	*locktag_dict;

	locktag_dict = (LOCKTAG_DICT *)malloc(sizeof(LOCKTAG_DICT));
	locktag_dict->label = strdup(label);
	memcpy(&locktag_dict->tag, locktag, sizeof(LOCKTAG));
	return(add_locktag_dict(locktag_dict));
};

static bool
unregister_locktag_label(char *label)
{
	LOCKTAG_DICT	*locktag_dict;

	locktag_dict = find_locktag_dict(label);
	if (locktag_dict == NULL)
		return false;
	return remove_locktag_dict(locktag_dict);
}

static LOCKTAG_DICT *
find_locktag_dict(char *label)
{
	LOCKTAG_DICT *curr = locktag_dict_head;

	if (curr == NULL)
		return NULL;
	for (; curr; curr = curr->next)
		if (strcmp(label, curr->label) == 0)
			return curr;
	return NULL;
}

static bool
add_locktag_dict(LOCKTAG_DICT *locktag_dict)
{
	if (locktag_dict_head == NULL)
	{
		locktag_dict_head = locktag_dict_tail = locktag_dict;
		locktag_dict->next = NULL;
		return true;
	}
	locktag_dict->next = NULL;
	locktag_dict_tail->next = locktag_dict;
	locktag_dict_tail = locktag_dict;
	return true;
}

static void
free_locktag_dict(LOCKTAG_DICT *locktag_dict)
{
	free(locktag_dict->label);
	free(locktag_dict);
}

static bool
remove_locktag_dict(LOCKTAG_DICT *locktag_dict)
{
	LOCKTAG_DICT *curr;

	curr = locktag_dict_head;

	if (locktag_dict_head == locktag_dict)
	{
		if (curr->next == NULL)
		{
			locktag_dict_head = locktag_dict_tail = NULL;
			free_locktag_dict(locktag_dict);
			return true;
		}
		curr = curr->next;
		free_locktag_dict(locktag_dict);
		return true;
	}
	for(curr = locktag_dict_head; curr; curr = curr->next)
	{
		if (curr->next == locktag_dict)
		{
			if (curr->next == locktag_dict_tail)
			{
				curr->next = NULL;
				locktag_dict_tail = curr;
			}
			else
			{
				curr->next = curr->next->next;
			}
			free_locktag_dict(locktag_dict);
			return true;
		}
	}
	return false;
}

static PGPROC *
find_pgproc_pgprocno(int pgprocno)
{
	if (pgprocno < 0)
		return MyProc;
	if (pgprocno >= ProcGlobal->allProcCount) 
		elog(ERROR, "Pgprocno is out of bounds. Max should be %d.", ProcGlobal->allProcCount);
	return &ProcGlobal->allProcs[pgprocno];
}
