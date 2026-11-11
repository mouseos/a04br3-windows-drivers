/* b9fb9157 writer/reader contract. Fixed dynamic child only; root guards stay strict.
 * Missing is unobserved. Well-shaped older epochs are stale, never current success. */
#if A04_VIDPN_CALLBACK_TRACE
static DWORD vidpn_callback_trace(void)
{
 HKEY key;DWORD e,type,bytes,n,index,children,epoch[108],again[108],r[108],kind,slot;WCHAR name[32];
 e=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\VidPnCallbackTraceV1",REG_OPTION_OPEN_LINK,KEY_READ,&key);
 if(e==ERROR_FILE_NOT_FOUND)return 0;
 if(e)return e;
 bytes=sizeof(epoch);e=RegQueryValueExW(key,L"EpochV1",NULL,&type,(BYTE *)epoch,&bytes);
 if(e)goto done;
 if(type!=REG_BINARY||bytes!=sizeof(epoch)){e=ERROR_INVALID_DATA;goto done;}
 if(epoch[0]!=1||epoch[1]!=432||epoch[2]!=0||epoch[4]>1||epoch[7]!=1||!epoch[6]||(!epoch[10]&&!epoch[11])||epoch[20]!=7||epoch[21]!=4||epoch[22]!=0){e=ERROR_INVALID_DATA;goto done;}
 e=RegQueryInfoKeyW(key,NULL,NULL,NULL,&children,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
 if(e)goto done;if(children){e=ERROR_INVALID_DATA;goto done;}
 for(index=0;index<45;index++){
  n=32;bytes=sizeof(r);e=RegEnumValueW(key,index,name,&n,NULL,&type,(BYTE *)r,&bytes);
  if(e==ERROR_NO_MORE_ITEMS){e=0;break;}if(e)goto done;
  if(!lstrcmpW(name,L"EpochV1"))continue;
  if(n!=4||name[0]!=L'C'||name[1]<L'1'||name[1]>L'7'||(name[2]!=L'S'&&name[2]!=L'F'&&name[2]!=L'L'&&name[2]!=L'U')||name[3]<L'0'||name[3]>L'3'||(name[2]!=L'S'&&name[3]!=L'0')){e=ERROR_INVALID_DATA;goto done;}
  if(type!=REG_BINARY||bytes!=sizeof(r)){e=ERROR_INVALID_DATA;goto done;}
  kind=name[1]-L'0';slot=name[3]-L'0';
  if(r[0]!=1||r[1]!=432||r[2]!=kind||r[4]>1||r[7]!=1||(name[2]==L'S'&&r[3]!=slot)||(name[2]==L'U'&&(kind!=1||r[4]!=1||r[5]!=0||r[51]!=0))||(name[2]==L'F'&&(r[4]!=1||r[5]<0x80000000u))){e=ERROR_INVALID_DATA;goto done;}
  /* r[6], r[8..11] bind sequence and both epochs. Stale remains permitted
   * as in the exact offline reader; it proves nothing about this start. */
  if(r[6]!=epoch[6]||memcmp(r+8,epoch+8,4*sizeof(DWORD)))continue;
 }
 if(index==45){e=ERROR_INVALID_DATA;goto done;}
 bytes=sizeof(again);e=RegQueryValueExW(key,L"EpochV1",NULL,&type,(BYTE *)again,&bytes);
 if(!e&&(type!=REG_BINARY||bytes!=sizeof(again)||memcmp(epoch,again,sizeof(epoch))))e=ERROR_RETRY;
 done:RegCloseKey(key);return e;
}
#endif
