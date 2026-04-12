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
- オプションの `Raster + Embree` シャドウモード。主レンダリングは従来どおりラスタライズし、遮蔽判定だけを Embree に任せます

## 必要環境

- C++17 コンパイラ
- CMake 3.10+
- OpenCV
- Assimp
- Eigen
- OpenMP 対応コンパイラ、推奨
- Embree 4、任意。ハイブリッドなラスタライズ + レイトレース陰影を使う場合に必要

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

Z バッファとシャドウマップで半精度深度を試す場合は、次のオプションを有効にします。

```powershell
cmake -S . -B build-half -DHAO_RENDER_DEPTH_HALF=ON
cmake --build build-half --config Release
```

半精度はメモリ帯域を削減できますが、深度精度のアーティファクトが増える可能性があります。デフォルトのビルドでは、より安全な性能基準として `float` 深度バッファを使用します。

頂点データの半精度格納を試す場合は、次のオプションを有効にします。

```powershell
cmake -S . -B build-vertex-half -DHAO_RENDER_VERTEX_HALF=ON
cmake --build build-vertex-half --config Release
```

このオプションでは、読み込んだ position、normal、tangent、bitangent、UV を `Eigen::half` として格納し、CPU の MVP 計算とシェーディングでは `float` に戻して使用します。一般的な CPU では主に頂点メモリ帯域の削減が目的であり、ハードウェアとコンパイラが半精度演算を効率よく実行できない限り、行列乗算が必ず高速化するとは限りません。

オプションの Embree バックエンドを有効にするには:

```powershell
cmake -S . -B build-embree -DHAO_RENDER_ENABLE_EMBREE=ON -DEMBREE_INCLUDE_DIR="path\to\embree\include" -DEMBREE_LIBRARY="path\to\embree4.lib"
cmake --build build-embree --config Release
```

Embree が見つからない場合でも、プロジェクトはそのままビルドされ、元の shadow map 経路へ自動で戻ります。

## 実行

ビルド出力ディレクトリで実行します。

```powershell
.\myrender.exe
```

モデルパスを指定することもできます。

```powershell
.\myrender.exe ..\Resources\MAIFU\IF.fbx
```

起動時にシャドウバックエンドを直接指定することもできます。

```powershell
.\myrender.exe ..\Resources\MAIFU\IF.fbx --shadow-technique=embree
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
- `4`: 元の shadow map 経路に切り替え
- `5`: Embree が利用可能なとき `Raster + Embree` 陰影モードに切り替え
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
