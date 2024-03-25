#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <float.h>
#include "cjson.h"

/* -------------------------------------------------------------------------- */
/*                                 parameters                                 */
/* -------------------------------------------------------------------------- */

typedef struct {
    char *buffer;
    int length;
    int offset;
} PrintBuffer;

static void *(*cJson_malloc)(size_t sz) = malloc;
static void (*cJson_free)(void *ptr) = free;

static const char *ep;

static const unsigned char firstByteMark[7] = {
    0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
};

/* -------------------------------------------------------------------------- */
/*                              static functions                              */
/* -------------------------------------------------------------------------- */

static CJson* cJson_new_item(void) {
    CJson *nd = (CJson*) cJson_malloc(sizeof(CJson));
    if (nd) memset(nd, 0x00, sizeof(CJson));
    return nd;
}

static char* cJson_strdup(const char* str) {
    size_t len;
    char *copy;
    len = strlen(str) + 1;
    if (!(copy = (char*) cJson_malloc(len))) return NULL;
    memcpy(copy, str, len);
    return copy;
}

static void suffix_object(CJson *prev, CJson *item) {
    prev->next = item;
    item->prev = prev;
}

static int cJson_strcasecmp(const char *s1, const char *s2) {
    if (!s1) return (s1 == s2) ? 0 : 1;
    if (!s2) return 1;
    for (; tolower(*s1) == tolower(*s2); ++s1, ++s2) {
        if (*s1 == 0) return 0;
    }
    return tolower(*(const unsigned char *) s1) - tolower(*(const unsigned char *) s2);
}

static const char* skip(const char *in) {
    while (in && *in && (unsigned char) *in <= 32) ++in;
    return in;
}

static CJson* create_reference(CJson *item) {
    CJson *ref = cJson_new_item();
    if (!ref) return 0;
    memcpy(ref, item, sizeof(CJson));
    ref->string = 0;
    ref->type |= cJson_IsReference;
    ref->next = ref->prev = NULL;
    return ref;
}

// 大于等于 x 的最小2的幂
static int pow2gt(int x) {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

static int update(PrintBuffer *p) {
    char *str;
    if (!p || !p->buffer) return 0;
    str = p->buffer + p->offset;
    return p->offset + strlen(str);
}

static char* ensure(PrintBuffer *p, int needed) {
    char *newBuffer;
    int newSize;
    if (!p || !p->buffer) return 0;
    
    needed += p->offset;
    if (needed <= p->length) return p->buffer + p->offset;

    newSize = pow2gt(needed);
    newBuffer = (char *) cJson_malloc(newSize);
    if (!newBuffer) {
        cJson_free(p->buffer);
        p->length = 0;
        p->buffer = NULL;
        return NULL;
    }
    // if (newBuffer) {
    //     memcpy(newBuffer, p->buffer, p->length);
    // }
    memcpy(newBuffer, p->buffer, p->length);
    cJson_free(p->buffer);
    p->length = newSize;
    p->buffer = newBuffer;
    return newBuffer + p->offset;
}

/* -------------------------------------------------------------------------- */
/*                                  functions                                 */
/* -------------------------------------------------------------------------- */

CJson* cJson_CreateFalse(void) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_False;
    }
    return item;
}

CJson* cJson_CreateTrue(void) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_True;
    }
    return item;
}

CJson* cJson_CreateBool(int b) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = b ? CJSON_True : CJSON_False;
    }
    return item;
}

CJson* cJson_CreateNull(void) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_NULL;
    }
    return item;
}

CJson* cJson_CreateNumber(double num) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_Number;
        item->dValue = num;
        item->iValue = (int) num;
    }
    return item;
}

CJson* cJson_CreateString(const char *string) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_String;
        item->sValue = cJson_strdup(string);
    }
    return item;
}

CJson* cJson_CreateArray(void) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_Array;
    }
    return item;
}

CJson* cJson_CreateObject(void) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_Object;
    }
    return item;
}

