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
an immutable playback identity. The sections below trace the configured
generator and one callback-selection event. Synchronization, delayed decoder
callbacks, preload and ad/DJ transitions remain unresolved. The payload's ID and object +0x110
must not be assumed interchangeable across transitions.

The extended probe verifies 184 instructions, callable tables and two
identity literals. Its JSON records the semantic inference separately from
the instruction observations. No Spotify execution, runtime hooks or
production reader changes were made.

## Configured generator provenance

The construction path now connects the candidate to its configured byte
generator. This is conditional static dataflow for the pinned artifact, not
an observation of a running client or a guarantee that a callable cannot be
replaced later.

1. Root factory 0xc42f3c supplies function 0xc485c8 at caller stack +0xa0
   (0xc4423f/0xc44246). Factory 0xc45a30 reads that argument at rbp +0x3d0
   and puts it into a callable with table 0x19f14e0. Its copy/move entry
   0x899a00 preserves the function pointer; invoke entry 0x899a18 calls it
   with the output buffer as rcx.
2. That callable reaches manager constructor 0xc5afac at caller stack +0xa0,
   corresponding to callee rbp +0x280. It moves into manager storage +0x8e8
   (pointer +0x920), then is cloned into a temporary factory configuration.
   Helpers 0xc5b900 and 0xc8d584 transfer it into factory storage +0x58
   (pointer +0x90) at manager +0x938. Helper 0x5ca48 handles both inline
   callable movement and heap-pointer transfer.
3. Manager routine 0xc6b5f8 passes manager +0x938 to 0xc8df88. For the
   input variant tag 0 at +0x2e0, that dispatches to 0xc8d6c4. Configuration
   builder 0xc8e6f0 clones factory +0x90 into config storage +0x98
   (pointer +0xd0). The config passes through 0xc8d23c and 0xc914a0 to
   object constructor 0xc81640, which clones it into object storage +0xa8
   (pointer +0xe0) before filling object +0x110.
4. Configured function 0xc485c8 requests exactly 16 bytes through 0x17d448c.
   That helper obtains a thread-local provider through 0x17d4578 and calls
   its virtual slot +8 with output pointer and size. Provider construction
   0x17d40c4 installs table 0x1c93938. Its fill entry 0x17d434c gathers
   words through virtual slot +0x18 and copies the requested byte count
   into the caller's output. This establishes the byte-generation path; it
   does not establish uniqueness, cryptographic properties or safe lifetime.

## Audio-driver recovery callback selection

Routine 0xc89c84 only enters this handling path when object byte +0x500 is
2. When the earlier 0xc8fac0 branch returns false, it calls 0xc8fa88. That
predicate matches category identity from 0x13e1b2c and error value 12. The
category object at 0x25aeb50 points to table 0x1bac490; its name entry
returns `audio`, and its message entry maps 12 to
`playback_error_audio_driver`.

On that predicate's true branch, 0xc89c84 binds the object pointer to
callback 0xc84bf4, previously traced to replace object +0x110 and clear the
payload variant. It hands the callback to 0xc854d4, which forwards it to
component object +0x10, virtual slot +0x48. The later call through callback
storage +0x10 is a manager/cleanup operation, not evidence that the callback
body ran. This recovery path must be included in identity acceptance tests.

The configured receiver can also be traced statically. The manager's +0x10
interface is supplied as the factory/config first member and becomes
object +0x10. Manager construction installs table 0x1a2b090 on that interface;
slot +0x48 resolves to 0xc6cb80. That function forwards the original callback
through r9 to 0xc66b54, which passes it at caller stack +0x30 to 0xc66eb8
(callee rbp +0x1350, then local rsp +0x68).

When helper 0xc6542c returns non-null and the selected branch completes,
0xc66eb8 calls 0xc71a80 and 0xc8afa4, then loads the original callback.
If its manager field +0x10 is non-null, 0xc67369 directly calls its invoke
field +0x18 with the callback storage as rcx. For the binding above, that
executes 0xc84bf4 synchronously on this call chain: the callback retrieves
the bound object, resets object +0x500 to zero, replaces the ID, and clears
the payload variant. This is a conditional direct-invocation path, not a
claim that every recovery takes this branch or that the client was observed
executing it. The full effects of the preceding helpers, receiver lifetime,
decoder cancellation, delayed callbacks and cross-thread ordering remain
unverified.

