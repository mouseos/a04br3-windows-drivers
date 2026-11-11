# Buttons HID binding reader V2

`read-buttons-device-bindings.ps1`だけのsource候補。既存collection6687/run232には未統合。通常readerの同load private rootへ渡す関数であり、追加hive load、書込み、device/SCM照会を行わない。driverやtimer観測は変更しない。

VID/PIDでHID familyを除外せず、最大512 family、各8 instance、合計1024 instanceを探索する。結果へ格納するHID子は最大32。family/instance名・値サイズ・型・既存OpenPathのregistry link拒否は維持する。

採用根拠は次のいずれか。結果ごとに親pathと根拠を保存する。

- Buttons親の実REG_SZ ParentIdPrefix + `&` + 4桁hexがHID instance全体と一致。
- 子の実REG_SZ ParentがButtons device-instance IDと完全一致（任意の`Enum\`接頭辞のみ除去）。

明示Parentがprefixと矛盾する場合、複数Buttons親に一致する場合、Parentの型/形式が解釈不能な場合はunknown。ContainerID一致は補助情報だけで採用しない。同containerの別機能を包含しない。Parentプロパティストア等の未実装形式を推測decodeしない。親のprefix一致は保存された命名上の関係でありlive stackや入力動作の証明ではない。

`hid_search_status`:

- `found`: 完全な探索で少なくとも1個の保存親関係を確認。他の子にunknownが残る場合もある。
- `not_found`: 完全な探索で一致0、かつ関係未解決instanceも0。保存範囲での不在のみ。
- `unknown`: 親/Enum欠落、未解決関係のみ、読取失敗、矛盾、上限到達等。

`complete`は探索とreadの完了、`hid_search_complete`はさらに未解決関係0を表す。既存leaf欠落はpresent=false。関連採用後のleafにread失敗があればerrorsとread_complete=falseを残し、完全結果にしない。上限到達はtruncated=true。判定不能な無関係HIDの内容は結果へ含めず、未解決件数だけを残す。

actual231の保存receiptはButtons ParentIdPrefix `4&1bb6ab18&0`を含むが、HID全family探索やParent/Container値は保存されていない。したがって新readerでの実collection発見・不在は検証不能。host確認では既保存leaf byteを部分入力とし、未収集値を実際の不存在と認定しない。別途、明示的な合成fixtureで非VID familyの親prefix一致・矛盾拒否・明示別親・上限を確認した。新hive load/実機試験なし。PowerShellローカルparser/モックでの検証でありWindows RT実行確認ではない。

採用先はV2 schema/追加statusを扱う必要がある。既receiptや凍結payloadのschemaを書き換えない。
