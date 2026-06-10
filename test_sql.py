"""Blueis SQL 系统测试脚本 v2"""
import socket
import time
import sys
import subprocess

PASS = 0
FAIL = 0

def send_cmd(s, cmd):
    """发送命令并读取响应"""
    s.sendall((cmd + '\r\n').encode())
    time.sleep(0.3)
    data = b''
    s.settimeout(0.8)
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    return data.decode('utf-8', errors='replace').strip()

def check(test_name, response, expected_in, not_expected_in=None):
    """检查响应中是否包含/不包含指定字符串"""
    global PASS, FAIL
    ok = True
    for exp in (expected_in if isinstance(expected_in, list) else [expected_in]):
        if exp not in response:
            print(f"  FAIL {test_name}: expected '{exp}'")
            print(f"    got: {response[:200]}")
            ok = False
            break
    if ok and not_expected_in:
        for nexp in (not_expected_in if isinstance(not_expected_in, list) else [not_expected_in]):
            if nexp in response:
                print(f"  FAIL {test_name}: should NOT contain '{nexp}'")
                print(f"    got: {response[:200]}")
                ok = False
                break
    if ok:
        print(f"  PASS {test_name}")
        PASS += 1
    else:
        FAIL += 1

def connect():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 6380))
    time.sleep(0.3)
    s.settimeout(0.5)
    try:
        s.recv(4096)  # welcome
    except:
        pass
    return s

# =============================================================
print("=" * 60)
print("Blueis SQL System Test")
print("=" * 60)

s = connect()

# ---------------------------------------------------------------
# 一、数据库操作测试
# ---------------------------------------------------------------
print("\n[1. Database Operations]")

# T1: SHOW DATABASES (初始，应有 default)
r = send_cmd(s, 'show databases;')
check("T1 SHOW DATABASES (initial)", r, "default")

# T2: CREATE DATABASE
r = send_cmd(s, 'create database testdb;')
check("T2 CREATE DATABASE testdb", r, "OK")

# T3: SHOW DATABASES (应包含 default 和 testdb)
r = send_cmd(s, 'show databases;')
check("T3 SHOW DATABASES has 2 dbs", r, ["default", "testdb"])

# T4: CREATE DATABASE 重复创建
r = send_cmd(s, 'create database testdb;')
check("T4 CREATE DATABASE duplicate no error", r, "OK")

# T5: USE 不存在的数据库
r = send_cmd(s, 'use nonexistent;')
check("T5 USE nonexistent should error", r, "ERR")

# T6: USE testdb
r = send_cmd(s, 'use testdb;')
check("T6 USE testdb", r, "OK")

# T7: 在未选库时执行 SQL（新建连接测试）
s2 = connect()
r = send_cmd(s2, 'create table t1(id int);')
check("T7 SQL without DB should error", r, "ERR")
s2.close()

# T8: DROP DATABASE
r = send_cmd(s, 'drop database testdb;')
check("T8 DROP DATABASE testdb", r, "OK")

# T9: SHOW DATABASES (应只剩 default)
r = send_cmd(s, 'show databases;')
check("T9 After DROP only default", r, "default", "testdb")

# T10: 重建 testdb 并 USE
send_cmd(s, 'create database testdb;')
r = send_cmd(s, 'use testdb;')
check("T10 Recreate and USE testdb", r, "OK")

# ---------------------------------------------------------------
# 二、表 CRUD 测试
# ---------------------------------------------------------------
print("\n[2. Table CRUD Operations]")

# T11: CREATE TABLE (用 char 类型)
r = send_cmd(s, 'create table users(id int primary key, name char(32), age int);')
check("T11 CREATE TABLE users", r, "OK")

# T12: SHOW TABLES
r = send_cmd(s, 'show tables;')
check("T12 SHOW TABLES has users", r, "users")

# T13: INSERT
r = send_cmd(s, "insert into users(id, name, age) values(1, 'Alice', 25);")
check("T13 INSERT Alice", r, "OK")

# T14: INSERT 多条
r = send_cmd(s, "insert into users(id, name, age) values(2, 'Bob', 30);")
check("T14 INSERT Bob", r, "OK")

r = send_cmd(s, "insert into users(id, name, age) values(3, 'Charlie', 35);")
check("T15 INSERT Charlie", r, "OK")

# T16: SELECT *
r = send_cmd(s, 'select * from users;')
check("T16 SELECT * returns data", r, ["Alice", "Bob", "Charlie"])

# T17: SELECT 带条件
r = send_cmd(s, "select * from users where name = 'Alice';")
check("T17 SELECT WHERE name=Alice", r, "Alice", "Bob")

# T18: UPDATE
r = send_cmd(s, "update users set age = 26 where name = 'Alice';")
check("T18 UPDATE Alice age=26", r, "OK")

# T19: 验证 UPDATE 生效
r = send_cmd(s, "select * from users where name = 'Alice';")
check("T19 Verify UPDATE age=26", r, "26")

# T20: DELETE
r = send_cmd(s, "delete from users where name = 'Charlie';")
check("T20 DELETE Charlie", r, "OK")

# T21: 验证 DELETE 生效
r = send_cmd(s, 'select * from users;')
check("T21 Verify DELETE - no Charlie", r, ["Alice", "Bob"], "Charlie")

# T22: DROP TABLE
r = send_cmd(s, 'drop table users;')
check("T22 DROP TABLE users", r, "OK")

