%%ffmpeg -framerate 24 -f image2 -start_number 547 -i ..\videoframes\frame%%05d.png -vcodec libx264 -crf 20 -pix_fmt yuv420p -vf scale=1902:-2 output.mp4
ffmpeg -framerate 24 -f image2 -i ..\videoframes\frame%%05d.png -vcodec libx264 -crf 20 -pix_fmt yuv420p -vf scale=1902:-2 output.mp4
