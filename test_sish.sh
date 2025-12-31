#!/home/rinta/Documents/github/Sish/zsh-5.9/Src/zsh
# Sishテストスクリプト

echo "🌸 Sish (Sister Shell) テスト 🌸"
echo ""

echo "✅ 1. 通常のコマンド実行"
ls /tmp > /dev/null && echo "   成功"

echo ""
echo "❌ 2. コマンド未検出エラー"
giu

echo ""
echo "❌ 3. パーミッションエラー"
cat /etc/shadow

echo ""
echo "❌ 4. ファイル未検出"
cat /nonexistent-file.txt

echo ""
echo "❌ 5. ゼロ除算"
echo $((10 / 0))

echo ""
echo "テスト完了"
