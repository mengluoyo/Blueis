# Git 指令速查

## 基础概念

```
工作区          暂存区          本地仓库         远程仓库
(改文件)  →  (git add)  →  (git commit)  →  (git push)
```

- **工作区**：你正在编辑的文件
- **暂存区**：`git add` 后，准备提交的文件快照
- **本地仓库**：`git commit` 后，存在本地的版本历史
- **远程仓库**：GitHub 上的版本

---

## 一、查看状态

```bash
git status              # 哪些文件改了、哪些已暂存
git log                 # 查看提交历史（q 退出）
git log --oneline       # 一行一条，简洁模式
git diff                # 工作区 vs 暂存区，具体改了啥
git diff --staged       # 暂存区 vs 上次提交，具体改了啥
```

---

## 二、分支操作

```bash
git branch                    # 列出本地所有分支，当前分支带 * 号
git branch -a                 # 列出所有分支（含远程）
git checkout -b feature/xxx   # 创建新分支并切换过去
git checkout master           # 切换到 master 分支
git branch -d feature/xxx     # 删除已合并的分支
git branch -D feature/xxx     # 强制删除分支（没合并也删）
```

**分支命名习惯**：
```
feature/resp      # 新功能
fix/xxx           # 修 bug
```

---

## 三、提交

```bash
git add file1.cpp file2.h     # 添加指定文件到暂存区
git add src/protocol/         # 添加整个目录
git add -A                    # 添加所有改动
git commit -m "提交信息"       # 提交
```

---

## 四、推送 & 拉取

```bash
git push origin feature/xxx        # 推送当前分支到远程
git push -u origin feature/xxx     # 首次推送并建立跟踪（之后直接 git push）
git pull                           # 拉取远程更新并合并到本地
git fetch                          # 只拉取远程信息，不合并
```

---

## 五、合并

```bash
git checkout master               # 先切换到目标分支
git merge feature/xxx             # 把 feature/xxx 合并进来
git push origin master            # 推合并后的结果
```

---

## 六、撤销

```bash
git checkout -- file.cpp          # 撤销工作区改动（回到上次 commit 状态）
git reset HEAD file.cpp           # 从暂存区移除（unstage）
git reset --soft HEAD~1           # 撤销最近一次 commit，改动保留在暂存区
git reset --hard HEAD~1           # 撤销最近一次 commit，改动全部丢弃（危险）
```

---

## 七、GitHub 配置

```bash
# 设置代理（国内访问 GitHub 用）
git config --global http.proxy http://127.0.0.1:7890

# 取消代理
git config --global --unset http.proxy

# 查看当前配置
git config --list

# 设置用户名和邮箱
git config user.name "mengluoyo"
git config user.email "1159746090@qq.com"
```

---

## 八、日常工作流

### 开发新功能

```bash
git checkout master                         # 1. 回到主分支
git checkout -b feature/aof                # 2. 创建功能分支
# ... 写代码 ...
git add -A                                  # 3. 暂存
git commit -m "Add AOF persistence"         # 4. 提交
# ... 继续写 ...
git add -A                                  # 5. 再次暂存
git commit -m "Add AOF rewrite"             # 6. 再次提交
git push -u origin feature/aof             # 7. 推送到 GitHub
```

### 合并回主分支

```bash
git checkout master                         # 1. 回主分支
git merge feature/aof                      # 2. 合并
git push origin master                      # 3. 推送
git branch -d feature/aof                  # 4. 删除功能分支
```

### 改了一半需要切分支

```bash
git stash                   # 暂存当前改动
# ... 切分支、干别的事 ...
git checkout 原分支
git stash pop               # 恢复之前的改动
```

---

## 九、提交信息规范

```
简短描述（50 字以内）

- 详细改动点 1
- 详细改动点 2
```

示例：
```
Add RESP protocol support

- RespParser: decode RESP commands from redis-cli
- RespWriter: encode responses to RESP format
- Auto-detect protocol by checking first byte
```
