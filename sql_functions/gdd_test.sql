/*
 * Test start - end
 */
/*
DROP FUNCTION IF EXISTS gdd_test_init_test(cstring);
CREATE OR REPLACE FUNCTION gdd_test_init_test(cstring)
	RETURNS INT
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_init_test';
*/

/*
DROP FUNCTION IF EXISTS gdd_test_finish_test();
CREATE OR REPLACE FUNCTION gdd_test_finish_test()
	RETURNS INT
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_finish_test';
*/

/*
 * Display process and txn info
 */
DROP FUNCTION IF EXISTS gdd_test_show_myself();
CREATE OR REPLACE FUNCTION gdd_test_show_myself()
	RETURNS TABLE(system_id TEXT, pid INT, pgprocno INT, lxn INT)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_myself';

DROP FUNCTION IF EXISTS gdd_test_show_proc(int);
CREATE OR REPLACE FUNCTION gdd_test_show_proc(int)
	RETURNS TABLE(system_id TEXT, pid INT, pgprocno INT, lxn INT)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_proc';

/*
 * Test to set external lock locktag.
 */
DROP FUNCTION IF EXISTS gdd_test_set_locktag_external_myself(cstring, bool);
CREATE OR REPLACE FUNCTION
	gdd_test_set_locktag_external_myself
		(cstring, bool)
	RETURNS TABLE  (label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_set_locktag_external_myself';

DROP FUNCTION IF EXISTS gdd_test_set_locktag_external_pgprocno(cstring, int, bool);
CREATE OR REPLACE FUNCTION
	gdd_test_set_locktag_external_pgprocno
		(cstring, int, bool)
	RETURNS TABLE  (label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_set_locktag_external_pgprocno';

DROP FUNCTION IF EXISTS gdd_test_set_locktag_external_pid(cstring, int, bool);
CREATE OR REPLACE FUNCTION
	gdd_test_set_locktag_external_pid
		(cstring, int, bool)
	RETURNS TABLE  (label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_set_locktag_external_pid';

/*
 * Acquire external lock.
 */
DROP FUNCTION IF EXISTS gdd_test_external_lock_acquire_myself(cstring);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_acquire_myself(cstring)
	RETURNS TABLE (label text, result text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_acquire_myself';

DROP FUNCTION IF EXISTS gdd_test_external_lock_acquire_pid(cstring, int);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_acquire_pid(int)
	RETURNS TABLE (label text, result text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_acquire_pid';

DROP FUNCTION IF EXISTS gdd_test_external_lock_acquire_pgpricno(cstring, int);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_acquire_pgpricno(int)
	RETURNS TABLE (label text, result text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_acquire_pgprocno';

/*
 * Display PGPROC wait status
 */
DROP FUNCTION IF EXISTS gdd_describe_pid(int);
CREATE OR REPLACE FUNCTION
	gdd_describe_pid(int)
	RETURNS TABLE
	(pid int, pgprocno int, waitstatus text, wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int,
	 wlocktype text, wlockmethod int, lockleaderpid int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_describe_pid';

DROP FUNCTION IF EXISTS gdd_describe_pgprocno(int);
CREATE OR REPLACE FUNCTION
	gdd_describe_pgprocno(int)
	RETURNS TABLE
	(pid int, pgprocno int, waitstatus text, wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int,
	 wlocktype text, wlockmethod int, lockleaderpid int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_describe_pgprocno';

DROP FUNCTION IF EXISTS gdd_describe_myself();
CREATE OR REPLACE FUNCTION
	gdd_describe_myself()
	RETURNS TABLE
	(pid int, pgprocno int, waitstatus text, wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int,
	 wlocktype text, wlockmethod int, lockleaderpid int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_describe_myself';

/*
 * Check if the backend is waiting for EXTERNAL LOCK and it has property.
 */
DROP FUNCTION IF EXISTS gdd_if_has_external_lock_pid(int);
CREATE OR REPLACE FUNCTION
	gdd_if_has_external_lock_pid(int)
	RETURNS TABLE (has_external_lock bool, has_external_lock_property bool)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_if_has_external_lock_pid';

DROP FUNCTION IF EXISTS gdd_if_has_external_lock_pgprocno(int);
CREATE OR REPLACE FUNCTION
	gdd_if_has_external_lock_pgprocno(int)
	RETURNS TABLE (has_external_lock bool, has_external_lock_property bool)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_if_has_external_lock_pgprocno';

DROP FUNCTION IF EXISTS gdd_if_has_external_lock_myself();
CREATE OR REPLACE FUNCTION
	gdd_if_has_external_lock_myself()
	RETURNS TABLE (has_external_lock bool, has_external_lock_property bool)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_if_has_external_lock_myself';

/*
 * Display waiting external lock and its prpoerty.
 */
DROP FUNCTION IF EXISTS gdd_test_show_waiting_external_lock_pid(int);
CREATE OR REPLACE FUNCTION
	gdd_test_show_waiting_external_lock_pid(int)
	RETURNS TABLE (pid int, wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int,
 				   wlocktype text, wlockmethod int, dsn text, target_pid int, target_pgprocno int, target_txn int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_waiting_external_lock_pid';

DROP FUNCTION IF EXISTS gdd_test_show_waiting_external_lock_pgprocno(int);
CREATE OR REPLACE FUNCTION
	gdd_test_show_waiting_external_lock_pgprocno(int)
	RETURNS TABLE (pid int, wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int,
 				   wlocktype text, wlockmethod int, dsn text, target_pid int, target_pgprocno int, target_txn int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_waiting_external_lock_pgprocno';

DROP FUNCTION IF EXISTS gdd_test_show_waiting_external_lock_myself();
CREATE OR REPLACE FUNCTION
	gdd_test_show_waiting_external_lock_myself()
	RETURNS TABLE (pid int, wlocktag1 int, wlocktag2 int, wlocktag3 int, wlocktag4 int,
 				   wlocktype text, wlockmethod int, dsn text, target_pid int, target_pgprocno int, target_txn int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_waiting_external_lock_myself';

/*
 * DeadLock check
 */
DROP FUNCTION IF EXISTS gdd_test_deadlock_check_pid(int);
CREATE OR REPLACE FUNCTION
	gdd_test_deadlock_check_pid(int)
	RETURNS CSTRING
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_deadlock_check_pid';

DROP FUNCTION IF EXISTS gdd_test_deadlock_check_myself();
CREATE OR REPLACE FUNCTION
	gdd_test_deadlock_check_myself()
	RETURNS CSTRING
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_deadlock_check_myself';

/*
 * Set external lock property
 */
DROP FUNCTION IF EXISTS gdd_test_external_lock_set_properties_pid(cstring, int, cstring, int, int, int, bool);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_set_properties_pid(cstring, int, cstring, int, int, bool)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_set_properties_pid';

DROP FUNCTION IF EXISTS gdd_test_external_lock_set_properties_pgprocno(cstring, int, cstring, int, int, int, bool);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_set_properties_pgprocno(cstring, int, cstring, int, int, int, bool)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_set_properties_pgprocno';

DROP FUNCTION IF EXISTS gdd_test_external_lock_set_properties_myself(cstring, cstring, int, int, int, bool);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_set_properties_myself(cstring, cstring, int, int, int, bool)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_set_properties_myself';

/*
 * Show all the external locks registered in this debug environment.
 */
DROP FUNCTION IF EXISTS gdd_test_show_registered_external_lock();
CREATE OR REPLACE FUNCTION
	gdd_test_show_registered_external_lock
		(OUT label text, OUT field1 int, OUT field2 int, OUT field3 int, OUT field4 int, OUT locktype text, OUT lockmethod int,
		 OUT target_dsn text, OUT target_pid int, OUT target_pgprocno int, OUT target_txn int)
	RETURNS SETOF record
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_registered_external_lock';

/*
 * Put the external lock to watiLock
 */
DROP FUNCTION IF EXISTS gdd_test_external_lock_wait_pid(cstring, int);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_wait_pid(cstring, int)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_wait_pid';

DROP FUNCTION IF EXISTS gdd_test_external_lock_wait_pgprocno(cstring, int);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_wait_pgprocno(cstring, int)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_wait_pgprocno';

DROP FUNCTION IF EXISTS gdd_test_external_lock_wait_myself(cstring);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_wait_myself(cstring)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_wait_myself';


/*
 * Detach specified external lock from waitLock
 */

DROP FUNCTION IF EXISTS gdd_test_external_lock_unwait_pid(cstring, int);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_unwait_pid(cstring, int)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_unwait_pid';

DROP FUNCTION IF EXISTS gdd_test_external_lock_unwait_pgprocno(cstring, int);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_unwait_pgprocno(cstring, int)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_unwait_pgprocno';

DROP FUNCTION IF EXISTS gdd_test_external_lock_unwait_myself(cstring);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_unwait_myself(cstring)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_unwait_myself';

/*
 * Display deadlock info
 */
DROP FUNCTION IF EXISTS gdd_show_deadlock_info();
CREATE OR REPLACE FUNCTION
	gdd_show_deadlock_info
		(OUT row_num int, OUT locktag_field1 int, OUT locktag_field2 int, OUT locktag_field4 int,
		 OUT locktag_type text, OUT lockmethodid int, OUT lockmode text, OUT pid int, OUT pgprocno int, OUT txid int)
	RETURNS SETOF record
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_show_deadlock_info';

/*
 * Release external lock
 */
DROP FUNCTION IF EXISTS gdd_test_external_lock_release(cstring);
CREATE OR REPLACE FUNCTION
	gdd_test_external_lock_release(cstring)
	RETURNS BOOL
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_external_lock_release';

/*
 * Local Dead Lock Check
 */
DROP FUNCTION IF EXISTS gdd_test_deadlock_check_pid(int);
CREATE OR REPLACE FUNCTION
	gdd_test_deadlock_check_pid(int)
	RETURNS CSTRING
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_deadlock_check_pid';

DROP FUNCTION IF EXISTS gdd_test_deadlock_check_myself();
CREATE OR REPLACE FUNCTION
	gdd_test_deadlock_check_myself()
	RETURNS CSTRING
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_deadlock_check_myself';

