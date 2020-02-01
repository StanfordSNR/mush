AM_CPPFLAGS = $(CXX17_FLAGS) $(SSL_CFLAGS) -I$(srcdir)/../util -I$(srcdir)/../http
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

check_PROGRAMS = ringbuffer.test http-readers.test http-writer.test

ringbuffer_test_SOURCES = ringbuffer-test.cc
ringbuffer_test_LDADD = ../util/libmushutil.a

http_readers_test_SOURCES = http-readers-test.cc
http_readers_test_LDADD = ../util/libmushutil.a ../http/libmushhttp.a

http_writer_test_SOURCES = http-writer-test.cc
http_writer_test_LDADD = ../util/libmushutil.a ../http/libmushhttp.a

TESTS = ringbuffer.test http-readers.test http-writer.test
