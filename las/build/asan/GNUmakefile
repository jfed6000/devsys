# Memory-checking build: AddressSanitizer catches reads/writes past the end
# of an allocation (e.g. past a symbol's real name length). Slow; for testing.
include ../unix/GNUmakefile

CFLAGS += -fsanitize=address -fno-omit-frame-pointer
LFLAGS += -fsanitize=address
