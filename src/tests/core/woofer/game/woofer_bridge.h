/*
 * Woofer bridge, game side (docs/woofer-bridge/PROTOCOL.md).
 *
 * Game independent: copy woofer_bridge.c and .h into the next Woofer game and
 * call Woofer_Init with its own game id. Messages are only sent when the
 * emulator answers the handshake (WooferCitra); on a real 3DS or in another
 * emulator nothing is sent and building a message costs next to nothing.
 * The word list (sdmc:/woofer/vocab.jsonl) works everywhere.
 */
#ifndef WOOFER_BRIDGE_H
#define WOOFER_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#define WOOFER_PROTOCOL 1
#define WOOFER_INFO_TYPE 0x574F4F46u /* 'WOOF' for svcGetSystemInfo */
#define WOOFER_HOST_PANEL_VISIBLE 1u
#define WOOFER_MAX_MESSAGE 8192
#define WOOFER_MAX_DEPTH 8

typedef struct WooferMsg
{
    char buf[WOOFER_MAX_MESSAGE];
    size_t len;
    size_t start;      /* where the JSON object begins (after "WOOF1 ") */
    int depth;
    int first[WOOFER_MAX_DEPTH];
    int overflow;
} WooferMsg;

/* Handshake and "hello". Returns 1 when the bridge is active. Strings may be
 * NULL except game. Call once at start-up. */
int Woofer_Init(const char *game, const char *title, const char *lang, const char *src, const char *build);
int Woofer_Active(void);
const char *Woofer_Game(void);
/* WOOFER_HOST_* bits of the emulator, asked anew on every call. */
unsigned Woofer_HostFlags(void);

/* Building a message. Keys are NULL inside arrays. A WooferMsg is 8 KiB:
 * keep it static rather than on the stack. */
void Woofer_Begin(WooferMsg *m, const char *type);
void Woofer_Str(WooferMsg *m, const char *key, const char *utf8);
void Woofer_StrN(WooferMsg *m, const char *key, const char *utf8, size_t n);
void Woofer_Int(WooferMsg *m, const char *key, long value);
void Woofer_Hex64(WooferMsg *m, const char *key, uint64_t value);
void Woofer_ArrayBegin(WooferMsg *m, const char *key);
void Woofer_ArrayEnd(WooferMsg *m);
void Woofer_ObjectBegin(WooferMsg *m, const char *key);
void Woofer_ObjectEnd(WooferMsg *m);
/* Closes and sends the message. Returns 1 when sent, 0 when the bridge is
 * not active or the message did not fit. */
int Woofer_Send(WooferMsg *m);

/* Short messages. */
int Woofer_SendClear(void);
int Woofer_SendSaved(const char *s, const char *p);

/* Word list: Woofer_BeginVocab starts {"v":1,"game":...}; add the other
 * keys, then Woofer_SaveVocab appends it as one line. Returns 1 on success. */
void Woofer_BeginVocab(WooferMsg *m);
int Woofer_SaveVocab(WooferMsg *m);

/* Folder of vocab.jsonl, "sdmc:/woofer" unless changed (tests). */
extern const char *gWooferVocabDir;

#ifndef __3DS__
/* Host builds (tests): stand-ins for the two system calls. */
extern int (*gWooferSystemInfo)(int64_t *out, uint32_t type, int32_t param);
extern void (*gWooferOutput)(const char *text, size_t length);
#endif

#endif