CJson* cJson_CreateIntArray(const int *numbers, int count) {
    int i;
    CJson *n = NULL, *p = NULL;
    CJson *a = cJson_CreateArray();
    for (i = 0; a && i < count; i++) {
        n = cJson_CreateNumber(numbers[i]);
        if (!i) { // head (first element) of the array
            a->child = n;
        } else {
            suffix_object(p, n);
        }
        p = n;
    }
    return a;
}

CJson* cJson_CreateFloatArray(const float *numbers, int count) {
    int i;
    CJson *n = NULL, *p = NULL;
    CJson *a = cJson_CreateArray();
    for (i = 0; a && i < count; i++) {
        n = cJson_CreateNumber(numbers[i]);
        if (!i) { // head (first element) of the array
            a->child = n;
        } else {
            suffix_object(p, n);
        }
        p = n;
    }
    return a;
}

CJson* cJson_CreateDoubleArray(const double *numbers, int count) {
    int i;
    CJson *n = NULL, *p = NULL;
    CJson *a = cJson_CreateArray();
    for (i = 0; a && i < count; i++) {
        n = cJson_CreateNumber(numbers[i]);
        if (!i) { // head (first element) of the array
            a->child = n;
        } else {
            suffix_object(p, n);
        }
        p = n;
    }
    return a;
}

CJson* cJson_CreateStringArray(const char **strings, int count) {
    int i;
    CJson *n = NULL, *p = NULL;
    CJson *a = cJson_CreateArray();
    for (i = 0; a && i < count; i++) {
        n = cJson_CreateString(strings[i]);
        if (!i) { // head (first element) of the array
            a->child = n;
        } else {
            suffix_object(p, n);
        }
        p = n;
    }
    return a;
}

const char *cJson_GetErrorPtr(void) {
    return ep;
}

int cJson_GetArraySize(CJson *array) {
    CJson *cj = array->child;
    int count = 0;
    while (cj) {
        cj = cj->next;
        ++count;
    }
    return count;
}

CJson* cJson_GetArrayItem(CJson *array, int item) {
    CJson *cj = array->child;
    while (cj && item > 0) {
        cj = cj->next;
        --item;
    }
    return cj;
}

CJson* cJson_GetObjectItem(CJson *object, const char *string) {
    CJson *cj = object->child;
    while (cj && cJson_strcasecmp(cj->string, string)) {
        cj = cj->next;
    }
    return cj;
}

void cJson_InitHooks(CJson_Hooks *hooks) {
    if (!hooks) { // 重置 hooks
        cJson_malloc = malloc;
        cJson_free = free;
        return;
    }
    cJson_malloc = (hooks->malloc_fn) ? hooks->malloc_fn : malloc;
    cJson_free = (hooks->free_fn) ? hooks->free_fn : free;
}

CJson* cJson_Duplicate(CJson *item, int recurse) {
    CJson *newItem, *cptr, *nptr = NULL, *newChild;

    if (!item) return NULL;
    
    newItem = cJson_new_item();
    if (!newItem) return NULL;

    newItem->type = item->type & (~cJson_IsReference);
    newItem->iValue = item->iValue;
    newItem->dValue = item->dValue;
    if (item->sValue) {
        newItem->sValue = cJson_strdup(item->sValue);
        if (!newItem->sValue) {
            cJson_Delete(newItem);
            return NULL;
        }
    }
    if (item->string) {
        newItem->string = cJson_strdup(item->string);
        if (!newItem->string) {
            cJson_Delete(newItem);
            return NULL;
        }
    }

    if (!recurse) return newItem;
    cptr = item->child;
    while (cptr) {
        newChild = cJson_Duplicate(cptr, 1);
        if (!newChild) {
            cJson_Delete(newItem);
            return NULL;
        }
        if (nptr) {
            nptr->next = newChild;
            newChild->prev = nptr;
            nptr = newChild;
        } else {
            newItem->child = newChild;
            nptr = newChild;
        }
        cptr = cptr->next;
    }

    return newItem;
}

void cJson_AddItemToArray(CJson *array, CJson *item) {
    CJson *cj = array->child;
    if (!item) return;
    if (!cj) {
        array->child = item;
    } else {
        while (cj && cj->next) cj = cj->next;
        suffix_object(cj, item);
    }
}

