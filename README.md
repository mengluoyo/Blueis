# Blueis 使用说明书

## 1. 简介

Blueis 是一个轻量级的内存缓存数据库，仿照 Redis 设计。支持两种方式操作数据：

- **Redis 风格命令** — SET / GET / HSET 等
- **SQL 语句** — SELECT / INSERT / CREATE TABLE 等

数据存储在内存中，重启后自动从快照文件恢复。

## 2. 启动服务器

```bash
# 进入 build 目录启动
cd build
./blueis.exe
```

启动后显示：

```
[INFO] Blueis listening on port 6380
Blueis v0.1.0 ready. Port: 6380
Connect via: telnet 127.0.0.1 6380
```

> 如果启动时存在 `data/blueis.json` 快照文件，会自动加载之前保存的数据。

## 3. 连接

打开 Windows 命令提示符（cmd），输入：

```bash
telnet 127.0.0.1 6380
```

> 如果提示 "telnet 不是内部命令"，去 `控制面板 → 程序 → 启用或关闭 Windows 功能` 勾选 `Telnet Client`。

连接成功后显示欢迎信息，即可输入命令。

## 4. Redis 命令

以下命令不区分大小写。

### String（字符串）

| 命令 | 用法 | 示例 | 返回值 |
|------|------|------|--------|
| SET | `SET key value` | `SET name Alice` | `+OK` |
| GET | `GET key` | `GET name` | `$Alice` |
| DEL | `DEL key` | `DEL name` | `:1` 或 `:0` |
| EXISTS | `EXISTS key` | `EXISTS name` | `:1` 或 `:0` |
| KEYS | `KEYS pattern` | `KEYS user:*` | 每行一个 key |

**KEYS 通配符**：`*` 匹配所有，`prefix*` 匹配前缀，`*suffix` 匹配后缀。

### Hash（哈希）

| 命令 | 用法 | 示例 | 返回值 |
|------|------|------|--------|
| HSET | `HSET key field value` | `HSET user:1 name Alice` | `+OK` |
| HGET | `HGET key field` | `HGET user:1 name` | `$Alice` |
| HDEL | `HDEL key field` | `HDEL user:1 age` | `:1` 或 `:0` |
| HEXISTS | `HEXISTS key field` | `HEXISTS user:1 name` | `:1` 或 `:0` |
| HGETALL | `HGETALL key` | `HGETALL user:1` | `field: value` 每行一对 |
| HKEYS | `HKEYS key` | `HKEYS user:1` | 每行一个字段名 |

### 持久化

| 命令 | 说明 |
|------|------|
| `SAVE` | 手动保存当前所有数据到 `data/blueis.json` |

> 服务器关闭时也会自动保存。

## 5. SQL 语句

### CREATE TABLE — 创建表

```sql
CREATE TABLE user (
    id        INT PRIMARY KEY,
    name      VARCHAR(64),
    age       INT,
    email     VARCHAR(128)
);
```

支持的列类型：`INT`, `VARCHAR(n)`, `BOOL`, `FLOAT`

### INSERT — 插入数据

```sql
INSERT INTO user (id, name, age, email)
VALUES (1, 'Alice', 30, 'alice@example.com');

-- 多行插入
INSERT INTO user (id, name, age) VALUES
    (2, 'Bob', 25),
    (3, 'Charlie', 35);
```

### SELECT — 查询数据

```sql
-- 查询所有列
SELECT * FROM user;

-- 指定列
SELECT name, age FROM user;

-- 条件查询
SELECT * FROM user WHERE age > 25;

-- 模糊查询
SELECT * FROM user WHERE name LIKE '%li%';

-- AND / OR
SELECT * FROM user WHERE age > 25 AND age < 40;

-- 支持的比较符: =  !=  >  <  >=  <=
```

### UPDATE — 更新数据

```sql
UPDATE user SET age = 31 WHERE id = 1;

-- 更新多个字段
UPDATE user SET age = 32, email = 'new@example.com' WHERE id = 1;
```

### DELETE — 删除数据

```sql
-- 删除匹配的行
DELETE FROM user WHERE id = 3;

-- 删除所有行
DELETE FROM user;
```

### DROP TABLE — 删除表

```sql
DROP TABLE user;
```

### SHOW TABLES — 查看所有表

```sql
SHOW TABLES;
```

## 6. SQL 查询结果示例

```
> SELECT * FROM user;
+----+-------+-----+----------+
| id | name  | age | email    |
+----+-------+-----+----------+
| 1  | Alice | 31  | alice@.. |
| 2  | Bob   | 25  | (null)   |
+----+-------+-----+----------+
2 rows in set
```

## 7. 数据持久化

- **手动保存**：输入 `SAVE` 命令
- **关闭自动保存**：按 `Ctrl+C` 正常退出服务器，数据自动写入 `data/blueis.json`
- **启动自动加载**：下次启动时自动恢复

## 8. 常见问题

**Q: 关闭终端后数据还在吗？**

正常按 `Ctrl+C` 退出会触发自动保存。直接关闭终端窗口可能丢失未保存的数据，建议重要操作后执行 `SAVE`。

**Q: 支持中文吗？**

目前 UTF-8 编码在网络传输中可能存在显示问题，建议使用英文。

**Q: 支持多个客户端同时连接吗？**

支持。每个连接在独立线程中处理，互不干扰。

---

*Blueis v0.1.0*
