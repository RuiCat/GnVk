#!/bin/bash
cd "$(dirname "$0")"
TEST="${1:-01}"
EXE="build/test_${TEST}"

if [ ! -f "$EXE" ]; then
  echo "❌ 找不到: $EXE"
  echo ""
  echo "用法: ./run.sh <编号>"
  echo ""
  echo "可用测试:"
  for f in build/test_*; do
    [ -f "$f" ] && echo "  $(basename "$f")"
  done
  echo ""
  echo "编译: make test N=<编号>"
  exit 1
fi

export LD_LIBRARY_PATH=".:$LD_LIBRARY_PATH"
echo "🚀 运行: test_${TEST}"
exec "$EXE"
