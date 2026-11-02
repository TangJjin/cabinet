"""Convert the real LVGL test renderer's PPM frames to portable PNG previews."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
target=root/'docs/ui';target.mkdir(parents=True,exist_ok=True)
for source in (root/'build/ui').glob('*.ppm'):
    Image.open(source).save(target/(source.stem+'.png'))
print(f'LVGL previews: {target}')
