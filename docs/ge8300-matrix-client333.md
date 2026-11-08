# 独立333：標準D3D11行列VS資格（作業中・未署名）

332のpackage/source/mediaを凍結し、別branchで行列VS+SAMPLEの標準clientを準備する。
現段階は実HLSLと四つのCB行列・独立CPU pixel oracleのsourceのみ。D3DCompile/DXBC生成、新client統合/署名、GPU readbackは未実施。GPU成功やreset解消を宣言しない。

入力triangleは(-1,1,.5,1)/(1,1,.5,1)/(-1,-1,.5,1)を保持し、4行DP4のVSで実CB0を適用する。CPUは頂点を事前変換せず、期待pixel境界の算術確認にのみ使う。CB行列は4種類の非一様scale/translation。POINT+CLAMPの非一様2x2textureと複数CB由来UVを次に組み合わせる。補間varying由来UVは未対応。

oracleは画像の生成/driver入力ではなく、Mapされた実pixelとの比較用。pixel-centerの整数cross productで境界を判定し、丸めが曖昧な対角上pixelを持たない矩形寸法を選ぶ。

次のreset証拠が届けばこの作業より実機根因修正を優先する。旧327を再実行しない。
