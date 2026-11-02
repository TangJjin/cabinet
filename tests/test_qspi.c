/* Exercise the production QSPI BSP and boot journal across persistent ADP modes.
 * The model accepts legacy commands only with the active address width, like the
 * flash protocol; dedicated commands always require four address bytes. */
#include "bsp_board.h"
#include "bsp_qspi.h"
#include "cabinet_boot.h"
#include <assert.h>
#include <stdio.h>
static uint8_t memory[0x20000],sr3,wel;
static uint32_t tick,af_f[16],af_g[16],status_writes;
static QSPI_CommandTypeDef transaction;
void bsp_watchdog_feed(void) {}
uint32_t HAL_GetTick(void) { return tick++; }
void HAL_Delay(uint32_t ms) { tick+=ms; }
void HAL_GPIO_Init(void *port,GPIO_InitTypeDef *g)
{
    unsigned i;uint32_t *af=port==GPIOF?af_f:af_g;
    for(i=0;i<16;++i)if(g->Pin&(1U<<i))af[i]=g->Alternate;
}
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *c) { (void)c;return HAL_OK; }
HAL_StatusTypeDef HAL_QSPI_Init(QSPI_HandleTypeDef *q) { (void)q;return HAL_OK; }
HAL_StatusTypeDef HAL_QSPI_Command(QSPI_HandleTypeDef *q,QSPI_CommandTypeDef *c,uint32_t timeout)
{
    uint32_t width,erase=0;(void)q;(void)timeout;transaction=*c;
    if(c->AddressMode!=QSPI_ADDRESS_NONE) {
        int dedicated=c->Instruction==0x0C||c->Instruction==0x12||c->Instruction==0x21||c->Instruction==0xDC;
        width=dedicated?32U:(sr3&1)?32U:24U;
        if(c->AddressSize!=width)return HAL_ERROR;
    }
    switch(c->Instruction) {
    case 0x66:break;
    case 0x99:sr3=(uint8_t)((sr3&~1U)|((sr3>>1)&1U));wel=0;break;
    case 0x06:wel=2;break;
    case 0x04:wel=0;break;
    case 0x01:case 0x11:case 0x31:++status_writes;return HAL_ERROR;
    case 0x20:case 0x21:erase=4096;break;
    case 0xD8:case 0xDC:erase=65536;break;
    default:break;
    }
    if(erase) {
        assert(wel&&c->Address%erase==0&&c->Address+erase<=sizeof(memory));
        memset(memory+c->Address,255,erase);wel=0;
    }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_QSPI_Receive(QSPI_HandleTypeDef *q,uint8_t *out,uint32_t timeout)
{
    (void)q;(void)timeout;
    switch(transaction.Instruction) {
    case 0x9F:assert(transaction.NbData==3);out[0]=0xEF;out[1]=0x40;out[2]=0x19;break;
    case 0x05:*out=wel;break;
    case 0x15:*out=sr3;break;
    case 0x0B:case 0x0C:
        assert(transaction.Address+transaction.NbData<=sizeof(memory)&&transaction.DummyCycles==8);
        memcpy(out,memory+transaction.Address,transaction.NbData);break;
    default:return HAL_ERROR;
    }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_QSPI_Transmit(QSPI_HandleTypeDef *q,uint8_t *data,uint32_t timeout)
{
    uint32_t i;(void)q;(void)timeout;
    assert(transaction.Instruction==0x02||transaction.Instruction==0x12);
    assert(wel&&transaction.Address+transaction.NbData<=sizeof(memory));
    assert((transaction.Address&255U)+transaction.NbData<=256U);
    for(i=0;i<transaction.NbData;++i)memory[transaction.Address+i]&=data[i];
    wel=0;return HAL_OK;
}
static int read_cb(void *ctx,int space,uint32_t a,void *p,size_t n)
{(void)ctx;assert(space==0);return bsp_qspi_read(a,p,n);}
static int write_cb(void *ctx,int space,uint32_t a,const void *p,size_t n)
{(void)ctx;assert(space==0);return bsp_qspi_write(a,p,n);}
static int erase_cb(void *ctx,int space,uint32_t a,size_t n)
{(void)ctx;assert(space==0);return bsp_qspi_erase(a,n);}
static const CabinetBootOps ops={read_cb,erase_cb,write_cb,0,0};
static void exercise(uint8_t power_on_mode)
{
    uint8_t data[600],back[600];CabinetBootRecord record={0},loaded;CabinetPackage *p=&record.installed;unsigned i;
    memset(memory,255,sizeof(memory));sr3=power_on_mode;status_writes=0;
    assert(bsp_qspi_init()==0&&bsp_qspi_jedec_id==0xEF4019&&bsp_qspi_status3==power_on_mode);
    assert(af_f[6]==9&&af_f[7]==9&&af_f[10]==9&&af_f[8]==10&&af_f[9]==10&&af_g[6]==10);
    p->magic=CABINET_PACKAGE_MAGIC;p->format=1;p->board=CABINET_BOARD_ID;p->version=CABINET_FW_VERSION;
    p->cm7_address=CABINET_CM7_BASE;p->cm4_address=CABINET_CM4_BASE;p->cm7_size=p->cm4_size=2048;p->payload_size=4096;
    p->header_crc=cabinet_crc32(p,60);
    assert(cabinet_boot_save(&ops,&record)==0&&record.sequence==1);
    assert(cabinet_boot_load(&ops,&loaded)==0&&!memcmp(&record,&loaded,sizeof(record)));
    /* Power-on/reset preserves ADP, and the committed journal remains readable. */
    assert(bsp_qspi_init()==0&&bsp_qspi_status3==power_on_mode);
    assert(cabinet_boot_load(&ops,&loaded)==0&&!memcmp(&record,&loaded,sizeof(record)));
    record.result=CAB_BOOT_CONFIRMED;assert(cabinet_boot_save(&ops,&record)==0&&record.sequence==2);
    assert(cabinet_boot_load(&ops,&loaded)==0&&!memcmp(&record,&loaded,sizeof(record)));
    for(i=0;i<sizeof(data);++i)data[i]=(uint8_t)(i*13U);
    assert(bsp_qspi_write(0x20F0,data,sizeof(data))==0);
    assert(bsp_qspi_read(0x20F0,back,sizeof(back))==0&&!memcmp(data,back,sizeof(data)));
    assert(bsp_qspi_erase(0,65536)==0);
    for(i=0;i<65536;++i)assert(memory[i]==255);
    assert(status_writes==0&&sr3==power_on_mode); /* no nonvolatile mode changes */
}
int main(void)
{
    exercise(0x60);exercise(0x63);
    puts("PASS: production QSPI BSP: PF10 AF9, ADP=0/1 boot journal save/reload, cross-page program, 4K/64K erase");
    return 0;
}
