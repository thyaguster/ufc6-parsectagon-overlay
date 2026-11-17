import sys
sys.path.insert(0,'scripts')
from decode_fight_stats_blob import read
pid=22300
for target in (0x10f3e5256e,0x10f518c4cf,0x10f638a6be,0xfc8305256e,0xfc8438c4cf,0xfc8558a6be):
 try:b=read(pid,target-65536,131072)
 except Exception as ex:print(hex(target),'read error');continue
 print(hex(target),[(p.decode('ascii','replace'),b.count(p)) for p in (b'\x08Success\x00',b'\x08Output\x00',b'\x08RED_CORNER_FIGHTER\x00',b'\x08ROUNDS\x00',b'\x08TOTAL\x00')])
