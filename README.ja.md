# haorender

haorender は C++ で実装した CPU ソフトウェアレンダラーです。モデル読み込み、カメラ制御、ラスタライズ、深度バッファ、シャドウマップ、Phong シェーディング、初期段階の PBR マテリアル対応など、レンダリングパイプライン全体を学習・実験するためのプロジェクトです。

このリポジトリは本番向けエンジンではなく、学習と検証を重視しています。現在の目的は、パイプラインを見通しやすく、デバッグしやすい形で保ちながら、OpenCV ウィンドウ上で実モデルをインタラクティブに確認できる性能を確保することです。

## 他の言語

- [English](README.md)
- [简体中文](README.zh-CN.md)

## 機能

- Assimp による FBX/OBJ 系モデルの読み込み
- OpenCV ウィンドウへの描画とマウスによるカメラ操作
- 透視投影、ビューポート変換、背面カリング、Z バッファ
- カメラがモデルに近づいたときに巨大なスクリーン領域へ投影される三角形を避けるための視錐台クリッピング
- tile ベースのラスタライズと OpenMP 並列化
- diffuse、normal、specular、および一部 PBR テクスチャの読み込み
- metallic、roughness、AO、emissive の PBR チャンネルマッピング切り替え
- sRGB/linear 変換、トーンマッピング、法線強度制御、安定したスペキュラー応答を含む Phong シェーディング
- 高解像度シャドウマップ、近距離/遠距離カスケード、グラデーション PCF によるソフトシャドウ

## 必要環境

- C++17 コンパイラ
- CMake 3.10+
- OpenCV
- Assimp
- Eigen
- OpenMP 対応コンパイラ、推奨

Windows 環境では、現在プロジェクト直下の `assimp-vc143-mtd.dll` を利用しています。Assimp のインストール場所が異なる場合は、`CMakeLists.txt` を調整するか、CMake 設定時に正しい include/CMake パスを指定してください。

## ビルド

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Eigen または Assimp が見つからない場合は、明示的にパスを渡します。

```powershell
cmake -S . -B build -DEIGEN3_INCLUDE_DIR="path\to\eigen3" -DASSIMP_INCLUDE_DIR="path\to\assimp\include"
cmake --build build --config Release
```

## 実行

ビルド出力ディレクトリで実行します。

```powershell
.\myrender.exe
```

モデルパスを指定することもできます。

```powershell
.\myrender.exe ..\Resources\MAIFU\IF.fbx
```

`main.cpp` のデフォルトモデルパスは次の通りです。

```text
../Resources/MAIFU/IF.fbx
```

## 操作

- 左ドラッグ：カメラを回転
- 右ドラッグ：カメラをパン
- マウスホイール：ズーム
- `r`：カメラをリセット
- `w`、`a`、`s`、`d`：モデルを簡易回転
- `1`、`2`、`3`：PBR チャンネルマッピングのプリセット切り替え
- `Esc`：終了

## 現在のメモ

- 現在のレンダラーは CPU ベースで、学習とデバッグのために多くのパイプライン処理をプロジェクト内に残しています。
- PBR パスは基本実装がありますが、多くのモデルは diffuse テクスチャのみを持つため、Phong パスも重要です。
- シャドウはカスケード分割と PCF により改善されていますが、shadow map 生成はまだ主要な性能コストです。
- 次の大きな拡張として、現在のモデル/マテリアル読み込みを共有する OpenGL Renderer などの GPU バックエンドが有力です。

## ディレクトリ構成

```text
include/        モデル、レンダラー、シェーダー、描画補助、画像補助のヘッダー
Resources/      サンプルモデルとテクスチャ
main.cpp        OpenCV アプリケーションループとカメラ操作
model.cpp       Assimp によるモデル、マテリアル、テクスチャ読み込み
render.cpp      レンダリング制御、シャドウ pass、tile 分割、フレーム描画
shader.cpp      ラスタライズ、クリッピング、フラグメントシェーディング、シャドウ、PBR/Phong ロジック
Drawer.cpp      描画補助関数
tgaimage.cpp    TGA 画像サポート
```

## License

現時点では明示的なライセンスは追加されていません。公開配布や外部コントリビューションを受け入れる前に、LICENSE ファイルを追加することを推奨します。
