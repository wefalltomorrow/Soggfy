# Native identity compatibility audit — November 8, 2026

The RC74 executable-only check was incorrect. Decoder discovery belongs in
Spotify.dll. Historical RC74 packages and reports are preserved unchanged; this
record supersedes their incompatibility conclusion.

Pinned DLL: version 1.3.1.234, 42,852,224 bytes, SHA256
`210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`.
Upstream: Mainkill1/Floggfy v1.3-RC6, commit
`7b825e3b942596c3e912fbbf7a0487abd7578e47`.

Method: map PE sections into SizeOfImage, apply DIR64 relocations, invoke
unmodified upstream discovery against the mapped bytes. No Spotify code was
executed and no live hooks were installed. The local Zig probe is diagnostic
only; shipped binaries still require MinGW CI.

| Static result | Evidence |
| --- | --- |
| Capture discovery | Found all six requested functions |
| Dispatcher | Unique anchor, RVA 0xe5c8d8 |
| Playback pump | RVA 0xe5d518, unwind end 0xe5dc07 |
| Stereo pump anchor | Unique at RVA 0xe5d9a4 |
| Decoder member | Pump loads member +0x70 before dispatcher call |
| Ogg capture ownership | Verified delta -0x88 |
| FLAC capture ownership | Verified delta 0 |
| AAC ownership | Not verified |

These results establish discovery compatibility, not runtime identity correctness.
The upstream playback-ID pointer path {0x78, 0x1e8, 0x9b8}, its encoding and
lifetime are not verified. The pump's +0x78 access uses a helper, requiring
further analysis. Pointer reuse, preloading, staged transitions and interstitial
attribution need dedicated fixtures and pinned-client acceptance. No stall fix
or comprehensive ad isolation is claimed.

Follow-up static inspection: pump RVA 0xe5d587 loads member +0x78 and calls
0x13b51b8, which selects a front pointer through container-like indexing.
Constructor 0xe5b088 stores its third argument at +0x78; caller 0xe423cc
forwards an external argument. This has not resolved the concrete object type.
It neither validates nor disproves the upstream pointer path: a container can
be a prefix of a larger object, and literal displacement searches cannot prove
the absence of composed offsets. No runtime hooks were added by this audit.

A bounded indirect-reference follow-up found the constructor caller at slot
+0x10 of the pointer table at RVA 0x1a52c00 (entry RVA 0x1a52c10). Functions
0xe41cd4 and 0xe422f8 allocate 0x118 bytes and store that table pointer at the
object's start. This suggests an indirect callable object and explains why the
earlier direct-call search was insufficient; the actual invocation and concrete
argument provenance are still unresolved. Evidence was obtained with the local
Python/Capstone scripts against a byte-verified PE mapping. It is not REA provider
evidence: Ghidra was unavailable. No playback-ID layout or lifetime is inferred
from this table observation.

## Parent and optional state argument follow-up

The subsequent bounded trace resolves the invocation argument on one construction
path. Factory 0xe41cd4 requests a 0x998-byte allocation, constructs it with
0xe4c000, and passes a callable whose table is 0x1a52c00. Constructor 0xe4c000
clones that callable through slot zero (0xe422f8) into its member at +0x88.
Function 0xe52ad0 loads that member and invokes slot +0x10 at 0xe52b9c, with
its third argument set to the containing object +0x20. The callable 0xe423cc
forwards this argument to 0xe5b088, which stores it at pump owner +0x78.
This resolves the earlier missing argument provenance for this path; it does
not establish all dynamic dispatch paths or runtime identity.

Consequently the upstream second dereference, source +0x1e8, selects the
containing object's +0x208 pointer. At 0xe4c2ba–0xe4c2d9, the constructor
initializes that pointer to its own +0x200 unless an optional external argument
is non-null. In the null-argument case, if this fallback remains unchanged
through pump use, the upstream final 16-byte read would start at parent +0xbb8
and end at +0xbc8, beyond the 0x998-byte allocation. This is a conditional
static counterexample to assuming that discovery alone validates the path.
It is not an observed runtime read, proof that playback takes this fallback,
or a claim of universal client incompatibility. Page readability would not
establish that those bytes belong to the object.

The external argument is also traced one level farther: caller 0xe42aec passes
the address of its second argument's +0x3b8 member into 0xe41cd4, which
dereferences it and forwards the value into constructor 0xe4c000. Its concrete
type, nullability during real playback, ownership and lifetime remain unknown.
A literal +0x208 reference scan found several virtual calls through this
member; it cannot exclude later writes via aliases or composed offsets.
Constructor linear disassembly also encounters a jump at 0xe4ca06 to
0x224f884; this audit does not claim complete control-flow recovery.

Reproduction: run tools/probe_native_identity_parent.py with the pinned DLL
and an output JSON path (requires pefile and Capstone; an optional
--capstone-path selects a local installation). The probe rejects any other
DLL hash, maps the PE itself, checks selected instructions from unwind starts
and the callable table, and saves instruction bytes and neighboring windows.
Local evidence: outputs/Soggfy-native-parent-path-20261108.json. No client code
was executed and no identity reader or hook was enabled. Next evidence needed:
the external state's concrete layout, later mutations, ID encoding, lifecycle,
reuse and preloading/interstitial transitions.
