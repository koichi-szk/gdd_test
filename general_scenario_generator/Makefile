#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_ctl
#
# Portions Copyright (c) 2022, Koichi Suzuki
# Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/bin/pg_ctl/Makefile
#
#-------------------------------------------------------------------------

PGFILEDESC = "pg_ctl - starts/stops/restarts the PostgreSQL server"
PGAPPICON=win32

subdir = src/bin/pg_ctl
top_builddir = /hdd2/koichi/postgres-global-deadlock/postgres
include $(top_builddir)/src/Makefile.global

# On Windows, we need to link with libpq, just for use of pqexpbuffer;
# but let's not pull that in on platforms where we don't need it.
ifeq ($(PORTNAME), win32)
override CPPFLAGS := -I$(libpq_srcdir) -I$(top_builddir)/src/interfaces/libpq $(CPPFLAGS)
LDFLAGS_INTERNAL += $(libpq_pgport)
SUBMAKE_LIBPQ := submake-libpq
endif

OBJS1=	general_scenario_generator.o $(WIN32RES)
OBJS2=	bg_workload.o $(WIN32RES)


# Add PG library path and libpq for link option.
LDFLAGS_EX := $(LDFLAGS_EX) -L $(shell pg_config --libdir) -lpq

# Add include directory and disable optimization for gdb use even in production
CFLAGS := $(CFLAGS) -O0 -g3 -I $(shell pg_config --includedir)

all: general_scenario_generator bg_workload

general_scenario_generator: $(OBJS1) | submake-libpgport $(SUBMAKE_LIBPQ)
	$(CC) $(CFLAGS) $(OBJS1) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

bg_workload: $(OBJS2) | submake-libpgport $(SUBMAKE_LIBPQ)
	$(CC) $(CFLAGS) $(OBJS2) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

install: all installdirs
	$(INSTALL_PROGRAM) general_scenario_generator$(X) '$(DESTDIR)$(bindir)/general_scenario_generator$(X)'
	$(INSTALL_PROGRAM) bg_workload$(X) '$(DESTDIR)$(bindir)/bg_workload$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/general_scenario_generator$(X)'
	rm -f '$(DESTDIR)$(bindir)/bg_workload$(X)'

clean distclean maintainer-clean:
	rm -f general_scenario_generator$(X) $(OBJS1)
	rm -f bg_workload$(X) $(OBJS2)
	rm -rf tmp_check

check:
	$(prove_check)

installcheck:
	$(prove_installcheck)
