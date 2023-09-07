Volume Rendering Demo
=====================

![Example images](/images/preview.jpg?raw=true)

This is a small application demonstrating GPU-accelerated volume rendering of
computed tomography (CT) image data. I wrote this program years ago while I was
at university and I had almost forgotten about it when I rediscovered it on an
old backup disk.

The program rotates the camera around a volume and interpolates smoothly between
different color transfer functions. It isn't really useful for any particular
purpose, but I still think the effect looks cool. You can watch in in action
[here](https://versiotech.com/shared/VolumeRenderingDemo.mp4).

Building etc.
-------------

The program was originally created using [MITK](https://mitk.org/) and Qt4. I
could not get it to compile with current releases of these toolkits. Eventually,
I'd like to create an updated version and perhaps get rid of the MITK dependency
altogether. Until then, this repository serves as a backup so I don't lose the
code again.