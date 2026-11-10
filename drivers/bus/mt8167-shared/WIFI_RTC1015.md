# run63: retained RTC clock admission

run63の1009 Initialize generation32はSESSION_ACQUIRE=C00000A3、NDIS=C0010011。
FW・校正境界には未到達。既存parent1014のAcquireは、RTC PDN1 bit8が既に
セット済みだとDeviceNotReadyへ変換していた。両Windows起動前の保存済み
preloaderログはPDN1=0300、CON=0407。親D0・音声は正常。

CT3 kernel4.4 mtk_rtc_common.c rtc_gpio_enable_32kとmt6351/mtk_rtc_hal.c
hal_rtc_set_gpio_32k_statusはper-user bitmapの冪等setであり排他lockではない。
従来exclusive APIは維持。既定OFFのA04_SHARED_WIFI_RTC_BORROWを1015候補だけ
有効化し、既存bit8と実CON出力有効を両方読めたときだけinherited leaseとする。
borrow時にRTCへ書かず、releaseでも借用bitを消さない。他userとoutputを保持。
bit無しは従来通り自分のbitを取得・release。bit有り/output無効や不明read、
二重取得は拒否。RTC output観測は物理32k測定・FW成功・NDIS成功の証明ではない。

GPU/SD/audio/USBの既存1014 flagsは維持。新INF1015と専用build入口を追加。
1009のSYS/FW/init-boundaryを変更せず親だけ更新する候補。実機未投入。
