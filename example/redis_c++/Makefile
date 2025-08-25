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

# CLI specific libraries (only needed for redis_cli)
CLI_DYNAMIC_LINKINGS = -lreadline -lncurses
# Static linkings for available static libraries
STATIC_BASIC_LINKINGS = -lleveldb
# Dynamic linkings with custom RPATH (will be bundled)
BUNDLED_DYNAMIC_LINKINGS = -lgflags -lprotobuf
# System dynamic linkings (must remain dynamic)
SYSTEM_DYNAMIC_LINKINGS = -lssl -lcrypto -lpthread -lz -ldl

# Set RPATH to look for libraries in ./lib directory relative to executable
RPATH_FLAGS = -Wl,-rpath,'$$ORIGIN/lib' -Wl,-rpath,'$$ORIGIN'

PRESS_SOURCES = redis_press.cpp
NAMING_PRESS_SOURCES = redis_naming_press.cpp
CLI_SOURCES = redis_cli.cpp
SERVER_SOURCES = redis_server.cpp

PRESS_OBJS = $(addsuffix .o, $(basename $(PRESS_SOURCES))) 
NAMING_PRESS_OBJS = $(addsuffix .o, $(basename $(NAMING_PRESS_SOURCES))) 
CLI_OBJS = $(addsuffix .o, $(basename $(CLI_SOURCES))) 
SERVER_OBJS = $(addsuffix .o, $(basename $(SERVER_SOURCES)))

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

.PHONY:all
all: redis_press redis_naming_press redis_cli redis_server

.PHONY:package
package: redis_naming_press
	@echo "> Creating deployment package..."
	@mkdir -p redis_naming_press_deploy/lib
	@cp redis_naming_press redis_naming_press_deploy/
	@echo "> Copying required libraries..."
	@if [ -f /usr/local/lib/libgflags.so.2.2 ]; then \
		cp /usr/local/lib/libgflags.so.2.2 redis_naming_press_deploy/lib/; \
		ln -sf libgflags.so.2.2 redis_naming_press_deploy/lib/libgflags.so.2; \
	elif [ -f /usr/lib64/libgflags.so.2.2 ]; then \
		cp /usr/lib64/libgflags.so.2.2 redis_naming_press_deploy/lib/; \
		ln -sf libgflags.so.2.2 redis_naming_press_deploy/lib/libgflags.so.2; \
	else \
		find /usr -name "libgflags.so*" 2>/dev/null | head -1 | xargs -I {} cp {} redis_naming_press_deploy/lib/ || echo "Warning: libgflags.so not found"; \
	fi
	@if [ -f /usr/local/lib/libprotobuf.so.25 ]; then \
		cp /usr/local/lib/libprotobuf.so.25 redis_naming_press_deploy/lib/; \
		ln -sf libprotobuf.so.25 redis_naming_press_deploy/lib/libprotobuf.so; \
	elif [ -f /usr/lib64/libprotobuf.so.25 ]; then \
		cp /usr/lib64/libprotobuf.so.25 redis_naming_press_deploy/lib/; \
		ln -sf libprotobuf.so.25 redis_naming_press_deploy/lib/libprotobuf.so; \
	else \
		find /usr -name "libprotobuf.so*" 2>/dev/null | head -1 | xargs -I {} cp {} redis_naming_press_deploy/lib/ || echo "Warning: libprotobuf.so not found"; \
	fi
	@if [ -f README_redis_naming_press.md ]; then cp README_redis_naming_press.md redis_naming_press_deploy/; fi
	@if [ -f redis_test_data.txt ]; then cp redis_test_data.txt redis_naming_press_deploy/; fi
	@if [ -f DEPLOYMENT_GUIDE.md ]; then cp DEPLOYMENT_GUIDE.md redis_naming_press_deploy/; fi
	@echo "> Creating startup script..."
	@cat > redis_naming_press_deploy/start.sh << 'EOF'
#!/bin/bash
# Redis命名服务压测工具启动脚本
# 自动设置库路径并启动程序

# 获取脚本所在目录
SCRIPT_DIR="$$(cd "$$(dirname "$$0")" && pwd)"

# 设置库路径
export LD_LIBRARY_PATH="$$SCRIPT_DIR/lib:$$LD_LIBRARY_PATH"

# 启动程序
exec "$$SCRIPT_DIR/redis_naming_press" "$$@"
EOF
	@chmod +x redis_naming_press_deploy/start.sh
	@echo "> Package created: redis_naming_press_deploy/"
	@echo "> Usage: ./redis_naming_press_deploy/start.sh --help"

.PHONY:clean
clean:
	@echo "> Cleaning"
	rm -rf redis_press redis_naming_press redis_cli redis_server $(PRESS_OBJS) $(NAMING_PRESS_OBJS) $(CLI_OBJS) $(SERVER_OBJS)
	rm -rf redis_naming_press_deploy

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
