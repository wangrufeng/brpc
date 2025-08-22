# Redis Naming Service 压测工具

这是一个基于brpc开发的Redis命名服务压测工具，专门用于测试通过命名服务访问的Redis集群的性能。

## 功能特性

- **命名服务支持**: 支持通过命名服务地址访问Redis集群
- **自动数据生成**: 自动生成UUID风格的测试数据，无需配置文件
- **配置文件支持**: 可选择从配置文件读取测试用的key-value数据
- **双压测模式**: 支持纯GET压测和GET/SET混合压测
- **缓存命中率模拟**: 通过混合存在和不存在的key来模拟真实业务场景
- **详细性能统计**: 支持P50/P99/P99.9/P99.99延迟分位数统计
- **自动过期控制**: SET操作自动添加过期时间，避免影响线上业务
- **Redis认证支持**: 支持密码认证
- **多线程压测**: 支持pthread和bthread两种线程模型

## 编译

### 使用Makefile
```bash
cd example/redis_c++
make redis_naming_press
```

### 使用CMake
```bash
cd example/redis_c++
mkdir build && cd build
cmake ..
make redis_naming_press
```

## 测试数据配置

工具支持两种方式提供测试数据：

### 1. 自动生成数据（推荐）

默认情况下，工具会自动生成UUID风格的测试数据：

- **Key格式**: 类似 `d47be52136432348b2dcdb7425d73bb581y1` 的36字符随机字符串
- **Value格式**: 指定字节数的随机字符串
- **数据量**: 默认生成10000个key-value对

生成参数：
- `key_length`: key长度（默认36字符）
- `value_size`: value大小（默认10字节）
- `key_count`: 生成的key-value对数量（默认10000个）

### 2. 配置文件（可选）

如果指定了`config_file`参数，则从配置文件读取数据，格式如下：

```
# 注释行以#开头
user:1001={"name":"张三","age":25,"city":"北京"}
user:1002={"name":"李四","age":30,"city":"上海"}
session:abc123={"user_id":1001,"login_time":"2024-01-01 10:00:00"}
cache:product:100={"id":100,"name":"iPhone 15","price":7999}
counter:page_view=1000000
config:max_connections=1000
```

## 使用方法

### 基本用法（自动生成数据）

```bash
# 纯GET压测模式（自动生成10000个key-value对）
./redis_naming_press \
  -redis_naming_service="http://your-redis-naming-service:6380" \
  -test_mode="get_only" \
  -thread_num=50 \
  -hit_rate=0.8

# GET/SET混合压测模式（自定义数据规格）
./redis_naming_press \
  -redis_naming_service="http://your-redis-naming-service:6380" \
  -test_mode="mixed" \
  -key_length=32 \
  -value_size=100 \
  -key_count=50000 \
  -thread_num=50 \
  -hit_rate=0.8 \
  -set_ratio=0.1
```

### 使用配置文件

```bash
# 使用配置文件的压测
./redis_naming_press \
  -redis_naming_service="http://your-redis-naming-service:6380" \
  -config_file="redis_test_data.txt" \
  -test_mode="get_only" \
  -thread_num=50 \
  -hit_rate=0.8
```

### 主要参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `redis_naming_service` | 无 | Redis命名服务地址（必填） |
| `config_file` | 空 | 配置文件路径（可选，不指定则自动生成数据） |
| `key_length` | 36 | 自动生成的key长度（字符数） |
| `value_size` | 10 | 自动生成的value大小（字节数） |
| `key_count` | 10000 | 生成的key-value对数量 |
| `test_mode` | `get_only` | 压测模式：`get_only`或`mixed` |
| `thread_num` | 50 | 压测线程数 |
| `hit_rate` | 0.8 | 缓存命中率（0.0-1.0） |
| `set_ratio` | 0.1 | 混合模式下SET操作比例（0.0-1.0） |
| `expire_seconds` | 60 | SET操作的过期时间（秒） |
| `use_bthread` | false | 是否使用bthread |
| `connection_type` | `single` | 连接类型：`single`/`pooled`/`short` |
| `load_balancer` | `c_murmurhash` | 负载均衡算法 |
| `redis_password` | 无 | Redis密码 |
| `timeout_ms` | 1000 | RPC超时时间（毫秒） |
| `connect_timeout_ms` | 500 | 连接超时时间（毫秒） |
| `max_retry` | 3 | 最大重试次数 |

### 高级用法示例

