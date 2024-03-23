#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <float.h>
#include "cjson.h"

/* ------------------------------- parameters ------------------------------- */

static void *(*cJson_malloc)(size_t sz) = malloc;
static void (*cJson_free)(void *ptr) = free;

static const char *ep;

/* ---------------------------- static functions ---------------------------- */

// extern CJson* cJson_CreateFalse(void);
// extern CJson* cJson_CreateTrue(void);
// extern CJson* cJson_CreateBool(int b);
// extern CJson* cJson_CreateNull(void);
// extern CJson* cJson_CreateNumber(double num);
// extern CJson* cJson_CreateString(const char *string);
// extern CJson* cJson_CreateArray(void);
// extern CJson* cJson_CreateObject(void);

static CJson* cJson_new_item(void) {
    CJson *nd = (CJson*) cJson_malloc(sizeof(CJson));
    if (nd) {
        memset(nd, 0x00, sizeof(CJson));
    }
    return nd;
}

static char* cJson_strdup(const char* str) {
    size_t len;
    char *copy;
    len = strlen(str) + 1;
    if (!(copy = (char*) cJson_malloc(len))) {
        return NULL;
    }
    memcpy(copy, str, len);
    return copy;
}

static void suffix_object(CJson *prev, CJson *item) {
    prev->next = item;
    item->prev = prev;
}

/* -------------------------------- functions ------------------------------- */

CJson* cJson_CreateFalse(void) {
    CJson *item = cJson_new_item();
    if (item) {
        item->type = CJSON_False;
    }
    return item;
}

CJson* cJson_CreateFalse(void) {
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

const char *cJson_GetErrorPtr(void) {
    return ep;
}

