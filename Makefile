CC=gcc

THISMACHINE := $(shell uname -srm | sed -e 's/ /-/g')
THISSYSTEM	:= $(shell uname -s)

TARGETLIB   ?= libbintex.$(THISSYSTEM).a
TARGETDIR   ?= ./../_hbpkg/$(THISMACHINE)/bintex

SRCDIR      := .
INCDIR      := .
BUILDDIR    := build
PRODUCTDIR  := bin
RESDIR      := 
SRCEXT      := c
DEPEXT      := d
OBJEXT      := o

#CFLAGS      := -std=gnu99 -O -g -Wall
CFLAGS      := -std=gnu99 -O3
LIB         := 
INC         := -I$(INCDIR)
INCDEP      := -I$(INCDIR)

#SOURCES     := $(shell find $(SRCDIR) -type f -name "*.$(SRCEXT)")
SOURCES     := $(shell ls $(SRCDIR)/*.$(SRCEXT))
OBJECTS     := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))




all: resources $(TARGETLIB)
remake: cleaner all
	
#Make the Directories
install:
	mkdir -p $(TARGETDIR)
	cp ./bintex.h $(TARGETDIR)
	cp $(PRODUCTDIR)/* $(TARGETDIR)


#Copy Resources from Resources Directory to Target Directory
resources: directories

#Make the Directories
directories:
	@mkdir -p $(PRODUCTDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@$(RM) -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
cleaner: clean
	@$(RM) -rf $(PRODUCTDIR)

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

#Build the static library
libbintex.Darwin.a: $(OBJECTS)
	ar -rcs $(PRODUCTDIR)/libbintex.a $(OBJECTS)
	ranlib $(PRODUCTDIR)/libbintex.a

libbintex.Linux.a: $(OBJECTS)
	ar -rcs $(PRODUCTDIR)/libbintex.a $(OBJECTS)
	ranlib $(PRODUCTDIR)/libbintex.a

#Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<
	@$(CC) $(CFLAGS) $(INCDEP) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

#Non-File Targets
.PHONY: all remake clean cleaner resources


