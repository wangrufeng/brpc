// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// Redis命名服务压测工具
//
// 这是一个基于brpc开发的多线程Redis压测客户端，专门用于测试通过命名服务访问的Redis集群。
// 主要功能包括：
// 1. 支持命名服务地址连接Redis集群
// 2. 自动生成UUID风格的测试数据
// 3. 支持两种压测模式：纯GET和GET/SET混合
// 4. 模拟真实的缓存命中率场景
// 5. 提供详细的性能统计信息
// 6. 自动为SET操作添加过期时间，避免影响线上业务

#include <brpc/channel.h>
#include <brpc/policy/redis_authenticator.h>
#include <brpc/redis.h>
#include <brpc/server.h>
#include <bthread/bthread.h>
#include <butil/logging.h>
#include <butil/string_printf.h>
#include <bvar/bvar.h>
#include <fstream>
#include <gflags/gflags.h>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

// ==================== 命令行参数定义 ====================

// 基础配置参数
DEFINE_int32(thread_num, 50, "压测线程数量");
DEFINE_bool(use_bthread, true, "是否使用bthread发送请求（默认使用bthread）");
DEFINE_string(connection_type, "single",
              "连接类型：single（单连接）、pooled（连接池）、short（短连接）");

// Redis连接配置
DEFINE_string(redis_naming_service, "",
              "Redis命名服务地址，例如：http://xxx.redis:6380（必填）");
DEFINE_string(load_balancer, "rr",
              "负载均衡算法：rr、random、c_murmurhash、c_md5等");
DEFINE_string(redis_password, "", "Redis密码（可选）");

// 网络和重试配置
DEFINE_int32(timeout_ms, 50, "RPC超时时间（毫秒）");
DEFINE_int32(connect_timeout_ms, 500, "连接超时时间（毫秒）");
DEFINE_int32(max_retry, 0, "最大重试次数（不包括首次请求）");
DEFINE_int32(backup_request_ms, -1, "备份请求超时时间（毫秒），-1表示禁用");

// 测试数据生成配置
DEFINE_string(
    config_file, "",
    "包含key-value对的配置文件路径（可选，如果不指定则自动生成数据）");
DEFINE_int32(key_length, 36, "自动生成的key长度（字符数）");
DEFINE_int32(value_size, 10, "自动生成的value大小（字节数）");
DEFINE_int32(key_count, 1000, "生成的key-value对数量");

// 测试配置参数
DEFINE_string(test_mode, "get_only",
              "压测模式：get_only（纯GET）或mixed（GET/SET混合）");
DEFINE_double(hit_rate, 0.8, "缓存命中率（0.0-1.0），用于模拟真实业务场景");
DEFINE_double(set_ratio, 0.5, "混合模式下SET操作的比例（0.0-1.0）");
DEFINE_int32(expire_seconds, 60, "SET操作的过期时间（秒），避免影响线上业务");

// 调试和监控配置
DEFINE_bool(dont_fail, false, "遇到错误时是否继续运行（默认遇到错误会退出）");
DEFINE_int32(dummy_port, -1, "监控服务端口，-1表示不启动监控服务");

// ==================== 性能统计变量 ====================

// GET操作的性能统计
bvar::LatencyRecorder g_get_latency_recorder("redis_get"); // GET操作延迟统计
bvar::Adder<int> g_get_error_count("redis_get_error_count"); // GET错误计数
bvar::Adder<int> g_get_hit_count("redis_get_hit_count");     // GET命中计数
bvar::Adder<int> g_get_miss_count("redis_get_miss_count"); // GET未命中计数

// SET操作的性能统计
bvar::LatencyRecorder g_set_latency_recorder("redis_set"); // SET操作延迟统计
bvar::Adder<int> g_set_error_count("redis_set_error_count"); // SET错误计数
bvar::Adder<int> g_set_success_count("redis_set_success_count"); // SET成功计数

// ==================== 数据结构定义 ====================

