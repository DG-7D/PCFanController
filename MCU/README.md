- 操作
    - PA5: +入力 / ロータリエンコーダA相入力
    - PA6: -入力 / ロータリエンコーダB相入力
    - PA7: 決定入力
- 7セグ
    - PA1: データ出力
    - PA3: クロック出力
    - PA4: ラッチ出力
- ファン
    - PB1: 回転数パルス入力
    - PB0: PWM出力
- その他
    - PA0: RESET / UPDI
    - PA2: MISO (未使用)

```
ATtiny404

   VDD GND
7R PA4 PA3 7C
i  PA5 PA2 x
i  PA6 PA1 7D
i  PA7 PA0 x
   PB3 PB0 FP
   PB2 PB1 FT
```

```
74HC595
QB  VCC
QC   QA
QD   SI 7D
QE    G GND
QF  RCK 7R
QG  SCK 7C
QH  SCL VCC
GND QH'
```

```
pio run -t fuses
pio run -t upload
```