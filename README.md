# travelers-board-split

NINA-B302-00B（nRF52840）を使う travelers-board-split の ZMK v0.3 設定です。
現時点では、分割側を仮定しない central-only 構成です。BLE 名は ZMK v0.3 の制限により
`travelers-split`（16文字以内）です。

## 現在の構成

- BLE HID と 5 x 6 の `col2row` マトリクスを有効化しています。実装する物理キーは 26 個です。
- NINA-B302 内蔵の 32 MHz / 32.768 kHz 水晶を使用します。
- 電池残量は外部 `zmk-module-battery-voltage-divider` module で測定します。P0.13 で
  1 Mohm / 1 Mohm / 100 nF の分圧器を有効化し、300 ms 待機後に P0.04（SAADC AIN2）を
  読み、BLE Battery Service へ報告します。既定の線形換算は 2.0 V = 0%、3.0 V = 100% です。
- USB、UF2、MCUboot は使用しません。SWD で書き込みます。

## ビルド

workspace root で実行します。

```sh
just init config/travelers-board-split
just build travelers-board-split-central -p
```

通常の成果物は `firmware/travelers-board-split-central.hex` です。起動構成の比較では、
Downloads に以下の名前で保存した成果物を使います。

| ファイル | 構成 | アプリ開始アドレス |
|---|---|---:|
| `travelers-board-split-central-direct.hex` | 構成 A: 直接起動 | `0x00000000` |
| `travelers-board-split-central-mbr.hex` | 構成 B: MBR 版 | `0x00001000` |

## 起動構成の比較

nRF52840 はリセット時に Flash 先頭 `0x00000000` のベクタテーブルを参照します。
先頭に何を置くかで起動経路が決まります。

- **MBR**: Nordic の小さな起動層。SoftDevice または後段イメージへ制御を渡します。
- **SoftDevice (S140)**: Nordic 提供の BLE スタック。MBR を含む HEX として配布されます。
- **Bootloader**: MCUboot / UF2 bootloader 等。更新イメージの選択・検証・起動を行います。
- **ZMK アプリ**: Zephyr、Zephyr Bluetooth Controller、ZMK、キーマップ等を含みます。

### 構成 A: 直接起動（採用）

```text
0x00000000  ┌─────────────────────────────┐
            │ ZMK / Zephyr アプリ          │
            │ - ベクタテーブル             │
            │ - Zephyr BLE Controller      │
            │ - ZMK BLE HID                │
0x000e0000  ├─────────────────────────────┤
            │ NVS / settings               │ 128 KiB
0x00100000  └─────────────────────────────┘
```

MBR、SoftDevice、Bootloader は置かず、ZMK 自身が先頭から起動します。全消去後に
direct HEX だけを書いて BLE HID として動作することを実機確認済みであり、今後の基準構成です。

### 構成 B: MBR を残す ZMK 起動版（比較用）

```text
0x00000000  ┌─────────────────────────────┐
            │ MBR                         │ 4 KiB
0x00001000  ├─────────────────────────────┤
            │ ZMK / Zephyr アプリ          │
0x000d4000  ├─────────────────────────────┤
            │ NVS / settings               │ 128 KiB
0x000f4000  ├─────────────────────────────┤
            │ Bootloader 予約域（空）      │ 48 KiB
0x00100000  └─────────────────────────────┘
```

全消去後は S140 HEX を使って先頭 4 KiB の MBR を導入します。次に `0x1000` 開始の
ZMK HEX を書くため、SoftDevice 本体は上書きされ、最終配置には残りません。以後の通常更新は
MBR 版 ZMK HEX のみです。構成 B の HEX は比較用に生成済みですが、採用構成ではありません。

### 構成 C: MBR + S140 SoftDevice 常駐版

```text
0x00000000  ┌─────────────────────────────┐
            │ MBR                         │
0x00001000  ├─────────────────────────────┤
            │ S140 SoftDevice             │
0x00026000  ├─────────────────────────────┤
            │ SoftDevice 前提の ZMK アプリ │
0x000e0000  ├─────────────────────────────┤
            │ NVS / settings               │
0x00100000  └─────────────────────────────┘
```

構成 B とは別物です。アプリ側も SoftDevice 前提で `0x26000` にリンクする必要があります。
現在この構成の HEX は生成していません。構成 A/B の HEX に S140 を追加してはなりません。
Flash 領域が重なります。

### 構成 D: 通常 Bootloader 常駐版

```text
0x00000000  ┌─────────────────────────────┐
            │ Bootloader (例: MCUboot)    │
            ├─────────────────────────────┤
            │ Primary application slot    │
            ├─────────────────────────────┤
            │ Secondary / update slot     │
            ├─────────────────────────────┤
            │ NVS / settings              │
0x00100000  └─────────────────────────────┘
```

更新経路は UF2 / USB DFU / BLE DFU 等を選んだ Bootloader に依存します。現基板では未採用です。
将来採用する場合は、Flash slot、署名、復旧経路を含めて別途設計します。

## OpenOCD の起動と接続

CR2032 を外し、デバッガからのみターゲットへ給電します。VDD、SWDIO、SWCLK、共通 GND を
接続します。ST-Link 使用時は次を実行します。

```sh
openocd -f interface/stlink.cfg -f target/nrf52.cfg
```

別ターミナルから Telnet 接続します。

```sh
telnet localhost 4444
```

