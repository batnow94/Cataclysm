# comment these to toggle them as one sees fit.
# WARNINGS will spam hundreds of warnings, mostly safe, if turned on
# DEBUG is best turned on if you plan to debug in gdb -- please do!
# PROFILE is for use with gprof or a similar program -- don't bother generally
#WARNINGS = -Wall -Wextra -Wno-switch -Wno-sign-compare -Wno-missing-braces -Wno-unused-parameter -Wno-char-subscripts
#DEBUG = -g
#PROFILE = -pg
OTHERS = -O3 -std=gnu++17 -MMD -MP

ODIR1 = obj_cataclysm
ODIR2 = obj_socrates_daimon

TARGET1 = cataclysm
TARGET2 = socrates-daimon

OS  = $(shell uname -o)
CXX = g++

CFLAGS = $(WARNINGS) $(DEBUG) $(PROFILE) $(OTHERS)

ifeq ($(OS), Msys)
LDFLAGS = -static -lncurses -lgdi32
else
LDFLAGS = -lncurses
endif
LDFLAGS += -pthread -Llib/host -lz_format_util -lz_stdio_log -lz_stdio_c

SOURCES1 = $(filter-out stdafx.cpp socrates-daimon.cpp,$(wildcard *.cpp))
OBJS1 = $(addprefix $(ODIR1)/,$(SOURCES1:.cpp=.o))

SOURCES2 = calendar.cpp catacurse.cpp color.cpp html.cpp item.cpp itypedef.cpp\
  json.cpp keypress.cpp mapdata.cpp mtypedef.cpp options.cpp output.cpp\
  recent_msg.cpp rng.cpp saveload.cpp skill.cpp socrates-daimon.cpp trapdef.cpp
OBJS2 = $(addprefix $(ODIR2)/,$(SOURCES2:.cpp=.o))

.PHONY: all clean
all: $(TARGET1) $(TARGET2)

$(TARGET1): $(ODIR1) $(OBJS1)
	$(CXX) -o $@ $(CFLAGS) -DCATACLYSM $(OBJS1) $(LDFLAGS)

$(TARGET2): $(ODIR2) $(OBJS2)
	$(CXX) -o $@ $(CFLAGS) -DSOCRATES_DAIMON $(OBJS2) $(LDFLAGS)

$(ODIR1) $(ODIR2):
	mkdir $@

$(ODIR1)/%.o: %.cpp
	$(CXX) $(CFLAGS) -DCATACLYSM -c $< -o $@

$(ODIR2)/%.o: %.cpp
	$(CXX) $(CFLAGS) -DSOCRATES_DAIMON -c $< -o $@

clean:
	rm -f $(TARGET1) $(TARGET2) $(ODIR1)/*.[od] $(ODIR2)/*.[od]


# Zaimoni.STL header & library builds
.PHONY: libs clean_libs
libs:
	make -C Zaimoni.STL/Pure.C/ host_install
	make -C Zaimoni.STL/Pure.C/stdio.log/ host_install

clean_libs:
	rm -f lib/host/*.a
	make -C Zaimoni.STL/Pure.C/ clean
	make -C Zaimoni.STL/Pure.C/stdio.log/ clean


-include $(OBJS1:.o=.d)
-include $(OBJS2:.o=.d)
