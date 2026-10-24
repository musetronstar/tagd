# Detection fixtures

These fixtures are locally constructed, not classified to establish expectations.
`pixel.png` is a 1x1 RGB PNG with CRC-checked chunks and a zlib scanline.
`random.bin` repeats all byte values; the PNG and PDF negative examples are
incomplete/invalid formats. A detector recognizing the misleading PDF header is
reported as a false positive, not treated as a test-suite failure.
`late.bin` has a deliberately specified marker at byte 70000, described by
`late.magic`, to demonstrate a real sample-size limit independently of the system
database. Empty and text files have their literal meanings.

Run `make -C tagd/tagd/tests magic_evaluation` from the workspace. The TSV report
lists independent expectations, observed MIME, sample size, and library/database
identity. Wrong specific types count as both false positives and false negatives;
generic misses count as false negatives. Larger samples are not ground truth.