/**
 * 键值对结构体
 * 用于存储测试数据
 */
struct KeyValuePair {
  std::string key;   // Redis键
  std::string value; // Redis值
};

/**
 * 线程参数结构体
 * 传递给每个工作线程的参数
 */
struct SenderArgs {
  int thread_id;                       // 线程ID
  brpc::Channel *redis_channel;        // Redis连接通道
  std::vector<KeyValuePair> *kv_pairs; // 测试用的键值对数据
  std::vector<std::string> *miss_keys; // 用于模拟缓存未命中的键
};

// ==================== 工具函数 ====================

/**
 * 生成UUID风格的随机字符串
 *
 * @param length 字符串长度
 * @return 生成的随机字符串
 *
 * 生成类似 d47be52136432348b2dcdb7425d73bb581y1 的字符串
 * 使用数字和小写字母的组合
 */
std::string GenerateUUIDLikeKey(int length) {
  static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  static const int charset_size = sizeof(charset) - 1;

  static thread_local std::random_device rd;
  static thread_local std::mt19937 gen(rd());
  static thread_local std::uniform_int_distribution<> dis(0, charset_size - 1);

  std::string result;
  result.reserve(length);

  for (int i = 0; i < length; ++i) {
    result += charset[dis(gen)];
  }

  return result;
}

/**
 * 生成指定大小的随机value
 *
 * @param size value的字节大小
 * @return 生成的随机value字符串
 *
 * 生成指定字节数的随机字符串作为value
 */
std::string GenerateRandomValue(int size) {
  static const char charset[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  static const int charset_size = sizeof(charset) - 1;

  static thread_local std::random_device rd;
  static thread_local std::mt19937 gen(rd());
  static thread_local std::uniform_int_distribution<> dis(0, charset_size - 1);

  std::string result;
  result.reserve(size);

  for (int i = 0; i < size; ++i) {
    result += charset[dis(gen)];
  }

  return result;
}

/**
 * 自动生成测试用的键值对数据
 *
 * @param kv_pairs 输出参数，存储生成的键值对
 * @param count 生成的键值对数量
 * @param key_length 键的长度
 * @param value_size 值的大小（字节）
 * @return 成功返回true
 */
bool GenerateKeyValuePairs(std::vector<KeyValuePair> *kv_pairs, int count,
                           int key_length, int value_size) {
  LOG(INFO) << "Generating " << count << " key-value pairs...";
  LOG(INFO) << "Key length: " << key_length << " characters";
  LOG(INFO) << "Value size: " << value_size << " bytes";

  kv_pairs->clear();
  kv_pairs->reserve(count);

  // 使用set来确保key的唯一性
  std::unordered_set<std::string> used_keys;

  for (int i = 0; i < count; ++i) {
    KeyValuePair kv;

    // 生成唯一的key
    do {
      kv.key = GenerateUUIDLikeKey(key_length);
    } while (used_keys.find(kv.key) != used_keys.end());

    used_keys.insert(kv.key);

    // 生成value
    kv.value = GenerateRandomValue(value_size);

    kv_pairs->push_back(kv);

    // 每生成1000个输出一次进度
    if ((i + 1) % 1000 == 0) {
      LOG(INFO) << "Generated " << (i + 1) << "/" << count
                << " key-value pairs";
    }
  }

  LOG(INFO) << "Successfully generated " << kv_pairs->size()
            << " key-value pairs";
  return true;
}

/**
 * 从配置文件加载键值对数据
 *
 * @param filename 配置文件路径
 * @param kv_pairs 输出参数，存储加载的键值对
 * @return 成功返回true，失败返回false
 *
 * 配置文件格式：
 * - 每行一个键值对，格式为 key=value
 * - 以#开头的行为注释
 * - 空行会被忽略
 * - 键和值两端的空白字符会被自动去除
 */
bool LoadKeyValuePairs(const std::string &filename,
                       std::vector<KeyValuePair> *kv_pairs) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open config file: " << filename;
    return false;
  }

  std::string line;
  int line_num = 0;
  while (std::getline(file, line)) {
    line_num++;

    // 跳过空行和注释行
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // 查找等号分隔符
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
      LOG(WARNING) << "Invalid line " << line_num
                   << " in config file: " << line;
      continue;
    }

    KeyValuePair kv;
    kv.key = line.substr(0, pos);
    kv.value = line.substr(pos + 1);

    // 去除键和值两端的空白字符
    kv.key.erase(0, kv.key.find_first_not_of(" \t"));
    kv.key.erase(kv.key.find_last_not_of(" \t") + 1);
    kv.value.erase(0, kv.value.find_first_not_of(" \t"));
    kv.value.erase(kv.value.find_last_not_of(" \t") + 1);

    // 只添加非空键
    if (!kv.key.empty()) {
      kv_pairs->push_back(kv);
    }
  }

  LOG(INFO) << "Loaded " << kv_pairs->size() << " key-value pairs from "
            << filename;
  return !kv_pairs->empty();
}

