// This is a debug extension to be used with XBDM on a debug Xbox kernel
//
// Can be installed in E:/dxt and will be auto-loaded at startup.
// You can communicate with this DXT through telnet.
// Connect to your Xbox debug IP on port 731.
// It will respond to "hello!" commands via XBDM.
//
// Warning:
//
// - Raw and Virtual address in binary must be the same.
// - Must have an import table.
//   - If an import fails, loading fails.
//   - If an import has name and ordinal, the name is used (will likely fail).
// - This doesn't use the CRT0, so constructors won't run.
// - TLS (Thread local storage) won't work.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <winapi/winbase.h>
#include <xbdm/xbdm.h>
#include <xboxkrnl/xboxkrnl.h>

#include "command_processor_util.h"

static const DWORD kTag = 0x64656d6f;  // 'demo'

// Command prefix that will be handled by this processor.
static const char kHandlerName[] = "demo";

typedef struct CommandTableEntry {
  const char *command;
  HRESULT (*processor)(const char *, char *, DWORD, DM_CMDCONT *);
} CommandTableEntry;

// Basic immediate request->response.
static HRESULT HandleBasicRequest(const char *command, char *response,
                                  DWORD response_len, DM_CMDCONT *ctx);

// Receive binary data from the client.
static HRESULT HandleReceiveBinary(const char *command, char *response,
                                   DWORD response_len, DM_CMDCONT *ctx);
static HRESULT __stdcall ReceiveBinaryData(DM_CMDCONT *ctx, char *response,
                                           DWORD response_len);

// Send binary data to the client.
static HRESULT HandleSendBinary(const char *command, char *response,
                                DWORD response_len, DM_CMDCONT *ctx);
static HRESULT __stdcall SendBinaryData(DM_CMDCONT *ctx, char *response,
                                        DWORD response_len);

// Send a large buffer to the client, prefixed with the size of the buffer.
static HRESULT HandleSendSizePrefixedBinary(const char *command, char *response,
                                            DWORD response_len,
                                            DM_CMDCONT *ctx);
static HRESULT __stdcall SendPrepopulatedBinaryData(DM_CMDCONT *ctx,
                                                    char *response,
                                                    DWORD response_len);

// Send multiline text response to the client.
static HRESULT HandleSendMultiline(const char *command, char *response,
                                   DWORD response_len, DM_CMDCONT *ctx);
static HRESULT __stdcall SendMultilineData(DM_CMDCONT *ctx, char *response,
                                           DWORD response_len);

// Send a message to the notification channel.
static HRESULT HandleSendNotification(const char *command, char *response,
                                      DWORD response_len, DM_CMDCONT *ctx);

// Enumerates the command table.
static HRESULT HandleHello(const char *command, char *response,
                           DWORD response_len, DM_CMDCONT *ctx);
static HRESULT __stdcall SendHelloData(DM_CMDCONT *ctx, char *response,
                                       DWORD response_len);

static const CommandTableEntry kCommandTable[] = {
    {"hello", HandleHello},
    {"basicrequest", HandleBasicRequest},
    {"receivebin", HandleReceiveBinary},
    {"sendbin", HandleSendBinary},
    {"sendsizeprefixedbin", HandleSendSizePrefixedBinary},
    {"sendmultiline", HandleSendMultiline},
    {"sendnotification", HandleSendNotification},
};
static const DWORD kCommandTableNumEntries =
    sizeof(kCommandTable) / sizeof(kCommandTable[0]);

static HRESULT __stdcall ProcessCommand(const char *command, char *response,
                                        DWORD response_len, DM_CMDCONT *ctx) {
  const char *subcommand = command + sizeof(kHandlerName);

  const CommandTableEntry *entry = kCommandTable;
  for (DWORD i = 0; i < kCommandTableNumEntries; ++i, ++entry) {
    DWORD len = strlen(entry->command);
    if (!strncmp(subcommand, entry->command, len)) {
      return entry->processor(subcommand + len, response, response_len, ctx);
    }
  }

  return XBOX_E_UNKNOWN_COMMAND;
}

