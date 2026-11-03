# MT8167 EINT register foundation

This callback-based helper describes the MT8167 EINT register operations
needed by a future shared interrupt controller. The 169-line, six-bank layout
and register meanings are based on Linux upstream
[`pinctrl-mt8167.c`](https://github.com/torvalds/linux/blob/master/drivers/pinctrl/mediatek/pinctrl-mt8167.c)
and [`mtk-eint.c`](https://github.com/torvalds/linux/blob/master/drivers/pinctrl/mediatek/mtk-eint.c).
No Linux code is incorporated.

It is not a Windows GPIO controller or an interrupt handler. The eventual
driver must own and lock the shared controller and implement PnP, GpioClx,
resource assignment, interrupt delivery, pinmux, debounce and wake. Both-edge
emulation is not provided. Host tests do not validate real hardware behavior.
