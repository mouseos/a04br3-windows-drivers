#!/usr/bin/env python3
"""照合用共有ライブラリを引数で受け、全1968〜2095年の日付を検証する。"""
import ctypes,datetime,sys
class Raw(ctypes.Structure):
    _fields_=[(s,ctypes.c_uint16) for s in ('sec','min','hour','day','dow','month','year')]
class Civil(ctypes.Structure):
    _fields_=[(s,ctypes.c_uint16) for s in ('year','month','day','hour','min','sec','weekday')]
lib=ctypes.CDLL(sys.argv[1]);decode=lib.mt6392_rtc_decode
decode.argtypes=[ctypes.POINTER(Raw),ctypes.POINTER(Civil)];decode.restype=ctypes.c_int
valid=invalid=0
for y in range(1968,2096):
 for m in range(1,13):
  for d in range(0,33):
   r=Raw(59,59,23,d,0xffff,m,y-1968);out=Civil(*([0xa5a5]*7));before=bytes(out)
   try:dt=datetime.date(y,m,d)
   except ValueError:
    assert decode(ctypes.byref(r),ctypes.byref(out))==-2 and bytes(out)==before
    invalid+=1;continue
   assert decode(ctypes.byref(r),ctypes.byref(out))==0
   assert tuple(getattr(out,s) for s,_ in out._fields_)==(y,m,d,23,59,59,(dt.weekday()+1)%7)
   valid+=1
for name,n in [('sec',60),('min',60),('hour',24),('month',0),('month',13),('year',128),('year',65535)]:
 r=Raw(0,0,0,1,0,1,56);setattr(r,name,n);out=Civil(*([0xa5a5]*7));before=bytes(out)
 assert decode(ctypes.byref(r),ctypes.byref(out))==-2 and bytes(out)==before
assert decode(None,ctypes.byref(Civil()))==-2
assert decode(ctypes.byref(Raw()),None)==-2
print(f'MT6392 RTC: {valid} valid dates / {invalid} impossible dates + field bounds PASS')
