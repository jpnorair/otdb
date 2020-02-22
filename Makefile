CC := gcc
LD := ld

THISMACHINE ?= $(shell uname -srm | sed -e 's/ /-/g')
THISSYSTEM	?= $(shell uname -s)


APP         ?= otdb
PKGDIR      := ../_hbpkg/$(THISMACHINE)
SYSDIR      := ../_hbsys/$(THISMACHINE)
EXT_DEF     ?= 
EXT_INC     ?= 
EXT_LIB     ?=
EXT_LIBFLAGS ?=
EXT_LIBS    ?= 
VERSION     ?= 1.0.a

# Try to get git HEAD commit value
ifneq ($(INSTALLER_HEAD),)
    GITHEAD := $(INSTALLER_HEAD)
else
    GITHEAD := $(shell git rev-parse --short HEAD)
endif

ifeq ($(MAKECMDGOALS),debug)
	APPDIR      := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)_debug
	DEBUG_MODE  := 1
	CFLAGS      ?= -std=gnu99 -O0 -g -Wall -pthread -D__DEBUG__
else
	APPDIR      := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)
	DEBUG_MODE  := 0
	CFLAGS      ?= -std=gnu99 -O3 -pthread
endif


# Make sure the LD_LIBRARY_PATH includes the _hbsys directory
ifneq ($(findstring $(SYSDIR)/lib,$(LD_LIBRARY_PATH)),)
	error "$(SYSDIR)/lib not in LD_LIBRARY_PATH.  Please update your settings to include this."
endif

ifeq ($(THISSYSTEM),Darwin)
# Mac can't do conditional selection of static and dynamic libs at link time.
#	PRODUCTS := libjudy.$(THISSYSTEM).dylib libjudy.$(THISSYSTEM).a
	PRODUCTS := otdb.$(THISSYSTEM).a
	LIBBSD :=
else ifeq ($(THISSYSTEM),Linux)
#	PRODUCTS := otdb.$(THISSYSTEM).so otdb.$(THISSYSTEM).a
    PRODUCTS := otdb.POSIX.a
    ifneq ($(findstring OpenWRT,$(THISMACHINE)),)
        LIBBSD := -lfts
    else
        LIBBSD :=
    endif
        
else ifeq ($(THISSYSTEM),CYGWIN_NT-10.0)
	PRODUCTS := otdb.POSIX.a
	LIBBSD :=
else
	error "THISSYSTEM set to unknown value: $(THISSYSTEM)"
endif

DEFAULT_DEF := -D__HBUILDER__ -DOTDB_PARAM_GITHEAD=\"$(GITHEAD)\"
LIBMODULES  := argtable cJSON clithread cmdtab bintex m2def libotfs hbuilder-lib $(EXT_LIBS)
#SUBMODULES  := main client test
SUBMODULES  := main client

#SRCEXT      := c
#DEPEXT      := d
#OBJEXT      := o

INC         := -I. -I./include -I./$(SYSDIR)/include 
INCDEP      := -I.
LIBINC      := -L./$(SYSDIR)/lib
LIB         := -largtable -lbintex -lcJSON -lclithread -lcmdtab -lhbuilder -lotfs -loteax -ljudy -ltalloc -lm -lc $(LIBBSD)


# Makesystem variables
# Export the following variables to the shell: will affect submodules
OTDB_PKG   := $(PKGDIR)
OTDB_DEF   := $(DEFAULT_DEF) $(EXT_DEF)
OTDB_INC   := $(INC) $(EXT_INC)
OTDB_LIB   := $(LIB) $(EXT_LIBFLAGS)
OTDB_LIBINC:= $(LIBINC) $(EXT_LIB)
OTDB_BLD   := $(BUILDDIR)
OTDB_APP   := $(APPDIR)
export OTDB_PKG
export OTDB_DEF
export OTDB_INC
export OTDB_LIB
export OTDB_LIBINC
export OTDB_BLD
export OTDB_APP


deps: $(LIBMODULES)
all: release $(PRODUCTS)
release: directories $(APP)

debug: directories $(APP).debug

obj: $(SUBMODULES)
pkg: deps all install
remake: cleaner all


install: 
	@rm -rf $(PKGDIR)/$(APP).$(VERSION)
	@mkdir -p $(PKGDIR)/$(APP).$(VERSION)
	@cp $(APPDIR)/* $(PKGDIR)/$(APP).$(VERSION)/
	@rm -f $(PKGDIR)/$(APP)
	@ln -s $(APP).$(VERSION) ./$(PKGDIR)/$(APP)
	cd ../_hbsys && $(MAKE) sys_install INS_MACHINE=$(THISMACHINE) INS_PKGNAME=otdb

directories:
	@mkdir -p $(APPDIR)
	@mkdir -p $(BUILDDIR)

# Clean only this machine
clean:
	@$(RM) -rf $(BUILDDIR)
	@$(RM) -rf $(BUILDDIR)_debug
	@$(RM) -rf $(APPDIR)

# Clean all builds
cleaner: 
	@$(RM) -rf ./build
	@$(RM) -rf ./bin





# Linker for Client library
# Build the static library
# Note: testing with libtool now, which may be superior to ar
otdb.Darwin.a: $(OBJECTS)
	libtool -o $(APPDIR)/libotdb.a -static $(OBJECTS)

otdb.POSIX.a: $(OBJECTS)
#	$(eval LIBTOOL_OBJ := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(eval LIBTOOL_OBJ := $(shell find $(BUILDDIR) -type f -name "*.o"))
	ar rcs -o $(APPDIR)/libotdb.a $(OBJECTS)

# Build shared library
otdb.Linux.so: $(OBJECTS)
#	$(eval LIBTOOL_OBJ := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(eval LIBTOOL_OBJ := $(shell find $(BUILDDIR) -type f -name "*.o"))
	$(CC) -shared -fPIC -Wl,-soname,libotdb.so.1 -o $(APPDIR)/libotdb.so.$(VERSION) $(LIBTOOL_OBJ) -lc


# Linker for OTDB application
$(APP): $(SUBMODULES) 
#	$(eval OBJECTS := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(eval OBJECTS := $(shell find $(BUILDDIR) -type f -name "*.o"))
	$(CC) $(CFLAGS) $(OTDB_DEF) $(OTDB_INC) $(OTDB_LIBINC) -o $(APPDIR)/$(APP) $(OBJECTS) $(OTDB_LIB)

$(APP).debug: $(SUBMODULES)
#	$(eval OBJECTS_D := $(shell find $(BUILDDIR) -type f -name "*.d$(OBJEXT)"))
	$(eval OBJECTS_D := $(shell find $(BUILDDIR) -type f -name "*.do"))
	$(CC) $(CFLAGS) $(OTDB_DEF) -D__DEBUG__ $(OTDB_INC) $(OTDB_LIBINC) -o $(APPDIR)/$(APP).debug $(OBJECTS_D) $(OTDB_LIB)



#Library dependencies (not in otdb sources)
$(LIBMODULES): %: 
	cd ./../$@ && $(MAKE) pkg

#otdb submodules
$(SUBMODULES): %: directories
	cd ./$@ && $(MAKE) -f $@.mk obj EXT_DEBUG=$(DEBUG_MODE)
	

#Non-File Targets
.PHONY: deps all release debug obj pkg remake install directories clean cleaner

