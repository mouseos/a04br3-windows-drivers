#include <assert.h>
#include <stdint.h>
#include "shared_parent_contract.h"
static VOID ref(PVOID p){(void)p;}
static int read_button(PVOID p,USHORT *v){(void)p;(void)v;return -1;}
int main(void)
{
    int ctx=0;A04_SHARED_INTERFACE i={0};
    struct a04_button_shared_selection selection={0};
    assert(a04_button_shared_keypad_resource(0x10002000,0x1000,1,0x20002000,0x1000,1));
    assert(!a04_button_shared_keypad_resource(0x1000f000,0x1000,1,0x20002000,0x1000,1));
    assert(!a04_button_shared_keypad_resource(0x10002000,0x1000,0,0x20002000,0x1000,1));
    assert(!a04_button_shared_keypad_resource(0x10002000,0x1000,1,0x20002000,0x1000,0));
    assert(!a04_button_shared_keypad_resource(0x10002000,0x1000,1,UINT64_MAX-10,0x1000,1));
    assert(!a04_button_shared_keypad_resource(0x10002000,0x1000,1,0x20002000,0x800,1));
    assert(!a04_button_shared_select(&selection,0x1000f000,0x1000,1,0x20002000,0x1000,1) && !selection.seen);
    assert(a04_button_shared_select(&selection,0x10002000,0x1000,1,0x20002000,0x1000,1) && selection.translated==0x20002000);
    assert(!a04_button_shared_select(&selection,0x10002000,0x1000,1,0x30002000,0x1000,1) && selection.translated==0x20002000);
    i.Size=sizeof(i);i.Version=A04_SHARED_INTERFACE_VERSION;i.Context=&ctx;i.InterfaceReference=ref;i.InterfaceDereference=ref;i.ButtonPmicRead=read_button;
    assert(a04_button_shared_interface_valid(&i));
    i.Size--;assert(!a04_button_shared_interface_valid(&i));i.Size=sizeof(i);
    i.Version++;assert(!a04_button_shared_interface_valid(&i));i.Version=A04_SHARED_INTERFACE_VERSION;
    i.Context=0;assert(!a04_button_shared_interface_valid(&i));i.Context=&ctx;
    i.InterfaceReference=0;assert(!a04_button_shared_interface_valid(&i));i.InterfaceReference=ref;
    i.InterfaceDereference=0;assert(!a04_button_shared_interface_valid(&i));i.InterfaceDereference=ref;
    i.ButtonPmicRead=0;assert(!a04_button_shared_interface_valid(&i));
}
