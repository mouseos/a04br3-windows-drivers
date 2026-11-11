/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Normalize ONLY two proven installer-owned fields AFTER successful target DIF.
 * The full original root digest still checks all other values, including extras.
 */
#ifndef A04_FILTER_ROOT_TRANSITION_COUNT
#define A04_FILTER_ROOT_TRANSITION_COUNT 0u
#endif
static DWORD filter_root_completed_mask;
static DWORD filter_root_normalize(const WCHAR *service,const WCHAR *name,
    DWORD *type,DWORD *bytes,const BYTE *data,BYTE digest[32])
{
#if A04_FILTER_ROOT_TRANSITION_COUNT
 DWORD i,j,n,at,end,seen;const WCHAR *s=(const WCHAR *)data;
 for(i=0;i<A04_FILTER_ROOT_TRANSITION_COUNT;i++){
  const struct filter_root_transition *t=&filter_root_transitions[i];
  if(lstrcmpiW(service,t->service)||!(filter_root_completed_mask&(1u<<t->target)))continue;
  if(!lstrcmpiW(name,L"DisplayName")){
   n=(lstrlenW(t->display)+1u)*sizeof(WCHAR);
   if(*type!=REG_SZ||*bytes!=n||memcmp(data,t->display,n))return ERROR_REVISION_MISMATCH;
   *bytes=t->display_bytes;memcpy(digest,t->display_hash,32);return 0;
  }
  if(!lstrcmpiW(name,L"Owners")){
   if(*type!=REG_MULTI_SZ||*bytes<4||(*bytes&1))return ERROR_REVISION_MISMATCH;
   n=*bytes/2u;if(s[n-1]||s[n-2])return ERROR_REVISION_MISMATCH;
   for(at=0,seen=0;at<n-1;at=end+1){
    for(end=at;end<n&&s[end];end++);
    if(end==at||end>=n)return ERROR_REVISION_MISMATCH;
    for(j=0;j<t->owner_count;j++)if(!lstrcmpiW(s+at,t->owners[j]))break;
    if(j==t->owner_count||(seen&(1u<<j)))return ERROR_REVISION_MISMATCH;
    seen|=1u<<j;
   }
   if(at!=n-1||seen!=(t->owner_count==32?0xffffffffu:((1u<<t->owner_count)-1u)))return ERROR_REVISION_MISMATCH;
   *bytes=t->owners_bytes;memcpy(digest,t->owners_hash,32);return 0;
  }
 }
#endif
 return 0;
}
