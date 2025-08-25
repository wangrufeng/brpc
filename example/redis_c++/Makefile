# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

BRPC_PATH = ../../
include $(BRPC_PATH)/config.mk
CXXFLAGS+=$(CPPFLAGS) -std=c++0x -DNDEBUG -O2 -pipe -W -Wall -fPIC -fno-omit-frame-pointer
HDRS+=$(BRPC_PATH)/output/include
LIBS+=$(BRPC_PATH)/output/lib
HDRPATHS = $(addprefix -I, $(HDRS))
LIBPATHS = $(addprefix -L, $(LIBS))
COMMA=,
SOPATHS=$(addprefix -Wl$(COMMA)-rpath$(COMMA), $(LIBS))

# ==================== 部署包配置 ====================
# 部署包目录名
DEPLOY_DIR = redis_naming_press_deploy
# 部署包中的库目录名
LIB_SUBDIR = lib
# 完整的库目录路径
DEPLOY_LIB_DIR = $(DEPLOY_DIR)/$(LIB_SUBDIR)

# ==================== 依赖库版本配置 ====================
# gflags库配置
GFLAGS_SO_VERSION = 2.2
GFLAGS_SO_MAJOR = 2
GFLAGS_SO_NAME = libgflags.so.$(GFLAGS_SO_VERSION)
GFLAGS_SO_LINK = libgflags.so.$(GFLAGS_SO_MAJOR)

# protobuf库配置
PROTOBUF_SO_VERSION = 25
PROTOBUF_SO_NAME = libprotobuf.so.$(PROTOBUF_SO_VERSION)
PROTOBUF_SO_LINK = libprotobuf.so

# 常见库路径
LIB_SEARCH_PATHS = /usr/local/lib /usr/lib64 /usr/lib

# ==================== 链接库分类 ====================
# CLI specific libraries (only needed for redis_cli)
CLI_DYNAMIC_LINKINGS = -lreadline -lncurses
# Static linkings for available static libraries
STATIC_BASIC_LINKINGS = -lleveldb
# Dynamic linkings with custom RPATH (will be bundled)
BUNDLED_DYNAMIC_LINKINGS = -lgflags -lprotobuf
# System dynamic linkings (must remain dynamic)
SYSTEM_DYNAMIC_LINKINGS = -lssl -lcrypto -lpthread -lz -ldl

# Set RPATH to look for libraries in lib subdirectory
RPATH_FLAGS = -Wl,-rpath,'$$ORIGIN/$(LIB_SUBDIR)'

# ==================== 源文件配置 ====================
PRESS_SOURCES = redis_press.cpp
NAMING_PRESS_SOURCES = redis_naming_press.cpp
CLI_SOURCES = redis_cli.cpp
SERVER_SOURCES = redis_server.cpp

PRESS_OBJS = $(addsuffix .o, $(basename $(PRESS_SOURCES))) 
NAMING_PRESS_OBJS = $(addsuffix .o, $(basename $(NAMING_PRESS_SOURCES))) 
CLI_OBJS = $(addsuffix .o, $(basename $(CLI_SOURCES))) 
SERVER_OBJS = $(addsuffix .o, $(basename $(SERVER_SOURCES)))

