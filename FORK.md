# mdxmini fork

Based on gzaffin/mdxmini commit bf8fb02342ed996be6e73d38130c265e54ab5f9f.

Changes:

 - Library-only CMake build option and optional loader regression tests.
 - In-memory LZX 0.42 decompression of the MDX music body, preserving the title, PDX filename, original file location and PDX lookup.
 - Bounded input reads, back-reference checks, required end marker and a 16 MiB decompression limit. The compressed input is never modified on disk.
 - Bounds checks for titles, PDX names and track/voice offset tables. Protected cryptmdx wrappers (including Metal Sight) are rejected rather than played.
 - Failed-open cleanup and repeat-safe close; public mdx_is_fading accessor.

The LZX decoder is a bounded C port of Mamiya's LZX042.NAS, KUMAamp project, (C) Mamiya 2000, GPL. MDXWin credits the same original decoder.
Reference: https://github.com/FIX94/in_mdx/blob/master/LZX042/LZX042.NAS
The fork retains mdxmini's GPL license; see COPYING.

Build with CMake. Set MDXMINI_BUILD_PLAYER=OFF for a library-only build and
MDXMINI_BUILD_TESTS=ON to enable CTest regression checks.
The mdxmini_test executable accepts MDX paths to verify duration and audio;
paths after --reject must fail to load cleanly. Samples are not bundled.
