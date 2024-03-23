/*
    严格遵守 C 的语法，不用 C++
*/

#ifndef CJSON_H
#define CJSON_H

// #include <stdlib.h>

// 按照 C 语言的链接规范进行处理
#ifdef __cplusplus
extern "C"
{
#endif

// 类型
#define CJSON_False  0
#define CJSON_True   1
#define CJSON_NULL   2
#define CJSON_Number 3
#define CJSON_String 4
#define CJSON_Array  5
#define CJSON_Object 6

#define cJson_IsReference 256
#define cJson_IsConstString 512

// CJson 结构体
typedef struct CJson {
    struct CJson *next;
    struct CJson *prev;
    struct CJson *child; // 对于Array/Object，child指向这个Array/Object
    
    int type; // 类型

    // ? 能不能用 union
    char *sValue;
    int iValue;
    double dValue;

    char *string;
} CJson;

// ? 自动机回退 ??
typedef struct CJson_Hooks {
    // 两个函数指针
    void *(*malloc_fn)(size_t sz);
    void (*free_fn)(void *ptr);
} CJson_Hooks;


extern void cJson_Hooks_Init(CJson_Hooks* hooks);

extern CJson* cJson_Parse(const char *value);

extern char* cJson_Print(CJson *item);
extern char* cJson_PrintUnformatted(CJson *item);
extern char* cJson_PrintBuffered(CJson *item, int prebuff, int fmt);

// 删除一个CJson实体及其所有子实体
extern void cJson_Delete(CJson *cj);

extern int cJson_GetArraySize(CJson *array);
extern CJson* cJson_GetArrayItem(CJson *array, int item);
extern CJson* cJson_GetObjectItem(CJson *object, const char *string);

// 用于分析解析失败的情况
extern const char* cJson_GetErrorPtr(void);

// 创建一个对应类型的CJson项
extern CJson* cJson_CreateFalse(void);
extern CJson* cJson_CreateTrue(void);
extern CJson* cJson_CreateBool(int b);
extern CJson* cJson_CreateNull(void);
extern CJson* cJson_CreateNumber(double num);
extern CJson* cJson_CreateString(const char *string
);
extern CJson* cJson_CreateArray(void);
extern CJson* cJson_CreateObject(void);

// 创建有count个项的Array
extern CJson* cJson_CreateIntArray(const int *numbers, int count);
extern CJson* cJson_CreateFloatArray(const float *numbers, int count);
extern CJson* cJson_CreateDoubleArray(const double *numbers, int count);
extern CJson* cJson_CreateStringArray(const char **strings, int count);

// 向对应的Array/Object添加项
extern void cJson_AddItemToArray(CJson *array, CJson *item);
extern void cJson_AddItemToObject(CJson *object, const char *string, CJson *item);
extern void cJson_AddItemToObjectCS(CJson *object, const char *string, CJson *item); // Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the cJSON object

// 快速添加Object的宏
#define cJson_AddFalseToObject(object, name)        cJson_AddItemToObject(object, name, cJson_CreateFalse())
#define cJson_AddTrueToObject(object, name)         cJson_AddItemToObject(object, name, cJson_CreateTrue())
#define cJson_AddBoolToObject(object, name, b)      cJson_AddItemToObject(object, name, cJson_CreateBool(b))
#define cJson_AddNullToObject(object, name)         cJson_AddItemToObject(object, name, cJson_CreateNull())
#define cJson_AddNumberToObject(object, name, n)    cJson_AddItemToObject(object, name, cJson_CreateNumber(n))
#define cJson_AddStringToObject(object, name, s)    cJson_AddItemToObject(object, name, cJson_CreateString(s))


// 向对应的Array/Object添加已有项（不会破坏原有的CJson项）
extern void cJson_AddItemReferenceToArray(CJson *array, CJson *item);
extern void cJson_AddItemReferenceToObject(CJson *object, const char *string, CJson *item);

// 从Array/Object上删除/断开CJson项
extern CJson* cJson_DetachItemFromArray(CJson *array, int which);
extern void cJson_DeleteItemFromArray(CJson *array, int which);
extern CJson* cJson_DetachItemFromObject(CJson *object, const char *string);
extern void cJson_DeleteItemFromObject(CJson *object, const char *string);

// 更新数组的项
extern void cJson_InsertItemInArray(CJson *array, int which, CJson *newItem); // 将已有项右移
extern void cJson_ReplaceItemInArray(CJson *array, int which, CJson *newItem);

// 拷贝一个CJson项 // ? 拷贝？duplicate
extern CJson* cJson_Duplicate(CJson *item, int recurse); // ? recurse???
// TODO: 函数说明

extern CJson* cJson_ParseWithOpts(const char *value, const char **returnParseEnd, int requireNullTerminated);

// ? 缩小 ??
extern void cJson_Minify(char *json);

// 当赋予整型值时，也要传播到dValue
#define cJson_SetIntValue(object, value)    ((object) ? (object)->iValue = (object)->dValue = (value) : (value) )
#define cJson_SetNumberValue(object, value) ((object) ? (object)->iValue = (object)->dValue = (value) : (value) )

#ifdef __cplusplus
}
#endif

#endif