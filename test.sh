#!/bin/bash
# full test sequence, you can put your own test sequence here
export REGRESS="bytea_unzip";
make clean $@;
make $@;
make check $@ | tee make_check.out;
export REGRESS=;
