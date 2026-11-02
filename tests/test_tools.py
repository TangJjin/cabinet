"""Tests use synthetic firmware and the actual exported binaries when available."""
import importlib.util,struct,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pack',ROOT/'tools/pack_firmware.py')
pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
def image(core,version=0x10000,size=2048):
    data=bytearray([0xA5]*size);base=pack.BASES[0 if core==7 else 1]
    struct.pack_into('<II',data,0,0x24008000 if core==7 else 0x30008000,base+0x441)
    struct.pack_into('<8I',data,0x400,0x43414249,pack.BOARD,version,core,1,0,0,0)
    return data
class ToolsTests(unittest.TestCase):
    def test_roundtrip(self):
        data=pack.build(image(7),image(4));self.assertEqual(pack.inspect(data)['version'],'1.0.0')
    def test_corruption(self):
        data=pack.build(image(7),image(4))
        for index in [0,12,28,60,64,511,len(data)-1]:
            broken=bytearray(data);broken[index]^=1
            with self.assertRaises(ValueError):pack.inspect(broken)
        for broken in [data[:-1],data+b'\0',data[:20]]:
            with self.assertRaises(ValueError):pack.inspect(broken)
    def test_image_metadata_and_vectors(self):
        with self.assertRaises(ValueError):pack.build(image(7),image(4,0x10001))
        with self.assertRaises(ValueError):pack.build(image(4),image(7))
        with self.assertRaises(ValueError):pack.build(b'\0'*1024,image(4))
        for offset,value in [(0,0x20008000),(4,0x08000441),(4,0x08020440),(0x404,0)]:
            data=image(7);struct.pack_into('<I',data,offset,value)
            with self.assertRaises(ValueError):pack.build(data,image(4))
    def test_hex_validation(self):
        with tempfile.TemporaryDirectory() as folder:
            paths=[]
            for i,base in enumerate([0x08000000,0x08020000,0x08100000,0x08120000]):
                path=Path(folder)/f'{i}.hex';path.write_text(pack.record(0,4,(base>>16).to_bytes(2,'big'))+'\n'+pack.record(0,0,b'1234')+'\n'+pack.record(0,1,b'')+'\n');paths.append(path)
            output=Path(folder)/'all.hex';output.write_text(pack.merge_factory(paths));self.assertEqual(len(pack.read_hex(output)),16)
            with self.assertRaises(ValueError):pack.merge_factory([paths[0]]*4)
            paths[0].write_text(':040000003132333400\n:00000001FF\n')
            with self.assertRaises(ValueError):pack.read_hex(paths[0])
    def test_actual_build(self):
        a=ROOT/'build/MIPIProject_CM7.bin';b=ROOT/'build/MIPIProject_CM4.bin'
        if not a.exists() or not b.exists():self.skipTest('Build ARM targets first')
        data=pack.build(a.read_bytes(),b.read_bytes());self.assertLess(len(data),2*1024*1024)
        self.assertEqual(pack.inspect(data)['cm7']['address'],'0x08020000')
if __name__=='__main__':unittest.main(verbosity=2)
