DROP FUNCTION IF EXISTS gdd_test_init_test(cstring);
CREATE OR REPLACE FUNCTION gdd_test_init_test(cstring)
	RETURNS INT
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_init_test';

DROP FUNCTION IF EXISTS gdd_test_finish_test();
CREATE OR REPLACE FUNCTION gdd_test_finish_test()
	RETURNS INT
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_finish_test';

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

