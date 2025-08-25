# Redis命名服务压测工具部署指南

## 问题描述

编译好的`redis_naming_press`程序在其他服务器上运行时可能遇到动态库依赖问题：

```bash
$ ldd redis_naming_press
libgflags.so.2.2 => not found
libprotobuf.so.25 => not found
libleveldb.so.1 => not found
```

这是因为程序依赖了一些第三方动态库，而目标服务器上没有安装这些库。

## 解决方案

### 方案1：RPATH打包部署（推荐）

使用RPATH技术将依赖库与程序打包在一起，这是最实用的解决方案：

```bash
# 编译程序（已配置RPATH）
make clean
make redis_naming_press

# 自动创建部署包
make package
```

这将创建`redis_naming_press_deploy/`目录，包含：
- `redis_naming_press` - 主程序
- `lib/` - 依赖库目录（libgflags.so、libprotobuf.so）
- `start.sh` - 启动脚本
- 相关文档

**部署和使用：**
```bash
# 将整个目录复制到目标服务器
scp -r redis_naming_press_deploy/ user@target-server:/path/to/

# 在目标服务器上运行
cd /path/to/redis_naming_press_deploy/
./start.sh --help
./start.sh --redis_naming_service="redis://your-server:6379" --thread_num=10
```

**优势：**
- ✅ leveldb使用静态链接（无依赖）
- ✅ gflags和protobuf动态库打包（版本精确匹配）
- ✅ 系统库保持动态链接（兼容性好）
- ✅ 一键打包，部署简单
- ✅ 无需目标服务器安装额外依赖

### 方案2：完全静态链接

如果编译环境有所有静态库，可以尝试完全静态链接：

```bash
# 需要确保有libgflags.a和libprotobuf.a
make clean
make STATIC_BASIC_LINKINGS="-lgflags -lprotobuf -lleveldb" redis_naming_press

# 验证（应该只剩系统库）
ldd redis_naming_press
```

### 方案3：Docker容器化部署

创建Docker镜像来解决依赖问题：

```dockerfile
FROM centos:7

# 安装运行时依赖
RUN yum install -y openssl-libs zlib

# 复制部署包
COPY redis_naming_press_deploy/ /opt/redis_naming_press/

WORKDIR /opt/redis_naming_press
ENTRYPOINT ["./start.sh"]
```

### 方案4：手动依赖库打包

如果自动打包失败，可以手动操作：

```bash
# 创建部署目录
mkdir -p deploy/lib

# 复制程序
cp redis_naming_press deploy/

# 查找并复制依赖库
ldd redis_naming_press | grep -E "(gflags|protobuf)" | awk '{print $3}' | xargs -I {} cp {} deploy/lib/

# 创建启动脚本
cat > deploy/start.sh << 'EOF'
#!/bin/bash
export LD_LIBRARY_PATH=$(dirname $0)/lib:$LD_LIBRARY_PATH
exec $(dirname $0)/redis_naming_press "$@"
EOF

chmod +x deploy/start.sh
```

## 验证部署

### 检查RPATH配置

```bash
# 查看程序的RPATH设置
readelf -d redis_naming_press | grep RPATH

# 应该显示类似：
# 0x000000000000000f (RPATH) Library rpath: [$ORIGIN/lib:$ORIGIN]
```

### 检查依赖

```bash
# 查看动态库依赖
ldd redis_naming_press

# 期望结果：
# libgflags.so.2.2 => ./lib/libgflags.so.2.2 (0x...)
# libprotobuf.so.25 => ./lib/libprotobuf.so.25 (0x...)
# libleveldb.so.1 => not found (静态链接，正常)
```

### 测试运行

```bash
# 使用启动脚本测试
./start.sh --help

# 简单连接测试
./start.sh \
  --redis_naming_service="redis://your-redis-server:6379" \
  --thread_num=1 \
  --key_count=10 \
  --test_mode=get_only
```

## 技术原理

### RPATH工作机制

RPATH（Run-time search Path）是ELF可执行文件中的一个字段，指定动态链接器在运行时搜索共享库的路径。

- `$ORIGIN` - 可执行文件所在目录
- `$ORIGIN/lib` - 可执行文件同级的lib目录

### 库链接策略

修改后的Makefile采用混合链接策略：

