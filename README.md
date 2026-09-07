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
  VDD GND
7 PA4 PA3 7
i PA5 PA2 x
i PA6 PA1 7
i PA7 PA0 x
  PB3 PB0 F
  PB2 PB1 F
```

```
pio run -t fuses
pio run -t upload
```