void cJson_AddItemToObject(CJson *object, const char *string, CJson *item) {
    if (!item) return;
    if (item->string) {
        cJson_free(item->string);
    }
    item->string = cJson_strdup(string);
    cJson_AddItemToArray(object, item);
}

void cJson_AddItemToObjectCS(CJson *object, const char *string, CJson *item) {
    if (!item) return;
    if (!(item->type & cJson_IsConstString) && item->string) {
        cJson_free(item->string);
    }
    item->string = (char *) string;
    item->type |= cJson_IsConstString;
    cJson_AddItemToArray(object, item);
}

void cJson_AddItemReferenceToArray(CJson *array, CJson *item) {
    cJson_AddItemToArray(array, create_reference(item));
}

void cJson_AddItemReferenceToObject(CJson *object, const char *string, CJson *item) {
    cJson_AddItemToObject(object, string, create_reference(item));
}

CJson* cJson_DetachItemFromArray(CJson *array, int which) {
    CJson *cj = array->child;
    while (cj && which > 0) {
        cj = cj->next;
        --which;
    }
    if (!cj) return NULL;
    if (cj->prev) cj->prev->next = cj->next;
    if (cj->next) cj->next->prev = cj->prev;
    if (cj == array->child) array->child = cj->next;
    cj->prev = cj->next = NULL;
    return cj;
}

void cJson_DeleteItemFromArray(CJson *array, int which) {
    cJson_Delete(cJson_DetachItemFromArray(array, which));
}

CJson* cJson_DetachItemFromObject(CJson *object, const char *string) {
    int which = 0;
    CJson *cj = object->child;
    while (cj && cJson_strcasecmp(cj->string, string)) {
        cj = cj->next;
        ++which;
    }
    if (cj) return cJson_DetachItemFromArray(object, which);
    return NULL;
}

void cJson_DeleteItemFromObject(CJson *object, const char *string) {
    cJson_Delete(cJson_DetachItemFromObject(object, string));
}

void cJson_InsertItemInArray(CJson *array, int which, CJson *newItem) {
    CJson *cj = array->child;
    while (cj && which > 0) {
        cj = cj->next;
        --which;
    }
    if (!cj) {
        cJson_AddItemToArray(array, newItem);
        return;
    }
    newItem->next = cj;
    newItem->prev = cj->prev;
    cj->prev = newItem;
    if (cj == array->child) {
        array->child = newItem;
    } else {
        newItem->prev->next = newItem;
    }
}

void cJson_ReplaceItemInArray(CJson *array, int which, CJson *newItem) {
    CJson *cj = array->child;
    while (cj && which > 0) {
        cj = cj->next;
        --which;
    }
    if (!cj) return;
    newItem->next = cj->next;
    newItem->prev = cj->prev;
    if (newItem->next) newItem->next->prev = newItem;
    if (cj == array->child) {
        array->child = newItem;
    } else {
        newItem->prev->next = newItem;
    }
    cj->next = cj->prev = NULL;
    cJson_Delete(cj);
}

void cJson_ReplaceItemInObject(CJson *object, const char *string, CJson *newItem) {
    int which = 0;
    CJson *cj = object->child;
    while (cj && cJson_strcasecmp(cj->string, string)) {
        cj = cj->next;
        ++which;
    }
    if (cj) {
        newItem->string = cJson_strdup(string);
        cJson_ReplaceItemInArray(object, which, newItem);
    }
}

void cJson_Delete(CJson *cj) {
    CJson *temp;
    while (cj) {
        temp = cj->next;
        if (!(cj->type & cJson_IsReference) && cj->child) {
            cJson_Delete(cj->child);
        }
        if (!(cj->type & cJson_IsReference) && cj->sValue) {
            cJson_free(cj->sValue);
        }
        if (!(cj->type & cJson_IsConstString) && cj->string) {
            cJson_free(cj->string);
        }
        cJson_free(cj);
        cj = temp;
    }
}