```bash
# 大规模数据压测（100万个key，每个key 32字符，每个value 1KB）
./redis_naming_press \
  -redis_naming_service="http://redis-cluster.example.com:6380" \
  -redis_password="your_password" \
  -key_length=32 \
  -value_size=1024 \
  -key_count=1000000 \
  -test_mode="mixed" \
  -thread_num=100 \
  -use_bthread=true \
  -connection_type="pooled" \
  -hit_rate=0.9 \
  -set_ratio=0.05 \
  -expire_seconds=300

# 高并发纯读压测（小数据量，高命中率）
./redis_naming_press \
  -redis_naming_service="http://redis-cluster.example.com:6380" \
  -key_length=16 \
  -value_size=50 \
  -key_count=5000 \
  -test_mode="get_only" \
  -thread_num=200 \
  -use_bthread=true \
  -hit_rate=0.95 \
  -timeout_ms=500

# 模拟真实业务场景（中等数据量，中等命中率）
./redis_naming_press \
  -redis_naming_service="http://redis-cluster.example.com:6380" \
  -key_length=36 \
  -value_size=200 \
  -key_count=100000 \
  -test_mode="mixed" \
  -thread_num=80 \
  -hit_rate=0.75 \
  -set_ratio=0.15 \
  -expire_seconds=120
```

## 输出说明

### get_only模式输出示例
```
GET - QPS: 45231, Latency(us): 1105, P50: 892, P99: 2341, P99.9: 4567, P99.99: 8901, Hits: 36185, Misses: 9046, Errors: 0
```

### mixed模式输出示例
```
GET - QPS: 40508, Latency(us): 1234, P50: 987, P99: 2567, P99.9: 5123, P99.99: 9876, Hits: 32406, Misses: 8102, Errors: 0
SET - QPS: 4501, Latency(us): 1456, P50: 1123, P99: 2890, P99.9: 5678, P99.99: 10234, Success: 4501, Errors: 0
```

### 指标说明
- **QPS**: 每秒请求数
- **Latency(us)**: 平均延迟（微秒）
- **P50/P99/P99.9/P99.99**: 延迟分位数（微秒）
- **Hits**: 缓存命中次数
- **Misses**: 缓存未命中次数
- **Errors**: 错误次数
- **Success**: 成功次数（仅SET操作）

## 压测模式详解

### get_only模式
1. 启动时先生成测试数据（或从配置文件加载）
2. 将所有key通过SETEX命令写入Redis
3. 然后启动多个线程只进行GET操作
4. 根据`hit_rate`参数随机选择存在的key或不存在的key进行GET
5. 适合测试纯读场景的性能

### mixed模式
1. 生成测试数据（或从配置文件加载），不预先写入Redis
2. 每个线程根据`set_ratio`参数随机决定执行GET还是SET操作
3. GET操作根据`hit_rate`参数选择key
4. SET操作自动添加过期时间
5. 适合测试读写混合场景的性能

## UUID风格Key生成规则

自动生成的key采用类似UUID的格式：
- 使用字符集：`0123456789abcdefghijklmnopqrstuvwxyz`（数字+小写字母）
- 默认长度：36字符
- 示例：`d47be52136432348b2dcdb7425d73bb581y1`
- 保证唯一性：使用set去重确保不会生成重复的key

## 数据规模建议

根据不同的测试场景，建议的数据规模：

| 测试场景 | key_count | key_length | value_size | 说明 |
|----------|-----------|------------|------------|------|
| 快速验证 | 1,000 | 16 | 10 | 快速启动，验证功能 |
| 小规模压测 | 10,000 | 32 | 100 | 适合开发环境 |
| 中规模压测 | 100,000 | 36 | 500 | 适合测试环境 |
| 大规模压测 | 1,000,000+ | 36 | 1024+ | 适合生产环境验证 |

## 注意事项

1. **内存使用**: 大量数据会占用客户端内存，注意key_count和value_size的设置
2. **过期时间**: 所有SET操作都会自动添加过期时间，避免对线上业务造成影响
3. **命名服务**: 确保命名服务地址正确且可访问
4. **数据生成时间**: 大量数据生成需要时间，工具会显示进度
5. **线程数**: 根据机器性能和目标QPS合理设置线程数
6. **监控**: 建议同时监控Redis服务端的性能指标
7. **网络**: 注意网络延迟对测试结果的影响

## 故障排查

1. **连接失败**: 检查命名服务地址和网络连通性
2. **认证失败**: 检查Redis密码是否正确
3. **配置文件错误**: 检查文件格式和路径
4. **内存不足**: 减少key_count或value_size
5. **性能异常**: 检查线程数、连接类型等参数设置
6. **数据生成慢**: 大量数据生成需要时间，请耐心等待

## 与原版redis_press的区别

| 特性 | redis_press | redis_naming_press |
|------|-------------|-------------------|
| 服务发现 | 直连IP:PORT | 支持命名服务 |
| 测试数据 | 固定格式 | 自动生成+配置文件 |
| Key格式 | 简单递增 | UUID风格随机 |
| 数据规模 | 固定 | 可配置 |
| 延迟统计 | 基本统计 | 详细分位数 |
| 缓存命中率 | 不支持 | 可配置模拟 |
| 过期时间 | 不支持 | 自动设置 |
| 压测模式 | 单一模式 | 双模式支持 |
| 数据生成 | 无 | 自动生成进度显示 |
