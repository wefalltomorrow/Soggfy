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

A further caller trace finds two direct callers of 0xe42aec in PE unwind
ranges: 0x53bcc0 and 0xc46264. Both pass copies made by 0x539e00, which copies
the +0x3b8 pointer without changing it (0x53a154–0x53a15b). In 0xc46264,
that member comes from an external stack argument: [rbp+0xc80] is entry
stack +0x38, copied into the temporary configuration at [rbp+0x7e8]
(configuration base [rbp+0x430], hence +0x3b8). Direct caller 0xc47bcc
dereferences its corresponding incoming pointer and forwards the result.
0xc47bcc itself occurs in a pointer table at 0x1a29440, and 0x53bcc0 at
0x19a4a28. Thus these additional edges preserve external provenance; they
still do not identify the concrete state type or establish non-nullability.
The reproducible probe includes these instruction windows as well.

## Non-null state path and field boundaries

The next trace identifies a concrete non-null argument path, conditional on
the manager retaining the callable installed by the inspected construction:

1. Factory 0xc45a30 installs table 0x1a29430 on a 0x2f8-byte callable. Its
   invoke slot +0x10 is 0xc47bcc. The callable is passed to constructor
   0xc5afac through stack +0x48 and transferred into manager member +0x228
   by 0x5ca48. The heap callable pointer consequently occupies manager +0x260.
2. Helper 0xc6b560 loads that member, invokes slot +0x10, and supplies the
   address of a temporary pointer as its seventh argument. That pointer is
   incoming r8 +8 when r8 is non-null, and null otherwise. Caller 0xc88514
   supplies its own object as r8 and obtains the manager from object +0x20.
3. Together with the already traced forwarding chain through 0xc47bcc,
   0xc46264 and 0xe42aec, the external state is therefore invoking object +8.
   The upstream read at state +0x9b8 selects object +0x9c0 through +0x9cf.
4. Caller 0xc894b8 updates a tagged payload at object +0x7b8, then calls
   0xc88514 for variant tag 1. The read is thus payload +0x208 through +0x217.
   The variant helper 0xc7e5b4 calls either construction routine 0xc80988 or
   assignment routine 0xc82948. Both expose the same field boundaries.

| Region selected by upstream | Static field evidence |
| --- | --- |
| Payload +0x208, +0x209 | Separate byte fields copied by 0xc81b50 / 0xc82b9c at nested offset +0x1e0 / +0x1e1, with nested base payload +0x28 |
| Payload +0x20a through +0x20f | Not assigned by those field-copy sequences; apparent padding, not proven initialized ID bytes |
| Payload +0x210 through +0x217 | First word of an ownership pair at +0x210 / +0x218; assignment via 0x50714 moves both words and releases the old reference-counted owner |

This contradicts treating those 16 bytes as a demonstrated playback ID on
this particular path. A nonzero test could accept mixed fields containing
pointer bytes. It does not establish which path actual playback takes, rule
out later object/member replacement, identify the correct ID field, or prove
any lifetime or transition behavior. The null fallback and this non-null
case are separate conditional findings, not universal incompatibility claims.
The parent-path portion verifies 120 selected instructions plus table entries against
the pinned DLL and records both inferences with their conditions. Production
identity integration remains disabled pending a verified field and lifecycle.

## Playback-ID consumer, encoding and producer edge

The same pinned artifact provides semantic evidence for a different field;
this remains a static candidate, not a replacement production offset. In
0xc88514, r13 is the variant payload and rsi is payload +0x28. At 0xc88b2e,
the address nested +0x188 (payload +0x1b0) is passed to 0xc803ec, which forwards
it to the 0xc8fc60 session constructor. That constructor copies 16 bytes and
stores them in the session's first string. Consumer 0xc8fe98 passes that
string through 0xc900e4 for the log message
`Failed to commit pending BoomboxPlaybackSession (playback_id: %s), error: %s`.

Formatter 0xc900e4 requires string length 16 and calls leaf 0x17ea5d0. The
leaf uses the literal `0123456789abcdef`, emitting the high nibble then low
nibble for each byte in order. The resulting ID is 32 lowercase hex
characters. The leaf has no unwind entry: the probe decodes from the checked
direct-call target through its return, rather than inventing an unwind range.

For the previously traced variant-1 object, this field is object +0x968,
equivalent to state +0x960 only while state equals object +8. These are
conditional arithmetic relationships, not a supported pointer chain. A
producer edge in 0xc9190c copies object +0x110 into temporary payload +0x1b0
(stack offsets +0x4e0 minus +0x330), then transfers that payload through
0xc80988 and 0xc894b8. The nested copy at 0xc81b8e/0xc81b97 preserves the
16 bytes. Another caller, 0xc92690, has the same source and relative layout.

The object constructor invokes callable object +0xe0, slot +0x10, with
object +0x110 as its output. Routine 0xc84bf4 invokes that callable again,
writes the returned 16-byte result to object +0x110 and subsequently clears the
payload variant at +0x7b8 (tag at +0xae8), destroying variant 1 through
0xc821b8 where applicable. Thus the object address alone cannot establish
an immutable playback identity. The callable implementation, event causing
that mutation, synchronization, delayed decoder callbacks, preload and
ad/DJ transitions remain unresolved. The payload's ID and object +0x110
must not be assumed interchangeable across transitions.

The extended probe verifies 184 instructions, callable tables and two
identity literals. Its JSON records the semantic inference separately from
the instruction observations. No Spotify execution, runtime hooks or
production reader changes were made.