/**
 * 生成用于模拟缓存未命中的键
 *
 * @param kv_pairs 原始键值对数据
 * @param miss_keys 输出参数，存储生成的未命中键
 *
 * 通过在原始键后面添加"_miss"后缀来生成不存在的键，
 * 用于模拟真实业务中的缓存未命中场景
 */
void GenerateMissKeys(const std::vector<KeyValuePair> &kv_pairs,
                      std::vector<std::string> *miss_keys) {
  miss_keys->clear();
  miss_keys->reserve(kv_pairs.size());

  for (const auto &kv : kv_pairs) {
    miss_keys->push_back(kv.key + "_miss");
  }
  LOG(INFO) << "Generated " << miss_keys->size() << " miss keys";
}

/**
 * 初始化Redis中的测试数据
 *
 * @param channel Redis连接通道
 * @param kv_pairs 要初始化的键值对数据
 * @return 成功返回true，失败返回false
 *
 * 在get_only模式下，需要先将所有测试数据写入Redis，
 * 然后才能进行GET操作的压测。使用SETEX命令自动设置过期时间。
 * 为了避免单个请求过大，采用批量处理的方式。
 */
bool InitializeKeys(brpc::Channel *channel,
                    const std::vector<KeyValuePair> &kv_pairs) {
  LOG(INFO) << "Initializing " << kv_pairs.size() << " keys with SETEX...";

  const int batch_size = 100; // 批量大小，避免单个请求过大

  for (size_t i = 0; i < kv_pairs.size(); i += batch_size) {
    brpc::RedisRequest request;
    brpc::RedisResponse response;
    brpc::Controller cntl;

    // 计算当前批次的结束位置
    size_t end = std::min(i + batch_size, kv_pairs.size());

    // 为当前批次的每个键值对添加SETEX命令
    for (size_t j = i; j < end; ++j) {
      if (!request.AddCommand("SETEX %s %d %s", kv_pairs[j].key.c_str(),
                              FLAGS_expire_seconds, kv_pairs[j].value.c_str(),
                              kv_pairs[j].value.size())) {
        LOG(ERROR) << "Failed to add SETEX command for key: "
                   << kv_pairs[j].key;
        return false;
      }
    }

    // 发送批量请求
    channel->CallMethod(NULL, &cntl, &request, &response, NULL);
    if (cntl.Failed()) {
      LOG(ERROR) << "Failed to initialize keys batch " << i / batch_size << ": "
                 << cntl.ErrorText();
      return false;
    }

    // 检查每个命令的响应
    for (size_t j = 0; j < response.reply_size(); ++j) {
      if (response.reply(j).is_error()) {
        LOG(ERROR) << "SETEX failed for key " << kv_pairs[i + j].key << ": "
                   << response.reply(j).error_message();
        return false;
      }
    }

    // 每处理1000个key输出一次进度
    if ((end) % 1000 == 0 || end == kv_pairs.size()) {
      LOG(INFO) << "Initialized " << end << "/" << kv_pairs.size() << " keys";
    }
  }

  LOG(INFO) << "Successfully initialized all keys";
  return true;
}

