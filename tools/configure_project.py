"""Regenerate Keil target source lists and scatter files, keeping device/debug settings."""
from pathlib import Path
import copy, re, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
project=ROOT/'MDK-ARM/MIPIProject.uvprojx'
tree=ET.parse(project); targets=tree.getroot().find('Targets')
templates={n.findtext('TargetName'):copy.deepcopy(n) for n in targets}
def set_value(node,path,value):
    e=node.find(path)
    if e is None:
        e=node
        for part in path.split('/'):
            child=e.find(part)
            if child is None: child=ET.SubElement(e,part)
            e=child
    e.text=str(value)
def relative(p): return '../'+str(p).replace('\\','/')
common_inc=['Common/Inc','Common/Inc/openamp','Drivers/STM32H7xx_HAL_Driver/Inc',
 'Drivers/STM32H7xx_HAL_Driver/Inc/Legacy','Drivers/CMSIS/Device/ST/STM32H7xx/Include','Drivers/CMSIS/Include']
common_inc += [str(p.relative_to(ROOT)).replace('\\','/') for p in (ROOT/'APP').iterdir() if p.is_dir()]
ipc_inc=['Middlewares/OpenAMP/open-amp/lib/include','Middlewares/OpenAMP/libmetal/lib/include']
metal=['device.c','init.c','io.c','log.c','shmem.c','system/generic/condition.c','system/generic/irq.c',
 'system/generic/time.c','system/generic/cortexm/sys.c','system/generic/generic_device.c',
 'system/generic/generic_init.c','system/generic/generic_io.c']
