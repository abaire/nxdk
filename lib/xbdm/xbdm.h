#ifndef __XBDM_H__
#define __XBDM_H__

#include <processthreadsapi.h>
#include <xboxkrnl/xboxkrnl.h>
#include <xbdm/xbdm_err.h>

typedef void *PDMN_MODLOAD;        // FIXME: !!!
typedef void *PDMN_SESSION;        // FIXME: !!!
typedef void *PDM_NOTIFY_FUNCTION; // FIXME: !!!

#define DM_PERSISTENT 1
#define DM_MODLOAD 5

// All DXT targets must have exactly one DXT_ENTRY declaration as an entrypoint
#define DXT_ENTRY(pfUnloadParamName)                                               \
  int main() {                                                                 \
    (void)KeTickCount;                                                         \
    return 0;                                                                  \
  }                                                                            \
  void DxtEntry(ULONG * pfUnloadParamName)

typedef struct _DM_CMDCONT *PDM_CMDCONT;

typedef HRESULT(__stdcall *PDM_CMDCONTPROC)(PDM_CMDCONT pdmcc, LPSTR szResponse,
                                            DWORD cchResponse);

//! Contains contextual information used when a debug command processor needs to
//! do something more than immediately reply with a simple, short response.
typedef struct _DM_CMDCONT {
  //! Function to be invoked to actually send or receive data. This function
  //! will be called repeatedly until `bytes_remaining` is set to 0.
  PDM_CMDCONTPROC HandlingFunction;
  //! When receiving data, this will be set to the number of bytes in `buffer`
  //! that contain valid received data. When sending data, this must be set by
  //! the debug processor to the number of valid bytes in `buffer` that should
  //! be sent.
  DWORD DataSize;
  //! Buffer used to hold send/receive data. XBDM will create a small buffer by
  //! default, this can be reassigned to an arbitrary buffer allocated by the
  //! debug processor, provided it is cleaned up.
  PVOID Buffer;
  //! The size of `buffer` in bytes.
  DWORD BufferSize;
  //! Arbitrary data defined by the command processor.
  PVOID CustomData;
  //! Used when sending a chunked response, indicates the number of bytes
  //! remaining to be sent. `handler` will be called repeatedly until this is
  //! set to 0, indicating completion of the send.
  DWORD BytesRemaining;
} DM_CMDCONT;

typedef HRESULT(__stdcall *PDM_CMDPROC)(LPCSTR szCommand, LPSTR szResponse,
                                        DWORD cchResponse, PDM_CMDCONT pdmcc);

typedef struct _DM_COUNTDATA {
  LARGE_INTEGER CountValue;
  LARGE_INTEGER RateValue;
  DWORD CountType;
} DM_COUNTDATA, *PDM_COUNTDATA;

#define DMAPI __declspec(dllimport)

DMAPI HRESULT NTAPI DmClosePerformanceCounter(HANDLE hCounter);
DMAPI HRESULT NTAPI DmNotify(PDMN_SESSION Session, DWORD dwNotification,
                             PDM_NOTIFY_FUNCTION pfnHandler);
DMAPI HRESULT NTAPI DmOpenNotificationSession(DWORD dwFlags,
                                              PDMN_SESSION *pSession);
DMAPI HRESULT NTAPI DmOpenPerformanceCounter(LPCSTR szName, HANDLE *phCounter);
DMAPI HRESULT NTAPI DmQueryPerformanceCounterHandle(HANDLE hCounter,
                                                    DWORD dwType,
                                                    PDM_COUNTDATA);
//! Register a new processor for commands with the given prefix.
DMAPI HRESULT NTAPI DmRegisterCommandProcessor(LPCSTR szProcessor,
                                               PDM_CMDPROC pfn);
DMAPI HRESULT NTAPI DmSendNotificationString(LPCSTR sz);

//! A function that may be invoked by DmRegisterCommandProcessorEx to create a
//! dedicated handler thread.
typedef HANDLE(__stdcall *PDM_CREATETHREADPROC)(
    LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
    DWORD dwCreationFlags, LPDWORD lpThreadId);

// Register a new processor for commands with the given prefix, running in a
// dedicated thread created with the given `create_thread_func`.
DMAPI HRESULT NTAPI DmRegisterCommandProcessorEx(
    LPCSTR prefix, PDM_CMDPROC proc, PDM_CREATETHREADPROC create_thread_func);

// Allocate a new block of memory with the given tag.
DMAPI PVOID NTAPI DmAllocatePoolWithTag(DWORD size, DWORD tag);

// Free the given block, which was previously allocated via
// DmAllocatePoolWithTag.
DMAPI VOID NTAPI DmFreePool(PVOID block);

typedef PVOID PDM_WALK_MODULES;
typedef struct DMN_MODLOAD {
  char name[260];
  void *base;
  DWORD size;
  DWORD timestamp;
  DWORD checksum;
  DWORD unknown_flags;
} DMN_MODLOAD;

DMAPI HRESULT NTAPI DmWalkLoadedModules(PDM_WALK_MODULES *ppdmwm,
                                        DMN_MODLOAD *pdmml);
DMAPI HRESULT NTAPI DmCloseLoadedModules(PDM_WALK_MODULES pdmwm);

#endif // #ifndef __XBDM_H__