# T23: 验证 DROP TABLE - 表格输出含 "0 rows"
r = send_cmd(s, 'show tables;')
check("T23 After DROP - 0 tables", r, "0 rows")

# ---------------------------------------------------------------
# 三、多表 & 多数据库测试
# ---------------------------------------------------------------
print("\n[3. Multi-table & Multi-database]")

# T24: 创建多表
send_cmd(s, 'create table students(id int primary key, name varchar(20), grade char(1));')
send_cmd(s, "insert into students(id, name, grade) values(1, 'Tom', 'A');")
send_cmd(s, "insert into students(id, name, grade) values(2, 'Jerry', 'B');")
send_cmd(s, 'create table courses(id int primary key, title varchar(50));')
send_cmd(s, "insert into courses(id, title) values(101, 'Math');")
send_cmd(s, "insert into courses(id, title) values(102, 'English');")

r = send_cmd(s, 'show tables;')
check("T24 Multi-table SHOW TABLES", r, ["students", "courses"])

r = send_cmd(s, 'select * from students;')
check("T25 SELECT students data", r, ["Tom", "Jerry"])

r = send_cmd(s, 'select * from courses;')
check("T26 SELECT courses data", r, ["Math", "English"])

# T27: 切换到另一个数据库，应看不到 testdb 的表
send_cmd(s, 'create database otherdb;')
send_cmd(s, 'use otherdb;')
r = send_cmd(s, 'show tables;')
check("T27 Switch DB - tables isolated", r, "0 rows")

# T28: 在 otherdb 中建表
send_cmd(s, 'create table orders(id int primary key, item varchar(50));')
send_cmd(s, "insert into orders(id, item) values(1, 'Book');")
r = send_cmd(s, 'select * from orders;')
check("T28 otherdb table data", r, "Book")

# T29: 切回 testdb 验证数据还在
send_cmd(s, 'use testdb;')
r = send_cmd(s, 'select * from students;')
check("T29 Switch back - data still there", r, "Tom")

# ---------------------------------------------------------------
# 四、边界与错误处理测试
# ---------------------------------------------------------------
print("\n[4. Edge Cases & Error Handling]")

# T30: 不存在的表
r = send_cmd(s, 'select * from nonexistent;')
check("T30 SELECT nonexistent table", r, "ERR")

# T31: 重复主键 - 不应崩溃
send_cmd(s, "insert into students(id, name, grade) values(1, 'Dup', 'F');")
r = send_cmd(s, 'select * from students;')
check("T31 Duplicate PK - no crash", r, "Tom")

# T32: DROP 不存在的数据库
r = send_cmd(s, 'drop database nonexistent_db;')
check("T32 DROP nonexistent database", r, "OK")

# T33: 大量数据插入
for i in range(4, 14):
    send_cmd(s, f"insert into students(id, name, grade) values({i}, 'Stu{i}', 'C');")
r = send_cmd(s, 'select * from students;')
check("T33 Batch insert 10 rows", r, "Stu4")

# T34: UPDATE 无匹配行
r = send_cmd(s, "update students set grade = 'Z' where name = 'Nobody';")
check("T34 UPDATE no match", r, "OK")

# T35: DELETE 无匹配行
r = send_cmd(s, "delete from students where name = 'Nobody';")
check("T35 DELETE no match", r, "OK")

# T36: SHOW DATABASES 包含所有库
r = send_cmd(s, 'show databases;')
check("T36 SHOW DATABASES all 3 dbs", r, ["default", "testdb", "otherdb"])

# T37: CREATE TABLE 无主键
r = send_cmd(s, 'create table logs(id int, msg varchar(100));')
check("T37 CREATE TABLE without PK", r, "OK")

# T38: DROP 当前正在使用的数据库
send_cmd(s, 'use otherdb;')
r = send_cmd(s, 'drop database otherdb;')
check("T38 DROP current database", r, "OK")
# 验证 current_db 被清空
r = send_cmd(s, 'create table t(id int);')
check("T39 After DROP current DB - SQL blocked", r, "ERR")

s.close()

# ---------------------------------------------------------------
# 五、持久化测试
# ---------------------------------------------------------------
print("\n[5. Persistence Test]")

# 用新连接执行 SAVE
saver = connect()
send_cmd(saver, 'use testdb;')

# T40: SAVE
r = send_cmd(saver, 'save;')
check("T40 SAVE", r, "OK")

saver.close()
time.sleep(1)

# T41: 重启服务器，验证数据恢复
print("\n  --- Restart server to verify persistence ---")

# Kill server
subprocess.run(['taskkill', '/F', '/IM', 'blueis.exe'], capture_output=True)
time.sleep(1)

# Restart
subprocess.Popen(['./build/blueis.exe'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2)

# 重新连接验证
s = connect()
r = send_cmd(s, 'show databases;')
check("T41 After restart SHOW DATABASES", r, ["default", "testdb"])

send_cmd(s, 'use testdb;')
r = send_cmd(s, 'select * from students;')
check("T42 After restart testdb data", r, "Tom")

r = send_cmd(s, 'show tables;')
check("T43 After restart SHOW TABLES", r, ["students", "courses"])

# 验证 otherdb 被 DROP 了
r = send_cmd(s, 'show databases;')
check("T44 otherdb was dropped before SAVE", r, "default", "otherdb")

s.close()

# =============================================================
print("\n" + "=" * 60)
print(f"Results: {PASS} passed, {FAIL} failed")
print("=" * 60)

sys.exit(0 if FAIL == 0 else 1)
