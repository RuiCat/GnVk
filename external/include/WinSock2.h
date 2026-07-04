/* Compatibility wrapper: WinSock2.h → winsock2.h
 * Windows headers use mixed case which works on case-insensitive NTFS
 * but fails when cross-compiling from case-sensitive Linux filesystems. */
#include <winsock2.h>