# ==================== 平台相关链接选项 ====================
ifeq ($(SYSTEM),Darwin)
 ifneq ("$(LINK_SO)", "")
	STATIC_LINKINGS += -lbrpc
 else
	# *.a must be explicitly specified in clang
	STATIC_LINKINGS += $(BRPC_PATH)/output/lib/libbrpc.a
 endif
	# Link options for CLI (with readline/ncurses + basic libs)
	CLI_LINK_OPTIONS_SO = $^ $(STATIC_LINKINGS) -Wl,-Bstatic $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(CLI_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
	CLI_LINK_OPTIONS = $^ $(STATIC_LINKINGS) -Wl,-Bstatic $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(CLI_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
	# Link options for other programs (without readline/ncurses)
	BASIC_LINK_OPTIONS_SO = $^ $(STATIC_LINKINGS) -Wl,-Bstatic $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
	BASIC_LINK_OPTIONS = $^ $(STATIC_LINKINGS) -Wl,-Bstatic $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
else ifeq ($(SYSTEM),Linux)
	STATIC_LINKINGS += -lbrpc
	# Link options for CLI (with readline/ncurses + basic libs)
	CLI_LINK_OPTIONS_SO = -Xlinker "-(" $^ -Xlinker "-)" $(STATIC_LINKINGS) -Wl,-Bstatic $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(CLI_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
	CLI_LINK_OPTIONS = -Xlinker "-(" $^ -Wl,-Bstatic $(STATIC_LINKINGS) $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic -Xlinker "-)" $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(CLI_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
	# Link options for other programs (without readline/ncurses)
	BASIC_LINK_OPTIONS_SO = -Xlinker "-(" $^ -Xlinker "-)" $(STATIC_LINKINGS) -Wl,-Bstatic $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
	BASIC_LINK_OPTIONS = -Xlinker "-(" $^ -Wl,-Bstatic $(STATIC_LINKINGS) $(STATIC_BASIC_LINKINGS) -Wl,-Bdynamic -Xlinker "-)" $(BUNDLED_DYNAMIC_LINKINGS) $(SYSTEM_DYNAMIC_LINKINGS) $(RPATH_FLAGS)
endif

# ==================== 构建目标 ====================
.PHONY:all
all: redis_press redis_naming_press redis_cli redis_server

.PHONY:package
package: redis_naming_press
	@echo "> Creating deployment package..."
	@mkdir -p $(DEPLOY_LIB_DIR)
	@cp redis_naming_press $(DEPLOY_DIR)/
	@echo "> Copying required libraries to $(LIB_SUBDIR) directory..."
	@$(call copy_library,$(GFLAGS_SO_NAME),$(GFLAGS_SO_LINK),gflags)
	@$(call copy_library,$(PROTOBUF_SO_NAME),$(PROTOBUF_SO_LINK),protobuf)
	@if [ -f README_redis_naming_press.md ]; then cp README_redis_naming_press.md $(DEPLOY_DIR)/; fi
	@echo "> Package created: $(DEPLOY_DIR)/"
	@echo "> Usage: cd $(DEPLOY_DIR) && ./redis_naming_press --help"

# 定义复制库文件的函数
# 参数: $(1)=库文件名, $(2)=符号链接名, $(3)=库描述
define copy_library
	@found=false; \
	for path in $(LIB_SEARCH_PATHS); do \
		if [ -f "$$path/$(1)" ]; then \
			echo "  Found $(3) library: $$path/$(1)"; \
			cp "$$path/$(1)" $(DEPLOY_LIB_DIR)/; \
			if [ "$(1)" != "$(2)" ]; then \
				ln -sf $(1) $(DEPLOY_LIB_DIR)/$(2); \
			fi; \
			found=true; \
			break; \
		fi; \
	done; \
	if [ "$$found" = "false" ]; then \
		echo "  Warning: $(3) library ($(1)) not found in standard paths"; \
		libfile=$$(find /usr -name "$(1)" 2>/dev/null | head -1); \
		if [ -n "$$libfile" ]; then \
			echo "  Found $(3) library: $$libfile"; \
			cp "$$libfile" $(DEPLOY_LIB_DIR)/; \
			if [ "$(1)" != "$(2)" ]; then \
				ln -sf $(1) $(DEPLOY_LIB_DIR)/$(2); \
			fi; \
		else \
			echo "  Error: $(3) library not found anywhere"; \
		fi; \
	fi
endef

.PHONY:clean
clean:
	@echo "> Cleaning"
	rm -rf redis_press redis_naming_press redis_cli redis_server $(PRESS_OBJS) $(NAMING_PRESS_OBJS) $(CLI_OBJS) $(SERVER_OBJS)
	rm -rf $(DEPLOY_DIR)

# ==================== 编译目标 ====================
redis_press:$(PRESS_OBJS)
	@echo "> Linking $@"
ifneq ("$(LINK_SO)", "")
	$(CXX) $(LIBPATHS) $(SOPATHS) $(BASIC_LINK_OPTIONS_SO) -o $@
else
	$(CXX) $(LIBPATHS) $(BASIC_LINK_OPTIONS) -o $@
endif

redis_naming_press:$(NAMING_PRESS_OBJS)
	@echo "> Linking $@"
ifneq ("$(LINK_SO)", "")
	$(CXX) $(LIBPATHS) $(SOPATHS) $(BASIC_LINK_OPTIONS_SO) -o $@
else
	$(CXX) $(LIBPATHS) $(BASIC_LINK_OPTIONS) -o $@
endif

redis_cli:$(CLI_OBJS)
	@echo "> Linking $@"
ifneq ("$(LINK_SO)", "")
	$(CXX) $(LIBPATHS) $(SOPATHS) $(CLI_LINK_OPTIONS_SO) -o $@
else
	$(CXX) $(LIBPATHS) $(CLI_LINK_OPTIONS) -o $@
endif

redis_server:$(SERVER_OBJS)
	@echo "> Linking $@"
ifneq ("$(LINK_SO)", "")
	$(CXX) $(LIBPATHS) $(SOPATHS) $(BASIC_LINK_OPTIONS_SO) -o $@
else
	$(CXX) $(LIBPATHS) $(BASIC_LINK_OPTIONS) -o $@
endif

%.o:%.cpp
	@echo "> Compiling $@"
	$(CXX) -c $(HDRPATHS) $(CXXFLAGS) $< -o $@

%.o:%.cc
	@echo "> Compiling $@"
	$(CXX) -c $(HDRPATHS) $(CXXFLAGS) $< -o $@
