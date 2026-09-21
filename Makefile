CC ?= cc
WAYLAND_SCANNER ?= wayland-scanner

TARGET := rogdrop
GEN_DIR := protocols/generated

PKGS := wayland-client xkbcommon

WAYLAND_PROTOCOLS ?= $(shell pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null)
WLR_PROTOCOLS ?= $(shell pkg-config --variable=pkgdatadir wlr-protocols 2>/dev/null)

EXT_SOURCE_XML := $(WAYLAND_PROTOCOLS)/staging/ext-image-capture-source/ext-image-capture-source-v1.xml
EXT_COPY_XML := $(WAYLAND_PROTOCOLS)/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml
EXT_TOPLEVEL_XML := $(WAYLAND_PROTOCOLS)/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
XDG_SHELL_XML := $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
LAYER_SHELL_XML := $(WLR_PROTOCOLS)/unstable/wlr-layer-shell-unstable-v1.xml
WLR_COPY_XML := $(WLR_PROTOCOLS)/unstable/wlr-screencopy-unstable-v1.xml

EXT_SOURCE_H := $(GEN_DIR)/ext-image-capture-source-v1-client-protocol.h
EXT_SOURCE_C := $(GEN_DIR)/ext-image-capture-source-v1-protocol.c
EXT_COPY_H := $(GEN_DIR)/ext-image-copy-capture-v1-client-protocol.h
EXT_COPY_C := $(GEN_DIR)/ext-image-copy-capture-v1-protocol.c
EXT_TOPLEVEL_H := $(GEN_DIR)/ext-foreign-toplevel-list-v1-client-protocol.h
EXT_TOPLEVEL_C := $(GEN_DIR)/ext-foreign-toplevel-list-v1-protocol.c
XDG_SHELL_H := $(GEN_DIR)/xdg-shell-client-protocol.h
XDG_SHELL_C := $(GEN_DIR)/xdg-shell-protocol.c
LAYER_SHELL_H := $(GEN_DIR)/wlr-layer-shell-unstable-v1-client-protocol.h
LAYER_SHELL_C := $(GEN_DIR)/wlr-layer-shell-unstable-v1-protocol.c
WLR_COPY_H := $(GEN_DIR)/wlr-screencopy-unstable-v1-client-protocol.h
WLR_COPY_C := $(GEN_DIR)/wlr-screencopy-unstable-v1-protocol.c

PROTO_HEADERS := \
	$(EXT_SOURCE_H) \
	$(EXT_COPY_H) \
	$(EXT_TOPLEVEL_H) \
	$(XDG_SHELL_H) \
	$(LAYER_SHELL_H) \
	$(WLR_COPY_H)

PROTO_CODE := \
	$(EXT_SOURCE_C) \
	$(EXT_COPY_C) \
	$(EXT_TOPLEVEL_C) \
	$(XDG_SHELL_C) \
	$(LAYER_SHELL_C) \
	$(WLR_COPY_C)

SRC := \
	src/main.c \
	src/wayland.c \
	src/input.c \
	src/capture.c \
	src/overlay.c \
	src/shm.c

OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)
PROTO_OBJ := $(PROTO_CODE:.c=.o)

CPPFLAGS += -I$(GEN_DIR) $(shell pkg-config --cflags $(PKGS))
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wshadow -MMD -MP
LDLIBS += $(shell pkg-config --libs $(PKGS))

.PHONY: all clean run check

all: check $(TARGET)

check:
	@test -n "$(WAYLAND_PROTOCOLS)" || \
		(echo "WAYLAND_PROTOCOLS is unset and wayland-protocols was not found via pkg-config" >&2; exit 1)
	@test -n "$(WLR_PROTOCOLS)" || \
		(echo "WLR_PROTOCOLS is unset. The included shell.nix sets it explicitly." >&2; exit 1)
	@pkg-config --exists $(PKGS) || \
		(echo "Missing build dependency: $(PKGS)" >&2; exit 1)
	@test -f "$(EXT_SOURCE_XML)" || (echo "Missing $(EXT_SOURCE_XML)" >&2; exit 1)
	@test -f "$(EXT_COPY_XML)" || (echo "Missing $(EXT_COPY_XML)" >&2; exit 1)
	@test -f "$(LAYER_SHELL_XML)" || (echo "Missing $(LAYER_SHELL_XML)" >&2; exit 1)

$(TARGET): $(OBJ) $(PROTO_OBJ)
	$(CC) $(OBJ) $(PROTO_OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.c $(PROTO_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(GEN_DIR)/%.o: $(GEN_DIR)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(GEN_DIR):
	mkdir -p $@

$(EXT_SOURCE_H): $(EXT_SOURCE_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(EXT_SOURCE_C): $(EXT_SOURCE_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(EXT_COPY_H): $(EXT_COPY_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(EXT_COPY_C): $(EXT_COPY_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(EXT_TOPLEVEL_H): $(EXT_TOPLEVEL_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(EXT_TOPLEVEL_C): $(EXT_TOPLEVEL_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(XDG_SHELL_H): $(XDG_SHELL_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(XDG_SHELL_C): $(XDG_SHELL_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(LAYER_SHELL_H): $(LAYER_SHELL_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(LAYER_SHELL_C): $(LAYER_SHELL_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(WLR_COPY_H): $(WLR_COPY_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(WLR_COPY_C): $(WLR_COPY_XML) | $(GEN_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJ) $(DEP) $(PROTO_OBJ) $(PROTO_CODE) $(PROTO_HEADERS)

-include $(DEP)
