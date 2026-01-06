import ctranslate2
import transformers

# モデルのパス
model_path = "marian-en-jap-ct2"
# 元のHugging Faceモデル名
hf_model_id = "Helsinki-NLP/opus-mt-en-jap"

translator = ctranslate2.Translator(model_path, device="cpu")
tokenizer = transformers.AutoTokenizer.from_pretrained(hf_model_id)

def translate(text):
    # 【重要】src_textsとして渡し、正しい特殊トークンを付与させる
    # return_tensors=None にしてリスト形式で取得
    tokens = tokenizer(text, add_special_tokens=True).tokens()
    
    # 翻訳実行
    results = translator.translate_batch([tokens])
    output_tokens = results[0].hypotheses[0]
    
    # IDに変換してからデコード
    output_ids = tokenizer.convert_tokens_to_ids(output_tokens)
    return tokenizer.decode(output_ids, skip_special_tokens=True)

print(f"apple -> {translate('apple')}")
print(f"Hello -> {translate('Hello, how are you today?')}")