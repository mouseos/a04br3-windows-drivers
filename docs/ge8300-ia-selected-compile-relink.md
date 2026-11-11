# IA形式更新の差分リンク

`tools/ci/relink-ge8300-ia-float-umd.py` の
`--new-compiled-source-commit <実compile-manifestの40桁commit>` で、
後続の入力形式対応をコンパイルした2オブジェクトも明示選択できる。
省略時は元のFLOAT入力版 `323a42f594bb416d456f58df5c0d8aec795d6a5a` を維持する。
新manifestの期待SHAと各object/source/flagsの検査に加え、選択したcompile commitと
現在のrecipe checkoutにある両TUのGit blobsが完全一致しなければ拒否する。
旧26オブジェクト、451入力の差分制約、PCO、archive/link順序の検査は維持する。
出自を新recipe HEADへ付け替えるものではなく、native cache guardも変更しない。
指定compile commitのGitオブジェクトがないshallow checkoutでは、その公開commitも
取得してから実行する。通常のcoldビルド入口は変更しない。
