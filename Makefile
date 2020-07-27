# Copyright (C) 2020 Tristan. All Rights Reserved.
# See the COPYING file for licensing information.

.DEFAULT_GOAL := default

include standard.Makefile

# All the object files. By convention, each .cpp should have a corresponding
# object file. For more information, see the explanation above.
MAIN_BINARIES = bin/base/error_reporter.o \
	   bin/base/logger.o \
	   bin/base/media_type.o \
	   bin/base/strings.o \
	   bin/cgi/manager.o \
	   bin/connection/connection.o \
	   bin/connection/security_internals.o \
	   bin/http/client.o \
	   bin/http/client_error.o \
	   bin/http/server.o \
	   bin/http/server_launch_error.o \
	   bin/io/file.o \
	   bin/io/file_resolver.o \
	   bin/security/tls_configuration.o

PREREQUISITE_BINARIES = $(MAIN_BINARIES) \
	   bin/connection/memory_connection.o

default: bin/test.txt $(PREREQUISITE_BINARIES) server

# The 'all' target will compile all object files and generate the binary
# executable. This is the default target for 'make'.
all: default test

# The 'objects' target will compile all object files, but not generate the
# binary executable.
objects: bin/test.txt $(PREREQUISITE_BINARIES)

# The 'clean' target will remove all binaries generated by the build system.
# This will restore the state to a clean git clone.
clean:
	rm -rf bin
	rm -rf server

# The 'server' target will build the final binary executable.
server: main.cpp \
	base/logger.hpp \
	base/media_type.hpp \
	cgi/manager.hpp \
	http/configuration.hpp \
	http/server.hpp \
	security/policies.hpp \
	security/tls_configuration.hpp \
	$(MAIN_BINARIES)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(MAIN_BINARIES) $(LDFLAGS)

# The 'bin/test.txt' will ensure all directories required by the object files
# are present. The bin/test.txt file will be touch'ed so that mkdir will only
# be called once.
#
# When getting a 'directory does not exists' error e.g. after pulling from
# origin, creating the directory yourself can be a hassle, so maybe just
# execute the 'clean' target.
bin/test.txt:
	@mkdir bin
	@mkdir bin/base
	@mkdir bin/cgi
	@mkdir bin/connection
	@mkdir bin/http
	@mkdir bin/io
	@mkdir bin/security
	@mkdir bin/test
	@mkdir bin/test/http
	@touch bin/test.txt

bin/base/error_reporter.o: base/error_reporter.cpp \
	base/error_reporter.hpp \
	base/logger.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ base/error_reporter.cpp

bin/base/logger.o: base/logger.cpp \
	base/logger.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ base/logger.cpp

bin/base/media_type.o: base/media_type.cpp \
	base/media_type.hpp \
	base/logger.hpp \
	io/file.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ base/media_type.cpp

bin/base/strings.o: base/strings.cpp \
	base/strings.hpp \
	base/string.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ base/strings.cpp

bin/cgi/manager.o: cgi/manager.cpp \
	cgi/manager.hpp \
	cgi/script.hpp \
	http/request.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ cgi/manager.cpp

bin/connection/connection.o: connection/connection.cpp \
	connection/connection.hpp \
	base/logger.hpp \
	connection/security_internals.hpp \
	http/configuration.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ connection/connection.cpp

bin/connection/security_internals.o: connection/openssl.cpp \
	connection/security_internals.hpp \
	base/logger.hpp \
	connection/connection.hpp \
	http/configuration.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ connection/openssl.cpp

bin/connection/memory_connection.o: connection/memory_connection.cpp \
	connection/connection.hpp \
	connection/memory_userdata.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ connection/memory_connection.cpp

bin/http/client_error.o: http/client_error.cpp \
	http/client_error.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ http/client_error.cpp

bin/http/client.o: http/client.cpp \
	http/client.hpp \
	base/error_reporter.hpp \
	base/logger.hpp \
	base/media_type.hpp \
	base/strings.hpp \
	cgi/manager.hpp \
	cgi/script.hpp \
	connection/connection.hpp \
	http/client_error.hpp \
	http/configuration.hpp \
	http/request.hpp \
	http/server.hpp \
	http/utils.hpp \
	io/file.hpp \
	io/file_resolver.hpp \
	security/policies.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ http/client.cpp

bin/http/server.o: http/server.cpp \
	http/server.hpp \
	cgi/manager.hpp \
	base/logger.hpp \
	http/client.hpp \
	http/configuration.hpp \
	http/server_launch_error.hpp \
	io/file_resolver.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ http/server.cpp

bin/http/server_launch_error.o: http/server_launch_error.cpp \
	http/server_launch_error.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ http/server_launch_error.cpp

bin/io/file.o: io/file.cpp \
	io/file.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ io/file.cpp

bin/io/file_resolver.o: io/file_resolver.cpp \
	io/file_resolver.hpp \
	http/request.hpp \
	io/file.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ io/file_resolver.cpp

bin/security/tls_configuration.o: security/tls_configuration.cpp \
	security/tls_configuration.hpp \
	base/logger.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ security/tls_configuration.cpp

# the 'memory' target will invoke Valgrind, which will run the executable and
# can track memory usage. Memory leaks, double free()'s, use-after-free,
# uninitialised values, etc. can be found by using this tool.
memory:
	valgrind --num-callers=100 \
		 --leak-resolution=high \
		 --leak-check=full \
		 --track-origins=yes \
		 --show-leak-kinds=all \
		 --track-fds=yes \
		 ./server

# the 'cppcheck' target will invoke the cppcheck program. This program
# statically analyzes the code, and might catch bugs and suggest improvements.
cppcheck:
	cppcheck -I. -q --verbose --std=c++17 --enable=all .

# the 'infer' target will invoke the Infer program, so Infer is prerequisite.
# Infer will analyze the software for common bugs.
infer:
	infer run -- make

# the 'infer-clean' target will invoke the Infer program and makes sure older
# binaries will be cleaned up first, so that the complete software will be
# analyzed. Running infer again doesn't require a clean build, so use the
# 'infer' target after running the 'infer-clean' target once.
infer-clean:
	infer run -- make clean all

# A modern linter for C++, made by Facebook.
flint:
	flint++ -r -v .

include test/Makefile
