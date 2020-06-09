DROP FUNCTION IF EXISTS gdd_test_init_test;
CREATE OR REPLACE FUNCTION gdd_test_init_test(cstring)
	RETURNS INT
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_init_test';

CREATE OR REPLACE FUNCTION gdd_test_finish_test()
	RETURNS INT
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_finish_test';

CREATE OR REPLACE FUNCTION gdd_test_show_myself()
	RETURNS TABLE(system_id TEXT, pid INT, pgprocno INT, lxn INT)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_myself';

CREATE OR REPLACE FUNCTION gdd_test_show_proc(int)
	RETURNS TABLE(system_id TEXT, pid INT, pgprocno INT, lxn INT)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_show_proc';

CREATE OR REPLACE FUNCTION
	gdd_test_set_locktag_external_myself
		(cstring, bool)
	RETURNS TABLE  (label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_set_locktag_external_pgprocno';

CREATE OR REPLACE FUNCTION
	gdd_test_set_locktag_external_pgprocno
		(cstring, int, bool)
	RETURNS TABLE  (label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_set_locktag_external_pgprocno';

CREATE OR REPLACE FUNCTION
	gdd_test_set_locktag_external_pid
		(cstring, int, bool)
	RETURNS TABLE  (label text, field1 int, field2 int, field3 int, field4 int, locktype text, lockmethod int)
	LANGUAGE c VOLATILE
	AS 'gdd_test.so', 'gdd_test_set_locktag_external_pid';