// ==================== 工作线程函数 ====================

/**
 * 纯GET模式的工作线程函数
 *
 * @param void_args 线程参数（SenderArgs*）
 * @return NULL
 *
 * 在这个模式下，线程只执行GET操作：
 * 1. 根据hit_rate参数决定是访问存在的键还是不存在的键
 * 2. 统计命中、未命中和错误次数
 * 3. 记录延迟信息
 * 4. 在发生错误时进行适当的延迟，避免过度重试
 */
static void *get_only_sender(void *void_args) {
  SenderArgs *args = (SenderArgs *)void_args;

  // 初始化随机数生成器
  std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> hit_dis(0.0, 1.0); // 用于决定是否命中
  std::uniform_int_distribution<> key_dis(0, args->kv_pairs->size() -
                                                 1); // 选择存在的键
  std::uniform_int_distribution<> miss_dis(0, args->miss_keys->size() -
                                                  1); // 选择不存在的键

  while (!brpc::IsAskedToQuit()) {
    brpc::RedisRequest request;
    brpc::RedisResponse response;
    brpc::Controller cntl;

    // 根据命中率决定是否访问存在的键
    bool should_hit = hit_dis(gen) < FLAGS_hit_rate;
    std::string key;
    bool expect_hit = false;

    if (should_hit && !args->kv_pairs->empty()) {
      // 选择一个存在的键
      int idx = key_dis(gen);
      key = (*args->kv_pairs)[idx].key;
      expect_hit = true;
    } else if (!args->miss_keys->empty()) {
      // 选择一个不存在的键
      int idx = miss_dis(gen);
      key = (*args->miss_keys)[idx];
      expect_hit = false;
    } else {
      // 如果没有未命中键可用，回退到命中键
      if (!args->kv_pairs->empty()) {
        int idx = key_dis(gen);
        key = (*args->kv_pairs)[idx].key;
        expect_hit = true;
      } else {
        // 如果没有任何键可用，短暂休眠后继续
        bthread_usleep(1000);
        continue;
      }
    }

    // 添加GET命令
    if (!request.AddCommand("GET %b", key.c_str(), key.size())) {
      LOG(ERROR) << "Failed to add GET command";
      continue;
    }

    // 发送请求并记录延迟
    args->redis_channel->CallMethod(NULL, &cntl, &request, &response, NULL);
    const int64_t elp = cntl.latency_us();

    if (!cntl.Failed()) {
      // 请求成功，记录延迟
      g_get_latency_recorder << elp;

      if (response.reply_size() > 0) {
        if (response.reply(0).is_nil()) {
          // 键不存在（缓存未命中）
          g_get_miss_count << 1;
        } else if (!response.reply(0).is_error()) {
          // 键存在（缓存命中）
          g_get_hit_count << 1;
        } else {
          // Redis返回错误
          g_get_error_count << 1;
          LOG_IF(ERROR, !FLAGS_dont_fail)
              << "GET error: " << response.reply(0).error_message();
        }
      }
    } else {
      // 请求失败（网络错误、超时等）
      g_get_error_count << 1;
      CHECK(brpc::IsAskedToQuit() || !FLAGS_dont_fail)
          << "GET failed: " << cntl.ErrorText() << " latency=" << elp;
      // 发生错误时短暂休眠，避免过度重试
      bthread_usleep(50000);
    }
  }
  return NULL;
}

/**
 * GET/SET混合模式的工作线程函数
 *
 * @param void_args 线程参数（SenderArgs*）
 * @return NULL
 *
 * 在这个模式下，线程会随机执行GET或SET操作：
 * 1. 根据set_ratio参数决定执行GET还是SET操作
 * 2. GET操作根据hit_rate参数选择键
 * 3. SET操作自动添加过期时间
 * 4. 分别统计GET和SET操作的性能指标
 */