void cJson_Minify(char *json) {
    char *into = json;
    while (*json) {
        switch (*json) {
            case ' ':
            case '\t':
            case '\r':
            case '\n': json++; break;
            case '\"': // string
                *into++ = *json++;
                while (*json && *json != '\"') {
                    if (*json == '\\') *into++ = *json++;
                    *into++ = *json++;
                }
                *into++ = *json++;
                break;
            case '/': // comment
                if (*(json + 1) == '/') { // single line comment
                    while (*json && *json != '\n') ++json;
                } else if (*(json + 1) == '*') { // cross line comment
                    while (*json && !(*json == '*' && *(json + 1) == '/')) ++json;
                    json += 2;
                } else *into++ = *json++;
                break;
            default: *into++ = *json++; break;
        }
    }
    *into = 0; // null-terminate
}

/* -------------------------------------------------------------------------- */
/*                                   parsers                                  */
/* -------------------------------------------------------------------------- */

// 前置声明
static const char* parse_value(CJson *item, const char *value);

static const char* parse_number(CJson *item, const char *num) {
    double n = 0, sign = 1, scale = 0;
    int subScale = 0, signSubScale = 1;

    if (*num == '-') sign = -1, ++num;
    if (*num == '0') ++num;
    if (*num >= '1' && *num <= '9') {
        do {
            n = (n * 10.0) + (*num++ - '0');
        } while (*num >= '0' && *num <= '9');
    }
    if (*num == '.' && *(num + 1) >= '0' && *(num + 1) <= '9') {
        ++num;
        do {
            n = (n * 10.0) + (*num++ - '0');
            --scale;
        } while (*num >= '0' && *num <= '9');
    }
    if (*num == 'e' || *num == 'E') {
        ++num;
        if (*num == '+') ++num;
        else if (*num == '-') signSubScale = -1, ++num;
        while (*num >= '0' && *num <= '9') {
            subScale = (subScale * 10) + (*num++ - '0');
        }
    }

    n = sign * n * pow(10.0, (scale + subScale * signSubScale));
    item->dValue = n;
    item->iValue = (int) n;
    item->type = CJSON_Number;
    return num;
}

static unsigned parse_hex4(const char *str) {
    unsigned h = 0;

    if (*str >= '0' && *str <= '9') h += (*str) - '0';
    else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
    else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
    else return 0;

    h <<= 4;
    ++str;
    if (*str >= '0' && *str <= '9') h += (*str) - '0';
    else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
    else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
    else return 0;

    h <<= 4;
    ++str;
    if (*str >= '0' && *str <= '9') h += (*str) - '0';
    else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
    else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
    else return 0;

    h <<= 4;
    ++str;
    if (*str >= '0' && *str <= '9') h += (*str) - '0';
    else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
    else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
    else return 0;

    return h;
}

static const char* parse_string(CJson *item, const char *str) {
    const char *ptr = str + 1;
    char *ptr2;
    char *out;
    int len = 0;
    unsigned uc, uc2;

    if (*str != '\"') {
        ep = str;
        return 0;
    }
    while (*ptr != '\"' && *ptr && ++len) {
        if (*ptr++ == '\\') ++ptr;
    }
    out = (char *) cJson_malloc(len + 1);
    if (!out) return 0;

    ptr = str + 1;
    ptr2 = out;
    while (*ptr != '\"' && *ptr) {
        if (*ptr != '\\') *ptr2++ = *ptr++;
        else {
            ++ptr;
            switch (*ptr) {
                case 'b': *ptr2++ = '\b'; break;
                case 'f': *ptr2++ = '\f'; break;
                case 'n': *ptr2++ = '\n'; break;
                case 'r': *ptr2++ = '\r'; break;
                case 't': *ptr2++ = '\t'; break;
                case 'u': // transcode utf-16 to utf-8
                    uc = parse_hex4(ptr + 1);
                    ptr += 4;
                    if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) break;
                    if (uc >= 0xD800 && uc <= 0xDBFF) {
                        if (ptr[1] != '\\' || ptr[2] != 'u') break;
                        uc2 = parse_hex4(ptr + 3);
                        ptr += 6;
                        if (uc2 < 0xDC00 || uc2 > 0xDFFF) break;
                        uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
                    }
                    len = 4;
                    if (uc < 0x80) len = 1;
                    else if (uc < 0x800) len = 2;
                    else if (uc < 0x10000) len = 3;
                    ptr2 += len;
                    switch (len) {
                        case 4: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                        case 3: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                        case 2: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                        case 1: *--ptr2 = (uc | firstByteMark[len]);
                    }
                    ptr2 += len;
                    break;
                default: *ptr2++ = *ptr; break;
            }
            ++ptr;
        }
    }
    *ptr2 = 0;
    if (*ptr == '\"') ++ptr;
    item->sValue = out;
    item->type = CJSON_String;
    return ptr;
}

