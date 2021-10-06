begin;
select pg_backend_pid();
lock table t1 in access share mode;
