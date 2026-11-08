#include <stdint.h>
#include <string.h>
#include <wchar.h>
typedef void *PVOID; typedef uint32_t ULONG,*PULONG; typedef int32_t LONG,NTSTATUS; typedef uint8_t BOOLEAN; typedef wchar_t WCHAR;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_NO_SUCH_DEVICE (-1)
#define STATUS_INVALID_DEVICE_REQUEST (-2)
#define STATUS_BUFFER_TOO_SMALL (-3)
#define STATUS_NOT_SUPPORTED (-4)
#define NT_SUCCESS(x) ((x)>=0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define RtlZeroMemory(p,n) memset((p),0,(n))
#define RtlCopyMemory(d,s,n) memcpy((d),(s),(n))