static const char* parse_array(CJson *item, const char *value) {
    CJson *child;
    if (*value != '[') {
        ep = value;
        return NULL;
    }
    item->type = CJSON_Array;
    value = skip(value + 1);
    if (*value == ']') return value + 1; // empty array

    item->child = child = cJson_new_item();
    if (!item->child) return NULL;
    value = skip(parse_value(child, skip(value)));
    if (!value) return NULL;

    while (*value == ',') {
        CJson *newItem;
        if (!(newItem = cJson_new_item())) return NULL;
        child->next = newItem;
        newItem->prev = child;
        child = newItem;
        value = skip(parse_value(child,skip(value + 1)));
        if (!value) return NULL;
    }
    if (*value == ']') return value + 1;
    ep = value;
    return NULL;
}

static const char* parse_object(CJson *item, const char *value) {
    CJson *child;
    if (*value != '{') {
        ep = value;
        return NULL;
    }
    item->type = CJSON_Object;
    value = skip(value + 1);
    if (*value == '}') return value + 1; // empty object

    item->child = child = cJson_new_item();
    if (!item->child) return NULL;
    value = skip(parse_value(child, skip(value)));
    if (!value) return NULL;
    child->string = child->sValue;
    child->sValue = NULL;
    if (*value != ':') { // 失败
        ep = value;
        return NULL;
    }
    value = skip(parse_value(child, skip(value + 1)));
    if (!value) return NULL;

    while (*value == ',') {
        CJson *newItem;
        if (!(newItem = cJson_new_item())) return NULL;
        child->next = newItem;
        newItem->prev = child;
        child = newItem;
        value = skip(parse_string(child, skip(value + 1)));
        if (!value) return NULL;
        child->string = child->sValue;
        child->sValue = NULL;
        if (*value != ':') { // 失败
            ep = value;
            return NULL;
        }
        if (!value) return NULL;
    }
    if (*value == '}') return value + 1;
    ep = value;
    return NULL;
}

static const char* parse_value(CJson *item, const char *value) {
    if (!value) return 0;
    if (!strncmp(value, "false", 5)) {
        item->type = CJSON_False;
        return value + 5;
    }
    if (!strncmp(value, "true", 4)) {
        item->type = CJSON_True;
        return value + 4;
    }
    if (!strncmp(value, "null", 4)) {
        item->type = CJSON_NULL;
        return value + 4;
    }
    if (*value == '\"') return parse_string(item, value);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);
    if (*value == '[') return parse_array(item, value);
    if (*value == '{') return parse_object(item, value);
    // 失败
    ep = value;
    return NULL;
}

CJson* cJson_ParseWithOpts(const char *value, const char **returnParseEnd,  int requireNullTerminated) {
    const char *end = 0;
    CJson *cj = cJson_new_item();
    ep = 0;
    if (!cj) return NULL; // 失败
    
    end = parse_value(cj, skip(value));
    if (!end) { // 失败
        cJson_Delete(cj);
        return NULL;
    }

    if (requireNullTerminated) {
        end = skip(end);
        if (*end) {
            cJson_Delete(cj);
            ep = end;
            return NULL;
        }
    }
    if (returnParseEnd) *returnParseEnd = end;
    return cj;
}

