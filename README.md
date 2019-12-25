| LPC11U24 | Breakout | Function     | Notes
|---------:|---------:|:-------------|:---
|        1 |        1 |              |
|        2 |        2 |              |
|        3 |        3 | !RESET       | Add external pull-up
|        4 |        4 | ISP          | Pull-down switch
|        5 |        5 | Vss          |
|        6 |        6 | XTALIN       |
|        7 |        7 | XTALOUT      |
|        8 |        8 | Vdd          |
|        9 |        9 |              |
|       10 |       10 |              |
|       11 |       11 | PIO1_26      | RTC RST
|       12 |       12 | PIO1_27      | RTC IO
|       13 |       26 | PIO1_20      | RTC SCK
|       14 |       27 | USB_VBUS     | Connect to Vdd (with jumper)
|       15 |       28 |              |
|       16 |       29 |              |
|       17 |       30 |              |
|       18 |       31 |              |
|       19 |       32 | USB_D-       | 33R in series
|       20 |       33 | USB_D+       | 33R in series, 1.5k pull-up
|       21 |       34 |              |
|       22 |       35 | !USB_CONNECT | PNP switches USB_D+ pull-up, plug side
|       23 |       36 |              |
|       24 |       37 |              |
|       25 |       64 |              |
|       26 |       65 |              |
|       27 |       66 | PIO0_8       | LED7 ULN2803_P8
|       28 |       67 | PIO0_9       | LED6 ULN2803_P7
|       29 |       68 | PIO0_10      | LED5 ULN2803_P6
|       30 |       69 |              |
|       31 |       70 |              |
|       32 |       71 | PIO0_11      | LED4 ULN2803_P5
|       33 |       72 | PIO0_12      | LED3 ULN2803_P4
|       34 |       73 | PIO0_13      | LED2 ULN2803_P3
|       35 |       74 | PIO0_14      | LED1 ULN2803_P2
|       36 |       75 |              |
|       37 |       89 |              |
|       38 |       90 |              |
|       39 |       91 | PIO0_15      | LED0 ULN2803_P1
|       40 |       92 |              |
|       41 |       93 | Vss          |
|       42 |       94 |              |
|       43 |       95 |              |
|       44 |       96 | Vdd          |
|       45 |       97 | CT32B0_CAP0  | Connect to ISP pull-down switch
|       46 |       98 | RXD          | UART Pins used by bootloader
|       47 |       99 | TXD          | UART Pins used by bootloader
|       48 |      100 |              |


Commands:

SET TIME 2019-12-25-11:59:00
GET TIME
2019-12-25-11:59:00

SET {NEARLY,PAST} 00:15
GET {NEARLY,PAST}
00:15

SET {BREAKFAST,LUNCH,HOME,DINNER,BED} 22:12
GET {BREAKFAST,LUNCH,HOME,DINNER,BED}
22:12

RESET
