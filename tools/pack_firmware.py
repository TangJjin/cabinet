"""Create/inspect a paired STM32H747 TF upgrade package. Python standard library only."""
import argparse, binascii, json, struct
from pathlib import Path
BOARD=0x07470101
BASES=(0x08020000,0x08120000)
CAPACITY=0xE0000
def crc(data): return binascii.crc32(data)&0xFFFFFFFF
def inspect_image(data,core):
    if not 0x420<=len(data)<=CAPACITY: raise ValueError(f'CM{core}: size outside 1056..{CAPACITY}')
    magic,board,version,image_core,protocol,*reserved=struct.unpack_from('<8I',data,0x400)
    if (magic,board,image_core,protocol)!=(0x43414249,BOARD,core,1) or any(reserved):
        raise ValueError(f'CM{core}: invalid image metadata at +0x400 (wrong image/board/link address)')
    sp,reset=struct.unpack_from('<II',data)
    low,high=(0x24000000,0x24080000) if core==7 else (0x30000000,0x30040000)
    base=BASES[0 if core==7 else 1]
    if not (low<sp<=high and not sp%8 and reset&1 and base<=(reset&~1)<base+len(data)):
        raise ValueError(f'CM{core}: invalid vector table or memory partition')
    return version
def build(cm7,cm4):
    version=inspect_image(cm7,7)
    if inspect_image(cm4,4)!=version: raise ValueError('CM7 and CM4 firmware versions must match')
    fields=[0x43414250,1,BOARD,version,BASES[0],len(cm7),crc(cm7),BASES[1],len(cm4),crc(cm4),len(cm7)+len(cm4),0,0,0,0]
    head=struct.pack('<15I',*fields);return head+struct.pack('<I',crc(head))+cm7+cm4
def inspect(data):
    if len(data)<64: raise ValueError('Truncated header')
    h=struct.unpack_from('<16I',data)
    if h[:3]!=(0x43414250,1,BOARD) or h[4]!=BASES[0] or h[7]!=BASES[1] or any(h[11:15]):
        raise ValueError('Invalid package format, board, addresses or reserved fields')
    if h[15]!=crc(data[:60]) or h[10]!=h[5]+h[8] or len(data)!=64+h[10]:
        raise ValueError('Header CRC/length mismatch')
    a,b=data[64:64+h[5]],data[64+h[5]:]
    if inspect_image(a,7)!=h[3] or inspect_image(b,4)!=h[3] or crc(a)!=h[6] or crc(b)!=h[9]:
        raise ValueError('Image CRC/version mismatch')
    return {'board':f'0x{BOARD:08X}','version':f'{h[3]>>16}.{(h[3]>>8)&255}.{h[3]&255}',
            'cm7':{'address':f'0x{h[4]:08X}','bytes':h[5],'crc32':f'{h[6]:08X}'},
            'cm4':{'address':f'0x{h[7]:08X}','bytes':h[8],'crc32':f'{h[9]:08X}'}}
def read_hex(path):
    result={};upper=0;eof=False
    for line in Path(path).read_text(encoding='ascii').splitlines():
        if not line:continue
        if eof or not line.startswith(':'):raise ValueError(f'Invalid HEX: {path}')
        raw=bytes.fromhex(line[1:]);n=raw[0];address=int.from_bytes(raw[1:3],'big');kind=raw[3]
        if len(raw)!=n+5 or sum(raw)&255:raise ValueError(f'HEX checksum/length: {path}')
        if kind==0:
            for i,v in enumerate(raw[4:4+n]):
                a=upper+address+i
                if a in result:raise ValueError('Overlapping HEX records')
                result[a]=v
        elif kind==1:eof=True
        elif kind==4 and n==2:upper=int.from_bytes(raw[4:6],'big')<<16
        elif kind==2 and n==2:upper=int.from_bytes(raw[4:6],'big')<<4
        elif kind not in (3,5):raise ValueError(f'Unsupported HEX record {kind}')
    if not eof:raise ValueError('Missing HEX EOF')
    return result
def record(address,kind,data):
    body=bytes([len(data),address>>8,address&255,kind])+bytes(data)
    return ':'+(body+bytes([-sum(body)&255])).hex().upper()
def merge_factory(paths):
    if len(paths)!=4:raise ValueError('Factory image requires exactly four HEX files')
    merged={};parts=set()
    ranges=[(0x08000000,0x08020000),(0x08020000,0x08100000),(0x08100000,0x08120000),(0x08120000,0x08200000)]
    for path in paths:
        data=read_hex(path)
        if not data:raise ValueError('Empty HEX')
        lo,hi=min(data),max(data)
        part=next((i for i,(a,b) in enumerate(ranges) if a<=lo and hi<b),None)
        if part is None or part in parts or lo!=ranges[part][0]:raise ValueError('Missing/duplicate/wrong Flash partition')
        parts.add(part)
        if set(merged)&set(data):raise ValueError('HEX images overlap')
        merged.update(data)
    lines=[];upper=-1;addresses=sorted(merged);i=0
    while i<len(addresses):
        start=addresses[i];page=start>>16
        if page!=upper:lines.append(record(0,4,page.to_bytes(2,'big')));upper=page
        chunk=[merged[start]];i+=1
        while i<len(addresses) and len(chunk)<16 and addresses[i]==start+len(chunk) and addresses[i]>>16==upper:
            chunk.append(merged[addresses[i]]);i+=1
        lines.append(record(start&65535,0,chunk))
    return '\n'.join(lines+[record(0,1,b'')])+'\n'
def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('pack');p.add_argument('--cm7',type=Path,required=True);p.add_argument('--cm4',type=Path,required=True);p.add_argument('--out',type=Path,default=Path('build/UPDATE.BIN'))
    p=sub.add_parser('inspect');p.add_argument('package',type=Path)
    p=sub.add_parser('factory');p.add_argument('hex',type=Path,nargs=4);p.add_argument('--out',type=Path,default=Path('build/FACTORY.hex'))
    args=parser.parse_args()
    try:
        if args.command=='factory':
            output=merge_factory(args.hex);args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(output,encoding='ascii');print(args.out);return
        data=build(args.cm7.read_bytes(),args.cm4.read_bytes()) if args.command=='pack' else args.package.read_bytes()
        details=inspect(data)
        if args.command=='pack':args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_bytes(data)
        print(json.dumps(details,indent=2))
    except (ValueError,OSError,struct.error) as error:parser.exit(1,f'ERROR: {error}\n')
if __name__=='__main__':main()