CJson* cJson_Parse(const char *value) {
    return cJson_ParseWithOpts(value, 0, 0);
}

/* -------------------------------------------------------------------------- */
/*                                   printer                                  */
/* -------------------------------------------------------------------------- */

static char* print_number(CJson *item, PrintBuffer *p) {
    char *str = NULL;
    double d = item->dValue;
    if (d == 0) {
        if (p) str = ensure(p, 2);
        else str = (char *) cJson_malloc(2);
        if (str) strcpy(str, "0");
    } else if (fabs(((double) item->iValue) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
        if (p) str = ensure(p, 21);
        else str = (char *) cJson_malloc(21);
        if (str) sprintf(str, "%d", item->iValue);
    } else {
        if (p) str = ensure(p, 64);
        else str = (char *) cJson_malloc(64);
        if (str) {
            if (fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60) sprintf(str, "%.0f", d);
            else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9) sprintf(str, "%e", d);
            else sprintf(str, "%f", d);
        }
    }
    return str;
}

static char* print_string_ptr(const char *str, PrintBuffer *p) {
    const char *ptr;
    char *ptr2, *out;
    int len = 0, flag = 0;
    unsigned char token;

    for (ptr = str; *ptr; ptr++) {
        flag |= ((*ptr > 0 && *ptr < 32) || (*ptr == '\"') || (*ptr == '\\')) ? 1 : 0;
    }
    if (!flag) {
        len = ptr - str;
        if (p) out = ensure(p, len + 3);
        else out = (char *) cJson_malloc(len + 3);
        if (!out) return NULL;
        ptr2 = out;
        *ptr2++ = '\"';
        strcpy(ptr2, str);
        ptr2[len] = '\"';
        ptr2[len + 1] = 0;
        return out;
    }

    if (!str) {
        if (p) out = ensure(p, len + 3);
        else out = (char *) cJson_malloc(len + 3);
        if (!out) return NULL;
        strcpy(out, "\"\"");
        return out;
    }

    if (p) out = ensure(p, len + 3);
    else out = (char *) cJson_malloc(len + 3);
    if (!out) return NULL;

    ptr2 = out;
    ptr = str;
    *ptr2++ = '\"';
    while (*ptr) {
        if ((unsigned char) *ptr > 31 && *ptr != '\"' && *ptr != '\\') {
            *ptr2++ = *ptr++;
        } else {
            *ptr2++ = '\\';
            switch (token = *ptr++) {
                case '\\': *ptr2++ = '\\'; break;
                case '\"': *ptr2++ = '\"'; break;
                case '\b': *ptr2++ = 'b';  break;
                case '\f': *ptr2++ = 'f';  break;
                case '\n': *ptr2++ = 'n';  break;
                case '\r': *ptr2++ = 'r';  break;
                case '\t': *ptr2++ = 't';  break;
                default:
                    sprintf(ptr2, "u%04x", token);
                    ptr2 += 5;
                    break;
            }
        }
    }
    *ptr2++ = '\"';
    *ptr2++ = 0;
    return out;
}

static char* print_string(CJson *item, PrintBuffer *p) {
    return print_string_ptr(item->sValue, p);
}

