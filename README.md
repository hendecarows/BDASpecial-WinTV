# BDASpecial-WinTV

[Hauppauge WinTV-soloHD (Europe)][link_solohd] チューナーの [BonDriver_BDA][link_BonDriver_BDA]
プラグインです。[codex][link_codex] で実装しています。

* em28xxx の TS1 と TS2 のヌルパケット破棄フラグを解除します
* I2C コマンドで CNR を取得します

## ビルド

Visual Studio 2026 を使用してビルドします。

```powershell
git submodule update --init
msbuild WinTV.vcxproj /p:Configuration=Release /p:Platform=x64
```

## インストール

`x64/Release/WinTV.dll` を `BonDriver_BDA.dll` と同じディレクトリにコピーし、
BonDriver の INI ファイルの `[Tuner]` セクションに `UseSpecial="WinTV"` を設定してください。
サンプルの INI ファイルでは soloHD DVB-C のフィルター名を使用しています。
`FriendlyName0` には、使用する BDA チューナーフィルターの名前を設定してください。

## ライセンス

GPL v2

[link_solohd]: https://www.hauppauge.co.uk/site/products/data_solohd.html
[link_BonDriver_BDA]: https://github.com/radi-sh/BonDriver_BDA
[link_codex]: https://openai.com/ja-JP/codex
