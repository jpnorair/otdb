CC=gcc

THISMACHINE := $(shell uname -srm | sed -e 's/ /-/g')
THISSYSTEM	:= $(shell uname -s)

TARGET      ?= otdb
TARGETDIR   ?= bin
PKGDIR      ?= ../_hbpkg/$(THISMACHINE)
SYSDIR      ?= ../_hbsys
EXT_DEF     ?= 
EXT_INC     ?= 
EXT_LIBFLAGS ?= 
EXT_LIBS ?= 
VERSION     ?= "0.1.0"

DEFAULT_DEF := -D__HBUILDER__
LIBMODULES  := argtable cJSON cmdtab bintex hbuilder-lib libotfs $(EXT_LIBS)
SUBMODULES  := main test

BUILDDIR    := build

SRCEXT      := c
DEPEXT      := d
OBJEXT      := o

CFLAGS_DEBUG:= -std=gnu99 -O -g -Wall -pthread
CFLAGS      := -std=gnu99 -O3 -pthread
INC         := -I. -I./$(PKGDIR)/argtable -I./$(PKGDIR)/bintex -I./$(PKGDIR)/cJSON -I./$(PKGDIR)/cmdtab -I./$(PKGDIR)/hbuilder -I./$(PKGDIR)/libotfs -I./$(PKGDIR)/m2def
INCDEP      := -I.
LIB         := -largtable -lbintex -lcJSON -lcmdtab -lhbuilder -lotfs -L./$(PKGDIR)/argtable -L./$(PKGDIR)/bintex -L./$(PKGDIR)/cJSON -L./$(PKGDIR)/cmdtab -L./$(PKGDIR)/hbuilder -L./$(PKGDIR)/libotfs
OTDB_PKG   := $(PKGDIR)
OTDB_DEF   := $(DEFAULT_DEF) $(EXT_DEF)
OTDB_INC   := $(INC) $(EXT_INC)
OTDB_LIB   := $(LIB) $(EXT_LIBFLAGS)

#OBJECTS     := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)")
#MODULES     := $(SUBMODULES) $(LIBMODULES)

# Export the following variables to the shell: will affect submodules
export OTDB_PKG
export OTDB_DEF
export OTDB_INC
export OTDB_LIB


all: directories $(TARGET)
debug: directories $(TARGET).debug
obj: $(SUBMODULES) $(LIBMODULES) 
remake: cleaner all
pkg: all install

install: 
	@mkdir -p $(SYSDIR)/bin
	@mkdir -p $(PKGDIR)/$(TARGET).$(VERSION)
	@cp $(TARGETDIR)/$(TARGET) $(PKGDIR)/$(TARGET).$(VERSION)
	@rm -f $(SYSDIR)/bin/$(TARGET)
	@cp $(TARGETDIR)/$(TARGET) $(SYSDIR)/bin
# TODO	@ln -s hbuilder.$(VERSION) ./$(PKGDIR)/otdb/bin/$(TARGET)

directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@$(RM) -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
cleaner: clean
	@$(RM) -rf $(TARGETDIR)

#Linker
$(TARGET): $(SUBMODULES) $(LIBMODULES)
	$(eval OBJECTS := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(CC) $(CFLAGS) $(OTDB_DEF) -o $(TARGETDIR)/$(TARGET) $(OBJECTS) $(OTDB_LIB)

$(TARGET).debug: $(SUBMODULES) $(LIBMODULES)
	$(eval OBJECTS := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(CC) $(CFLAGS_DEBUG) $(OTDB_DEF) -D__DEBUG__ -o $(TARGETDIR)/$(TARGET).debug $(OBJECTS) $(OTDB_LIB)

#Library dependencies (not in otdb sources)
$(LIBMODULES): %: 
	cd ./../$@ && $(MAKE) lib && $(MAKE) install

#otdb submodules
$(SUBMODULES): %: $(LIBMODULES) directories
	cd ./$@ && $(MAKE) -f $@.mk obj

#Non-File Targets
.PHONY: all remake clean cleaner

