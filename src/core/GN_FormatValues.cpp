// GN_FormatValues.cpp — 变量值调整信息（sADJUST_VALUE_*）与值序列化/打印（原 GN_Format.cpp 拆分）。
#include "GN.h"
#ifdef _GN_FORMAT_H_
    /******************************************************* 变量值调整信息 ************************************************************/
    GN::sADJUST_VALUE_GROUP::~sADJUST_VALUE_GROUP() {
        if(args) {
            uint8 i = 0;
            for(; i < argn; ++i) {
                switch(args[i]->type) {
                    case eVALUE_INT: delete(sADJUST_VALUE_INT *)args[i]; break;
                    case eVALUE_FLOAT: delete(sADJUST_VALUE_FLOAT *)args[i]; break;
                    case eVALUE_ULONG: delete(sADJUST_VALUE_ULONG *)args[i]; break;
                    case eVALUE_BYTE: delete(sADJUST_VALUE_BYTE *)args[i]; break;
                    default: delete args[i]; break;
                }
            }
            delete[] args;
            args = 0;
        }
    }
    GN_API void GN::ValuePrint(sVALUE_NAME *value, void *v, void *ptv) {
        switch(value->type) {

#define GT_TEMP_VALUE_PRINT_CASE(TYPE, T)                                                                        \
    case GN::eVALUE_##TYPE: {                                                                                    \
        GN::sADJUST_VALUE_##TYPE *var = (GN::sADJUST_VALUE_##TYPE *)value;                                       \
        int v1 = (int)(*(ptv ? (T *)v : var->ptr));                                                              \
        if(var->enum_name) {                                                                                     \
            P<P<char>> enum_names = GN_StrSplitToData(var->enum_name, '|');                                      \
            GN_Log("  %s = %s\n", var->name, (v1 < (int)enum_names.Num() ? enum_names[v1].Begin() : "无效值!")); \
        } else {                                                                                                 \
            GN_Log("  %s = %d\n", var->name, v1);                                                                \
        }                                                                                                        \
    } break;

            GT_TEMP_VALUE_PRINT_CASE(BYTE, uint8)
            GT_TEMP_VALUE_PRINT_CASE(INT, int)
            case GN::eVALUE_FLOAT: {
                GN::sADJUST_VALUE_FLOAT *var = (GN::sADJUST_VALUE_FLOAT *)value;
                GN_Log("  %s = %f\n", var->name, *(ptv ? (float *)v : var->ptr));
            } break;
            case GN::eVALUE_ULONG: {
                GN::sADJUST_VALUE_ULONG *var = (GN::sADJUST_VALUE_ULONG *)value;
                GN_Log("  %s = %08x\n", var->name, *(ptv ? (unsigned int *)v : var->ptr));
            } break;
        }
    }
    GN_API void GN::ValuesNotEqualExec(const uint8 *data, sVALUE_NAME **args, uint8 argn, void (*proc)(sVALUE_NAME *, void *, void *), void *us_arg) {
#define GT_TEMP_VALUES_FIND_SIZE(PROC)                     \
    uint8 *ptr = (uint8 *)data;                            \
    void *v;                                               \
    unsigned int tsize;                                    \
    uint8 i = 0;                                           \
    for(; i < argn; ++i) {                                 \
        tsize = ValueTypeSize((eVALUE_TYPE)args[i]->type); \
        v = ((GN::sADJUST_VALUE_ULONG *)args[i])->ptr;     \
        PROC;                                              \
        ptr += tsize;                                      \
    }
        GT_TEMP_VALUES_FIND_SIZE(if(memcmp(v, ptr, tsize)) proc(args[i], ptr, us_arg))
    }
    GN_API unsigned int GN::ValuesRead(const uint8 *data, sVALUE_NAME **args, uint8 argn) {
        GT_TEMP_VALUES_FIND_SIZE(memcpy(v, ptr, tsize))
        return ptr - data;
    }
    GN_API unsigned int GN::ValuesWrite(uint8 *data, sVALUE_NAME **args, uint8 argn) {
        uint8 *ptr = (uint8 *)data;
        void *v;
        unsigned int tsize;
        uint8 i = 0;
        if(data) {
            for(; i < argn; ++i) {
                tsize = ValueTypeSize((eVALUE_TYPE)args[i]->type);
                v = ((GN::sADJUST_VALUE_ULONG *)args[i])->ptr;
                memcpy(ptr, v, tsize);
                ptr += tsize;
            }
        } else {
            for(; i < argn; ++i) {
                tsize = ValueTypeSize((eVALUE_TYPE)args[i]->type);
                ptr += tsize;
            }
        }
        return ptr - data;
    }
#endif //_GN_FORMAT_H_
