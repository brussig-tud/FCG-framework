# Image decoder fixtures

Generated locally with Pillow 11.1.0; no external artwork. All fixtures are 3 by 2 pixels.

`colors.png`, `colors.tga`, and lossless `colors.webp` contain, row by row:
red (alpha 128), green, blue; white, black, yellow (all other alpha values 255).
`palette.bmp` uses an indexed red/green/blue palette with rows 0,1,2 and 2,1,0.
`palette.gif` uses the same pixels with green designated transparent.
`solid.jpg` is RGB (240,30,10), encoded at quality 95; decoder output is tested with tolerance.

Binary fixtures are checked in so builds do not need Python or Pillow.