// Trivial request-response pattern.
// Request parameters may be processed with the CPParseCommandParameters method
// and associated extractors exported by the Dynamic DXT loader if desired.
static HRESULT HandleBasicRequest(const char *command, char *response,
                                  DWORD response_len, DM_CMDCONT *ctx) {
  response[0] = 0;
  strncat(response, "Response!", response_len);
  return XBOX_S_OK;
}

// Commands can receive binary files from the client by setting up the
// DM_CMDCONT and returning XBOX_S_SEND_BINARY.
static HRESULT HandleReceiveBinary(const char *command, char *response,
                                   DWORD response_len, DM_CMDCONT *ctx) {
  // Some mechanism to determine end-of-data from the data alone must be used.
  // Generally this would be done by specifying the length as a command
  // parameter or having a hardcoded size up front, but it'd also be possible to
  // parse the actual data sent and handle it from there if desired (e.g., for
  // Pascal-style strings that prefix the data with the size).
  CommandParameters cp;
  LONG result = CPParseCommandParameters(command, &cp);
  if (result < 0) {
    return CPPrintError(result, response, response_len);
  }
  uint32_t length;
  BOOL length_found = CPGetUInt32("length", &length, &cp);
  CPDelete(&cp);

  if (!length_found) {
    response[0] = 0;
    strncat(response, "Missing required 'length' param", response_len);
    return XBOX_E_FAIL;
  }

  // However setting up the DM_CMDCONT to do the receive is.
  // In a realistic situation, it is often useful to set ctx->CustomData to
  // something that contains additional information about the upload. For
  // example, additional parameters that may have been passed to this handler,
  // extra memory buffers, etc...
  // For this trivial example, no CustomData is necessary so it is set to NULL,
  // although it is probably also fine to leave whatever value is in there, as
  // long as the `handler` method does not make use of it.
  ctx->CustomData = NULL;

  // XBDM provides a small built-in buffer in the DM_CMDCONT that can be
  // used if desired. XBDM handles calling the handler multiple times if more
  // data is received than the buffer can hold.
  // Alternatively, a buffer can be allocated and assigned here:
  //  ctx->Buffer = DmAllocatePoolWithTag(length, some_tag_integer);
  //  ctx->BufferSize = length;

  // BytesRemaining should be initialized to the total size expected by this
  // receive command, but setting it to anything > 0 should cause XBDM to
  // continue to expect binary data from the client.
  ctx->BytesRemaining = length;

  // The handler method will be invoked as XBDM receives chunks of data. It is
  // up to the handler function to deal with the received data and eventually
  // to set ctx->BytesRemaining to 0, indicating that the receive is completed.
  ctx->HandlingFunction = ReceiveBinaryData;

  // Sending back a message is not actually necessary.
  response[0] = 0;
  strncat(response, "Ready to receive binary data", response_len);

  return XBOX_S_SEND_BINARY;
}

