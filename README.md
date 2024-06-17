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

### ファームウェアの書込み
picoのBOOTSELを押しながらUSBケーブルを接続するとエクスプローラーが起ち上がり、picoのフォルダが表示されます(winの場合）<br>
そこにfirmwareフォルダのfirmware.uf2をコピーしてください<br>
ファームウェアの更新が完了するとpicoが再起動します

### 参考サイト
- ういよめん氏　Arkanoid Paddle (spinner) controller Rotary encoder<br>
https://www.thingiverse.com/thing:5404242

- わいくん氏　ゲーム基板用センサー図鑑<br>
https://www.ykuns-mechanical-club.com/game%20sensor.html
