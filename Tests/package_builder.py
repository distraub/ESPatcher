import copy,importlib.util,json,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('builder',root/'Tools/build_instruments.py');builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
source=root/'Library/Definitions/amsynth.json';base=json.loads(source.read_text())
assert builder.pack(base,source.parent)==(root/'Library/amsynth.espkg').read_bytes()
for mutate in [lambda d:d.update(id='../bad'),lambda d:d['engines'][0].update(id='wrong_engine'),lambda d:d['engines'][0]['defaults'].__setitem__(3,float('nan')),lambda d:d['engines'][0]['defaults'].__setitem__(8,1),lambda d:d['engines'][0]['scenes'][0]['bindings'].__setitem__(0,19),lambda d:d['engines'][0]['scenes'][0]['pads'].__setitem__(0,49),lambda d:d['engines'][0]['scenes'][0].update(layout='script')]:
    data=copy.deepcopy(base);mutate(data)
    try:builder.pack(data,source.parent)
    except (ValueError,UnicodeError):pass
    else:raise AssertionError('Invalid package accepted')
print('PASS package builder: deterministic bytes, identities/manifest agreement, defaults, transport, bindings and pads')
