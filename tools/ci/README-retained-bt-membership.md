# SignedRender SCMの保持BT catalog検証

`build-combined200-service.ps1`のSignedRenderUpdate経路は、非実行の`bt_first_install`に残る旧2013 directoryへアクセスせず、実admissionが選んだ`policy.packages.bt`を使用します。非SignedRenderの既経路は変更していません。BT first-install/未結合maskやBT更新をこの新経路で許可しません。

`retained_bt_membership.py`は保持baselineのSYS/version/date、policy input pins、signed manifestのsource/version/file table、実INF/SYS/CAT/member byteを照合します。得られたCATとmemberを既SignTool /pa /cへ渡し、CAT signerは既BCBF identityに限定します。Windows検証後に同planを実byteから再計算し、途中変更を拒否します。manifestへplan hash・verifier source hash・Windows検証結果を記録します。hostで作るplanのWindows_CAT_membership_verified=falseは事前選択のsnapshotで、実Windows検証の結果は上位retained_bt_membership.Windows_CAT_membership_verifiedに記録されます。

旧2013 fileを復元したりpinを省略したりする回避ではありません。実保持BT2018四fileは既1515 pin closureに含まれています。失敗した旧service241の署名EXEだけを採用しません。今回停止はbuilderの最終service manifest出力前、native呼出し前です。旧失敗out/logは保持し、新sourceと新service/native outputだけを使用してください。

Turingのruntime/pre/post guard修正は別担当・別commitです。本修正はbuilderと専用CI verifierだけを変更し、runtime C/既engine/capture helpersには触れません。このcommit単独では新UMD preservedのP1が解消したとは扱いません。mainが両修正を統合したsourceでhost policy/headerを新outへ再生成・独立reviewし、その実hashをWindowsへ渡してください。

## host検証

```text
python3 -B -m unittest discover -s tests -p retained_bt_membership_test.py -v
python3 -B tools/ci/retained_bt_membership.py --workspace ROOT --policy POLICY --out NEW_PLAN
python3 -B tools/ci/retained_bt_membership.py --workspace ROOT --policy POLICY --verify-plan NEW_PLAN
```

fixtureは旧2013 file欠落でも保持2018を選ぶこと、first-install/更新/mask/別baselineを拒否すること、manifest/CAT/member pin欠落、CAT実byte変更、policyだけの差替え、source/version relabelを拒否することを確認します。実host policy241でも選択と再照合を実施し、Windows署名検証は未実行のまま記録します。

## mainのWindows検証計画（本担当未実行）

1. 旧main-SCM-native.logと旧service241 outを保存し配備禁止にする。Turing修正と本commitを統合した実source commitを固定し、clean checkoutと新service/native outを用意する。
2. hostで同実241 mapからpolicy/headerを新outへ生成し、実policy SHAを記録。BT retained baselineは2018、BT install/first/deferred bitは0であることを確認する。
3. 同sourceのverifierをWindows上で実policyへ適用し、planが2018 CATとINF/SYSを指すことを確認。旧2013 pathが存在しなくても新SignedRender経路に依存しないことを確認する。
4. 実SignTool /pa /cでplanの全memberを検証し、CAT Get-AuthenticodeSignature=Valid/BCBFを確認。事後verify-planも成功させる。未署名・変更CAT等のnegative fixtureは隔離scratchで行い、原packageへ変更を加えない。
5. 新outでSCM1TUをcompile/link/sign。最終peripheral-service-manifest.jsonがretained BT proof、source_TUs=1、driver_rebuilt=false、collection compile config、実source/policy/header hashを持つことを確認する。
6. そのserviceの実plan hashだけを使い、原241 SYSTEM trioから新outへnative patch/verifyを一度実行。status/errors/原本不変、load/unload/privilege restore全0、target service241一件/7values、related fingerprints不変を確認する。native inventory receiptをnative patch resultへ改名流用しない。
7. source/manifest/署名・CAT proof/clone lifecycleの実回収検証とruntime guardの独立review完了まで、media/seal/実機配備へ進まない。GPU/active0成功は主張しない。
