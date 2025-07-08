![USBパドルコントローラ外観](https://github.com/yagennankoz/USBPaddleController/blob/main/image/USBPaddleController.jpg "USBパドルコントローラ外観")
<br>
![USBパドルコントローラ内部](https://github.com/yagennankoz/USBPaddleController/blob/main/image/USBPaddleController_inside.jpg "USBパドルコントローラ内部")
<br>
# USBPaddleController

raspberrypi picoを使用したUSBパドルコントローラです<br>
左右移動と左右ボタンのみ操作可能なUSBマウスとして動作しますので、それらの入力を受け付けるソフトであれば大抵動くんじゃないかと思います
<br>

### パドル本体について
パドルコントローラ部は、ういよめん氏作のパドルコントローラを使用します<br>
https://www.thingiverse.com/thing:5404242
<br>
ういよめん氏のBOOTHで購入する、自分で3Dプリントする等の手段で別途ご用意ください<br>
https://uiyomengames.booth.pm/items/3914858

### パドル以外のハードウェア
hardwareフォルダにはパドルコントローラに組み付けるセンサー基板とpicoを載せるメイン基板の回路図とガーバーを入れておきました<br>
センサー基板は位置がずれると正常に動きを検知できなくなるようです<br>
パドルコントローラの「ギア4」の中心が二つのセンサーの中心付近にくるようにしてください<br>

### 使用部品
| 部品 | 型番 | 個数 |
| --- | --- | --- |
| マイコン | RaspberryPi pico | 1 |
| ダイオード | 1N914 | 2 |
| インバータ | 74HC14<br>(U74HC14L-D14-T) | 1 |
| 透過型フォトインタラプタ | SG206 | 2 |
| カーボン抵抗 | 180Ω | 2 |
| カーボン抵抗 | 2.2kΩ | 2 |
| XHコネクタ ベース付ポスト トップ型 2P | B2B-XH-A(LF)(SN) | 4(SW3/4が不要なら2) |
| XHコネクタ ベース付ポスト トップ型 4P | B4B-XH-A(LF)(SN) | 2 |
| XHコネクタ ハウジング 2P | XHP-2 | 4(SW3/4が不要なら2) |
| XHコネクタ ハウジング 4P | XHP-4 | 2 |
| XHコネクター ハウジング用コンタクト | SXH-001T-P0.6 | 16(SW3/4が不要なら12) |
| 24φネジ式押しボタン | PS-14-DN | 2 |
| ネジ径12Φ押しボタン |  | 2 |

### ファームウェアの書込み
picoのBOOTSELを押しながらUSBケーブルを接続するとエクスプローラーが起ち上がり、picoのフォルダが表示されます(winの場合）<br>
そこにfirmwareフォルダのfirmware.uf2をコピーしてください<br>
ファームウェアの更新が完了するとpicoが再起動します<br>

### 低速モード起動
X68000Z用ソフト「メタルオレンジEX」をプレイしたところ、感度が良すぎて扱いきれなかったので、低速モードを追加しました<br>
両方のボタンを同時押ししながらUSBパドルコントローラを起動すると、感度が1/6の低速モードになります<br>

### 速度切り替えボタン
SW3は速度切り替えボタンです<br>
押下するごとに高速→中速→低速→高速…と切り替わります<br>

### X/Y軸切り替えボタン
SW4はX/Y軸切り替えボタンです<br>
押下するごとにマウスの移動方向が切り替わります<br>
X軸移動のみでは「メタルオレンジEX」のメニュー操作ができなかったので追加しました<br>

### ケースについて
パドルコントローラのケースはサイズ違いのものを用意しました<br>
小さい方は20cmX20cmのヒートベッドで出力可能ですが、パドル操作側の手の置き場がなく若干操作しづらくなっています<br>
大きい方はケースの締め付けにインサートナットを使用しますので別途ご用意ください<br>
また、大サイズにはサイドのSW3/4用のボタン穴の有無で2種類あります<br>
ボタン不要であれば穴のないものを使用してください<br>

### 参考サイト
- ういよめん氏　Arkanoid Paddle (spinner) controller Rotary encoder<br>
https://www.thingiverse.com/thing:5404242

- わいくん氏　ゲーム基板用センサー図鑑<br>
https://www.ykuns-mechanical-club.com/game%20sensor.html
