# eBPF Programs Makefile

CLANG ?= clang
BPFOOL ?= bpftool
LIBBPF_DIR ?= /usr/include/bpf
BUILD_DIR ?= build
VMLINUX_H := vmlinux.h
VMLINUX_BTF ?= /sys/kernel/btf/vmlinux

TARGETS := xdp_drop syscall_trace socket_filter cgroup_egress
BPF_OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .bpf.o,$(TARGETS)))
LOADERS := $(addprefix $(BUILD_DIR)/,$(addsuffix _loader,$(TARGETS)))

BPF_CFLAGS := -I$(CURDIR) -I$(LIBBPF_DIR) -O2 -g -target bpf
USER_CFLAGS := -I$(CURDIR) -I$(LIBBPF_DIR) -O2

all: $(BUILD_DIR) $(VMLINUX_H) $(BPF_OBJS) $(LOADERS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(VMLINUX_H):
	@test -r "$(VMLINUX_BTF)" || (echo "Error: missing $(VMLINUX_BTF); kernel BTF is required for CO-RE." && exit 1)
	@command -v $(BPFOOL) >/dev/null 2>&1 || (echo "Error: bpftool is required to generate $(VMLINUX_H)." && exit 1)
	@echo "Generating CO-RE header: $@"
	$(BPFOOL) btf dump file "$(VMLINUX_BTF)" format c > "$@"

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(VMLINUX_H)

# eBPF object compilation (kernel-side)
define EXAMPLE_RULES
$(BUILD_DIR)/$(1).bpf.o: $(1)/$(1).bpf.c $(VMLINUX_H) | $(BUILD_DIR)
	@echo "Compiling CO-RE program: $$<"
	$(CLANG) $(BPF_CFLAGS) -c $$< -o $$@

$(BUILD_DIR)/$(1)_loader: $(1)/$(1).c $(BUILD_DIR)/$(1).bpf.o logging/log_utils.c logging/log_utils.h | $(BUILD_DIR)
	@echo "Linking user loader: $$<"
	gcc $(USER_CFLAGS) $$< logging/log_utils.c -o $$@ -lbpf -lelf -lz -pthread
endef

$(foreach target,$(TARGETS),$(eval $(call EXAMPLE_RULES,$(target))))

# Helper target to view eBPF bytecode
dump-%: $(BUILD_DIR)/%.bpf.o
	llvm-objdump -d $<

.PHONY: all clean dump-%
