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
- presets を使う場合は CMake 3.20+ 推奨。手動設定では CMake 3.10+ に対応
- OpenCV
- Qt 5 Widgets
- Assimp
- Eigen
- OpenMP 対応コンパイラ、推奨
- Embree 4、任意。ハイブリッドなラスタライズ + レイトレース陰影を使う場合に必要

注意: リポジトリ直下の `assimp-vc143-mtd.dll` は過去の実行時 DLL であり、Assimp の開発パッケージではありません。`include/eigen3` と `include/assimp` も完全な SDK ではないため、ソースからビルドする場合は Eigen / Assimp / OpenCV / Qt5 の完全な開発パッケージをインストールしてください。

## ビルド

Windows では vcpkg manifest mode を推奨します。このリポジトリには `vcpkg.json` と `CMakePresets.json` が含まれています。

```powershell
git clone https://github.com/microsoft/vcpkg $env:USERPROFILE\vcpkg
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
& "$env:USERPROFILE\vcpkg\vcpkg.exe" install --triplet x64-windows
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"

cmake --preset windows-vcpkg-vs2022
cmake --build --preset windows-vcpkg-vs2022-release
```

依存関係を手動でインストールしている場合は、CMake package のパスを渡します。

```powershell
cmake --preset windows-vs2022-manual-deps `
  -DOpenCV_DIR="path\to\opencv\build" `
  -DQt5_DIR="path\to\Qt5\lib\cmake\Qt5" `
  -DEigen3_DIR="path\to\eigen3\share\eigen3\cmake" `
  -Dassimp_DIR="path\to\assimp\lib\cmake\assimp-5.x"
cmake --build --preset windows-vs2022-manual-release
```

CMake config package がない場合は、include / library を直接指定できます。

```powershell
cmake -S . -B build/vs2022-manual -G "Visual Studio 17 2022" -A x64 `
  -DOpenCV_DIR="path\to\opencv\build" `
  -DQt5_DIR="path\to\Qt5\lib\cmake\Qt5" `
  -DEIGEN3_INCLUDE_DIR="path\to\eigen3" `
  -DASSIMP_INCLUDE_DIR="path\to\assimp\include" `
  -DASSIMP_LIBRARY="path\to\assimp.lib" `
  -DHAO_RENDER_ENABLE_EMBREE=OFF
cmake --build build/vs2022-manual --config Release
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

## License / Third-Party Notices

haorender は Apache License 2.0 の下で公開されています。ライセンス全文は [LICENSE](LICENSE)、配布時の帰属表示は [NOTICE](NOTICE) を参照してください。

このリポジトリおよび Windows 向け配布物には、Qt、OpenCV、Assimp、Eigen、Embree などのサードパーティーソフトウェアが含まれる、または依存する場合があります。これらのコンポーネントにはそれぞれ独自のライセンスと notice が適用されます。バイナリを再配布する場合は、必要なライセンス文書と notice を同梱してください。
