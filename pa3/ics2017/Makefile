-include nemu/Makefile.git

default:
	@echo "Please run 'make' under any subprojects to compile."

COUNT_PATHS := 'nemu/*.c' 'nemu/*.h'
CMP_ARGS := $(filter-out cmp,$(MAKECMDGOALS))
ifneq ($(filter cmp,$(MAKECMDGOALS)),)
ifneq ($(word 1,$(CMP_ARGS)),)
n := $(word 1,$(CMP_ARGS))
endif
ifneq ($(word 2,$(CMP_ARGS)),)
m := $(word 2,$(CMP_ARGS))
endif
ifeq ($(strip $(n)),)
CURRENT_PA_N := $(shell git rev-parse --abbrev-ref HEAD 2>/dev/null | sed -n 's/^pa\([0-9][0-9]*\)$$/\1/p')
ifneq ($(CURRENT_PA_N),)
n := $(CURRENT_PA_N)
else
$(error 没有指名 n，且当前分支名不是 pa数字；请使用 make cmp <n> [m] 或 make cmp n=<n> [m=<m>])
endif
endif
endif
m ?= 0
 
clean:
	-$(MAKE) -C nemu clean
	-$(MAKE) -C nexus-am clean
	-$(MAKE) -C nanos-lite clean
	-$(MAKE) -C navy-apps clean

count:
	@echo "total lines:"
	@git ls-files -z $(COUNT_PATHS) | xargs -0 cat | wc -l
	@echo "nonblank lines:"
	@git ls-files -z $(COUNT_PATHS) | xargs -0 grep -hEv '^[[:space:]]*$$' | wc -l

cmp:
	@echo "compare: pa$(n) relative to pa$(m)"
	@git diff --numstat pa$(m) pa$(n) -- $(COUNT_PATHS) | awk '{ add += $$1; del += $$2 } END { printf "added: %d\ndeleted: %d\nnet: %d\n", add, del, add - del }'

submit: clean
	git gc
	cd .. && tar cj $(shell basename `pwd`) > $(STU_ID).tar.bz2

ifneq ($(filter cmp,$(MAKECMDGOALS)),)
ifneq ($(CMP_ARGS),)
$(CMP_ARGS):
	@:
endif
endif

.PHONY: default clean count cmp submit $(CMP_ARGS)
