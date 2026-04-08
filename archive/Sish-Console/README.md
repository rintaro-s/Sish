# Sish Console

Sish Console は GTK4 + VTE4 で作る「Sish 専用のモダンなターミナル」です。

## 必要な依存（Linux / Debian・Ubuntu系）

GTK4 と VTE4 は Rust crate だけでは入らないので、先に OS 側の開発パッケージが必要です。

```bash
sudo apt update
sudo apt install -y \
  build-essential pkg-config \
  libgtk-4-dev \
  libvte-2.91-gtk4-dev \
  libgraphene-1.0-dev \
  libcairo2-dev \
  libpango1.0-dev
```

ディストリによってパッケージ名が違う場合があります（その場合は `gtk4.pc` / `vte-2.91-gtk4.pc` が入るパッケージを入れてください）。

## ビルド

```bash
cd Sish-Console
cargo build
```

## 実行

```bash
cd Sish-Console
cargo run
```

## よくあるエラー

- `The system library gtk4 was not found`:
  - `libgtk-4-dev` が未導入です。
- `graphene-gobject-1.0.pc was not found`:
  - `libgraphene-1.0-dev` が未導入です。
