# CLAUDE.md

## 项目概括

Blueis — 仿 Redis 的轻量级内存缓存数据库，C++17，Windows/MinGW。支持 Redis 命令和 SQL 语句两种方式操作数据，通过 TCP 6380 端口对外服务。

**已完成（v1.0）**：
- Phase 1: TCP Server + String 命令（SET/GET/DEL/EXISTS/KEYS）
- Phase 2: 完整 SQL 引擎（CREATE TABLE / INSERT / SELECT / UPDATE / DELETE / DROP TABLE / SHOW TABLES）
- Phase 3: Hash 命令（HSET/HGET/HDEL/HEXISTS/HGETALL/HKEYS）
- Phase 4: 终端行编辑（方向键、多行输入、全行重绘）
- Phase 5: RESP 协议兼容（RespParser + RespWriter, redis-cli 可直接连接）
- Phase 6: AOF 持久化（Append-Only File, 启动重放, SAVE 触发 rewrite）
- Phase 7: 过期机制（EXPIRE/EXPIREAT/TTL/PERSIST, 惰性删除 + 定期删除, AOF 持久化过期信息）

**待实现**：内存管理 → IO 多路复用 → 主从复制 → 哨兵/集群
详见 [需求文档_v1.0.md](需求文档_v1.0.md)

## 核心目录结构

```
├── main.cpp                  # 入口
├── include/common.h          # 共享类型: Command, DataType, TableMeta
├── src/server/               # TCP 服务器 (Winsock2, 多线程, 行编辑)
├── src/protocol/             # 协议层: CommandParser(文本) + RespParser/RESP Writer(RESP)
├── src/sql/                  # SQL 引擎: Lexer → Parser → AST → Executor → Formatter
├── src/storage/              # 存储引擎 (单例, shared_mutex, String + Hash + 表/行)
├── src/persistence/          # AOF 持久化 (命令日志追加 + rewrite)
└── build/                    # CMake 构建产物
```

## 常用开发命令

```bash
# 构建
cd build && cmake .. -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="D:/Program Files/mingw64/bin/g++.exe"
cmake --build build

# 启动服务器
./build/blueis.exe

# 连接测试
telnet 127.0.0.1 6380
```

注意事项: `<windows.h>` 污染 `ERROR` 宏，标准库头文件必须放在它之前；clangd 需配 `--target=x86_64-w64-mingw32` 才能解析 GCC 头文件。数据文件 `data/blueis.aof` 在运行目录下自动创建。