static HRESULT __stdcall ReceiveBinaryData(PDM_CMDCONT ctx, LPSTR response,
                                           DWORD response_len) {
  // This method will be invoked by XBDM as it receives binary data sent in
  // response to an XBOX_S_SEND_BINARY return value.

  // It is this handler's responsibility to do something useful with the data
  // (e.g., store it someplace less volatile than the ctx->Buffer, which will
  // potentially be reused even within a given SEND_BINARY interaction).
  // It is also up to this handler to indicate the end of the transaction by
  // setting ctx->BytesRemaining to 0 and to return error codes if appropriate.

  // The DM_CMDCONT contains a `DataSize` member which has been set by
  // XBDM to the number of bytes in ctx->Buffer that were populated with real
  // data from the client. It is possible that the client did not send enough
  // data to fill the buffer completely, so it is important to respect this
  // number to avoid processing garbage data.

  // In this demo, there is no potential for the client to cause an error, but
  // error handling would roughly follow this pattern:
#if 0
  if (data_is_invalid_for_some_reason) {
    // Clean up any resources that were allocated by the top level command
    // handler, e.g., if ctx->Buffer was set via DmAllocatePoolWithTag it should
    // be freed before the receive action is terminated via this subhandler
    // returning an error or a success with ctx->BytesRemaining == 0.

    // It is not necessary to populate the response message, but it may be used
    // to give the client some context about the failure.
    response[0] = 0;
    strncat(response, "Information about the failure", response_len);

    return XBOX_E_FAIL;
  }
#endif

  // A real application would do something interesting with the buffer; possibly
  // accumulate it over multiple invocations of this handler, decrementing
  // ctx->BytesRemaining until it == 0. See the handler in nxdk_dyndxt for a
  // realistic usecase:
  // https://github.com/abaire/nxdk_dyndxt/blob/76938e6d42d9f01cdd598c29a55f9d285c11394e/src/dxtmain.c#L283
  //
  // In this demo case, the data itself is ignored and we simply decrement
  // BytesRemaining until all data has been ignored.
  ctx->BytesRemaining -= ctx->DataSize;

  if (!ctx->BytesRemaining) {
    // In a real application, it'd be important to clean up any allocated
    // resources here, as XBDM will not invoke this handler again once it
    // returns XBOX_S_OK with ctx->BytesRemaining == 0.
    //
    // In this demo case, there is nothing to clean up.

    // It is not actually necessary to populate the response message.
    response[0] = 0;
    strncat(response, "All data received!", response_len);
  }

  // Returning S_OK indicates either that the receive should continue
  // (ctx->BytesRemaining > 0) or that the receive is completed
  // (ctx->BytesRemaining == 0).
  return XBOX_S_OK;
}

// Send binary data to the client.
static HRESULT HandleSendBinary(const char *command, char *response,
                                DWORD response_len, DM_CMDCONT *ctx) {
  // Sending a binary response involves supplying a handler procedure that will
  // be called repeatedly to populate the send buffer. The DM_CMDCONT's
  // buffer may also be replaced with a larger one for efficiency.
  // Finally, this method must return XBOX_S_BINARY to request that XBDM invoke
  // the handler repeatedly until it returns XBOX_S_NO_MORE_DATA.

  // In this demo, 4 bytes are returned to the client in 4 invocations of the
  // SendBinaryData handler. In a real application, the CustomData would likely
  // point to a more interesting contextual struct, and the handler would almost
  // certainly return more than a single byte per iteration.

  DWORD current_value = 4;
  ctx->CustomData = (void *)current_value;
  ctx->BytesRemaining = 3;
  ctx->HandlingFunction = SendBinaryData;

  // The default XBDM buffer is small, so it may be desirable to utilize a
  // larger buffer. The buffer could just be a global array, but heap allocation
  // is used for demonstration purposes. A heap allocated buffer must be freed
  // in the send handler.
  const DWORD kBufferSize = 4;
  BYTE *buffer = DmAllocatePoolWithTag(kBufferSize, kTag);
  if (!buffer) {
    response[0] = 0;
    strncat(response, "Failed to allocate send buffer", response_len);
    return XBOX_E_ACCESS_DENIED;
  }

  // No data will be sent by XBDM until after it invokes the `handler`
  // procedure, so there is no reason to populate buffer here.
  ctx->Buffer = buffer;
  ctx->BufferSize = kBufferSize;

  // Sending back a message is not actually necessary.
  response[0] = 0;
  strncat(response, "Returning 4 bytes of data", response_len);

  return XBOX_S_BINARY;
}

