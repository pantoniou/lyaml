.PHONY: all install check

all:
	./build-aux/luke

install:
	./build-aux/luke PREFIX=/here install

check:
	specl -v1freport spec/*_spec.yaml
