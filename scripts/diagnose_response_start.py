import sys
sys.path.insert(0,'scripts')
from decode_fight_stats_blob import read,decode
pid=22300
for target in (0x10f3e5256e,0x10f518c4cf,0x10f58b1658,0x10f638a6be,0xfc8305256e,0xfc8438c4cf,0xfc8558a6be,0xfd352720a8):
 try:b=read(pid,target-65536,131072)
 except Exception as ex:print(hex(target),'read err',ex);continue
 off=65536;pat=b'\x08Success\x00\x02\x01\x08Output\x00\x01';pos=b.rfind(pat,0,off)
 if pos<0: print(hex(target),'no start');continue
 try:r,n=decode(b[pos:]);print(hex(target),'distance',off-pos,'size',n,'keys',list(r.get('Output',{}).keys())[:8])
 except Exception as ex: print(hex(target),'distance',off-pos,'decode error',str(ex)[:60])