static HRESULT __stdcall SendBinaryData(DM_CMDCONT *ctx, char *response,
                                        DWORD response_len) {
  // This handler is responsible for populating `ctx->Buffer` with response
  // data, setting `ctx->DataSize` to the number of valid bytes in the buffer,
  // and returning either XBOX_S_OK (if more data needs to be sent) or
  // XBOX_S_NO_MORE_DATA if all data has already been sent.
  //
  // Note that the `BytesRemaining` field is unused in the context of binary-
  // sending and can be ignored entirely or used by this handler to determine
  // when to stop sending data. In this demo, `CustomData` is used to determine
  // the end condition and `BytesRemaining` is ignored.

  if (!ctx->CustomData) {
    // Since the buffer was allocated by us, it is important to clean it up
    // here as XBDM will not invoke this handler again once it returns XBOX_S_OK
    // with ctx->BytesRemaining == 0.
    DmFreePool(ctx->Buffer);

    // It is not actually necessary to populate the response message.
    response[0] = 0;
    strncat(response, "Done sending bytes!", response_len);
    return XBOX_S_NO_MORE_DATA;
  }

  // In a real application, it'd almost certainly be desirable to send back more
  // than a single byte per invocation of this handler. Using a single byte
  // allows this demo to show how to send more than a buffer's worth of data.

  // Response data is copied into ctx->Buffer.
  BYTE *dest = (BYTE *)ctx->Buffer;
  dest[0] = (DWORD_PTR)ctx->CustomData & 0xFF;

  // ctx->DataSize is updated to indicate how many bytes of ctx->Buffer are
  // populated.
  ctx->DataSize = 1;

  --ctx->CustomData;

  return XBOX_S_OK;
}

// Send binary data to the client.
static HRESULT HandleSendSizePrefixedBinary(const char *command, char *response,
                                            DWORD response_len,
                                            DM_CMDCONT *ctx) {
  // Demonstrates sending a large buffer to the client where the first 4 bytes
  // contain the size of the buffer.

  ctx->HandlingFunction = SendPrepopulatedBinaryData;

  const DWORD kDataSize = 1024 * 1024;
  BYTE *buffer = DmAllocatePoolWithTag(kDataSize + 4, kTag);
  if (!buffer) {
    response[0] = 0;
    strncat(response, "Failed to allocate send buffer", response_len);
    return XBOX_E_ACCESS_DENIED;
  }

  // No data will be sent by XBDM until after it invokes the `handler`
  // procedure, but the buffer contents also will not be touched by the system
  // so it may be initialized here.
  memcpy(buffer, &kDataSize, sizeof(kDataSize));
  for (DWORD i = 0; i < kDataSize; ++i) {
    buffer[i + 4] = i & 0xFF;
  }

  ctx->CustomData = (void *)kDataSize;
  ctx->Buffer = buffer;
  ctx->BytesRemaining = kDataSize + 4;
  ctx->BufferSize = kDataSize + 4;

  // Sending back a message is not actually necessary.
  snprintf(response, response_len, "Returning size prefixed data");

  return XBOX_S_BINARY;
}

static HRESULT __stdcall SendPrepopulatedBinaryData(DM_CMDCONT *ctx,
                                                    char *response,
                                                    DWORD response_len) {
  // This handler is responsible for populating `ctx->Buffer` with response
  // data, setting `ctx->DataSize` to the number of valid bytes in the buffer,
  // and returning either XBOX_S_OK (if more data needs to be sent) or
  // XBOX_S_NO_MORE_DATA if all data has already been sent.
  //
  // Note that the `BytesRemaining` field is unused in the context of binary-
  // sending and can be ignored entirely or used by this handler to determine
  // when to stop sending data. In this demo, `CustomData` is used to determine
  // the end condition and `BytesRemaining` is ignored.

  if (!ctx->BytesRemaining) {
    // Since the buffer was allocated by us, it is important to clean it up
    // here as XBDM will not invoke this handler again once it returns XBOX_S_OK
    // with ctx->BytesRemaining == 0.
    DmFreePool(ctx->Buffer);

    // It is not actually necessary to populate the response message.
    response[0] = 0;
    strncat(response, "Done sending bytes!", response_len);
    return XBOX_S_NO_MORE_DATA;
  }

  // ctx->DataSize is updated to indicate how many bytes of ctx->Buffer are
  // populated.
  ctx->DataSize = ctx->BytesRemaining;
  ctx->BytesRemaining = 0;

  return XBOX_S_OK;
}

