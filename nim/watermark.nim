import varnish
import pixie, strutils

# This demo program implements watermarking through POST
# requests (although it could be done in many other ways).
# The watermark JPG is embedded into the program using the
# Nim keyword staticRead(), which is fine.
# If we want to use another watermark, simply send a new
# program. It will not disturb previously running requests.
# The image to be watermarked should probably be retrieved
# from a backend, but just for this demo we are embedding
# it too, but we are decoding it live in order to show what
# the actual processing time would look like: It is around
# ~100ms on my laptop.
# You can also POST an image, and it will be watermarked
# too (expecting PNGs).

const raw_image = staticRead("../assets/spooky.jpg")
const raw_watermark = staticRead("../assets/big_watermark.jpg")
let wmark = decodeImage(raw_watermark)

proc on_get(url: string, arg: string): (int, string, string) {.cdecl} =
    # TODO: Fetch from URL instead
    var image = decodeImage(raw_image)

    image.draw(wmark, blendmode = MultiplyBlend)
    let output = encodeImage(image, PngFormat)

    Http.set("X-Memory-Usage: " & formatSize(nim_workmem_current()))

    set_cacheable(false, 1.0f, 0.0f, 0.0f)
    (200, "image/png", output)

proc on_post(url: string, arg: string, ctype: string, buffer: string): (int, string, string) {.cdecl} =
    # Here we watermark a POSTed image
    var image = decodeImage(buffer)

    image.draw(wmark, blendmode = MultiplyBlend)
    let output = encodeImage(image, PngFormat)

    Http.set("X-Memory-Usage: " & formatSize(nim_workmem_current()))

    set_cacheable(false, 1.0f, 0.0f, 0.0f)
    (200, "image/png", output)

proc on_error(url: string, arg: string, exception: string): (int, string, string) {.cdecl} =
    (500, "text/plain", exception)

set_get_handler(on_get)
set_post_handler(on_post)
set_error_handler(on_error)