static void *mixed_sender(void *void_args) {
  SenderArgs *args = (SenderArgs *)void_args;

  // 初始化随机数生成器
  std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> op_dis(0.0, 1.0); // 用于决定操作类型
  std::uniform_real_distribution<> hit_dis(0.0, 1.0); // 用于决定GET是否命中
  std::uniform_int_distribution<> key_dis(0,
                                          args->kv_pairs->size() - 1); // 选择键
  std::uniform_int_distribution<> miss_dis(0, args->miss_keys->size() -
                                                  1); // 选择未命中键

  while (!brpc::IsAskedToQuit()) {
    brpc::RedisRequest request;
    brpc::RedisResponse response;
    brpc::Controller cntl;

    // 根据SET比例决定操作类型
    bool is_set_op = op_dis(gen) < FLAGS_set_ratio;

    if (is_set_op && !args->kv_pairs->empty()) {
      // ========== SET操作 ==========
      int idx = key_dis(gen);
      const auto &kv = (*args->kv_pairs)[idx];

      // 使用SETEX命令自动设置过期时间
      if (!request.AddCommand("SETEX %s %d %b", kv.key.c_str(),
                              FLAGS_expire_seconds, kv.value.c_str(),
                              kv.value.size())) {
        LOG(ERROR) << "Failed to add SETEX command";
        continue;
      }

      // 发送SET请求
      args->redis_channel->CallMethod(NULL, &cntl, &request, &response, NULL);
      const int64_t elp = cntl.latency_us();

      if (!cntl.Failed()) {
        // SET请求成功
        g_set_latency_recorder << elp;
        if (response.reply_size() > 0 && !response.reply(0).is_error()) {
          g_set_success_count << 1;
        } else {
          g_set_error_count << 1;
          LOG_IF(ERROR, !FLAGS_dont_fail)
              << "SETEX error: " << response.reply(0).error_message();
        }
      } else {
        // SET请求失败
        g_set_error_count << 1;
        CHECK(brpc::IsAskedToQuit() || !FLAGS_dont_fail)
            << "SETEX failed: " << cntl.ErrorText() << " latency=" << elp;
      }
    } else {
      // ========== GET操作 ==========
      bool should_hit = hit_dis(gen) < FLAGS_hit_rate;
      std::string key;

      // 根据命中率选择键
      if (should_hit && !args->kv_pairs->empty()) {
        int idx = key_dis(gen);
        key = (*args->kv_pairs)[idx].key;
      } else if (!args->miss_keys->empty()) {
        int idx = miss_dis(gen);
        key = (*args->miss_keys)[idx];
      } else {
        // 回退到存在的键
        if (!args->kv_pairs->empty()) {
          int idx = key_dis(gen);
          key = (*args->kv_pairs)[idx].key;
        } else {
          bthread_usleep(1000);
          continue;
        }
      }

      // 添加GET命令
      if (!request.AddCommand("GET %s", key.c_str())) {
        LOG(ERROR) << "Failed to add GET command";
        continue;
      }

      // 发送GET请求
      args->redis_channel->CallMethod(NULL, &cntl, &request, &response, NULL);
      const int64_t elp = cntl.latency_us();

      if (!cntl.Failed()) {
        // GET请求成功
        g_get_latency_recorder << elp;
        if (response.reply_size() > 0) {
          if (response.reply(0).is_nil()) {
            g_get_miss_count << 1;
          } else if (!response.reply(0).is_error()) {
            g_get_hit_count << 1;
          } else {
            g_get_error_count << 1;
            LOG_IF(ERROR, !FLAGS_dont_fail)
                << "GET error: " << response.reply(0).error_message();
          }
        }
      } else {
        // GET请求失败
        g_get_error_count << 1;
        CHECK(brpc::IsAskedToQuit() || !FLAGS_dont_fail)
            << "GET failed: " << cntl.ErrorText() << " latency=" << elp;
      }
    }
  }
  return NULL;
}