static char* print_array(CJson *item, int depth, int fmt, PrintBuffer *p) {
    char **entries;
    char *out = NULL, *ptr, *ret;
    int len = 5, i = 0;
    CJson *child = item->child;
    int entryNum = 0, fail = 0;
    size_t tmplen = 0;

    while (child) child = child->next, ++entryNum;
    if (!entryNum) {
        if (p) out = ensure(p, 3);
        else out = (char *) cJson_malloc(3);
        if (out) strcpy(out, "[]");
        return out;
    }

    if (p) {
        i = p->offset;
        ptr = ensure(p, 1);
        if (!ptr) return NULL;
        *ptr = '[';
        p->offset++;
        child = item->child;
        while (child && !fail) {
            print_value(child, depth + 1, fmt, p);
            p->offset = update(p);
            if (child->next) {
                len = fmt ? 2 : 1;
                ptr = ensure(p, len + 1);
                if (!ptr) return NULL;
                *ptr++ = ',';
                if (fmt) *ptr++ = ' ';
                *ptr = 0;
                p->offset += len;
            }
            child = child->next;
        }
        ptr = ensure(p, 2);
        if (!ptr) return NULL;
        *ptr++ = ']';
        *ptr = 0;
        out = (p->buffer) + i;
    } else {
        entries = (char **) cJson_malloc(entryNum * sizeof(char *));
        if (!entries) return NULL;
        memset(entries, 0x00, entryNum * sizeof(char *));
        child = item->child;
        while (child && !fail) {
            ret = print_value(child, depth + 1, fmt, 0);
            entries[i++] = ret;
            if (ret) len += strlen(ret) + 2 + (fmt ? 1 : 0);
            else fail = 1;
            child = child->next;
        }
        if (!fail) out = (char *) cJSON_malloc(len);
        if (!out) fail = 1;
        if (fail) {
            for (i = 0; i < entryNum; i++) {
                if (entries[i]) cJson_free(entries[i]);
            }
            cJson_free(entries);
            return NULL;
        }
        *out = '[';
        ptr = out + 1;
        *ptr = 0;
        for (i = 0; i < entryNum; i++) {
            tmplen = strlen(entries[i]);
            memcpy(ptr, entries[i], tmplen);
            ptr += tmplen;
            if (i != entryNum - 1) {
                *ptr++ = ',';
                if (fmt) *ptr++ = ' ';
                *ptr = 0;
            }
            cJson_free(entries[i]);
        }
        cJson_free(entries);
        *ptr++ = ']';
        *ptr = 0;
    }
    return out;
}

static char* print_object(CJson *item, int depth, int fmt, PrintBuffer *p) {
    char **entries = NULL, **names = NULL;
    char *out = NULL, *ptr, *ret, *str;
    int len = 7, i = 0, j;
    CJson *child = item->child;
    int entryNum = 0, fail = 0;
    size_t tmplen = 0;

    while (child) child = child->next, ++entryNum;
    if (!entryNum) {
        if (p) out = ensure(p, fmt ? depth + 4 : 3);
        else out = (char *) cJson_malloc(fmt ? depth + 4 : 3);
        if (!out) return NULL;
        ptr = out;
        *ptr++ = '{';
        if (fmt) {
            *ptr++ = '\n';
            for (i = 0; i < depth - 1; i++) *ptr++ = '\t';
        }
        *ptr++ = '}';
        *ptr++ = 0;
        return out;
    }

    if (p) {
        i = p->offset;
        len = fmt ? 2 : 1;
        ptr = ensure(p, len + 1);
        if (!ptr) return NULL;
        *ptr++ = '{';
        if (fmt) *ptr++ = '\n';
        *ptr = 0;
        p->offset += len;
        child = item->child;
        ++depth;
        while (child) {
            if (fmt) {
                ptr = ensure(p, depth);
                if (!ptr) return NULL;
                for (j = 0; j < depth; j++) *ptr++ = '\t';
                p->offset += depth;
            }
            print_string_ptr(child->string, p);
            p->offset = update(p);
            
            len = fmt ? 2 : 1;
			ptr = ensure(p, len);
            if (!ptr) return NULL;
			*ptr++ = ':';
            if (fmt) *ptr++ = '\t';
			p->offset += len;

            print_value(child, depth, fmt, p);
            p->offset = update(p);

            len = (fmt ? 1 : 0) + (child->next ? 1 : 0);
			ptr = ensure(p, len + 1);
            if (!ptr) return NULL;
			if (child->next) *ptr++ = ',';
			if (fmt) *ptr++ = '\n';
            *ptr = 0;
			p->offset += len;
			child = child->next;
        }
		ptr = ensure(p, fmt ? (depth + 1) : 2);
        if (!ptr) return 0;
		if (fmt) {
            for (i = 0; i < depth - 1; i++) *ptr++ = '\t';
        }
		*ptr++ = '}';
        *ptr = 0;
		out = (p->buffer) + i;
    } else {
        entries = (char **) cJSON_malloc(entryNum * sizeof(char *));
        if (!entries) return NULL;
        names = (char **) cJSON_malloc(entryNum * sizeof(char *));
        if (!names) {
            cJson_free(entries);
            return NULL;
        }
        memset(entries, 0x00, sizeof(entryNum * sizeof(char *)));
        memset(names, 0x00, sizeof(entryNum * sizeof(char *)));

        child = item->child;
        ++depth;
        if (fmt) len += depth;
        while (child) {
			names[i] = str = print_string_ptr(child->string, 0);
			entries[i++] = ret = print_value(child, depth, fmt, 0);
			if (str && ret) len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0);
            else fail = 1;
			child = child->next;
        }

        if (!fail) out = (char *) cJSON_malloc(len);
		if (!out) fail = 1;

        if (fail) {
            for (i = 0; i < entryNum; i++) {
                if (names[i]) cJson_free(names[i]);
                if (entries[i]) cJson_free(entries[i]);
            }
            cJson_free(names);
            cJson_free(entries);
            return NULL;
        }

		*out = '{';
        ptr = out + 1;
        if (fmt) *ptr++ = '\n';
        *ptr = 0;
		for (i = 0;i < entryNum; i++) {
			if (fmt) {
                for (j = 0; j < depth; j++) *ptr++ = '\t';
            }
			tmplen = strlen(names[i]);
            memcpy(ptr, names[i], tmplen);
            ptr += tmplen;
			*ptr++ = ':';
            if (fmt) *ptr++ = '\t';
			strcpy(ptr, entries[i]);
            ptr += strlen(entries[i]);
			if (i != entryNum-1) *ptr++ = ',';
			if (fmt) *ptr++ = '\n';
            *ptr=0;
			cJSON_free(names[i]);
            cJSON_free(entries[i]);
		}
		
		cJSON_free(names);
        cJSON_free(entries);
		if (fmt) {
            for (i = 0; i < depth - 1; i++) *ptr++ = '\t';
        }
		*ptr++ = '}';
        *ptr++ = 0;
    }
    return out;
}

