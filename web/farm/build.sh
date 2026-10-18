#!/bin/sh
# Builds the native sound engine (Packages/AWCore/Sources/AWDSP) to WebAssembly for the Farm web build.
# Needs clang 18 with the wasm32-wasi sysroot (Ubuntu: wasi-libc libc++-18-dev-wasm32 libclang-rt-18-dev-wasm32).
set -e
here=$(cd "$(dirname "$0")" && pwd)
dsp="$here/../../Packages/AWCore/Sources/AWDSP"
exports=""
for f in create destroy render set_location play play_count_in stop set_context cue cancel_cue audition live_on live_off \
         metronome add_sample take_ticks take_notes playhead; do
  exports="$exports,--export=aw_engine_$f"
done
clang++-18 --target=wasm32-wasi --sysroot=/usr -O2 -std=c++20 -fno-exceptions -DNDEBUG \
  -I "$dsp/include" -I "$dsp" -mexec-model=reactor \
  -Wl,--export=malloc,--export=free$exports \
  "$dsp"/Core/*.cpp "$dsp"/DSP/*.cpp "$dsp"/Modules/*.cpp "$here/wasm-shim.cpp" -lc++abi \
  -o "$here/awdsp.wasm"
echo "built $here/awdsp.wasm ($(wc -c < "$here/awdsp.wasm") bytes)"
