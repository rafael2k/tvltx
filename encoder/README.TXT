# Commands to create compatible streams to TVLTX Encoder Test mode
# add "-re" to run ffmpeg in the speed of playback time (put before input)

# To create the audio
ffmpeg -f lavfi -i sine=frequency=1000 -c:a pcm_s16le -f s16le -ar 48000 -ac
2 output.raw

# To create the audio
ffmpeg -f lavfi -i testsrc=size=1280x720:rate=25 -pix_fmt uyvy422 output.yuv


Bellow there is bash script with an usage example:

#!/bin/sh

# tests to create pes, in order to evaluate wtf is happening...
# in order to create some test files for the test:
#

rm audio.pcm
rm video.yuv

mkfifo audio.pcm
mkfifo video.yuv

# Audio
ffmpeg -f lavfi -i sine=frequency=1000 -c:a pcm_s16le -f s16le -ar 48000 -ac 2 -y audio.pcm &

# Video
ffmpeg -f lavfi -i testsrc=size=1280x720:rate=25 -pix_fmt uyvy422 -y video.yuv &

sleep 1
tm-muxer tvltx.muxer.xml &

sleep 1
../encoder_no_bm -b 2 -i 720p25 -o 720p25 -v 4000 -n 1 -a 128 -j 0x3 -s video-1.pes,audio1-1.pes -z &


