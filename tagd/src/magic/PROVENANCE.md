# Media-type lookup provenance

* Upstream: https://github.com/datasets/media-types
* Commit: `3ef7c3b628ffd821d277f3ef649e5509b4aad0c0`
* Source: `data/media-types.csv`, copied byte-for-byte.
* SHA-256: `f23c5e9fef8a3f814d5508d84218d6093f9c7a6096e321378061ed4229e27c97`
* License: upstream declares ODC-PDDL-1.0; its unchanged README declaration is in `LICENSE.upstream.md`.
* Corrections: `canonical-extensions.csv`, with expected upstream values and rationale.

Refresh explicitly: compare upstream rows and the full effective generated mapping,
review stale/redundant corrections, then commit inputs, provenance, and generated
outputs together. Builds never fetch data. Missing-extension rows are omitted.

The shared detector adapts the structured MIME/charset/result ownership contract
from `tagr/filemagic/src/tagd-filemagic.c`. It classifies caller-owned complete
samples once, copies borrowed libmagic strings, and does not acquire input itself.
The default database is the explicit system path `/usr/share/misc/magic.mgc`;
applications may pass another trusted path. `MAGIC` is ignored. Decompression is
disabled. Resource limits retain finite documented defaults and cap byte/encoding
work at the supplied sample size. The initial filepile sample limit is 64 KiB.
