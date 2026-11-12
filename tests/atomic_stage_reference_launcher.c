/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * 参照ホストだけの標準CRT入口。置換処理と引数解析は同じC実装を呼ぶ。
 * ARM版は引き続きCRT不要のmainCRTStartupを使用する。 */
void a04_atomic_reference_entry(void);
int main(void)
{
    a04_atomic_reference_entry();
    return 2;
}
