noinst_LTLIBRARIES =
noinst_PROGRAMS =
TESTS =
lib_LTLIBRARIES =
libexec_PROGRAMS =
pkgconfig_DATA = ./niova.pc
# libexec installs are placed into the 'niova' parent directory
my_libexecdir = $(libexecdir)/niova
my_libexec_PROGRAMS =

my_includedir = $(includedir)/niova
my_include_HEADERS =

ACLOCAL_AMFLAGS = -I m4

## Raft
RAFT_NET_CORE_SOURCES = \
	src/include/raft_net.h \
	src/include/raft.h \
	src/raft_net.c

raft_includedir = $(includedir)/raft
raft_include_HEADERS = src/include/raft_net.h src/include/raft.h test/raft_test.h

RAFT_SERVER_CORE_SOURCES = $(RAFT_NET_CORE_SOURCES) src/raft_server.c

RAFT_SERVER_BACKEND_SOURCES = \
        src/include/raft_server_backend_rocksdb.h \
        src/include/raft_cf_registry.h \
        src/raft_server_backend_posix.c \
        src/raft_server_backend_rocksdb.c \
        src/raft_cf_registry.c

raft_include_HEADERS += src/include/raft_server_backend_rocksdb.h src/include/raft_cf_registry.h

RAFT_CLIENT_CORE_SOURCES = \
	src/include/raft_net.h \
	src/include/raft.h \
	src/include/raft_client.h \
	src/raft_net.c \
	src/raft_client.c

raft_include_HEADERS += src/include/raft_client.h

lib_LTLIBRARIES += src/libniova_raft.la
src_libniova_raft_la_LDFLAGS = \
	-version-info @MAJOR_VERSION@:@MINOR_VERSION@:@VERSION_SUFFIX@
# Make libniova a dependency so that constructors execute in the correct order
src_libniova_raft_la_LIBADD = $(NIOVA_LIBS)
src_libniova_raft_la_SOURCES = \
	$(RAFT_SERVER_CORE_SOURCES) $(RAFT_SERVER_BACKEND_SOURCES)

lib_LTLIBRARIES += src/libniova_raft_client.la
src_libniova_raft_client_la_LDFLAGS = \
	-version-info @MAJOR_VERSION@:@MINOR_VERSION@:@VERSION_SUFFIX@
src_libniova_raft_client_la_LIBADD = $(NIOVA_LIBS)
src_libniova_raft_client_la_SOURCES = $(RAFT_CLIENT_CORE_SOURCES)

my_libexec_PROGRAMS += test/raft-reference-client
test_raft_reference_client_SOURCES = test/raft-reference-client.c
test_raft_reference_client_LDADD = \
	$(NIOVA_LIBS) src/libniova_raft_client.la $(NIOVA_BT_LIB)

my_libexec_PROGRAMS += test/raft-reference-server
test_raft_reference_server_SOURCES =  \
	test/raft-reference-server.c
test_raft_reference_server_LDADD = \
	$(NIOVA_LIBS) src/libniova_raft.la $(ROCKSDB_LIBS) $(NIOVA_BT_LIB)
## end Raft

noinst_PROGRAMS += test/raft-net-test
test_raft_net_test_SOURCES =   \
	$(RAFT_NET_CORE_SOURCES) test/raft-net-test.c
test_raft_net_test_LDADD = $(NIOVA_LIBS) $(NIOVA_BT_LIB)
test_raft_net_test_CFLAGS = $(AM_CFLAGS) -DUNIT_TEST
TESTS += test/raft-net-test

autofmt:
	uncrustify -c tools/uncrustify.cfg --no-backup `find . -name "*.[ch]"` | tee /dev/null

autofmt-check:
	uncrustify -c tools/uncrustify.cfg -q --check `find . -name "*.[ch]"` | grep -c FAIL | grep ^0$ > /dev/null

version:
	echo "Version info: major=@MAJOR_VERSION@ minor=@MINOR_VERSION@ suffix=@VERSION_SUFFIX@"

show-flags:
	echo "AM_CFLAGS: ${AM_CFLAGS}"
	echo "CFLAGS: ${CFLAGS}"

gen-coverage:
	lcov --no-external -b . --capture --directory . \
		--output-file niova-coverage.out
	genhtml ./niova-coverage.out --output-directory ./niova-coverage
