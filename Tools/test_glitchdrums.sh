#!/bin/sh
set -eu
python3 Tools/build_glitchdrums.py
build/glitchdrums/benchmark build/glitchdrums/stock.pcm
node Tests/glitchdrums_port.cjs
