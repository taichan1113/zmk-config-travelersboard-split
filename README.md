# travelers-board-split ZMK Config

`travelers-board-split` は、nRF52840 を使う 42 キーの左右分割キーボード用 ZMK Config Module です。`main` は Adafruit nRF52 UF2 bootloader を常駐させる運用を採用しており、通常のファームウェア更新は USB mass-storage へ UF2 をコピーして行います。

左手（`travelers_board_split_left`）が central、右手（`travelers_board_split_right`）が peripheral です。PC と接続するのは左手で、Bluetooth と USB の表示名はどちらも `travelers-split` です。

## リポジトリ構成

標準的な ZMK Config Module の配置です。

| パス | 用途 |
| --- | --- |
| `boards/arm/` | left/right の board 定義と、両者で共有する hardware `.dtsi` |
| `config/` | 左右共通 keymap と west manifest |
| `build.yaml` | GitHub Actions とローカルビルドの対象定義 |
| `zephyr/module.yml` | out-of-tree board を ZMK module として認識させる定義 |
| `.github/workflows/build.yml` | push、pull request、手動実行時の firmware build |
| `BOARD_GUIDE.md` | UF2書き込み、Flash map、SWD復旧の詳細 |

## GitHub Actions でビルドする

`main` への push、pull request 作成時、または Actions タブからの手動実行で、左右の UF2 をビルドします。成功した run の **Artifacts** から `firmware` をダウンロードしてください。

| artifact | 書き込む対象 |
| --- | --- |
| `travelers-board-split-left.uf2` | 左手（central） |
| `travelers-board-split-right.uf2` | 右手（peripheral） |

左右には同じコミットで生成されたファームウェアを書き込みます。double-reset で `TRAVELERS` volume を出してから、該当する UF2 を volume の直下へコピーしてください。

## 依存 module

`config/west.yml` が必要な依存関係を宣言しています。GitHub Actions と通常の `west update` は、次の module を自動取得します。

- `zmk-module-battery-voltage-divider` — CR2032 向けの電圧・残量レポート
- `zmk-behavior-jp` — 日本語配列用 behavior

このキーボードでは、left が自身と right のバッテリー残量を取得し、right の残量は split 経由で補助 Battery Service として公開します。

## ローカルビルド

このリポジトリを単独でcloneした場合は、ZMK Configとして初期化してから、`build.yaml` のboard名を指定してビルドします。

```sh
west init -l config
west update
west zephyr-export

west build -s zmk/app -b travelers_board_split_left -d build/left -- \
  -DZMK_CONFIG="$PWD/config" -DZMK_EXTRA_MODULES="$PWD"
west build -s zmk/app -b travelers_board_split_right -d build/right -- \
  -DZMK_CONFIG="$PWD/config" -DZMK_EXTRA_MODULES="$PWD"
```

成果物は `build/left/zephyr/zmk.uf2` と `build/right/zephyr/zmk.uf2` です。既存 workspace などでこの repository を out-of-tree module として明示する場合を含む、詳細なビルド・書き込み・SWD復旧手順は [BOARD_GUIDE.md](BOARD_GUIDE.md) を参照してください。

## UF2運用と復旧

Flash map、初回導入または全消去後に必要な S140 v7.3.0 と bootloader MBR の書き込み、OpenOCDの使い方は [BOARD_GUIDE.md](BOARD_GUIDE.md) に記載しています。

旧来のSWD直接起動構成は [`archive/swd-direct`](https://github.com/taichan1113/zmk-config-travelersboard-split/tree/archive/swd-direct) branch にアーカイブされています。`main` のUF2用ファームウェアと混在させないでください。
