/* SPDX-License-Identifier: MIT */
#ifndef GEO_UMD_ADAPTER_INFO_TRACE_H
#define GEO_UMD_ADAPTER_INFO_TRACE_H
/* Failure-only bounded numeric snapshot shared by both UMD entrypoints.
 * Uses the existing collected umd-*.log namespace; never records pointers,
 * shader/application data, identifiers from a network, or invented values.
 * Trace failure does not change the OpenAdapter return or relax validation. */
static void geo_umd_wire_trace(const struct geo_adapter_memory_info *p,uint32_t mask) {
 static volatile LONG serial;static const WCHAR prefix[]=L"C:\\MT8167D3DClear\\umd-wire-";
 static const WCHAR digits[]=L"0123456789abcdef";
 static const char *keys[]={"WIRE_FAILURE_MASK","WIRE_SIZE","WIRE_VERSION","WIRE_MAGIC","WIRE_OWNER_OPS_VERSION","WIRE_CONTROL_VERSION","WIRE_TEXTURE_DRAW_VERSION","WIRE_SHADER_DRAW_VERSION","WIRE_BVNC_LOW","WIRE_BVNC_HIGH","WIRE_DRAW_VERSION","WIRE_FORMAT","WIRE_VERTEX_MAX_BYTES"};
 uint32_t values[13];WCHAR name[112];char text[640];UINT i,j,n,at=0,attempt;DWORD written;HANDLE file=INVALID_HANDLE_VALUE;
 values[0]=mask;values[1]=p->size;values[2]=p->version;values[3]=p->magic;values[4]=p->owner_ops_version;values[5]=p->control_version;
 values[6]=p->texture_draw_version;values[7]=p->shader_draw_version;values[8]=(uint32_t)p->compiler_bvnc;values[9]=(uint32_t)(p->compiler_bvnc>>32);
 values[10]=p->draw_version;values[11]=p->format;values[12]=p->vertex_max_bytes;
 CreateDirectoryW(L"C:\\MT8167D3DClear",NULL);
 for(attempt=0;attempt<4;attempt++){
  uint32_t id=GetCurrentProcessId(),tick=GetTickCount()^(uint32_t)InterlockedIncrement(&serial);
  for(n=0;prefix[n];n++)name[n]=prefix[n];
  for(j=0;j<8;j++)name[n++]=digits[(id>>(28-4*j))&15];name[n++]=L'-';
  for(j=0;j<8;j++)name[n++]=digits[(tick>>(28-4*j))&15];name[n++]=L'.';name[n++]=L'l';name[n++]=L'o';name[n++]=L'g';name[n]=0;
  file=CreateFileW(name,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
  if(file!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_FILE_EXISTS)break;
 }
 if(file==INVALID_HANDLE_VALUE)return;
 for(i=0;i<13;i++){
  const char *key=keys[i];while(*key)text[at++]=*key++;text[at++]='=';
  for(j=0;j<8;j++)text[at++]=(char)digits[(values[i]>>(28-4*j))&15];text[at++]='\r';text[at++]='\n';
 }
 if(WriteFile(file,text,at,&written,NULL)&&written==at)(void)FlushFileBuffers(file);CloseHandle(file);
}
#endif
