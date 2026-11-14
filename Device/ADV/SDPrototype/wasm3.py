Import("env")
from pathlib import Path
root=Path(env.subst("$PROJECT_DIR")).parents[2]
source=root/"Vendor/wasm3"
env.Append(CPPPATH=[str(source)])
names=["m3_"+name+".c" for name in "bind code compile core emit env exec function info module optimize parse".split()]+["gs_sandbox.c"]
env.BuildSources("$BUILD_DIR/wasm3",str(source),src_filter=["-<*>"]+["+<"+name+">" for name in names])
