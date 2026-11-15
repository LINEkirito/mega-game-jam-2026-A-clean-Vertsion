"""Prepare spoken dialogue at -16 LUFS (+/-0.5), <= -4 dBTP.

Requires FFmpeg on PATH, --ffmpeg, or the installed imageio_ffmpeg package.
Writes new mono 48 kHz PCM16 WAVs; never overwrites input or existing output.
Import outputs as Voice_* SoundWaves with Volume=1 and Pitch=1.
Use --check to validate existing WAVs without changing them.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import wave

TARGET = -16.0
PEAK = -4.0
TOLERANCE = 0.5


def run(ffmpeg, args):
    return subprocess.run([ffmpeg, '-hide_banner', '-nostdin', *args],
                          capture_output=True, text=True, encoding='utf-8', errors='replace', check=True)


def measure(ffmpeg, path):
    result = run(ffmpeg, ['-i', str(path), '-af',
        'loudnorm=I=-16:TP=-4:LRA=7:print_format=json', '-f', 'null', '-'])
    data = json.loads(re.findall(r'\{[^{}]*\}', result.stderr)[-1])
    return {'lufs': float(data['input_i']), 'true_peak_db': float(data['input_tp'])}


def valid(stats):
    return (math.isfinite(stats['lufs']) and abs(stats['lufs']-TARGET) <= TOLERANCE
            and stats['true_peak_db'] <= PEAK + 0.05)


def check_format(path):
    with wave.open(str(path), 'rb') as wav:
        if (wav.getnchannels(), wav.getframerate(), wav.getsampwidth()) != (1, 48000, 2):
            raise ValueError(f'{path}: expected mono, 48 kHz, PCM16')
        return wav.getnframes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs', nargs='+', type=Path)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--ffmpeg')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    ffmpeg = args.ffmpeg or shutil.which('ffmpeg')
    if not ffmpeg:
        import imageio_ffmpeg
        ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    files = sorted(p for item in args.inputs for p in
                   (item.glob('Voice_*.wav') if item.is_dir() else [item]))
    if not files or len({p.stem for p in files}) != len(files):
        parser.error('Need files with unique output names; directory mode selects Voice_*.wav')
    if not args.check and not args.output_dir:
        parser.error('--output-dir is required when preparing audio')
    report = []
    for source in files:
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        before = measure(ffmpeg, source)
        if not math.isfinite(before['lufs']):
            raise ValueError(f'{source}: silent or too short to measure')
        destination = source if args.check else args.output_dir / (source.stem+'.wav')
        if not args.check:
            if destination.exists() or destination.resolve() == source.resolve():
                raise FileExistsError(f'Refusing to overwrite {destination}')
            destination.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(prefix='dialogue-') as temp:
                mono = Path(temp)/'mono.wav'
                run(ffmpeg, ['-i', str(source), '-ac', '1', '-ar', '48000',
                             '-c:a', 'pcm_s16le', str(mono)])
                frames = check_format(mono)
                gain = TARGET-measure(ffmpeg, mono)['lufs']
                for attempt in range(10):
                    candidate = Path(temp)/f'pass-{attempt}.wav'
                    # Oversample the lookahead limiter; leave 0.3 dB for resampling peaks.
                    filters = (f'aresample=192000,volume={gain:.6f}dB,'
                               'alimiter=limit=0.609537:attack=5:release=50:'
                               'level=false:latency=true,aresample=48000')
                    run(ffmpeg, ['-i', str(mono), '-af', filters, '-c:a', 'pcm_s16le', str(candidate)])
                    after = measure(ffmpeg, candidate)
                    if valid(after):
                        if check_format(candidate) != frames:
                            raise ValueError('Limiter changed sample count')
                        with destination.open('xb') as out, candidate.open('rb') as inp:
                            shutil.copyfileobj(inp, out)
                        break
                    gain += TARGET-after['lufs']
                else:
                    raise ValueError(f'Cannot meet loudness/peak standard: {source}: {after}')
        check_format(destination)
        after = measure(ffmpeg, destination)
        if not valid(after):
            raise ValueError(f'Failed dialogue standard: {destination}: {after}')
        assert hashlib.sha256(source.read_bytes()).hexdigest() == source_hash
        report.append({'source': str(source), 'source_sha256': source_hash,
                       'output': str(destination), 'before': before, 'after': after,
                       'output_sha256': hashlib.sha256(destination.read_bytes()).hexdigest()})
        print(destination.name, after, flush=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