static char* print_value(CJson *item, int depth, int fmt, PrintBuffer *p) {
    char *out = NULL;
    if (!item) return NULL;
    if (p) {
        switch ((item->type) & 255) {
            case CJSON_False:
                out = ensure(p, 6);
                if (out) strcpy(out, "false");
                break;
            case CJSON_True:
                out = ensure(p, 5);
                if (out) strcpy(out, "true");
                break;
            case CJSON_NULL:
                out = ensure(p, 5);
                if (out) strcpy(out, "null");
                break;
            case CJSON_Number: out = print_number(item, p); break;
            case CJSON_String: out = print_string(item, p); break;
            case CJSON_Array:  out = print_array(item, depth, fmt, p); break;
            case CJSON_Object: out = print_object(item, depth, fmt, p); break;
        }
    } else {
        switch ((item->type) & 255) {
            case CJSON_False:  out = cJson_strdup("false"); break;
            case CJSON_True:   out = cJson_strdup("true");  break;
            case CJSON_NULL:   out = cJson_strdup("null");  break;
            case CJSON_Number: out = print_number(item, 0); break;
            case CJSON_String: out = print_string(item, 0); break;
            case CJSON_Array:  out = print_array(item, depth, fmt, 0); break;
            case CJSON_Object: out = print_object(item, depth, fmt, 0); break;
        }
    }
    return out;
}

char* cJson_Print(CJson *item) {
    return print_value(item, 0, 1, 0);
}

char* cJson_PrintBuffered(CJson *item, int preBuffer, int fmt) {
    PrintBuffer p;
    p.buffer = (char *) cJson_malloc(preBuffer);
    p.length = preBuffer;
    p.offset = 0;
    return print_value(item, 0, fmt, &p);
    // return p.buffer; // ???????
}

char* cJson_PrintUnformatted(CJson *item) {
    return print_value(item, 0, 0, 0);
}