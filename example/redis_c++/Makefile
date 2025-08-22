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
# Basic dynamic linkings for other programs
BASIC_DYNAMIC_LINKINGS = -lgflags -lprotobuf -lleveldb -lssl -lcrypto -lpthread -lz -ldl

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
	# Link options for CLI (with readline/ncurses)
	CLI_LINK_OPTIONS_SO = $^ $(STATIC_LINKINGS) $(CLI_DYNAMIC_LINKINGS)
	CLI_LINK_OPTIONS = $^ $(STATIC_LINKINGS) $(CLI_DYNAMIC_LINKINGS)
	# Link options for other programs (without readline/ncurses)
	BASIC_LINK_OPTIONS_SO = $^ $(STATIC_LINKINGS) $(BASIC_DYNAMIC_LINKINGS)
	BASIC_LINK_OPTIONS = $^ $(STATIC_LINKINGS) $(BASIC_DYNAMIC_LINKINGS)
else ifeq ($(SYSTEM),Linux)
	STATIC_LINKINGS += -lbrpc
	# Link options for CLI (with readline/ncurses)
	CLI_LINK_OPTIONS_SO = -Xlinker "-(" $^ -Xlinker "-)" $(STATIC_LINKINGS) $(CLI_DYNAMIC_LINKINGS)
	CLI_LINK_OPTIONS = -Xlinker "-(" $^ -Wl,-Bstatic $(STATIC_LINKINGS) -Wl,-Bdynamic -Xlinker "-)" $(CLI_DYNAMIC_LINKINGS)
	# Link options for other programs (without readline/ncurses)
	BASIC_LINK_OPTIONS_SO = -Xlinker "-(" $^ -Xlinker "-)" $(STATIC_LINKINGS) $(BASIC_DYNAMIC_LINKINGS)
	BASIC_LINK_OPTIONS = -Xlinker "-(" $^ -Wl,-Bstatic $(STATIC_LINKINGS) -Wl,-Bdynamic -Xlinker "-)" $(BASIC_DYNAMIC_LINKINGS)
endif

.PHONY:all
all: redis_press redis_naming_press redis_cli redis_server

.PHONY:clean
clean:
	@echo "> Cleaning"
	rm -rf redis_press redis_naming_press redis_cli redis_server $(PRESS_OBJS) $(NAMING_PRESS_OBJS) $(CLI_OBJS) $(SERVER_OBJS)

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
