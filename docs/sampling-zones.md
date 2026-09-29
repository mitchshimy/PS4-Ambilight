# Sampling zones

How the plugin decides which pixels to read, and one bug in it that took a while
to notice.

## Geometry

The default layout is 229 LEDs: 73 along the top, 41 on the right, 73 along the
bottom and 42 on the left (`led_count_top/right/bottom/left`). Each LED gets one
sample zone on the screen edge. `buildZoneGeometry()` in `zones.c` generates the
positions at load time with integer math only, since libm isn't linked into the
plugin.

Where physical LED 0 sits and which way the strip runs are settings, not code:

- `led_start_corner`: `bottom_left`, `bottom_right`, `top_left` or `top_right`
- `led_direction`: `clockwise` or `counterclockwise`
- `led_offset`: rotate the whole thing by N LEDs

That is 8 corner and direction combinations. In v2.0 the edge visit order for all
8 was checked in Python against the old hardcoded bottom-left, clockwise output
before it was ported to C, so the defaults produce exactly what v1.0 did.

Each zone is a `(2*scan_depth+1)^2` block averaged together (3x3 at the default
depth of 1). Depth goes up to 4. The cap is in the plugin because every step costs
`(2*depth+1)^2` tiled reads per zone per pass, and the old ceiling of 10 worked out
to roughly 54M tiled offset calculations per second at 512 zones and 240 Hz. Depth
4 brings that to about 10M.

Zones are also pulled inward by the letterbox detection when it finds black bars,
see [letterbox-detection](debugging/letterbox-detection.md).

## The duplicate corner bug (v2.7.4)

Each edge was generated inclusive of both endpoints. The corner shared by two edges
therefore got a zone from each side, so four corner pixels were each sampled twice,
and four LEDs showed the same color as a neighbor.

It was visible in the raw reads: zone 0 and zone 228 sent bit-identical pixel reads
on every single frame. The fix divides by the LED count instead of count minus one,
so each edge owns its start corner and leaves the far corner to the next edge.

If you change the geometry code, check that zone 0 and the last zone do not read the
same pixel, and that all 4 corners still land exactly on the screen corners.

## Tiling

Reads go through the console's tiled memory layout, not a linear one. The math is in
`tiling.c` (base-only tiling, confirmed against a real screenshot before the plugin
existed) and every read is bounds-checked against the padded buffer size before it
touches memory. `tools/ps4_detile_2dthin.c` is the standalone reference used to
cross-check it.
