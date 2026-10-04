"""Regenerate the firmware font subsets with lv_font_conv 1.5.3."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--font', type=Path, required=True)
parser.add_argument('--converter', type=Path, required=True)
args = parser.parse_args()
ui = root / 'RTT_elog_DMA_UART_ring_project/01_APP/ui'
manifest_path = ui / 'fonts/manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
assert hashlib.sha256(args.font.read_bytes()).hexdigest() == manifest['font_sha256']
for item in manifest['fonts']:
    target = ui / 'fonts' / item['file']
    original = (ui / 'generated/assets/fonts' / item['file']).read_text(encoding='utf-8')
    subprocess.run([
        'node', str(args.converter.resolve()), '--font', str(args.font.resolve()),
        '--size', str(item['size']), '--bpp', '4', '--no-compress',
        '--format', 'lvgl', '--symbols', item['symbols'], '--lv-include', 'lvgl.h',
        '--lv-font-name', target.stem, '-o', str(target),
    ], check=True)
    text = target.read_text(encoding='utf-8')
    for field in ['line_height', 'base_line']:
        value = re.search(r'\.' + field + r'\s*=\s*(\d+)', original).group(1)
        text = re.sub(r'(\.' + field + r'\s*=\s*)\d+', lambda match: match[1] + value, text)
        item[field] = int(value)
    license_text = original[original.index(' * Original Font License:'):
                            original.index(' * This file was automatically generated')]
    text = ('/*\n' + license_text +
            ' * Converted with lv_font_conv 1.5.3; original line height and baseline retained.\n */\n' + text)
    target.write_text(text, encoding='utf-8')
    item['sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
