PREFIX ?= /usr/local

INCS = `pkg-config --cflags yajl`

LIBS = `pkg-config --libs yajl`

WARN = \
	-Wall -Wextra -Werror \
	-Wno-error=unused-variable \
	-Wno-error=unused-parameter \
	-Wno-override-init \
	-Wshadow \
	-Wformat=2 \
	-Wundef

CFLAGS := $(INCS) -Os $(CFLAGS)
LDFLAGS := $(LIBS) $(CFLAGS)

CC ?= cc
