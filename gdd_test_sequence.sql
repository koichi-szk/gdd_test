/*
 * 基本シナリオの試験
 */
\x
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a int, b int);
INSERT INTO t1 values (0, 0), (1, 1);
select gdd_test_init_test('outf');
BEGIN;
LOCK TABLE t1 IN ACCESS EXCLUSIVE MODE;
select * from gdd_test_show_myself();
-- detach myself to gdb
select * from gdd_test_external_lock_acquire_myself('a');
select * from gdd_test_external_lock_acquire_myself('b');
select gdd_test_external_lock_set_properties_myself('a', 'host=ubuntu01 dbname=koichi dbuser=koichi', 100, 99, 100, true);
select gdd_test_external_lock_set_properties_myself('b', 'host=ubuntu02 dbname=koichi dbuser=koichi', 200, 199, 200, true);
select * from gdd_test_show_registered_external_lock();
select * from gdd_if_has_external_lock_myself();
select gdd_test_external_lock_wait_myself('a');
select * from gdd_if_has_external_lock_myself();
select * from gdd_describe_myself();
select gdd_test_deadlock_check_myself();
select gdd_test_external_lock_unwait_myself('a');

/* ここで以下のセッションを起動する。 */

/* 別のセッションでやって見る必要がある */
\x
BEGIN;
select * from gdd_test_show_myself();
-- ここで gdb を起動する。Deadlock Detector はこのプロセスで起動されるはずなので、上の結果のプロセスにアタッチすること。
LOCK TABLE t1 IN ACCESS EXCLUSIVE MODE;
\q


/*
 * シナリオ２ -- マルチパス EXTERNAL LOCK の検出
 */

-- 準備 --

\x
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a int, b int);
INSERT INTO t1 values (0, 0), (1, 1);

-- Transaction 0 ----
\x
BEGIN;
LOCK TABLE t1 IN ROW SHARE MODE;
select * from gdd_test_show_myself();
-- detach myself to gdb
select * from gdd_test_external_lock_acquire_myself('a');
select gdd_test_external_lock_set_properties_myself('a', 'host=ubuntu01 dbname=koichi dbuser=koichi', 100, 99, 100, true);
select * from gdd_test_show_registered_external_lock();
select * from gdd_if_has_external_lock_myself();
select gdd_test_external_lock_wait_myself('a');
select * from gdd_if_has_external_lock_myself();
select * from gdd_describe_myself();
select gdd_test_deadlock_check_myself();

-- Transaction 1 ----
\x
BEGIN;
LOCK TABLE t1 IN ROW SHARE MODE;
select * from gdd_test_show_myself();
-- detach myself to gdb
select * from gdd_test_external_lock_acquire_myself('b');
select gdd_test_external_lock_set_properties_myself('b', 'host=ubuntu02 dbname=koichi dbuser=koichi', 100, 99, 100, true);
select * from gdd_test_show_registered_external_lock();
select * from gdd_if_has_external_lock_myself();
select gdd_test_external_lock_wait_myself('b');
select * from gdd_if_has_external_lock_myself();
select * from gdd_describe_myself();
select gdd_test_deadlock_check_myself();

-- Transaction 2 ---
\x
BEGIN;
select * from gdd_test_show_myself();
-- ここで gdb を起動する。Deadlock Detector はこのプロセスで起動されるはずなので、上の結果のプロセスにアタッチすること。
LOCK TABLE t1 IN ACCESS EXCLUSIVE MODE;