The probe now verifies 644 unique instructions, pointer/table entries and
four literals. It records generator and recovery inferences separately from
the checked observations. No production offsets or runtime hooks were added.

## Recovery teardown and shared byte invalidation

Variant-1 destructor 0xc821b8 passes payload +0x320 to 0x7c608. That
helper constructs a reference-counted byte initialized to zero, then calls
0x7c1dc and 0x7c260 to replace the stored pointer/control-block pair. The
exchange returns the old pair; instruction 0x7c678 writes 1 to its byte
before releasing its reference. The exchange is bracketed by calls to
0x17f82ec and 0x17f82fc; their complete synchronization semantics are not
established by these checks.

The setup routine 0xc88514 obtains a reference-counted copy of the pair
at payload +0x320 through 0x181930 and 0x180aa4. Thus the destructor
changes the byte reachable through previously copied pairs rather than
merely clearing its own member. This is an invalidation mechanism. No
callback-drain guarantee follows from the byte write.

One consumer is now traced. On the payload +0x20 nonzero branch, 0xc88514
builds a callable with table 0x1a2cb90, a copied token at callable +8, the
object at +0x18 and payload at +0x20, then passes it to 0xc6ab84. Its
invoke entry 0xc83428 calls 0x1816e8 on the token. That helper copies the
pair, returns true for a null pointer or a nonzero pointed-to byte, and
otherwise returns false. The invoke entry skips its work on true. Only
after a false result does it load the object/payload pointers and dispatch
to 0xc893f8 or 0xc882e8 according to payload +0x220. This establishes
one guarded callback, not all consumers or actual runtime scheduling. A
callback already past the check is not shown to be stopped by invalidation.

Separately, 0xc88514 stores the native result pair from 0xc6b560 at
payload +0x220/+0x228. The destructor decrements the control block's
strong count at 0xc822a5 and only calls its destruction entry when that
count reaches zero. Releasing this payload's ownership therefore does not
by itself establish immediate destruction of the native playback object.
Other owners, remaining token consumers, outstanding callbacks and the ordering of
decoder teardown relative to ID replacement remain open. These are static
observations on the pinned artifact, not observed client execution.

## Guarded callback registration and identity copy

The configured receiver of 0xc6ab84 is now traced. Root factory 0xc42f3c
uses 0xc4512c to construct a heap callable with table 0x1a29310 and stores
its pointer in local callable storage +0x38 (rbp +0x4b8). That storage at
rbp +0x480 is passed at caller stack +0x50 to 0xc45a30. The latter
forwards it at caller stack +0x50 to 0xc5afac, whose rbp +0x230 argument
moves into manager callable storage +0x268, pointer +0x2a0.

Routine 0xc6ab84 loads that pointer and invokes slot +0x10, which resolves
to 0xc47c3c. Both functions forward the original fifth-argument callback
at caller stack +0x20. Function 0xc47c3c passes its callable payload
(callable +8) to 0xc46b8c, where the forwarded callback is read from
rbp +0x730. Routine 0xc41fb8 clones the supplied callback via its table
slot 0 into a separate wrapper. For the guarded callable, clone entry
0xc8339c copies the shared token plus object/payload pointers and retains
table 0x1a2cb90. Wrapper invoke 0xc47edc calls that clone's slot +0x10.
The token guard therefore survives this copy path.

Within 0xc46b8c, the 16 bytes at nested input +0x188 are copied into local
configuration at rbp +0x340, and the callback wrapper is constructed at
rbp +0x380. The same bytes are formatted through 0x17ea5d0 into a
32-character string. The input is payload +0x28 on the traced caller
path, so this is another copy/formatting use of the candidate at payload
+0x1b0. This connects the guarded callback and candidate to the same
configuration-building path; it does not establish later request dispatch,
callback timing, a decoder drain, or the identity of all emitted audio.

## Dispatcher-provider provenance

