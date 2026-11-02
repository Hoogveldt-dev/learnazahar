/*
 * Woofer bridge, game side. See woofer_bridge.h and
 * docs/woofer-bridge/PROTOCOL.md. Nothing in this file is specific to one game.
 */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "woofer_bridge.h"

#ifdef __3DS__
#include <3ds.h>

static int SystemInfo(int64_t *out, int32_t param)
{
    s64 value = 0;
    if (R_FAILED(svcGetSystemInfo(&value, WOOFER_INFO_TYPE, param)))
        return 0;
    *out = value;
    return 1;
}

static void Output(const char *text, size_t length)
{
    svcOutputDebugString(text, (s32)length);
}
#else
int (*gWooferSystemInfo)(int64_t *out, uint32_t type, int32_t param);
void (*gWooferOutput)(const char *text, size_t length);

static int SystemInfo(int64_t *out, int32_t param)
{
    return gWooferSystemInfo != NULL && gWooferSystemInfo(out, WOOFER_INFO_TYPE, param) == 0;
}

static void Output(const char *text, size_t length)
{
    if (gWooferOutput != NULL)
        gWooferOutput(text, length);
}
#endif

const char *gWooferVocabDir = "sdmc:/woofer";

static int sActive;
static char sGame[48];

/* ------------------------------------------------------------------------ */
/* Handshake                                                                */
/* ------------------------------------------------------------------------ */

int Woofer_Active(void)
{
    return sActive;
}

const char *Woofer_Game(void)
{
    return sGame;
}

unsigned Woofer_HostFlags(void)
{
    int64_t flags = 0;
    if (!sActive || !SystemInfo(&flags, 1))
        return 0;
    return (unsigned)flags;
}

int Woofer_Init(const char *game, const char *title, const char *lang, const char *src, const char *build)
{
    static WooferMsg hello;
    int64_t version = 0;

    snprintf(sGame, sizeof(sGame), "%s", game != NULL ? game : "");
    sActive = SystemInfo(&version, 0) && version >= WOOFER_PROTOCOL;
    if (!sActive)
        return 0;
    Woofer_Begin(&hello, "hello");
    Woofer_Str(&hello, "game", sGame);
    if (title != NULL)
        Woofer_Str(&hello, "title", title);
    if (lang != NULL)
        Woofer_Str(&hello, "lang", lang);
    if (src != NULL)
        Woofer_Str(&hello, "src", src);
    if (build != NULL)
        Woofer_Str(&hello, "build", build);
    Woofer_Send(&hello);
    return 1;
}

/* ------------------------------------------------------------------------ */
/* JSON                                                                     */
/* ------------------------------------------------------------------------ */

static void Put(WooferMsg *m, const char *s, size_t n)
{
    if (m->overflow || m->len + n + 2 > sizeof(m->buf)) /* room for "}" and NUL */
    {
        m->overflow = 1;
        return;
    }
    memcpy(m->buf + m->len, s, n);
    m->len += n;
    m->buf[m->len] = '\0';
}

static void PutC(WooferMsg *m, char c)
{
    Put(m, &c, 1);
}

