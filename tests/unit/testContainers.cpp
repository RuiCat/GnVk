// 容器单测：sLIST / sARRAY_VEC / cTREE2 / cMEM_POOL1。
#include "testFramework.h"
#include <GN.h>

TEST_CASE(containerListPushBack) {
    GN::sLIST<int> list;
    list.push_back(3);
    list.push_back(1);
    list.push_back(2);
    CHECK_EQ(list.size(), 3u);
    CHECK_EQ(list.front(), 3);
    CHECK_EQ(list.back(), 2);
}

TEST_CASE(containerListCopyKeepsSource) {
    GN::sLIST<int> list;
    list.push_back(1);
    list.push_back(2);

    GN::sLIST<int> copied(list);
    CHECK_EQ(copied.size(), 2u);
    copied.push_back(3);
    CHECK_EQ(copied.size(), 3u);
    CHECK_EQ(list.size(), 2u);
}

TEST_CASE(containerListRemove) {
    GN::sLIST<int> list;
    list.push_back(1);
    list.push_back(2);
    list.push_back(1);
    list.remove(1);
    CHECK_EQ(list.size(), 1u);
    CHECK_EQ(list.front(), 2);
}

TEST_CASE(containerListSort) {
    GN::sLIST<int> list;
    list.push_back(3);
    list.push_back(1);
    list.push_back(2);
    list.sort();
    CHECK_EQ(list.size(), 3u);
    CHECK_EQ(list.front(), 1);
    CHECK_EQ(list.back(), 3);
}

TEST_CASE(containerListClear) {
    GN::sLIST<int> list;
    list.push_back(7);
    list.clear();
    CHECK(list.empty());
    CHECK_EQ(list.size(), 0u);
}

TEST_CASE(containerArrayVecPush) {
    GN::sARRAY_VEC<int> array;
    array.push_back(7);
    array.push_back(9);
    CHECK_EQ(array.size(), 2u);
    CHECK_EQ(array[0], 7);
    CHECK_EQ(array[1], 9);
}

TEST_CASE(containerArrayVecPopBack) {
    GN::sARRAY_VEC<int> array;
    array.push_back(7);
    array.push_back(9);
    int value = 0;
    array.pop_back(value);
    CHECK_EQ(value, 9);
    CHECK_EQ(array.size(), 1u);
}

TEST_CASE(containerTreeInsertSearch) {
    GN::cTREE2<int> tree;
    int value = 42;
    GN::cTREE2<int>::TP *node = tree.insert(value);
    CHECK(node != 0);
    CHECK(*node != 0);
    if (node && *node) CHECK_EQ((*node)->zData, 42);

    GN::cTREE2<int>::TP *found = 0;
    int key = 42;
    CHECK_EQ(tree.search2_if(GT_CompareNodeNumber<int>, key, &found), 0);
    CHECK(found != 0 && *found != 0);
    if (found && *found) CHECK_EQ((*found)->zData, 42);
}

TEST_CASE(containerTreeErase) {
    GN::cTREE2<int> tree;
    int value = 42;
    tree.insert(value);
    CHECK(tree.erase(value));

    GN::cTREE2<int>::TP *found = 0;
    int key = 42;
    CHECK_EQ(tree.search2_if(GT_CompareNodeNumber<int>, key, &found), -1);
    CHECK(found == 0 || *found == 0);
}

TEST_CASE(containerMemPoolAllocFree) {
    GN::cMEM_POOL1 pool;
    pool.InitToData(4);
    uint8 *first = pool.Alloc();
    uint8 *second = pool.Alloc();
    CHECK(first != 0);
    CHECK(second != 0);
    CHECK(first != second);
    pool.Free(first);
    pool.Free(second);
    pool.Release();
}

TEST_CASE(containerListPushFrontPopFront) {
    GN::sLIST<int> list;
    list.push_front(1);
    list.push_front(2);
    CHECK_EQ(list.size(), 2u);
    CHECK_EQ(list.front(), 2);
    list.pop_front();
    CHECK_EQ(list.size(), 1u);
    CHECK_EQ(list.front(), 1);
}

TEST_CASE(containerArrayVecPushBackNeq) {
    GN::sARRAY_VEC<int> array;
    CHECK_EQ(array.push_back_neq(5), 0u);
    CHECK_EQ(array.size(), 1u);
    CHECK_EQ(array.push_back_neq(5), 0u);
    CHECK_EQ(array.size(), 1u);
    CHECK_EQ(array.push_back_neq(6), 1u);
    CHECK_EQ(array.size(), 2u);
}

TEST_CASE(containerTreeDuplicateInsert) {
    GN::cTREE2<int> tree;
    int value = 42;
    tree.insert(value);
    tree.insert(value);

    GN::cTREE2<int>::TP *found = 0;
    int key = 42;
    CHECK_EQ(tree.search2_if(GT_CompareNodeNumber<int>, key, &found), 0);
    CHECK(found != 0 && *found != 0);
    if (found && *found) CHECK_EQ((*found)->zData, 42);
}
