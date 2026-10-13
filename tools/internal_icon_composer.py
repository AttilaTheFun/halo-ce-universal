#!/usr/bin/env python3
"""Render the privately extracted retail artwork with Apple's icon treatment.

All generated artwork stays under ignored build/. This is personal branding,
so keep this helper on apple/ios-internal rather than the public PR branch.
"""
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def render_macos_icon():
    directory = ROOT / 'build/internal-artwork'
    source = directory / 'Assets.xcassets/AppIcon.appiconset/AppIcon.png'
    if not source.is_file():
        raise FileNotFoundError('Run tools/internal_disc_icon.py with your own ISO first')
    document = directory / 'Halo.icon'
    (document / 'Assets').mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, document / 'Assets/Halo.png')
    (document / 'icon.json').write_text(json.dumps({
        'fill': {'solid': 'srgb:0.02,0.04,0.06,1'},
        'groups': [{
            'name': 'Halo',
            'layers': [{'image-name': 'Halo.png', 'name': 'Retail artwork'}],
            'shadow': {'kind': 'neutral', 'opacity': 0.35},
            'translucency': {'enabled': False, 'value': 0.5},
        }],
        'supported-platforms': {'squares': 'shared'},
    }, indent=2))
    developer = Path(subprocess.check_output(['xcode-select', '-p'], text=True).strip())
    tool = developer.parent / 'Applications/Icon Composer.app/Contents/Executables/ictool'
    output = directory / 'Halo-macOS.png'
    subprocess.run([str(tool), str(document), '--export-image', '--output-file', str(output),
                    '--platform', 'macOS', '--rendition', 'Default',
                    '--width', '1024', '--height', '1024', '--scale', '1'],
                   check=True, stdout=subprocess.DEVNULL)
    return output


if __name__ == '__main__':
    print(render_macos_icon())
