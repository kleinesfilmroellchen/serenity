import youtube_dl
import subprocess
import os
import sys
from shutil import rmtree


def tryremove(path):
    if os.path.exists(path):
        os.remove(path)


video_url = input('URL for Bad Apple: ')

ytdl_opts = {
    'outtmpl': 'bad_apple.mp4',
    'merge_output_format': 'mp4',
}
tryremove('bad_apple.mp4')
with youtube_dl.YoutubeDL(ytdl_opts) as ydl:
    ydl.download([video_url])

if os.path.exists('frames'):
    rmtree('frames')
os.mkdir('frames')
tryremove('bad_apple.flac')

subprocess.run(['ffmpeg', '-i', 'bad_apple.mp4',
                '-r', '5', 'frames/%04d.png',
                '-acodec', 'flac', '-vn', 'bad_apple.flac'], stdout=sys.stdout)
