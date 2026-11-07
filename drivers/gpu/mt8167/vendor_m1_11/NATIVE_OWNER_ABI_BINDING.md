# WDDM owner ABIへのprovider接続

対象契約はHerschel branchの7f55dee4236f8df42e8cc5315538abaae51dc442、wddm-render/owner_abi.h、owner_interface.h、owner_control.h。sourceを読んでの接続評価であり、GUID query interfaceを公開したという意味ではない。GPU packet/runtimeは本provider担当、標準DDI/UMDはHerschel担当を維持する。

## 実装済みの実submit部分

`windows_native_submit.[ch]`の`a04_vendor_native_submit_prepared_pair`を、既存のvalidated native clear publisherから実際に呼ぶよう接続した。prepare済みのTA/3D kickを同版KCCBへ固定量だけ書き、従来どおりTA→3Dの順でwrite offsetとMMIO kickを公開する。pool allocation、mutex、stall、wait、fence polling、CPU mapping、completion callbackを含まない。実callerはprovider lifetimeとcommand_busyを所有する。将来DIRQLから使うcallerは本物のGPU割込同期ownerも保持しなければならない。

同一candidateへの二重publishはpublished latchで拒否し、曖昧な書込み前にlatchを立てる。warm再armは完全なTA/3D/UFO/cleanup/flush/GP検証後だけlatchを戻す。submit成功は受理だけで、GPU_DONEやCPU_VISIBLEを設定しない。既存のPASSIVE wait/readbackはsubmit primitiveの外に残る。失敗は従来callerがpoison+DMA保持する。

hostではIRQL5モデル・pool拒否・stall禁止でallocation/wait無し、empty queue検査、127→0→1のKCCB wrap、二重publish拒否、完了を捏造しないことを確認した。real IRQ、WDK compile/linkやWDDM接続の証拠ではない。既存1/16frameモデルと途中失敗/poison、DOD電源/fallbackも確認した。新providerをbuildする場合のINFは0.0.2.4。run33の072固定source/署名候補を書き換えない。

## ABIをまだ公開できない具体的条件

| ABI要求 | 現native owner | 次の実装 |
| --- | --- | --- |
| 同じVidMm locked MDL/PFN | private64KiB common-buffer blocksを持つ | exact MDLのNormal RAM/extent/locking保持、実adapter SG DMA→同じPFNへのGPU mapping、cache処理。別private outputの成功をbind成功にしない |
| 任意packed ARGB surface clear | 実GPU成功は800x600と固定colour profile、private output | BGRA byte order、pitch/dimensions、色変更時code/cache処理の実factory。256x256のDDI clientを今の800x600へ黙って変換しない |
| PREPARE_PASSIVE / SUBMIT_DISPATCH | 大半はPASSIVE prepare、実submitを切出し済み | metadata/queue予約とactual IRQ lock下の非待機submitをbinding stateへ接続。command_busyだけをreal IRQ同期の証拠にはしない |
| 実Dxgk DIRQL completion | KMDF WdfInterrupt所有、PASSIVE work/serviceとpolling完了 | GPU IRQを1つのreal registered Dxgk ISRへhandoffするPnP構成を合意。child PDOで二重所有しない。passive cache処理後に実GPU IRQが届く完了経路を設計し、polling DPCからDxgk通知しない |
| CPU_VISIBLE | private Device-word readbackを検証 | VidMm Normal CPU mappingと同じMDLへの実cache maintenanceを完了し、その後のIRQ eventだけvisibility flagを付ける |
| quiesce/reset/release proof | firmware power-off ackはbus/DMA stopではなく、exposed資源は保持 | live root除去/MMU invalidation、TA/3D/GP/FWCCB drain、IRQ callback rundown、epoch retirementを実装して初めてGEO_PROOF_REQUIREDを返す。IRQ maskだけでstop成功にしない |

現段階でGEO_OWNER_REQUIRED=63やGEO_IRQ_DXGK_REGISTERED、GEO_PROOF_REQUIREDを広告する根拠はない。したがって常時successのcallback tableや常時拒否だけのowner stubは作らず、draft GUIDのqueryを追加しない。実submit primitiveは将来bindingで共有可能だが、geo_owner_control_v2全体の実bindingは未完了。

## 主担当/Herschelへの具体的な分担依頼

provider側ではexact MDL/SG mapping、native prepare/submit/visibility/poison、epochとqueue lifetimeを引き続き担当する。HerschelはDDI/context/allocation/consumer rundownを担当し、標準DDIから任意GPU packetやVAを渡さない。主担当はGPU IRQ/PnPの単一owner構成を調整する必要がある。現KMDF providerのIRQを維持したままsidecar側でも同じIRQを登録する案には接続できない。

triangle/textureは別のproducer義務であり、このABI着手で完成扱いしない。capture05 VDM84bytesと1x1 image/sampler/texel factoryは確定実装済み。GPU-only prefix PPP48bytesと先行PDS code/data、VS/FS・PDS DMA一式の正規化生成は引き続き解析中。実機追加captureは主担当への事前handoff後だけ行う。
