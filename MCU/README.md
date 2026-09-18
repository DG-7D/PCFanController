- 操作
    - PA7: 表示ボタン
    - PA6: +ボタン
    - PA5: -ボタン
- 7セグ
    - PA1: データ出力
    - PA3: クロック出力
    - PA4: ラッチ出力
    - PB3: 有効出力 (Active Low)
- ファン
    - PB0: 回転数パルス入力
    - PB1: PWM出力
- その他
    - PA0: RESET / UPDI
    - PA2: MISO (未使用)
    - PB2: TxD

```
ATtiny404

   VDD GND
7R PA4 PA3 7C
i- PA5 PA2 x
i+ PA6 PA1 7D
iD PA7 PA0 UPDI
7E PB3 PB0 FT
Tx PB2 PB1 FP
```

```
74HC595

A QB  VCC
F QC   QA B
E QD   SI 7D
D QE    G GND
P QF  RCK 7R
C QG  SCK 7C
G QH  SCL VCC
  GND QH'

x QB  VCC
x QC   QA x
x QD   SI 7D
3 QE    G GND
2 QF  RCK 7R
1 QG  SCK 7C
0 QH  SCL VCC
  GND QH'
```

```
pio run -t fuses
pio run -t upload
```