static void PutString(WooferMsg *m, const char *s, size_t n)
{
    static const char hex[] = "0123456789abcdef";
    size_t i;
    PutC(m, '"');
    for (i = 0; i < n && s[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\')
        {
            char esc[2] = {'\\', (char)c};
            Put(m, esc, 2);
        }
        else if (c < 0x20)
        {
            char esc[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            Put(m, esc, 6);
        }
        else
            PutC(m, (char)c);
    }
    PutC(m, '"');
}

/* Comma and key before a value. */
static void Field(WooferMsg *m, const char *key)
{
    if (m->depth > 0 && m->depth <= WOOFER_MAX_DEPTH)
    {
        if (!m->first[m->depth - 1])
            PutC(m, ',');
        m->first[m->depth - 1] = 0;
    }
    if (key != NULL)
    {
        PutString(m, key, strlen(key));
        PutC(m, ':');
    }
}

static void Open(WooferMsg *m, const char *key, char bracket)
{
    Field(m, key);
    PutC(m, bracket);
    if (m->depth < WOOFER_MAX_DEPTH)
        m->first[m->depth] = 1;
    else
        m->overflow = 1;
    m->depth++;
}

static void Close(WooferMsg *m, char bracket)
{
    if (m->depth > 0)
        m->depth--;
    PutC(m, bracket);
}

static void Reset(WooferMsg *m, const char *prefix)
{
    m->len = 0;
    m->depth = 0;
    m->overflow = 0;
    m->buf[0] = '\0';
    Put(m, prefix, strlen(prefix));
    m->start = m->len;
    Open(m, NULL, '{');
}

void Woofer_Begin(WooferMsg *m, const char *type)
{
    Reset(m, "WOOF1 ");
    Woofer_Str(m, "t", type);
}

void Woofer_StrN(WooferMsg *m, const char *key, const char *utf8, size_t n)
{
    Field(m, key);
    PutString(m, utf8 != NULL ? utf8 : "", utf8 != NULL ? n : 0);
}

void Woofer_Str(WooferMsg *m, const char *key, const char *utf8)
{
    Woofer_StrN(m, key, utf8, utf8 != NULL ? strlen(utf8) : 0);
}

void Woofer_Int(WooferMsg *m, const char *key, long value)
{
    char num[24];
    int n = snprintf(num, sizeof(num), "%ld", value);
    Field(m, key);
    Put(m, num, (size_t)n);
}

void Woofer_Hex64(WooferMsg *m, const char *key, uint64_t value)
{
    char hex[17];
    snprintf(hex, sizeof(hex), "%08" PRIx32 "%08" PRIx32, (uint32_t)(value >> 32), (uint32_t)value);
    Woofer_Str(m, key, hex);
}

void Woofer_ArrayBegin(WooferMsg *m, const char *key)
{
    Open(m, key, '[');
}

void Woofer_ArrayEnd(WooferMsg *m)
{
    Close(m, ']');
}

void Woofer_ObjectBegin(WooferMsg *m, const char *key)
{
    Open(m, key, '{');
}

void Woofer_ObjectEnd(WooferMsg *m)
{
    Close(m, '}');
}

/* Closes the outer object; 1 when the message is complete. */
static int Finish(WooferMsg *m)
{
    if (m->depth != 1 || m->overflow)
        return 0;
    m->depth = 0;
    m->buf[m->len++] = '}'; /* Put kept room for it */
    m->buf[m->len] = '\0';
    return 1;
}

int Woofer_Send(WooferMsg *m)
{
    if (!sActive || !Finish(m))
        return 0;
    Output(m->buf, m->len);
    return 1;
}

int Woofer_SendClear(void)
{
    static WooferMsg msg;
    if (!sActive)
        return 0;
    Woofer_Begin(&msg, "clear");
    return Woofer_Send(&msg);
}

int Woofer_SendSaved(const char *s, const char *p)
{
    static WooferMsg msg;
    if (!sActive)
        return 0;
    Woofer_Begin(&msg, "saved");
    Woofer_Str(&msg, "s", s);
    Woofer_Str(&msg, "p", p);
    return Woofer_Send(&msg);
}

/* ------------------------------------------------------------------------ */
/* Word list                                                                */
/* ------------------------------------------------------------------------ */

void Woofer_BeginVocab(WooferMsg *m)
{
    Reset(m, "");
    Woofer_Int(m, "v", 1);
    Woofer_Str(m, "game", sGame);
}

int Woofer_SaveVocab(WooferMsg *m)
{
    char path[160];
    FILE *f;
    size_t n;
    int ok;

    if (!Finish(m) || m->len + 1 >= sizeof(m->buf))
        return 0;
    mkdir(gWooferVocabDir, 0777); /* fails harmlessly when it exists */
    snprintf(path, sizeof(path), "%s/vocab.jsonl", gWooferVocabDir);
    f = fopen(path, "a");
    if (f == NULL)
        return 0;
    /* One write for the whole line: a power cut can only cost this line. */
    m->buf[m->len] = '\n';
    n = m->len + 1 - m->start;
    ok = fwrite(m->buf + m->start, 1, n, f) == n;
    m->buf[m->len] = '\0';
    if (fclose(f) != 0)
        ok = 0;
    return ok;
}
