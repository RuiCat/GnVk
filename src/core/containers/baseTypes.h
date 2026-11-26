// GN_mini 容器片段的最小类型基础：与 GN.h 的原始 typedef 一致（C++11 允许相同重声明）。
// 存在原因：uint8/int8 等定义在 GN.h 伞头里，容器片段不能反向包含 GN.h（循环）。
// GN.h 的 typedef 抽离到独立头是后续 C10 任务（REFACTOR_PLAN B9）。
#ifndef GN_CONTAINERS_BASETYPES_H
#define GN_CONTAINERS_BASETYPES_H
typedef signed char int8;
typedef signed long long int64;
typedef unsigned char uint8;
typedef unsigned long long uint64;
#ifndef GN_API
#define GN_API // Linux 口径（GN_SysDef.h 同值）；Windows 口径由 GN_SysDef.h 先行定义
#endif
#endif // GN_CONTAINERS_BASETYPES_H
