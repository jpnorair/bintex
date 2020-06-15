# Copyright 2020, JP Norair
#
# Licensed under the OpenTag License, Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

CC := gcc
LD := ld

THISMACHINE ?= $(shell uname -srm | sed -e 's/ /-/g')
THISSYSTEM	?= $(shell uname -s)

VERSION     ?= 0.5.0
PACKAGEDIR  ?= ./../_hbpkg/$(THISMACHINE)/bintex.$(VERSION)

ifeq ($(THISSYSTEM),Darwin)
# Mac can't do conditional selection of static and dynamic libs at link time.
#	PRODUCTS := libbintex.dylib libbintex.a
	PRODUCTS := libbintex.a
else ifeq ($(THISSYSTEM),Linux)
	PRODUCTS := libbintex.so libbintex.a
else ifeq ($(THISSYSTEM),CYGWIN_NT-10.0)
	PRODUCTS := libbintex.a
else
	error "THISSYSTEM set to unknown value: $(THISSYSTEM)"
endif

SRCDIR      := .
INCDIR      := .
BUILDDIR    := build/$(THISMACHINE)
TARGETDIR   := bin/$(THISMACHINE)
RESDIR      := 
SRCEXT      := c
DEPEXT      := d
OBJEXT      := o

CFLAGS      ?= -std=gnu99 -O3 -fPIC
LIB         := $(EXT_LIB)
INC         := -I$(INCDIR) $(EXT_INC) 
INCDEP      := -I$(INCDIR) $(EXT_INC) 

#SOURCES     := $(shell find $(SRCDIR) -type f -name "*.$(SRCEXT)")
SOURCES     := $(shell ls $(SRCDIR)/*.$(SRCEXT))
OBJECTS     := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))



all: lib
lib: resources $(PRODUCTS)
remake: cleaner all
pkg: lib install

install:
	@rm -rf $(PACKAGEDIR)
	@mkdir -p $(PACKAGEDIR)
	@rm -f $(PACKAGEDIR)/../bintex
	@cp -R $(TARGETDIR)/* $(PACKAGEDIR)/
	@cp -R ./*.h $(PACKAGEDIR)/
	@ln -s bintex.$(VERSION) ./$(PACKAGEDIR)/../bintex
	cd ../_hbsys && $(MAKE) sys_install INS_MACHINE=$(THISMACHINE) INS_PKGNAME=bintex

#Copy Resources from Resources Directory to Target Directory
resources: directories

#Make the Directories
directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@$(RM) -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
cleaner: clean
	@$(RM) -rf libbintex.a
	@$(RM) -rf $(TARGETDIR)

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))
	
#Build the dynamic library
libbintex.so: $(OBJECTS)
	$(CC) -shared -fPIC -Wl,-soname,libbintex.so.1 -o $(TARGETDIR)/$@.$(VERSION) $(OBJECTS) -lc

libbintex.dylib: $(OBJECTS)
	$(CC) -dynamiclib -o $(TARGETDIR)/$@ $(OBJECTS)

#Build static library -- same on all POSIX
libbintex.a: $(OBJECTS)
	ar -rcs $(TARGETDIR)/$@ $(OBJECTS)
	ranlib $(TARGETDIR)/$@

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
.PHONY: all lib pkg remake clean cleaner resources


