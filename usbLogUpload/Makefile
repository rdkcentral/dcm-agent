##############################################################################
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##############################################################################

# Makefile for USB Log Upload module

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -Iinclude
LDFLAGS = 

# Directories
SRCDIR = src
INCDIR = include
TESTDIR = src/test
OBJDIR = obj
BINDIR = bin

# Target executable
TARGET = usblogupload

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(OBJDIR)/%.o)
TEST_TARGET = test_$(TARGET)

# Default target
all: $(BINDIR)/$(TARGET)

# Create directories
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BINDIR):
	@mkdir -p $(BINDIR)

# Build main executable
$(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build tests
test: $(BINDIR)/$(TEST_TARGET)

$(BINDIR)/$(TEST_TARGET): $(TEST_OBJECTS) $(filter-out $(OBJDIR)/usb_log_main.o, $(OBJECTS)) | $(BINDIR)
	$(CC) $(TEST_OBJECTS) $(filter-out $(OBJDIR)/usb_log_main.o, $(OBJECTS)) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(TESTDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Install target
install: $(BINDIR)/$(TARGET)
	install -D $(BINDIR)/$(TARGET) $(DESTDIR)/usr/bin/$(TARGET)

# Phony targets
.PHONY: all test clean install