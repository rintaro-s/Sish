#!/bin/bash
echo "=== 口調テスト ==="
for i in {0..6}; do
  echo ""
  echo "--- SISH_TONE=$i ---"
  SISH_TONE=$i /tmp/sish-test/bin/zsh -c 'hit clone' 2>&1 | head -3
done