// Send multiline text response to the client.
static HRESULT HandleSendMultiline(const char *command, char *response,
                                   DWORD response_len, DM_CMDCONT *ctx) {
  // Multiline responses are sent by returning XBOX_S_MULTILINE from the command
  // processor, which will cause the registered handler to be invoked repeatedly
  // until it returns an error or XBOX_S_NO_MORE_DATA.
  //
  // As with the other multi-part processors, it is often useful to set up
  // ctx->CustomData with some sort of contextual information. In this demo case,
  // it is simply set to an integer which will be decremented and returned to
  // the client until it == 0.
  ctx->CustomData = (void *)4;
  ctx->HandlingFunction = SendMultilineData;

  // It is not necessary to populate the response message, but if it is
  // populated here and not populated by the registered handler, this value will
  // be sent when the handler is exhausted.
  *response = 0;
  strncat(response, "Countdown...", response_len);
  return XBOX_S_MULTILINE;
}

static HRESULT __stdcall SendMultilineData(DM_CMDCONT *ctx, char *response,
                                           DWORD response_len) {
  // This method will be invoked by XBDM repeatedly until it returns an error
  // code or XBOX_S_NO_MORE_DATA.

  DWORD current_value = (DWORD)ctx->CustomData;
  --current_value;

  // For this demo case, the response is completed when the contextual counter
  // reaches 0.
  if (!current_value) {
    // In a real application, it'd be important to clean up any allocated
    // resources here, as XBDM will not invoke this handler again once it
    // returns XBOX_S_NO_MORE_DATA.

    // It is not actually necessary to populate the response message.
    response[0] = 0;
    strncat(response, "Done counting!", response_len);
    return XBOX_S_NO_MORE_DATA;
  }

  ctx->CustomData = (void *)current_value;

  // Multiline results are sent in the ctx->Buffer.
  //
  // NOTE: In this case it'd probably be fine to sprintf directly into the
  // buffer, but ctx->BufferSize is checked for sake of a more interesting
  // example.
  char msg[16] = {0};
  int message_len = 1 + snprintf(msg, sizeof(msg), "#%d", current_value);

  if (message_len > ctx->BufferSize) {
    // In a real application, it'd be important to clean up any allocated
    // resources here, as XBDM will not invoke this handler again once it
    // returns an error result.
    response[0] = 0;
    strncat(response, "Response buffer is too small", response_len);
    return XBOX_E_ACCESS_DENIED;
  }

  memcpy(ctx->Buffer, msg, message_len);

  return XBOX_S_OK;
}

// DmSendNotification demonstration.
static HRESULT HandleSendNotification(const char *command, char *response,
                                      DWORD response_len, DM_CMDCONT *ctx) {
  HRESULT result = DmSendNotificationString("demo!Notification");
  if (!XBOX_SUCCESS(result)) {
    response[0] = 0;
    strncat(response, "Sending failed!", response_len);
    return result;
  }

  response[0] = 0;
  strncat(response, "Notification sent!", response_len);
  return XBOX_S_OK;
}

static HRESULT HandleHello(const char *command, char *response,
                           DWORD response_len, DM_CMDCONT *ctx) {
  ctx->CustomData = 0;
  ctx->HandlingFunction = SendHelloData;
  *response = 0;
  strncat(response, "Available commands:", response_len);
  return XBOX_S_MULTILINE;
}

static HRESULT __stdcall SendHelloData(DM_CMDCONT *ctx, char *response,
                                       DWORD response_len) {
  DWORD current_index = (DWORD)ctx->CustomData++;

  if (current_index >= kCommandTableNumEntries) {
    return XBOX_S_NO_MORE_DATA;
  }

  const CommandTableEntry *entry = &kCommandTable[current_index];
  DWORD command_len = strlen(entry->command) + 1;
  if (command_len > ctx->BufferSize) {
    response[0] = 0;
    strncat(response, "Response buffer is too small", response_len);
    return XBOX_E_ACCESS_DENIED;
  }

  memcpy(ctx->Buffer, entry->command, command_len);
  return XBOX_S_OK;
}

DXT_ENTRY(pfUnload) {
  OutputDebugString("Hello World from demo dxt OutputDebugString!\n");
  DmSendNotificationString("Hello World from demo dxt via notification!");

  DmRegisterCommandProcessor(kHandlerName, ProcessCommand);
}
