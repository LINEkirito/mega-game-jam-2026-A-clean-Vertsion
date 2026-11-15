"""Prepare one BGM at -20 LUFS, <= -3 dBTP, stereo 48 kHz PCM16.
Example: python prepare_bgm.py input.mp3 BGM_Menu.wav --report G:/TheTechfiles/report.json
Original recording and existing output files are never overwritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
from prepare_dialogue import run


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--report', required=True, type=Path)
    p.add_argument('--ffmpeg')
    a = p.parse_args()
    if a.output.exists() or a.source.resolve() == a.output.resolve():
        raise FileExistsError('Output must be a new file')
    exe = a.ffmpeg or shutil.which('ffmpeg')
    if not exe:
        import imageio_ffmpeg
        exe = imageio_ffmpeg.get_ffmpeg_exe()
    def measure(path):
        r = run(exe, ['-i', str(path), '-af', 'loudnorm=I=-20:TP=-3:LRA=11:print_format=json', '-f', 'null', '-'])
        return json.loads(re.findall(r'\{[^{}]*\}', r.stderr)[-1])
    original_hash = hashlib.sha256(a.source.read_bytes()).hexdigest()
    before = measure(a.source)
    if before['input_i'] == '-inf':
        raise ValueError('Silent music cannot be normalized')
    filters = ('loudnorm=I=-20:TP=-3:LRA=11:linear=true'
               f":measured_I={before['input_i']}:measured_TP={before['input_tp']}"
               f":measured_LRA={before['input_lra']}:measured_thresh={before['input_thresh']}"
               f":offset={before['target_offset']}")
    with tempfile.TemporaryDirectory(prefix='bgm-') as folder:
        candidate = Path(folder)/'music.wav'
        run(exe, ['-i', str(a.source), '-af', filters, '-ar', '48000', '-ac', '2', '-c:a', 'pcm_s16le', str(candidate)])
        after = measure(candidate)
        if abs(float(after['input_i'])+20) > .5 or float(after['input_tp']) > -2.95:
            raise ValueError(f'Music failed loudness/peak check: {after}')
        a.output.parent.mkdir(parents=True, exist_ok=True)
        with a.output.open('xb') as out, candidate.open('rb') as inp:
            shutil.copyfileobj(inp, out)
    assert hashlib.sha256(a.source.read_bytes()).hexdigest() == original_hash
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps({'source_sha256': original_hash, 'before': before, 'after': after}, indent=2), encoding='utf-8')
    print(a.output, after['input_i'], 'LUFS', after['input_tp'], 'dBTP')


if __name__ == '__main__':
    main()