amp=['remoteproc/remoteproc_virtio.c','rpmsg/rpmsg.c','rpmsg/rpmsg_virtio.c','virtio/virtio.c','virtio/virtqueue.c']
hal=['hal','hal_cortex','hal_rcc','hal_rcc_ex','hal_pwr','hal_pwr_ex','hal_gpio','hal_hsem','hal_flash','hal_flash_ex']
for node in list(targets): targets.remove(node)
for core,boot in [(7,False),(4,False),(7,True),(4,True)]:
    name=('Cabinet_Bootloader' if core==7 else 'Cabinet_CM4_Gate') if boot else f'MIPIProject_CM{core}'
    target=copy.deepcopy(templates[f'MIPIProject_CM{core}']); target.find('TargetName').text=name
    for key in ['pArmCC','pCCUsed']: set_value(target,key,'6070000::V6.7::.\\ARMCLANG')
    set_value(target,'uAC6',1)
    opt=target.find('TargetOption'); common=opt.find('TargetCommonOption'); ads=opt.find('TargetArmAds')
    set_value(common,'OutputDirectory',name+'\\');set_value(common,'OutputName',name)
    set_value(common,'ListingPath',name+'\\');set_value(common,'CreateHexFile',1)
    for stage in ['BeforeMake','BeforeCompile','AfterMake']:
        for i in [1,2]:set_value(common,f'{stage}/RunUserProg{i}',0)
    set_value(ads,'Cads/Optim',4);set_value(ads,'Cads/v6Lang',3);set_value(ads,'Cads/v6LangP',3)
    set_value(ads,'Cads/wLevel',2)
    set_value(ads,'Cads/VariousControls/MiscControls','-std=gnu11 -Wno-everything -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers')
    set_value(ads,'ArmAdsMisc/useMicroLIB',1)
    defines=[f'CORE_CM{core}','USE_HAL_DRIVER','STM32H747xx','USE_PWR_LDO_SUPPLY','USE_FLASH_ECC=0','USE_SDIO_TRANSCEIVER=0']
    inc=[f'CM{core}/Core/Inc']+common_inc
    if boot: defines+=['CABINET_BOOTLOADER' if core==7 else 'CABINET_CM4_GATE']
    else:
        defines+=['VIRTIO_MASTER_ONLY' if core==7 else 'VIRTIO_SLAVE_ONLY','NO_ATOMIC_64_SUPPORT',
            'METAL_MAX_DEVICE_REGIONS=2','METAL_INTERNAL','RPMSG_BUFFER_SIZE=128','LV_LVGL_H_INCLUDE_SIMPLE','LV_CONF_INCLUDE_SIMPLE']
        port='FreeRTOS/portable/GCC/ARM_CM7/r0p1' if core==7 else 'FreeRTOS/portable/GCC/ARM_CM4F'
        inc+=ipc_inc+['FreeRTOS/include',port,'lvgl','lvgl/examples/porting']
    if core==7:inc+=['Middlewares/FatFs']
    set_value(ads,'Cads/VariousControls/Define',','.join(defines))
    set_value(ads,'Cads/VariousControls/IncludePath',';'.join(map(relative,inc)))
    set_value(ads,'LDads/ScatterFile',name+'.sct');set_value(ads,'LDads/umfTarg',0)
    set_value(ads,'LDads/Misc','--keep=*(CABINET_INFO)' if not boot else '')
    groups=target.find('Groups');groups.clear()
    def group(label,paths):
        g=ET.SubElement(groups,'Group');ET.SubElement(g,'GroupName').text=label;files=ET.SubElement(g,'Files')
        for p in paths:
            if not (ROOT/p).is_file(): raise FileNotFoundError(p)
            f=ET.SubElement(files,'File');ET.SubElement(f,'FileName').text=Path(p).name
            ET.SubElement(f,'FileType').text='2' if p.endswith('.s') else '1'
            ET.SubElement(f,'FilePath').text=relative(p)
    group('Startup',[f'MDK-ARM/startup_stm32h747xx_CM{core}.s','Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c'])
    group('Common',['Common/Src/cabinet_core.c','Common/Src/cabinet_retarget.c','APP/BOARD/bsp_board.c'])
    drivers=hal[:]
    if core==7:drivers+=['hal_iwdg','hal_qspi','hal_mdma','hal_sd','hal_sd_ex','ll_sdmmc','hal_rtc','hal_rtc_ex']
    if not boot:
        drivers+=['hal_dma','hal_dma_ex','hal_uart','hal_uart_ex']
        group('FreeRTOS',[f'FreeRTOS/{f}.c' for f in ['tasks','queue','list','timers','event_groups','stream_buffer']]+[
            'FreeRTOS/portable/MemMang/heap_4.c',port+'/port.c','Common/Src/cabinet_rtos_hooks.c'])
        group('OpenAMP',[f'Middlewares/OpenAMP/libmetal/lib/{p}' for p in metal]+
              [f'Middlewares/OpenAMP/open-amp/lib/{p}' for p in amp]+
              ['Common/Src/openamp.c','Common/Src/rsc_table.c','Common/Src/mbox_hsem.c','APP/IPC/bsp_ipc.c'])
        group('Application',[f'CM{core}/Core/Src/main.c',f'CM{core}/Core/Src/stm32h7xx_hal_msp.c',
             'Common/Src/cabinet_image.c','Common/Src/cabinet_modbus.c','Common/Src/cabinet_interrupts.c'])
    else:group('Boot',[f'Bootloader/CM{core}/main.c'])
    if core==7:
        group('Storage',['APP/QSPI/bsp_qspi.c','APP/RTC/bsp_rtc.c','APP/STORAGE/bsp_sd.c','APP/STORAGE/diskio.c',
              'APP/UPDATE/bsp_update.c','Common/Src/cabinet_boot.c','Middlewares/FatFs/ff.c'])
        if not boot:
            drivers+=['hal_i2c','hal_i2c_ex','hal_ltdc','hal_ltdc_ex','hal_dsi','hal_dma2d','hal_sdram','ll_fmc']
            group('Board display',[f'CM7/Core/Src/{p}.c' for p in ['gpio','dma','fmc','usart','i2c','dsihost','ltdc','dma2d']]+
                  [str(p.relative_to(ROOT)).replace('\\','/') for folder in ['LCD','TOUCH','SDRAM','UART','LED'] for p in sorted((ROOT/'APP'/folder).glob('*.c'))])
            group('Cabinet',['APP/SYSTEM/bsp_system.c','APP/SYSTEM/bsp_cabinet.c','APP/UI/bsp_cabinet_ui.c','APP/UI/cabinet_font_24.c'])
            group('LVGL port',['lvgl/examples/porting/lv_port_disp_template.c','lvgl/examples/porting/lv_port_indev_template.c'])
            group('LVGL',[str(p.relative_to(ROOT)).replace('\\','/') for p in sorted((ROOT/'lvgl/src').rglob('*.c'))])
    elif not boot:group('Sensors',['APP/SENSOR/bsp_sensor.c'])
    group('HAL',[f'Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_{p}.c' for p in dict.fromkeys(drivers)])
    targets.append(target)
    base=(0x08000000 if core==7 else 0x08100000)+(0 if boot else 0x20000)
    ram=0x24000000 if core==7 else 0x30000000;size=0x80000 if core==7 else 0x40000;limit=0x20000 if boot else 0xE0000
    if boot: regions=f' ER_CODE 0x{base:08X} 0x{limit:X} {{ *.o (RESET, +First) *(InRoot$$Sections) .ANY (+RO) }}\n'
    else: regions=f''' ER_VECTORS 0x{base:08X} 0x400 {{ *.o (RESET, +First) }}
 ER_INFO 0x{base+0x400:08X} FIXED 0x20 {{ *(CABINET_INFO) }}
 ER_CODE 0x{base+0x420:08X} 0x{limit-0x420:X} {{ *(InRoot$$Sections) .ANY (+RO) }}
'''
    (ROOT/f'MDK-ARM/{name}.sct').write_text(f'LR_FLASH 0x{base:08X} 0x{limit:X} {{\n'+regions+f' RW_RAM 0x{ram:08X} 0x{size:X} {{ .ANY (+RW +ZI) }}\n}}\n')
# uVision has one global file tree across targets. Explicit exclusions prevent
# boot targets from inheriting application files from the other target's tree.
union={}
for target in targets:
    for g in target.find('Groups'):
        bucket=union.setdefault(g.findtext('GroupName'),{})
        for f in g.find('Files'): bucket[f.findtext('FilePath')]=copy.deepcopy(f)
for target in targets:
    groups=target.find('Groups')
    enabled={f.findtext('FilePath') for g in groups for f in g.find('Files')}
    groups.clear()
    for name,files in union.items():
        g=ET.SubElement(groups,'Group');ET.SubElement(g,'GroupName').text=name;fs=ET.SubElement(g,'Files')
        for path,f in files.items():
            f=copy.deepcopy(f);option=ET.SubElement(f,'FileOption');prop=ET.SubElement(option,'CommonProperty')
            ET.SubElement(prop,'IncludeInBuild').text='1' if path in enabled else '0'
            fs.append(f)
ET.indent(tree,space='  ');tree.write(project,encoding='utf-8',xml_declaration=True)
print('Generated four Keil ARM Compiler 6 targets (no download/flash).')
