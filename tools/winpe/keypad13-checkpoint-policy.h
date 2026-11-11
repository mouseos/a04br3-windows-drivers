/* Fixed driver1.3/opt-in1.4 dynamic child; no service/root fingerprint exemption. */
#if A04_KEYPAD13_CHECKPOINT
static DWORD keypad13_checkpoint(void)
{
 HKEY key;DWORD e,type,bytes,index,epoch[4],again[4],r[20],n,i,stage,kind;WCHAR name[32];
 e=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Services\\Mt6392Keys\\Parameters\\NativeCheckpoint",REG_OPTION_OPEN_LINK,KEY_READ,&key);
 if(e==ERROR_FILE_NOT_FOUND)return 0; /* Writer absent is unobserved, not success. */
 if(e)return e;
 bytes=sizeof(epoch);e=RegQueryValueExW(key,L"LoadEpochV1",NULL,&type,(BYTE *)epoch,&bytes);
 if(e==ERROR_FILE_NOT_FOUND){DWORD values=0; e=RegQueryInfoKeyW(key,NULL,NULL,NULL,NULL,NULL,NULL,&values,NULL,NULL,NULL,NULL);if(!e&&values)e=ERROR_INVALID_DATA;goto done;}
 if(e)goto done;
 if(type!=REG_BINARY||bytes!=sizeof(epoch)||epoch[0]!=0x31424b41u||epoch[1]!=1||(!epoch[2]&&!epoch[3])){e=ERROR_INVALID_DATA;goto done;}
#if A04_KEYPAD14_CHECKPOINT
 const DWORD maximum_stage=31,maximum_values=33;
#else
 const DWORD maximum_stage=24,maximum_values=26;
#endif
 for(index=0;index<maximum_values;index++){
  n=32;bytes=sizeof(r);e=RegEnumValueW(key,index,name,&n,NULL,&type,(BYTE *)r,&bytes);
  if(e==ERROR_NO_MORE_ITEMS){e=0;break;}if(e)goto done;
  if(!lstrcmpW(name,L"LoadEpochV1"))continue;
  if(n!=3||name[0]!=L'B'||name[1]<L'0'||name[1]>L'3'||name[2]<L'0'||name[2]>L'9'){e=ERROR_INVALID_DATA;goto done;}
  stage=(name[1]-L'0')*10+name[2]-L'0';
  kind=stage==25?0:(stage==26||stage==27)?2:stage==13?3:(stage==11||stage==14)?2:(stage==1||stage==3||stage==5||stage==6||stage==7||stage==10)?0:1;
  if(type!=REG_BINARY||bytes!=sizeof(r)||stage<1||stage>maximum_stage){e=ERROR_INVALID_DATA;goto done;}
  if(r[0]!=epoch[0]||r[1]!=1||r[2]!=sizeof(r)||r[3]!=stage){e=ERROR_INVALID_DATA;goto done;}
  if(r[4]!=kind||(kind==0&&r[5])||(kind==2&&r[5]>1)){e=ERROR_INVALID_DATA;goto done;}
#if A04_KEYPAD14_CHECKPOINT
  if(stage>=25&&stage<=27){
   if(r[6]>0xffffu||r[7]>0xffffu||(r[8]&~31u)||!(r[8]&16u)||((r[8]&2u)&&!(r[8]&1u))||((r[8]&8u)&&!(r[8]&4u))||(r[10]&~0x000f030fu)||(r[11]&~0x000f030fu)){e=ERROR_INVALID_DATA;goto done;}
   if((stage==26&&(r[5]!=1||!(r[10]&256u)||!(r[8]&2u)))||(stage==27&&(r[5]!=1||!(r[11]&256u)||!(r[8]&8u)))){e=ERROR_INVALID_DATA;goto done;}
  }else if(stage>=28){
   if(r[5]||r[6]!=stage-27||r[7]!=1||r[8]!=2||r[9]!=3||r[10]||r[11]){e=ERROR_INVALID_DATA;goto done;}
  }
#endif
  for(i=16;i<20;i++)if(r[i]){e=ERROR_INVALID_DATA;goto done;}
  if((!r[12]&&!r[13])||r[13]>epoch[3]||(r[13]==epoch[3]&&r[12]>epoch[2])||r[15]<r[13]||(r[15]==r[13]&&r[14]<r[12])){e=ERROR_INVALID_DATA;goto done;}
  /* Older, structurally valid records are stale. Never interpret them as this epoch. */
  if(r[12]!=epoch[2]||r[13]!=epoch[3])continue;
 }
 if(index==maximum_values){e=ERROR_INVALID_DATA;goto done;}
 {DWORD children=0;e=RegQueryInfoKeyW(key,NULL,NULL,NULL,&children,NULL,NULL,NULL,NULL,NULL,NULL,NULL);if(!e&&children)e=ERROR_INVALID_DATA;}
 if(e)goto done;
 bytes=sizeof(again);e=RegQueryValueExW(key,L"LoadEpochV1",NULL,&type,(BYTE *)again,&bytes);
 if(!e&&(type!=REG_BINARY||bytes!=sizeof(again)||memcmp(epoch,again,sizeof(epoch))))e=ERROR_RETRY;
 done:RegCloseKey(key);return e;
}
#endif
