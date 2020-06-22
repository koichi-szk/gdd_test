\x
select gdd_test_init_test('outf');
BEGIN;
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
-- Test OK upto here, June 16, 2020.
-- K.Suzuki 次の関数書いていなかった。
select * from gdd_describe_myself();
select gdd_test_deadlock_check_myself();
select gdd_test_external_lock_unwait_myself('a');