```makefile
# 静态链接（有静态库的）
STATIC_BASIC_LINKINGS = -lleveldb

# 动态链接+RPATH（需要打包的）
BUNDLED_DYNAMIC_LINKINGS = -lgflags -lprotobuf

# 系统动态链接（保持兼容性）
SYSTEM_DYNAMIC_LINKINGS = -lssl -lcrypto -lpthread -lz -ldl

# RPATH设置
RPATH_FLAGS = -Wl,-rpath,'$$ORIGIN/lib' -Wl,-rpath,'$$ORIGIN'
```

## 故障排除

### 常见问题

1. **make package找不到库文件**
   ```bash
   # 手动查找库文件位置
   find /usr -name "libgflags.so*" 2>/dev/null
   find /usr -name "libprotobuf.so*" 2>/dev/null
   
   # 手动复制到lib目录
   cp /path/to/libgflags.so.x.x redis_naming_press_deploy/lib/
   ```

2. **目标服务器上仍然找不到库**
   ```bash
   # 检查RPATH是否正确设置
   readelf -d redis_naming_press | grep RPATH
   
   # 检查库文件是否存在
   ls -la redis_naming_press_deploy/lib/
   
   # 使用启动脚本而不是直接运行程序
   ./start.sh --help  # 正确
   ./redis_naming_press --help  # 可能失败
   ```

3. **库版本不兼容**
   ```bash
   # 检查库的符号版本
   objdump -T lib/libgflags.so.2.2 | grep GLIBC
   
   # 在相同或更新的系统上编译
   ```

### 调试命令

```bash
# 查看程序依赖的详细信息
ldd -v redis_naming_press

# 查看库搜索过程
LD_DEBUG=libs ./redis_naming_press --help

# 检查符号解析
LD_DEBUG=symbols ./redis_naming_press --help
```

## 性能和大小对比

| 方案 | 可执行文件大小 | 部署包大小 | 兼容性 | 部署复杂度 |
|------|----------------|------------|--------|------------|
| RPATH打包 | ~5MB | ~15MB | 高 | 低 |
| 完全静态 | ~30MB | ~30MB | 中 | 低 |
| Docker | ~5MB | ~200MB | 高 | 中 |
| 手动打包 | ~5MB | ~15MB | 中 | 高 |

## 最佳实践

1. **优先使用RPATH打包**：平衡了可移植性、性能和部署复杂度
2. **保留编译环境**：确保能重现相同的构建结果
3. **版本记录**：记录依赖库的确切版本
4. **测试验证**：在目标环境中充分测试
5. **自动化部署**：使用脚本自动化整个流程

## 自动化部署脚本

```bash
#!/bin/bash
# auto_deploy.sh - 完整的自动化部署脚本

set -e

echo "=== Redis命名服务压测工具自动部署 ==="

# 编译程序
echo "1. 编译程序..."
make clean
make redis_naming_press

# 创建部署包
echo "2. 创建部署包..."
make package

# 验证部署包
echo "3. 验证部署包..."
cd redis_naming_press_deploy

# 检查文件完整性
if [[ ! -f redis_naming_press ]]; then
    echo "错误：主程序文件缺失"
    exit 1
fi

if [[ ! -f start.sh ]]; then
    echo "错误：启动脚本缺失"
    exit 1
fi

if [[ ! -d lib ]]; then
    echo "错误：lib目录缺失"
    exit 1
fi

# 测试启动
echo "4. 测试程序启动..."
if ./start.sh --help > /dev/null 2>&1; then
    echo "✅ 程序启动测试通过"
else
    echo "❌ 程序启动测试失败"
    exit 1
fi

cd ..

echo "5. 部署包创建完成！"
echo "   位置: $(pwd)/redis_naming_press_deploy/"
echo "   大小: $(du -sh redis_naming_press_deploy/ | cut -f1)"
echo ""
echo "部署方法："
echo "  scp -r redis_naming_press_deploy/ user@target-server:/path/to/"
echo "  ssh user@target-server 'cd /path/to/redis_naming_press_deploy && ./start.sh --help'"
echo ""
echo "使用示例："
echo "  ./start.sh --redis_naming_service='redis://your-server:6379' --thread_num=10"
```

使用方法：
```bash
chmod +x auto_deploy.sh
./auto_deploy.sh
