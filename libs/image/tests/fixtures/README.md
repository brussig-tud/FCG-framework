# Image decoder fixtures

The integer-color fixtures below were generated locally with Pillow 11.1.0; no external artwork. Each is 3 by 2 pixels.

- `colors.png`, `colors.tga`, and lossless `colors.webp` contain, row by row:
red (alpha 128), green, blue; white, black, yellow (all other alpha values 255).
- `palette.bmp` uses an indexed red/green/blue palette with rows 0,1,2 and 2,1,0.
- `palette.gif` uses the same pixels with green designated transparent.
- `solid.jpg` is RGB (240,30,10), encoded at quality 95; decoder output is tested with tolerance.

Binary fixtures are checked in so builds do not need Python or Pillow.


# HDR fixtures

`float.exr` is an original, generated 1 × 1 uncompressed OpenEXR fixture with FLOAT B/G/R channels and values 275, 2,
0.125 respectively. Its header and scanline were constructed directly from the OpenEXR file layout; it has no
third-party image content. It tests generic-only static registration without linking a fixture writer.

HDR/PFM tests construct original small encoded inputs in memory. EXR-enabled tests generate additional PIZ/ZIP,
HALF/FLOAT and tiled/scanline files with the OpenEXR writer in the test scratch directory. Generated color-metadata
fixtures cover primary and white-point recognition, explicit transfers, selective guessing, and metadata precedence.
Radiance fixtures also check gamma decoding and RGB/XYZ conversion with Bradford white adaptation against independent
reference values. Primary guessing uses the conventional Radiance gamut, whose conversion changes numeric samples.

The regular tests do not require the 3 versions of the third-party "memorial" image. With OpenEXR enabled, the separate
`image-exr-assets` test compares local `assets/memorial.exr`, `assets/memorial.hdr`, and `assets/memorial.pfm` when all
three are present. An absent directory or incomplete set produces a CTest skip (exit code 77). If all three files are
present, decoding errors or mismatched samples fail the comparison. The images are not downloaded by the tests and are
not included among these fixtures.

The GPU `texture-smoke` test also uses any locally available Memorial file supported by the enabled handlers. It
compares texture samples with the image returned by each loading route: guessing, an upload fallback, and both
sample-preservation policies. Missing files are ignored; EXR is checked only when its handler is enabled. A mandatory
generated Radiance fixture checks conversion and upload of negative and above-one components without teaching assets.


# Running the checks

After building the framework, run from the repository root:

```sh
ctest --preset release -R 'image-|texture-smoke|install'
```

Repeat with `release-shared` for shared libraries, and check both `FCG_IMAGE_OPENEXR=OFF` and
`FCG_IMAGE_OPENEXR=ON`. The enabled configuration adds `image-exr-smoke` and `image-exr-assets`; the former uses
generated fixtures and the latter optionally compares the local Memorial assets described above. These tests do
not download the teaching assets. Documentation validation is described in
[Maintaining the framework documentation](../../../../docs/DOCS.md).
