#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for main
#
# IDENTIFICATION
#    src/backend/main/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/main
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = main.o main_cpp_wrapper.cc.o

main_cpp_wrapper.cc.o: main_cpp_wrapper.cc
	$(CXX) -DSEASTAR_API_LEVEL=6 -std=gnu++17 $(CPPFLAGS) -lprom -lpromhttp -lmicrohttpd -c $< -o $@

include $(top_srcdir)/src/backend/common.mk
