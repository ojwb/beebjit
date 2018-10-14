#!/bin/sh
set -e

gcc -Wall -W -Werror -g -o 6502jit \
    main.c bbc.c jit.c defs_6502.c x.c debug.c util.c state.c video.c via.c \
    emit_6502.c interp.c state_6502.c test.c \
    -lX11 -lXext -lpthread
gcc -Wall -W -Werror -g -o make_test_rom make_test_rom.c \
    util.c defs_6502.c emit_6502.c
gcc -Wall -W -Werror -g -o make_perf_rom make_perf_rom.c
./make_test_rom

echo 'Running JIT, debug.'
./6502jit -os test.rom -lang '' -opt jit:self-mod-all -d -r
echo 'Running built-in tests.'
./6502jit -t
echo 'Running JIT, opt.'
./6502jit -os test.rom -lang '' -opt jit:self-mod-all
echo 'Running JIT, opt, no-batch-ops.'
./6502jit -os test.rom -lang '' -opt jit:self-mod-all,jit:no-batch-ops
echo 'Running interpreter.'
./6502jit -os test.rom -mode interp

echo 'All is well!'