The provider used by 0xc46b8c is now traced back through its factories.
Function 0xc40e4c reads the first pointer from the table referenced by its
fourth argument, checks it for null, and passes it at caller stack +0xe8
to 0xc40b8c. That function reads rbp +0x1e8 and forwards the pointer at
caller stack +0xe8 to 0xc42f3c. The two frame bases are entry rsp -0xf8
and entry rsp -0xa78 respectively, so the latter reads rbp +0xb68.

The root factory retains this pointer at local rbp +0x88, then passes it
at caller stack +0x30 to 0xc42d2c. That constructor stores it at temporary
+0x1d0. The move into the heap callable by 0xc4512c places it at callable
+0x1d8, which is callable payload +0x1d0. No concrete implementation is
identified merely by following this service-table entry.

At 0xc46f61 the registration routine loads the provider, invokes virtual
slot +0x20 and retains the returned receiver. It moves the configuration
through 0x82c538 and passes the result to 0xc47044. That function loads
the receiver's callable pointer at +0x38 and invokes its slot +0x10, or
takes a separate null path. This identifies the dispatch boundary and
its provider source; the provider implementation, receiver consumption,
callback scheduling/completion and actual runtime selection
still require evidence. The pinned probe now verifies 693 unique
instruction locations plus its table entries and literals.

## Dispatch configuration move and cleanup

The subsequent audit refines the description of 0x82c538: it transfers ownership
rather than independently copying all configuration members. Caller 0xc46f71
passes rbp +0x2e0 as source. Therefore the candidate at rbp +0x340 is
configuration +0x60 and the callback wrapper at rbp +0x380 is configuration
+0xa0. Instructions 0x82c57c/0x82c580 copy the 16-byte candidate field.

For the wrapper, 0x82c538 loads its manager from source +0xb0 and calls it with
operation 1, destination wrapper in rdx and source wrapper in r8. Constructor
0xc41fb8 installed manager 0xbe280. Its operation-1 branch copies the owned
heap-wrapper pointer. The mover then copies the invoke entry (+0x18 relative
to the wrapper) and clears the source manager. Thus this transfer retains the
same heap wrapper and guarded callback clone; it does not make a new clone.

After the dispatch receiver invocation returns, 0xc47044 calls 0x82cdc0 on
the moved configuration. When its callback manager remains non-null, cleanup
calls it with operation 0. The configured manager conditionally destroys the
nested callable via table slot +0x20, then frees the 0x40-byte wrapper. The
source configuration is also cleaned up by the caller, with its transferred
manager already cleared.

This establishes a conditional move/cleanup path, not asynchronous retention
or callback completion. The receiver may consume or move the configuration;
its implementation is still unresolved. No inference that a returned dispatch
means all audio callbacks are drained follows from this cleanup. Exception
paths, runtime selection, outstanding consumers and synchronization remain open.
The pinned probe passes 752 unique instruction checks plus tables and literals;
report: outputs/Soggfy-native-config-move-20261108.json. No production reader
or runtime hook is enabled.

## Conditional streaming receiver and retained callback

The next static trace identifies a concrete candidate behind service-table
entry zero. Registry builder 0x3df310 appends two setup objects through
0x3d9ec4 and 0x3decc4. Their tables are 0x19543c0 and 0x1954500; slot +8
returns service IDs zero and 0xb8 respectively. Their name accessors return
`AdaptiveMediaStreamerSetupImpl` and `TimelineConductorSetupImpl`. The latter
table's slot +0x20 is the already traced factory 0xc40e4c, which requests
dependency zero before reading the first service pointer. This narrows the
candidate; it does not by itself prove registry population, uniqueness of
registration, or runtime resolution of that dependency.

On the service-zero candidate path, factory 0x82b530 calls constructor
0x82c788 and returns its object. The constructor installs table 0x19ed418,
whose slot +0x20 is 0x82d664: it returns object +0x150. Constructor helper
0x82c1b4 allocates a 0x100-byte callable with table 0x19ed4c0. The constructor
uses 0x8f794 to swap it into the receiver at object +0x150; on this heap
pointer branch the callable pointer ends up at object +0x188. Its invoke
entry, slot +0x10, is 0x82d3b8. These conclusions assume the registered
factory is selected and neither provider nor callable is subsequently replaced.

