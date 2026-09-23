# ww3-lab GPU sandbox
#
#   make            build everything for the GPU (cc89 = RTX 4090 / Ada)
#   make cpu        same sources, -acc=multicore, for correctness checking
#   make run        build and run everything
#   make info       what does your machine actually have?
#
# Override the target architecture if you are not on a 4090:
#   make CC_ARCH=cc80     (A100, RTX 3090)
#   make CC_ARCH=cc90     (H100)
#   make CC_ARCH=cc120    (Blackwell)

FC      ?= nvfortran
CC_ARCH ?= cc89
OPT     ?= -O3

ACCFLAGS = $(OPT) -acc -gpu=$(CC_ARCH) -Minfo=accel
CPUFLAGS = $(OPT) -acc=multicore
DCFLAGS  = $(OPT) -stdpar=gpu -gpu=$(CC_ARCH) -Minfo=stdpar

GPU_TARGETS = 00_hello_acc 01_dispersion 03_precision
DC_TARGET   = 02_do_concurrent

.PHONY: all cpu run info clean

all: $(GPU_TARGETS) $(DC_TARGET)

$(GPU_TARGETS): %: %.f90
	$(FC) $(ACCFLAGS) -o $@ $<

02_do_concurrent: 02_do_concurrent.f90
	$(FC) $(DCFLAGS) -o $@ $<

cpu: 01_dispersion.f90
	$(FC) $(CPUFLAGS) -o 01_dispersion_cpu 01_dispersion.f90
	@echo
	@echo "Built 01_dispersion_cpu with the SAME directives on CPU threads."
	@echo "Run both. If the numbers differ, you have a data-movement bug,"
	@echo "not a physics bug. This is the highest-value debugging trick in OpenACC."

info:
	@echo "== nvaccelinfo ==" ; nvaccelinfo || echo "  not found -- is the HPC SDK on PATH?"
	@echo ; echo "== nvidia-smi ==" ; nvidia-smi || true
	@echo ; echo "== compiler ==" ; $(FC) --version || true

run: all
	@for t in $(GPU_TARGETS) $(DC_TARGET); do \
	  echo "=== $$t ==============================="; \
	  NVCOMPILER_ACC_NOTIFY=1 ./$$t; echo; \
	done

clean:
	rm -f $(GPU_TARGETS) $(DC_TARGET) 01_dispersion_cpu *.mod *.o
