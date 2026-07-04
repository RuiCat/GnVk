#!/bin/bash
cd "$(dirname "$0")"
SAMPLE="${1:-Sample1}"
EXE="build/sample_${SAMPLE}"
if [ ! -f "$EXE" ]; then
  echo "❌ 找不到: $EXE"
  echo "可用示例:"
  ls build/sample_* 2>/dev/null | sed 's|build/sample_||' | tr '\n' ' '
  echo
  exit 1
fi
export LD_LIBRARY_PATH=".:$LD_LIBRARY_PATH"
echo "🚀 运行: $SAMPLE"
exec "$EXE"