// ==================== 主函数 ====================

int main(int argc, char *argv[]) {
  // 解析命令行参数
  GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

  // 检查必填参数
  if (FLAGS_redis_naming_service.empty()) {
    LOG(ERROR) << "redis_naming_service must be specified";
    return -1;
  }

  // 参数验证
  if (FLAGS_key_length <= 0) {
    LOG(ERROR) << "key_length must be positive";
    return -1;
  }
  if (FLAGS_value_size <= 0) {
    LOG(ERROR) << "value_size must be positive";
    return -1;
  }
  if (FLAGS_key_count <= 0) {
    LOG(ERROR) << "key_count must be positive";
    return -1;
  }

  // ========== 加载或生成测试数据 ==========
  std::vector<KeyValuePair> kv_pairs;

  if (!FLAGS_config_file.empty()) {
    // 从配置文件加载数据
    if (!LoadKeyValuePairs(FLAGS_config_file, &kv_pairs)) {
      LOG(ERROR) << "Failed to load key-value pairs from " << FLAGS_config_file;
      return -1;
    }
  } else {
    // 自动生成测试数据
    if (!GenerateKeyValuePairs(&kv_pairs, FLAGS_key_count, FLAGS_key_length,
                               FLAGS_value_size)) {
      LOG(ERROR) << "Failed to generate key-value pairs";
      return -1;
    }
  }

  // 生成用于模拟缓存未命中的键
  std::vector<std::string> miss_keys;
  GenerateMissKeys(kv_pairs, &miss_keys);

  // ========== 初始化Redis连接 ==========
  brpc::Channel channel;
  brpc::ChannelOptions options;

  // 设置基本连接参数
  options.protocol = brpc::PROTOCOL_REDIS;               // 使用Redis协议
  options.timeout_ms = FLAGS_timeout_ms;                 // RPC超时时间
  options.connect_timeout_ms = FLAGS_connect_timeout_ms; // 连接超时时间
  options.connection_type = FLAGS_connection_type;       // 连接类型
  options.max_retry = FLAGS_max_retry;                   // 最大重试次数
  options.backup_request_ms = FLAGS_backup_request_ms; // 备份请求超时时间

  // 设置Redis认证（如果提供了密码）
  std::unique_ptr<brpc::policy::RedisAuthenticator> auth;
  if (!FLAGS_redis_password.empty()) {
    auth.reset(new brpc::policy::RedisAuthenticator(FLAGS_redis_password));
    options.auth = auth.get();
  }

  // 初始化连接到命名服务
  if (channel.Init(FLAGS_redis_naming_service.c_str(),
                   FLAGS_load_balancer.c_str(), &options) != 0) {
    LOG(ERROR) << "Failed to initialize channel to "
               << FLAGS_redis_naming_service;
    return -1;
  }

  LOG(INFO) << "Successfully connected to redis naming service: "
            << FLAGS_redis_naming_service;

  // ========== 初始化测试数据（仅在get_only模式下） ==========
  if (FLAGS_test_mode == "get_only") {
    if (!InitializeKeys(&channel, kv_pairs)) {
      LOG(ERROR) << "Failed to initialize keys";
      return -1;
    }
  }

  // 启动监控服务（如果指定了端口）
  if (FLAGS_dummy_port >= 0) {
    brpc::StartDummyServerAt(FLAGS_dummy_port);
  }

  // ========== 启动工作线程 ==========
  std::vector<bthread_t> bids; // bthread ID数组
  std::vector<pthread_t> pids; // pthread ID数组
  bids.resize(FLAGS_thread_num);
  pids.resize(FLAGS_thread_num);
  std::vector<SenderArgs> args; // 线程参数数组
  args.resize(FLAGS_thread_num);

  // 根据测试模式选择工作函数
  void *(*sender_func)(void *) =
      (FLAGS_test_mode == "get_only") ? get_only_sender : mixed_sender;

  // 创建并启动工作线程
  for (int i = 0; i < FLAGS_thread_num; ++i) {
    // 初始化线程参数
    args[i].thread_id = i;
    args[i].redis_channel = &channel;
    args[i].kv_pairs = &kv_pairs;
    args[i].miss_keys = &miss_keys;

    if (!FLAGS_use_bthread) {
      // 使用pthread
      if (pthread_create(&pids[i], NULL, sender_func, &args[i]) != 0) {
        LOG(ERROR) << "Failed to create pthread";
        return -1;
      }
    } else {
      // 使用bthread
      if (bthread_start_background(&bids[i], NULL, sender_func, &args[i]) !=
          0) {
        LOG(ERROR) << "Failed to create bthread";
        return -1;
      }
    }
  }

  LOG(INFO) << "Started " << FLAGS_thread_num << " "
            << (FLAGS_use_bthread ? "bthreads" : "pthreads") << " in "
            << FLAGS_test_mode << " mode";

  // ========== 性能统计输出循环 ==========
  while (!brpc::IsAskedToQuit()) {
    sleep(1); // 每秒输出一次统计信息

    if (FLAGS_test_mode == "get_only") {
      // 纯GET模式的统计输出
      LOG(INFO)
          << "GET - QPS: " << g_get_latency_recorder.qps(1)
          << ", Latency(us): " << g_get_latency_recorder.latency(1)
          << ", P50: " << g_get_latency_recorder.latency_percentile(0.5)
          << ", P99: " << g_get_latency_recorder.latency_percentile(0.99)
          << ", P99.9: " << g_get_latency_recorder.latency_percentile(0.999)
          << ", P99.99: " << g_get_latency_recorder.latency_percentile(0.9999)
          << ", Hit Ratio: "
          << (float)g_get_hit_count.get_value() /
                 (g_get_hit_count.get_value() + g_get_miss_count.get_value())
          << ", Success: "
          << g_get_miss_count.get_value() + g_get_miss_count.get_value()
          << ", Errors: " << g_get_error_count.get_value();
    } else {
      // 混合模式的统计输出
      LOG(INFO)
          << "GET - QPS: " << g_get_latency_recorder.qps(1)
          << ", Latency(us): " << g_get_latency_recorder.latency(1)
          << ", P50: " << g_get_latency_recorder.latency_percentile(0.5)
          << ", P99: " << g_get_latency_recorder.latency_percentile(0.99)
          << ", P99.9: " << g_get_latency_recorder.latency_percentile(0.999)
          << ", P99.99: " << g_get_latency_recorder.latency_percentile(0.9999)
          << ", Hit Ratio: "
          << (float)g_get_hit_count.get_value() /
                 (g_get_hit_count.get_value() + g_get_miss_count.get_value())
          << ", Success: "
          << g_get_miss_count.get_value() + g_get_miss_count.get_value()
          << ", Errors: " << g_get_error_count.get_value();

      LOG(INFO) << "SET - QPS: " << g_set_latency_recorder.qps(1)
                << ", Latency(us): " << g_set_latency_recorder.latency(1)
                << ", P50: " << g_set_latency_recorder.latency_percentile(0.5)
                << ", P99: " << g_set_latency_recorder.latency_percentile(0.99)
                << ", P99.9: "
                << g_set_latency_recorder.latency_percentile(0.999)
                << ", P99.99: "
                << g_set_latency_recorder.latency_percentile(0.9999)
                << ", Success: " << g_set_success_count.get_value()
                << ", Errors: " << g_set_error_count.get_value();
    }
  }

  LOG(INFO) << "redis_naming_press is going to quit";

  // ========== 等待所有线程结束 ==========
  for (int i = 0; i < FLAGS_thread_num; ++i) {
    if (!FLAGS_use_bthread) {
      pthread_join(pids[i], NULL);
    } else {
      bthread_join(bids[i], NULL);
    }
  }

  return 0;
}
