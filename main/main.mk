CC := gcc
LD := ld

SUBAPP     := main
OTDB_PKG   ?=
OTDB_DEF   ?= -D__HBUILDER__
OTDB_INC   ?=
OTDB_LIB   ?= 

EXT_DEBUG  ?= 0

ifneq ($(EXT_DEBUG),0)
	ifeq ($(EXT_DEBUG),1)
		CFLAGS  ?= -std=gnu99 -O2 -Wall -pthread -D__DEBUG__
	else
		CFLAGS  ?= -std=gnu99 -O -g -Wall -pthread -D__DEBUG__
	endif
	SRCEXT      := c
	DEPEXT      := dd
	OBJEXT      := do
	BUILDDIR    := ../$(OTDB_BLD)
else 
	CFLAGS      ?= -std=gnu99 -O3 -pthread
	SRCEXT      := c
	DEPEXT      := d
	OBJEXT      := o
	BUILDDIR    := ../$(OTDB_BLD)
endif

SUBAPPDIR   := .

LIB         := $(OTDB_LIB)
LIBINC      := $(subst -L./,-L./../,$(OTDB_LIBINC))
INC			:= $(subst -I./,-I./../,$(OTDB_INC)) -I./../test
INCDEP      := $(INC)

SOURCES     := $(shell find . -type f -name "*.$(SRCEXT)")
OBJECTS     := $(patsubst ./%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

all: resources $(SUBAPP)
obj: $(OBJECTS)
remake: cleaner all


#Copy Resources from Resources Directory to Target Directory
resources: directories

#Make the Directories
directories:
	@mkdir -p $(SUBAPPDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@$(RM) -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
cleaner: clean
	@$(RM) -rf $(SUBAPPDIR)

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

#Direct build of the test app with objects
$(SUBAPP): $(OBJECTS)
	$(CC) $(INC) $(LIBINC) -o $(SUBAPPDIR)/$(SUBAPP) $^ $(LIB)

#Compile Stages
$(BUILDDIR)/%.$(OBJEXT): ./%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OTDB_DEF) $(INC) -c -o $@ $<
	@$(CC) $(CFLAGS) $(OTDB_DEF) $(INCDEP) -MM ./$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp
	

#Non-File Targets
.PHONY: all obj remake resources clean cleaner

