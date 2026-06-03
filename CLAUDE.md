# CLAUDE.md

## 项目概括

Blueis — 仿 Redis 的轻量级内存缓存数据库，C++17，Windows/MinGW。支持 Redis 命令和 SQL 语句两种方式操作数据，通过 TCP 6380 端口对外服务。

**已完成**：
- Phase 1: TCP Server + String 命令（SET/GET/DEL/EXISTS/KEYS）
- Phase 2: 完整 SQL 引擎（CREATE TABLE / INSERT / SELECT / UPDATE / DELETE / DROP TABLE / SHOW TABLES）
- Phase 3: Hash 命令（HSET/HGET/HDEL/HEXISTS/HGETALL/HKEYS）
- Phase 4: 终端行编辑（方向键、多行输入、全行重绘）
- Phase 5: RESP 协议兼容（RespParser + RespWriter, redis-cli 可直接连接，首次 recv 时检测协议）
- Phase 6: AOF 持久化（Append-Only File, 启动重放, SAVE 触发 rewrite, truncate 截断），RDB 持久化（二进制快照, String/Hash/Table/Expire 四类数据, save/bgsave, 自动保存）
- Phase 7: 过期机制（EXPIRE/EXPIREAT/TTL/PERSIST, 惰性删除 + 定期删除, AOF/RDB 持久化过期信息）

**待实现**：内存管理 → IO 多路复用 → 主从复制 → 哨兵/集群
详见 [需求文档_v1.0.md](需求文档_v1.0.md)

## 核心目录结构

```
├── main.cpp                  # 入口
├── include/common.h          # 共享类型: Command, DataType, TableMeta
├── src/server/               # TCP 服务器 (Winsock2, 多线程, 行编辑, 自动保存)
├── src/protocol/             # 协议层: CommandParser(文本) + RespParser/RespWriter(RESP)
├── src/sql/                  # SQL 引擎: Lexer → Parser → AST → Executor → Formatter
├── src/storage/              # 存储引擎 (单例, shared_mutex, String + Hash + 表/行, 过期字典)
├── src/persistence/          # 持久化: AOF (命令追加 + rewrite + truncate) + RDB (二进制快照)
└── build/                    # CMake 构建产物
```

## 持久化架构

RDB + AOF 混合持久化，由 `TcpServer::trigger_auto_save()` 统一调度：

- **自动保存**：后台线程每 60s 检查写计数 `m_write_count`，有写入则同步 RDB save + AOF truncate
- **手动 SAVE**：同步 RDB save + AOF truncate
- **手动 BGSAVE**：异步 RDB save（不影响主线程，但不触发 AOF truncate）
- **启动恢复**：先加载 RDB（若存在），再重放 AOF（增量命令覆盖 RDB 旧数据）

RDB 二进制格式（小端）：
```
MAGIC(10B) → VERSION(4B) → [TYPE(1B) + DATA]... → CHECKSUM(8B)
TYPE: 0x01=String, 0x02=Hash, 0x03=Table, 0x04=Expire
```

## 常用开发命令

```bash
# 构建 (Debug 模式用于 GDB 调试)
cd build && cmake .. -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="D:/Program Files/mingw64/bin/g++.exe"
cmake --build build

# Debug 构建
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="D:/Program Files/mingw64/bin/g++.exe"
cmake --build build

# 启动服务器
./build/blueis.exe

# 连接测试
telnet 127.0.0.1 6380
```

## 注意事项

- `<windows.h>` 污染 `ERROR` 宏，标准库头文件必须放在它之前
- clangd 需配 `--target=x86_64-w64-mingw32` 才能解析 GCC 头文件
- 数据文件 `data/blueis.rdb` 和 `data/blueis.aof` 在运行目录下自动创建
- 协议检测在连接建立时一次性完成（MSG_PEEK），不是每次 recv 都判断，避免 telnet 输入 `*` 字符被误判为 RESP
- 自动保存在后台线程中执行同步 save，因为已在独立线程不会阻塞主循环；勿改回 bgsave 否则与 truncate 存在竞态（RDB 未写完就清空 AOF = 数据丢失）