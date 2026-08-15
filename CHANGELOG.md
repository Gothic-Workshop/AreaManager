# Changelog

## Unreleased

- Added portal-room areas with support for assigning multiple rooms to one area.
- Added optional debug rendering for polygon borders.
- Allowed portal rooms and polygon geometry to be combined in one area.
- Added concave polygon ceilings with a separately interpolated height at every
  X/Y/Z vertex.

## 1.0.0

- Implemented polygon areas with optional Y bounds.
- Added Daedalus externals for creation, queries, registration, removal, and cleanup.
- Added 500 ms hero enter/exit processing with Daedalus callbacks.
- Added an optional periodic Daedalus callback for systems such as area music.
- Added independent minimum and maximum height bounds for Squirrel-compatible areas.
