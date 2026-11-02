#include "cabinet_boot.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NOR_SIZE 0x500000U
#define FLASH_SIZE 0x200000U
static unsigned char nor[NOR_SIZE],flash[FLASH_SIZE],saved_nor[NOR_SIZE],saved_flash[FLASH_SIZE];
static jmp_buf power_cut;static int cut_at,mutations,partial;
static uint8_t *resolve(int space,uint32_t a,size_t n)
{
    if(!space){assert(a<=NOR_SIZE&&n<=NOR_SIZE-a);return nor+a;}
    assert(a>=0x08000000U&&a-0x08000000U<=FLASH_SIZE&&n<=FLASH_SIZE-(a-0x08000000U));return flash+a-0x08000000U;
}
static int read_mem(void *ctx,int s,uint32_t a,void *p,size_t n)
{ (void)ctx;memcpy(p,resolve(s,a,n),n);return 0; }
static void allowed(int s,uint32_t a,size_t n)
{
    if(s)assert((a>=CABINET_CM7_BASE&&a+n<=0x08100000U)||(a>=CABINET_CM4_BASE&&a+n<=0x08200000U));
}
static int erase_mem(void *ctx,int s,uint32_t a,size_t n)
{
    uint8_t *p=resolve(s,a,n);(void)ctx;allowed(s,a,n);++mutations;
    if(mutations==cut_at){if(partial)memset(p,255,n/2);longjmp(power_cut,1);}
    memset(p,255,n);return 0;
}
static int write_mem(void *ctx,int s,uint32_t a,const void *data,size_t n)
{
    uint8_t *p=resolve(s,a,n);const uint8_t *q=data;size_t i;(void)ctx;allowed(s,a,n);
    if(s)assert(!(a&31U)&&!(n&31U));++mutations;
    if(mutations==cut_at){if(partial)for(i=0;i<n/2;++i)p[i]&=q[i];longjmp(power_cut,1);}
    for(i=0;i<n;++i){assert((p[i]&q[i])==q[i]);p[i]=q[i];}return 0;
}
static const CabinetBootOps ops={read_mem,erase_mem,write_mem,0,0};
static void image(uint8_t *p,size_t n,unsigned core,uint32_t version)
{
    CabinetImageInfo info={CABINET_INFO_MAGIC,CABINET_BOARD_ID,0,0,CABINET_PROTOCOL,{0,0,0}};
    uint32_t vectors[2];size_t i;
    for(i=0;i<n;++i)p[i]=(uint8_t)(i*37+version+core);
    vectors[0]=core==7?0x24070000:0x30030000;vectors[1]=(core==7?CABINET_CM7_BASE:CABINET_CM4_BASE)+0x441;
    memcpy(p,vectors,8);info.core=core;info.version=version;memcpy(p+CABINET_INFO_OFFSET,&info,sizeof(info));
}
static CabinetPackage package(uint32_t version,int in_flash)
{
    CabinetPackage h={0};uint8_t *a,*b;
    h.magic=CABINET_PACKAGE_MAGIC;h.format=1;h.board=CABINET_BOARD_ID;h.version=version;
    h.cm7_address=CABINET_CM7_BASE;h.cm4_address=CABINET_CM4_BASE;h.cm7_size=2304;h.cm4_size=2816;h.payload_size=h.cm7_size+h.cm4_size;
    a=in_flash?resolve(1,h.cm7_address,h.cm7_size):nor+CAB_STAGE+64;
    b=in_flash?resolve(1,h.cm4_address,h.cm4_size):a+h.cm7_size;
    image(a,h.cm7_size,7,version);image(b,h.cm4_size,4,version);
    h.cm7_crc=cabinet_crc32(a,h.cm7_size);h.cm4_crc=cabinet_crc32(b,h.cm4_size);h.header_crc=cabinet_crc32(&h,60);
    if(!in_flash)memcpy(nor+CAB_STAGE,&h,64);return h;
}
static void setup(void)
{
    CabinetBootRecord r={0};memset(nor,255,sizeof(nor));memset(flash,255,sizeof(flash));cut_at=0;mutations=0;
    r.installed=package(0x10000,1);assert(!cabinet_boot_save(&ops,&r));package(0x10001,0);
    assert(!cabinet_boot_request(&ops));
}
static uint32_t version(void)
{
    CabinetBootRecord r;CabinetImageInfo a,b;assert(!cabinet_boot_load(&ops,&r));
    read_mem(0,1,CABINET_CM7_BASE+0x400,&a,sizeof(a));read_mem(0,1,CABINET_CM4_BASE+0x400,&b,sizeof(b));
    assert(a.version==b.version&&a.version==r.installed.version);return a.version;
}
static void backup_state(void){memcpy(saved_nor,nor,sizeof(nor));memcpy(saved_flash,flash,sizeof(flash));}
static void restore_state(void){memcpy(nor,saved_nor,sizeof(nor));memcpy(flash,saved_flash,sizeof(flash));}
static void sweep(const char *label,uint32_t expected)
{
    int total,point,mode;backup_state();mutations=0;cut_at=0;assert(!cabinet_boot_run(&ops));total=mutations;
    for(mode=0;mode<2;++mode)for(point=1;point<=total;++point){
        restore_state();cut_at=point;partial=mode;mutations=0;
        if(!setjmp(power_cut)){(void)cabinet_boot_run(&ops);assert(!"power cut was not injected");}
        cut_at=0;assert(!cabinet_boot_run(&ops));assert(version()==expected);
    }
    printf("PASS: %s: %d power cuts (before/halfway through every persistent erase/program)\n",label,total*2);
}
int main(void)
{
    CabinetBootRecord r;CabinetPackage h;unsigned i;
    setup();assert(!cabinet_boot_run(&ops));assert(version()==0x10001);assert(!cabinet_boot_load(&ops,&r)&&r.attempts==1);
    assert(!cabinet_boot_run(&ops));assert(!cabinet_boot_run(&ops));assert(version()==0x10001);
    assert(!cabinet_boot_run(&ops));assert(version()==0x10000);assert(!cabinet_boot_load(&ops,&r)&&r.result==CAB_BOOT_ROLLED_BACK);
    setup();assert(!cabinet_boot_run(&ops));assert(!cabinet_boot_confirm(&ops,0x10001));
    for(i=0;i<5;++i)assert(!cabinet_boot_run(&ops));assert(version()==0x10001);
    setup();nor[CAB_STAGE+2000]^=1;assert(!cabinet_boot_run(&ops));assert(version()==0x10000);
    setup();memcpy(&h,nor+CAB_STAGE,64);h.board^=1;h.header_crc=cabinet_crc32(&h,60);memcpy(nor+CAB_STAGE,&h,64);
    assert(cabinet_boot_package_check(&ops,CAB_STAGE,&h));assert(!cabinet_boot_run(&ops));assert(version()==0x10000);
    setup();sweep("backup/install/trial journal",0x10001);
    setup();assert(!cabinet_boot_run(&ops));assert(!cabinet_boot_run(&ops));assert(!cabinet_boot_run(&ops));
    sweep("paired rollback",0x10000);
    /* Factory provisioning accepts only matching core/board/protocol metadata. */
    memset(nor,255,sizeof(nor));memset(flash,255,sizeof(flash));package(0x10000,1);cut_at=0;
    assert(!cabinet_boot_run(&ops));assert(version()==0x10000);
    puts("PASS: paired versions, candidate corruption, 3-boot rollback, healthy confirmation, factory initialization");return 0;
}