Flash 全 1 MiB を消去する場合は次を実行します。Flash、NVS、BLE bonding は消去されますが、
UICR は消去されません。ST-Link では CTRL-AP を要する `nrf52_recover` は使用しません。

```tcl
reset halt
flash erase_address 0x00000000 0x00100000
reset halt
```

## 各構成の OpenOCD 書込み手順

`program` は HEX 内のリンク時アドレスを使うため、オフセットは指定しません。

### 構成 A: 直接起動版

全消去後の書込みです。SoftDevice は書きません。

```tcl
reset halt
flash erase_address 0x00000000 0x00100000
program C:/Users/takec/Downloads/travelers-board-split-central-direct.hex verify reset
```

通常更新では全消去を省略できます。

```tcl
reset halt
program C:/Users/takec/Downloads/travelers-board-split-central-direct.hex verify reset
```

ベクタテーブル確認:

```tcl
mdw 0x00000000 2
```

### 構成 B: MBR 版

全消去後は S140 7.3.0 HEX で MBR を導入してから MBR 版 ZMK を書きます。

```tcl
reset halt
flash erase_address 0x00000000 0x00100000
program C:/Users/takec/Downloads/s140_nrf52_7.3.0_softdevice.hex verify
program C:/Users/takec/Downloads/travelers-board-split-central-mbr.hex verify reset
```

MBR が残っている通常更新では MBR 版 ZMK HEX だけを書きます。

```tcl
reset halt
program C:/Users/takec/Downloads/travelers-board-split-central-mbr.hex verify reset
```

ベクタテーブル確認:

```tcl
mdw 0x00000000 2    # MBR
mdw 0x00001000 2    # MBR 版 ZMK アプリ
```

### 構成 C: SoftDevice 常駐版

専用の `0x26000` 開始アプリを生成した場合だけ使います。

```tcl
reset halt
flash erase_address 0x00000000 0x00100000
program C:/Users/takec/Downloads/s140_nrf52_7.3.0_softdevice.hex verify
program C:/Users/takec/Downloads/<softdevice-layout-app.hex> verify reset
```

通常更新時は SoftDevice 領域を消去せず、対応するアプリ HEX だけを書きます。

### 構成 D: Bootloader 常駐版

Bootloader の partition と署名形式に合うファイルを使います。現在はプレースホルダーです。

```tcl
reset halt
flash erase_address 0x00000000 0x00100000
program C:/Users/takec/Downloads/<bootloader.hex> verify
program C:/Users/takec/Downloads/<bootloader-compatible-app.hex> verify reset
```

通常更新は Bootloader が提供する更新経路を使います。SWD で更新する場合も、正しい slot と
署名済みイメージが必要です。

構成 A と B の HEX は混在させず、開始アドレス（`0x00000000` / `0x00001000`）を確認します。

## SWD 接続不良の切り分けと復旧記録

構成 A の確認中、OpenOCD で以下の状態になりました。

```text
Info : Target voltage: 3.100574
Error: init mode failed (unable to connect to the target)
```

これは ST-Link 自体とターゲットの VCC/GND 基準電圧は認識できている一方、MCU との SWD 通信を
開始できない状態です。`nrf52_recover` の警告は ST-Link が CTRL-AP を利用できないという通常の
注意であり、この接続失敗の直接原因ではありません。

無通電かつ CR2032 を外した状態で、以下を確認します。

| 確認箇所 | 期待値 |
|---|---|
| ST-Link GND ↔ 基板 GND | 導通あり |
| ST-Link Vref ↔ 基板 VCC | 導通あり |
| ST-Link SWDIO ↔ 基板 SWDIO | 導通あり |
| ST-Link SWCLK ↔ 基板 SWCLK | 導通あり |
| SWDIO ↔ GND、SWCLK ↔ GND、SWDIO ↔ SWCLK | 低抵抗短絡なし |
| VCC ↔ GND | 定常的な低抵抗短絡なし（コンデンサ充電による一時的な低抵抗は除く） |

この基板では `P0.19` を matrix col0 と SWDCLK、`P0.20` を matrix col2 と SWDIO で共有します。
キーを押した状態、マトリクス配線、ダイオード、リフロー不良による短絡・負荷も SWD 接続に
影響します。接続時は該当キーを押さず、P0.19/P0.20 周辺を優先して確認します。

信号品質を確認するため、接続速度を落として再試行できます。

```sh
openocd -f interface/stlink.cfg -f target/nrf52.cfg -c "adapter speed 100"
```

低速化しても接続できず、導通・短絡に問題がなければ、ロジアナまたはオシロで基板側の
SWCLK/SWDIO を観測します。SWCLK がトグルしない場合は SWCLK 配線または ST-Link 側、SWCLK は
トグルして SWDIO 応答がない場合は SWDIO 配線・短絡・MCU 側を疑います。

今回の接続不良はリフローをやり直した後に解消し、OpenOCD で正常にターゲットを読めることを
確認しました。

## RESET スイッチ

RESET スイッチは通常の SWD 書込みには必須ではありません。SWDIO、SWCLK、VCC、GND が接続
されていれば OpenOCD がリセットできます。

ただし、手動再起動や、暴走したアプリへの「RESET を保持した状態での SWD 接続」には有用です。
デバッグ復旧にも使う場合は、RESET ピンを GND に落とすスイッチに加えて、SWD ヘッダの NRST と
ST-Link の NRST を接続し、OpenOCD 側でも reset 設定を追加します。