Receiver 0x82d3b8 moves the configuration through 0x82c538 into a local,
then moves it again before calling 0x82d7fc. The established manager-operation-1
semantics transfer the same guarded wrapper at configuration +0xa0 and clear
the old manager. Downstream 0x82d7fc allocates a 0x5a0-byte reference-counted
block, initializes its counts, and constructs the object at block +0x10 via
0x82bac0. It passes the candidate ID at configuration +0x60 in caller stack
+0x40 and the callback wrapper at +0xa0 in caller stack +0x58.

The 0x82bac0 frame base is entry rsp -0x2a8, making those arguments rbp
+0x2f0 and +0x308. It moves the wrapper into a temporary using manager
operation 1, clears the source manager, and forwards the ID and wrapper
at the same caller stack offsets to 0x8310d4. That constructor's frame base
is entry rsp -0x1098, so the arguments are rbp +0x10e0 and +0x10f8. It
copies the 16-byte ID into object +0x350 and moves the wrapper into object
+0x3a8, again clearing the source manager. The returned pair from 0x82d7fc
contains block +0x10 and the control block. Configuration cleanup therefore
does not destroy the transferred wrapper on this normal path.

The object table is 0x19ed718. Its destruction entry 0x836aa4 calls 0x834064,
which invokes the wrapper's manager with operation zero when present and
clears it. This establishes retained callback ownership and conditional cleanup,
not callback completion or synchronization with destruction. The existing
token guard still cannot stop a callback that has already passed its check.
Scheduling, all consumers, exception paths, decoder association, and actual
service resolution remain open. No production reader or hook is enabled.

The expanded pinned probe passes 981 unique instruction checks plus table
and literal checks. Evidence: outputs/Soggfy-native-stream-receiver-20261108.json.
This is Python/Capstone static evidence, not REA-provider or live-client evidence.

## Cleanup completion before wrapper destruction

Destructor 0x834064 calls 0x838940 before its previously traced destruction
of the retained wrapper at object +0x3a8. Helper 0x838940 clears object
+0x1d0 and +0x328 and passes their old pointers to cleanup callbacks
0x8381e8 and 0x838218 through 0x17d1de0. The dispatch receivers are object
+0x10 and +0x18 respectively. The first cleanup calls 0x83f0b8 and frees
the 0x248-byte allocation; the second invokes its object's destruction slot.
The internals of those component destructors are not established here.

The dispatch helper is more than a submission call. Leaf 0x17d2280 compares
the supplied receiver to a TLS pointer. On equality, 0x17d1de0 invokes the
cleanup inline. Otherwise it calls 0x17d1bfc, which allocates a shared 64-bit
counter initialized to one and builds a wrapper carrying the original
callback pointer and counter ownership. Helper 0x17d1f9c submits that wrapper
through receiver virtual slot +0x18. Actual receiver implementation and
scheduling order are still unresolved.

The wrapper's invoke entry is the leaf trampoline 0x17d1bf4, which loads its
heap state and tail-jumps to 0x17d1b94. That function invokes the original
cleanup, then atomically decrements the shared counter and calls 0x17f8418
on the transition to zero. After submission, 0x17d1bfc reads the same counter
and returns normally only after it reaches zero. While positive it calls
0x7e784, which loops until the observed value changes, using 0x17f84e8 as
the wait helper. A negative counter goes to a failure path.

Thus, on these normal returning paths, both submitted component cleanup
operations complete before the destructor proceeds to destroy the retained
wrapper. This narrows the synchronization gap; it does not prove that those
operations drain every audio callback or decoder task. Their component
internals, callback producers, all exception paths, runtime service selection,
and association with the decoder remain open. No runtime hook is enabled.

The pinned probe now checks 1,142 unique instructions plus tables and literals.
Evidence: outputs/Soggfy-native-cleanup-completion-20261108.json. Leaf decoding
ends at the trampoline's terminal jump; it does not treat the following
function as part of that leaf. Evidence remains Python/Capstone static analysis.
