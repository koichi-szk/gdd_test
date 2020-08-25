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

-- 試験完了。うまく動いた。---

-- *************************************************************************************************************
/*
 * シナリオ３ 単純な２つのデータベースにまたがる global deadlock check
 *
 * libpq-fe が backend から発行できないので、別バイナリにすることにした。pg_gdd_check_worker。これは PG の bin にインストールされる
 * ので、これを使うことにする。
 *
 * pg_gdd_check_worker は本番環境では backend が自動起動するが、試験ではデバッガを動かす必要があるので、手作業で起動するようにする。
 * そのため、backend では worker 起動はスキップするようにしてある。
 *
 * トランザクション１とトランザクション３は ksubuntu で、トランザクション２は ubuntu00 で実行することを想定している。
 */
-- トランザクション3 の External Lock Property
-- select gdd_test_external_lock_set_properties_myself('b', 'host=ksubuntu dbname=koichi user=koichi', pgprocno, pid, xid, true);`
-- トランザクション2 の External Lock Property
-- select gdd_test_external_lock_set_properties_myself('a', 'host=ubuntu00 dbname=koichi user=koichi', pgprocno, pid, xid, true);`

--- 端末の準備 -- それぞれ別のタブ。~koichi/bin/title に x-terminal でのタイトル設定方法がある。

1. T1, T2, T3
2. GDB-T1, GDB-T2-worker, GDB-T2, GDB-T3-worker, GDB-T3
3. gdd_test_sequence.sql, GIT廻り, テストコントロール, lock.c, lock.h

-- シナリオ3 ステップ１: 全体の準備、どこでやってもいい。

psql
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a int, b int);
INSERT INTO t1 values (0, 0), (1, 1);
\q

-- シナリオ3 ステップ2: Transaction 3 の準備: ksubuntu で実行 --------------------------------------------------------------

-- ksubuntu
psql
\x
BEGIN;
select * from gdd_test_show_myself();
-- これで T3 の TXID, PID, PGPROCNO がわかるので、次で使う

-- シナリオ3 ステップ3: Transaction 2 の準備: ubuntu00 ------------------------------------------------------------------

-- ubuntu00

psql -h ubuntu00
\x
BEGIN;
select * from gdd_test_external_lock_acquire_myself('b');
-- 以下の pgprocno, pid, xid は上記の gdd_test_show_myself() で得られた T3 の結果を使う
-- sql b << EOF で、次のコマンドを作ってくれる
-- select gdd_test_external_lock_set_properties_myself('b', 'host=ksubuntu dbname=koichi user=koichi', pgprocno, pid, xid, true);
select gdd_test_external_lock_set_properties_myself('b', 'host=ksubuntu dbname=koichi user=koichi', 99, 423307, 78, true);
select * from gdd_test_show_registered_external_lock();
select * from gdd_if_has_external_lock_myself();
select gdd_test_external_lock_wait_myself('b');
select * from gdd_if_has_external_lock_myself();
select * from gdd_describe_myself();
select * from gdd_test_show_myself();
-- これで T2 の TXID, PID, PGPROCNO がわかるので、次で使う

-- シナリオ3 ステップ4: Transaction 1 の準備 ------------------------------------------------------------------------

-- ksubuntu
psql
\x
BEGIN;
LOCK TABLE t1 IN ACCESS EXCLUSIVE MODE;
select * from gdd_test_external_lock_acquire_myself('a');
-- 以下の pgprocno, pid, xid は上記の gdd_test_show_myself() で得られた T2 の結果を使う
-- sql a << EOF で、次のコマンドを作ってくれる
-- select gdd_test_external_lock_set_properties_myself('a', 'host=ubuntu00 dbname=koichi user=koichi', 99, 8451, 118, true);
select gdd_test_external_lock_set_properties_myself('a', 'host=ubuntu00 dbname=koichi user=koichi', 99, 19562, 74, true);
select * from gdd_test_show_registered_external_lock();
select * from gdd_if_has_external_lock_myself();
select gdd_test_external_lock_wait_myself('a');
select * from gdd_if_has_external_lock_myself();
select * from gdd_describe_myself();
select * from gdd_test_show_myself();
-- ここで、上記で調べた T2 の TXID, PID, PGPROCNO を使う

-- ここまでで T1->T2->T3 の WfG が作れるようになる。

-- シナリオ3 ステップ5: Transaction 3 の操作 -------------------------------------------------------------------------------

-- 
-- ksubuntu
-- == 1 ==	まず gdb 起動して Transaction 3 にアタッチする。
--			ブレイクポイントは scenario3.source にある
LOCK TABLE t1 IN ACCESS EXCLUSIVE MODE;

-- 			これで T3 上の GlobalDeadlockCheck が動く。これをトレースすると、T2 に向けて GlobalDeadlockCheckRemote が
--			動き出す。pg_gdd_check_worker の起動コマンドをデバッガで取り出す。
-- == 2 ==	ここで、pg_gdd_check_worker をデバッガで動かす。== 1 == で取り出したコマンド引数を RUN で与えてトレースを開始する。
--			ブレイクポイントは worker.source にある。
-- == 3 == 	gdd_chec() 関数で、downstream データベースへの接続ポイントが得られるので、ubuntu00 上で gdd起動し、このプロセスに
--			アタッチする。これで、Transaction 3, Transaction2 のどちらも動きを調べることができる。

** ので、ここで T2 ノードで pg_global_deadlock_check_from_remote() が動く PID を調べる (P2)

--- Transaction 2 の操作 ----------------------------------------------------------------------------------
** T2 が動いているノードで gdb を上記 P2 にアタッチする
** これでノードの動きを調べる。
** LOCAL_WFG の生成のチェック、この後、GlobalDeadlockCheckRemote が T1 のノードをターゲットにするはずなので、pg_global_deadlock_check_from_remote が動く pid を調べる (P1)

-- Transaction 1 の調査 ------------------------------------------------------------------------------------
** gdb を P1 にアタッチ
** 動作のチェック
