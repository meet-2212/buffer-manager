# Compiler
CC = gcc
CFLAGS = -w

# Source files
SRCS = dberror.c storage_mgr.c buffer_mgr.c buffer_mgr_stat.c test_assign2_1.c
SRCS_CLOCK = dberror.c storage_mgr.c buffer_mgr.c buffer_mgr_stat.c test_assign2_2.c

# Output binaries
TEST1 = test_assign2_1
TEST2 = test_assign2_2

# Default target
all: $(TEST1) $(TEST2)

# Build the main test binary
$(TEST1): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TEST1)
	./$(TEST1)

# Build the CLOCK test binary
$(TEST2): $(SRCS_CLOCK)
	$(CC) $(CFLAGS) $(SRCS_CLOCK) -o $(TEST2)
	./$(TEST2)

# Clean up generated files
clean:
	$(RM) $(TEST1) $(TEST2)

