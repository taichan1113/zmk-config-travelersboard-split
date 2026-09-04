# travelers-board-split UF2 運用ガイド

`main` は Adafruit nRF52 UF2 bootloader を常駐させる構成です。通常更新は USB mass-storage にコピーする UF2 を使います。SWD は初回導入、全消去後の復旧、ハードウェアデバッグ専用です。

以前の直接起動（SWD で ZMK HEX を `0x00000000` に書く）構成は、[`archive/swd-direct`](https://github.com/taichan1113/zmk-config-travelersboard-split/tree/archive/swd-direct) branch にアーカイブしています。`main` の board／artifact と混在させないでください。

## board と役割

| board | 役割 | 通常更新 |
| --- | --- | --- |
| `travelers_board_split_left` | left central、USB HID 有効 | UF2 |
| `travelers_board_split_right` | right peripheral | UF2 |

left が PC と BLE 接続し、right は split BLE で left に接続します。left は自身と right の battery level を取得し、right の値を補助 Battery Service として公開します。

キーマトリクスの有効 position は左右合計42個です。

```text
row 0: 12    row 1: 12    row 2: 12    row 3: 6
```

right は `col-offset = <6>` を使い、left と同じ共通 keymap の右側 position を使用します。

## 採用 Flash map

| 領域 | 範囲 | 用途 |
| --- | --- | --- |
| MBR | `0x00000000-0x00000FFF` | Nordic MBR |
| S140 v7.3.0 | `0x00001000-0x00026FFF` | SoftDevice |
| code | `0x00027000-0x000D3FFF` | ZMK application（692 KiB） |
| storage | `0x000D4000-0x000F3FFF` | ZMK settings / BLE bonds（128 KiB） |
| bootloader | `0x000F4000-0x000FFFFF` | UF2 bootloader と設定領域 |

ZMK UF2 は `0x00027000` に link され、code 領域だけを書き換えます。`0x00000000` 開始の直接起動用 HEX を、この構成へ書き込んではいけません。

## ビルド

`config/west.yml` は次の外部 module を要求します。

- `zmk-module-battery-voltage-divider`
- `zmk-behavior-jp`

通常の west manifest を更新した後、left/right をそれぞれビルドします。現在の workspace で module を明示する場合は、次のように指定します。

```bash
west build -p always -s zmk/app -d /tmp/travelers-left -b travelers_board_split_left -- \
  -DZMK_CONFIG="$PWD/config/travelers-board-split/config" \
  -DZMK_EXTRA_MODULES="$PWD/config/travelers-board-split;$PWD/module/zmk-behavior-jp;$PWD/module/zmk-module-battery-voltage-divider"

west build -p always -s zmk/app -d /tmp/travelers-right -b travelers_board_split_right -- \
  -DZMK_CONFIG="$PWD/config/travelers-board-split/config" \
  -DZMK_EXTRA_MODULES="$PWD/config/travelers-board-split;$PWD/module/zmk-behavior-jp;$PWD/module/zmk-module-battery-voltage-divider"
```

成果物は各 build directory の `zephyr/zmk.uf2` です。`CONFIG_BOOTLOADER_MCUBOOT=n` を維持します。この bootloader は MCUboot ではありません。

## 通常更新

1. RESET を500 ms以内に2回押す。
2. `TRAVELERS` volume が現れることを確認する。
3. 対象 half の `zmk.uf2` を volume の直下へコピーする。
4. volume が切断され、ZMK が再起動することを確認する。

UF2 は left/right に対応するものを使用してください。left 用を right へ、またはその逆に書き込むと split の役割が入れ替わります。

## 初回導入と全消去後の復旧

SWDIO、SWCLK、GND、VDD を接続して OpenOCD を起動します。

```bash
openocd -f interface/stlink.cfg -f target/nrf52.cfg
```

OpenOCD telnet で、S140 v7.3.0、MBR を含む bootloader の順に書き込みます。

```tcl
reset halt
program /absolute/path/to/s140_nrf52_7.3.0_softdevice.hex verify
program /absolute/path/to/bootloader_mbr.hex verify reset
```

その後、double-reset で UF2 bootloader に入り、該当 half の `zmk.uf2` をコピーします。全消去は MBR、S140、bootloader、ZMK、settings、BLE bonds をすべて削除します。

## 確認項目

- left/right とも double-reset で `TRAVELERS` volume が出る。
- left の BLE HID と USB HID が動作する。
- right のキー入力が left 経由で PC に届く。
- left/right の battery reporting と settings persistence が動作する。
- UF2 更新後も split BLE 接続と keymap が維持される。
