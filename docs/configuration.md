# Application Configuration

Each `tagsh` or `httagd` process owns one volatile `_config` tagspace.
`tagd::config` implements the tagspace interface and owns private memory storage.
Applications and TAGL drivers borrow this object; they cannot access its backing
store directly. Selecting another command target does not change the source of
application configuration.

## Startup

Configuration is loaded in this order:

1. Establish application bootstrap configuration.
2. Load `<tagd-home>/_config/_config.tagl`, or the file supplied by `--config FILE`.
3. Validate the file selection, apply `--config-id` if supplied, otherwise infer
   a sole profile. Supply built-in defaults only for missing inherited settings.
4. Explicit command-line configuration overrides on the selected subject.

`--config` replaces the default file source. Neither source is a database;
files are parsed as TAGL, and there is no `_config.db` loading or saving path.
A missing default file is allowed. An existing invalid/unreadable default file,
or any missing/invalid/unreadable explicit file, fails startup.

`--home` determines where the default file is located, independently of option
order. Without it, the existing default home resolution is used (`HOME/.tagd`,
then the account home). If neither exists, memory operation remains available
with an absent home and no implicit config file. An explicit `--config FILE`
still loads. Named home-relative persistent storage requires a usable home;
direct-file `--db` storage does not share that requirement. A home value inside the file can configure subsequent
persistent storage selection but never redirects configuration loading. An
explicit `--home` also overrides that value at the final CLI layer.

```sh
tagsh --tagspace _config --dump
tagsh --config ./development.tagl --tagspace _config --dump
httagd --config ./development.tagl --bind-port 8080
```

`--tagspace _config` selects the initialized config object. `_default` names the
separate anonymous memory tagspace. The selected profile's `_use` predicate
supplies the startup target; explicit `--tagspace` takes precedence, and absent
both the application selects `_default`. All names beginning
with `_` are reserved: users cannot create or open persistent storage under those
names. `_config` cannot be destroyed or replaced by TAGL operations.

## TAGL Schema

Without a config file, application defaults use `default_config _sub _config`.
During file loading this subject starts without setting predicates so built-in
values cannot mask inherited settings. Immutable `_config:*` hard tags
identify settings; `_has` relations on a configuration subject hold their values
as modifiers. Files can update the default subject and declare additional profiles:

```tagl
>> default_config _sub _config _has
    _config:bind_port = 2112,
    _config:log_level = warning;

>> development _sub default_config _has
    _config:bind_port = 8080,
    _config:prompt = "development> ";

>> production _sub _config _has
    _config:bind_port = 9090;
```

Select the configuration subject independently of the active tagspace:

```sh
httagd --config ./config.tagl --config-id development
```

Both applications accept `--config-id ID` as a canonical profile override.
A file may select a profile with `%% _config_id ID;`; the last SET across includes
wins. Its context is retained for referent resolution after loading, permitting
forward declarations. This SET is legal only during config loading. Invalid file
selection fails even if CLI selection would override it.

Without explicit selection, exactly one file-defined profile is selected.
Parents and children both count; repeated declarations count once. Untouched
built-in default_config does not count. Multiple profiles require SET or CLI
selection; no profiles is an error. Missing IDs return TS_NOT_FOUND, and invalid
or ambiguous selections return TS_MISUSE. CLI lookup does not use referents.
The selected subject cannot change during runtime.

Accessors read the selected subject with `F_INCLUDE_INHERITED_PREDICATES`.
The nearest ancestor definition of each setting wins; an empty modifier clears
an inherited modifier where that setting permits an empty value. `development`
inherits log level `warning` above. Independent `production` inherits no values
from `default_config`; any missing built-in settings receive application defaults
after loading. Existing inherited values are never copied onto the selected tag.
Explicit CLI overrides are then written only to the selected subject.

All mutable descendants of `_config` can carry configuration predicates. Naming
conventions distinguish profiles from user-defined setting vocabulary; there is
no separate profile type. Every supplied built-in value is validated, including
values on unselected profiles.

Each setting has one optional modifier. PUT with a supplied value updates it;
a bare PUT preserves an existing local modifier. DELETE with a matching value
clears the modifier while retaining the predicate; bare DELETE removes the whole
local predicate. Clearing a value is rejected when the setting requires one,
including ports, log levels, and booleans. Startup file loading
suppresses duplicate assertions, so repeating defaults and reloading a dump are
valid. Ordinary runtime PUT retains normal `TS_DUPLICATE` behavior.

| Setting | Default | Value and runtime policy |
| --- | --- | --- |
| `_config:home` | Resolved tagd home, or absent | Path when available; fixed at runtime |
| `_config:bind_addr` | `localhost` | Nonempty address; fixed at runtime |
| `_config:bind_port` | `2112` | Integer 1–65535; fixed at runtime |
| `_config:log_level` | `error` | Existing log-level specification; editable |
| `_config:tpl_dir` | `./app/tpl/` | Template directory; editable |
| `_config:www_dir` | `./app/www/` | Static-file directory; editable |
| `_config:favicon` | Absent | Explicit filename, or derive `favicon.ico` under current www directory |
| `_config:default_view` | Absent | Registered view name, or built-in `tagl`; editable |
| `_config:fallback_error_view` | Absent | Registered ERROR view name, or built-in `tagl`; editable |
| `_config:prompt` | `tagd> ` | Prompt text; empty suppresses the prompt |
| `_config:echo_result_code` | `_true` | `_true` or `_false`; editable |

Before serving, httagd checks both view settings against the registered app views.
Unknown default views and fallback names without an ERROR handler fail startup
with `TS_NOT_FOUND`; absent settings and `tagl` are valid. View configuration is
read per request. If a fallback stops resolving, errors use built-in TAGL text.

The optional `httagd/app/config.tagl` selects `browse.html` for fallback errors:
run `httagd --config app/config.tagl` from the httagd directory. This file replaces
the home configuration source; it is not an extra layer of automatic app defaults.

The three fixed settings become immutable after startup configuration and storage
selection, before user `-t`/`-f` actions and the REPL or server loop. Attempts to
change them return an error without changing values or resources. Reasserting an
identical value follows ordinary duplicate handling.

Directory changes must name existing directories. Application-default
paths are allowed even when absent, preserving startup behavior for applications
that do not use those resources. Paths are interpreted relative to the process
working directory, except for the explicitly resolved default configuration file.

All built-in settings must remain present in the selected subject's effective
configuration, even settings with absent modifiers. Deleting a whole local predicate may
reveal a valid inherited value, but cannot remove a required effective value.
The selected subject and its ancestors cannot be deleted. Reparenting is allowed
only when the resulting effective settings remain valid. Runtime edits,
deletions, and moves that change an effective home or listener setting are
rejected before storage changes. A shadowed ancestor value or an unselected
profile's settings can still change. Logger updates follow the selected profile,
including changes to inherited log levels.

All predicates in a proposed configuration write are validated before any are
stored; conflicting values for the same setting in one write are rejected. This
does not add a general multi-statement transaction facility to TAGL.

Local runtime edits use the same validation as startup overrides:

```tagl
>> default_config _has _config:prompt = "live> ";
>> default_config _has _config:echo_result_code = _false;
<< default_config;
```

For optional user-defined settings, declare vocabulary beneath
`_config`, then attach values to the same subject:

```tagl
>> greeting _sub _config;
>> default_config _has greeting = "hello";
>> default_config _has greeting = "welcome";
!! default_config _has greeting = "welcome";
!! greeting;
```

These extension values are visible through TAGL but do not introduce new C++
application behavior. Ordinary supporting tags, relators, and file URL tags can
also be declared outside the profile hierarchy.

## Context and Referents

Configuration files can declare referents and use session contexts, including
Japanese names. Define context tags beneath `_config`, then map local names to
canonical configuration vocabulary:

```tagl
>> 日本語 _sub _config;
>> 設定 _refers_to default_config _context 日本語;
>> 持つ _refers_to _has _context 日本語;
>> ポート _refers_to _config:bind_port _context 日本語;
%% _context 日本語;
>> 設定 持つ ポート = 8181;
```

PUT and DELETE resolve aliases before checking setting types, inherited values,
and runtime restrictions. Storage applies exactly that validated canonical
operation. Referent definitions use ordinary tagspace validation and do not
change the canonical values read by application accessors. GET, QUERY, and
parser classification use the caller's context; `F_NO_TRANSFORM_REFERENTS`
explicitly disables transformation. Dumps and application accessors remain
canonical. File-loading context belongs to the loader session; subsequent
startup actions establish their own context.

Referents identify existing tags; they do not make descendant setting tags
interchangeable with their ancestors. Containment-based setting matching remains
future work when predicate IDs carry ranks. The executable Japanese example is
`tagsh/tests/config-japanese.tagl`, exercised by `tagsh/tests/tester.exp`.

## HTTP Access and Startup

Each HTTP request starts with the server's lifetime default and has its own
selection. TAGL SET may select/open already-registered `.db` targets by ID or
alias, or use already-open `.tagl` instances. A closed `.tagl` can only be loaded
locally: HTTP attempts return `TS_MISUSE`/403 before executing source statements.
Requests cannot supply filesystem locations, register undeclared targets,
include local files, remove aliases, or close shared instances.

Ordinary writes to registered user tagspaces are allowed. HTTP can select/open `_config`, including through aliases, only when the
operator chose it as the server startup default. Otherwise it returns
`TS_MISUSE`/403 before any read or new binding. Config PUT/DELETE is
rejected with 403 at each actual operation target, including after switches.
URL-targeted config writes are rejected before parsing the predicate body.
POST/PUT may create shared volatile aliases through controlled SET shorthand;
this exception does not permit arbitrary config or referent writes. Reads of
config remain available only under that startup-default policy. Local startup actions and shell operations still use
validated mutations. Registration is not per-target authorization; shared aliases
remain exposed to name squatting and growth until access controls are added. TODO: authenticated,
permission-checked configuration writes require a general access-control design.

`localhost` binds to IPv4 loopback (`127.0.0.1`). Use an explicit wildcard address
such as `0.0.0.0` to listen on all IPv4 interfaces.

Both applications treat `--dump` as dump-and-exit. Startup action errors abort
startup. Shell stream input stops at the first failing statement and exits
nonzero; subsequent statements are not executed. These are intentional
corrections to the older startup behavior.

## Runtime Consumers and Persistence

Shell and HTTP consumers retain references to config and read typed accessors
when needed. Accessors return owned values; an operation may retain a local copy
for consistency. The template directory, static-file directory, default view,
and favicon are read at use time. Logger-level changes update the existing
logger resource while preserving its output streams and removing obsolete role
overrides. A log-level property replaces the complete specification; repeated
CLI `--log-level` options accumulate into one specification in argument order.

Batch execution suppresses prompts and result codes as temporary execution state;
it does not overwrite settings changed by TAGL. Command-line parsing supplies
explicit overrides and ordered actions. Application startup chooses the storage,
installs loggers, executes actions, and enters the REPL or HTTP loop.

Runtime changes never rewrite the input file or create a config database. A dump
is ordinary TAGL output and can be saved explicitly by the user. A new process
starts again from defaults, its selected file, and its CLI overrides.

Runtime SET/UNSET switching, first-segment tagdurl routing, autoload, and
direct-file `--db` startup are implemented. Qualified foreign references remain
deferred. Request selection never changes the server default.

Dumps retain predicates with hard-tag objects. Hierarchy declarations precede
custom subordinate-relator restoration and predicate output, so a relator may
appear later than a tag that uses it. Config reads remain uncached; any future
optimization must preserve visibility of ancestor and referent changes.

## Tagspace descriptions and startup intent

The config object owns open runtime instances. Its private tags and referents
are the authoritative descriptions and names; a closed description remains
queryable and reserves its source and names. No registry or status predicates
are written. See [TAGL SET/UNSET](TAGL-spec.md#tagspace-set-and-unset).

```tagl
>> zoology _sub _tagspace;
>> selected_profile _sub default_config _use zoology = zoo;
```

With `--config-id selected_profile`, this selects `zoology` at startup and binds
`zoo` in context `_config`. A CLI `--tagspace` override still establishes that
alias but leaves the profile's unused target closed. Other profiles have no
selection effect. `_use` has one unmodified target or an optional alias modifier;
there may be at most one local declaration along any profile ancestry chain,
even if two declarations name the same target. Independent sibling branches
may choose different targets. Reparenting, deletion, and referent-resolved
mutations receive the same validation before storage changes.

Config files and their includes cannot execute SET `_use`; they remain in
`_config`. Runtime profile `_use` edits neither switch selection nor change
aliases. In `httagd`, the selected profile's effective startup target is frozen
before serving requests. CLI `--create` remains restricted to a non-reserved
home-layout database and refuses to clobber existing storage or reinterpret a
registered `.tagl`/external source. Local SET can open explicit file paths
without a tagd home directory.

## Direct-file databases

Both applications accept `--db PATH` or `--db file:///absolute/path.db`.
Only `.db` sources are accepted. Without `--create` the file must exist;
with `--create` it must not exist and its parent directory must already exist.
No home directory layout is created. Relative paths use the startup working
directory; literal `~/` uses HOME through the same resolver as TAGL sources.

`--db` disables implicit configuration loading and home-based discovery.
Combining it with `--home` is an error. Explicit `--config` and `--config-id`
remain supported. A home setting in the selected configuration produces a
nonfatal TAGL error on STDERR and cannot re-enable home lookup.

Source-first identity is shared with TAGL: reuse an existing source description,
otherwise derive the ID from the filename stem. Optional `--tagspace ID` must
match that canonical ID. Registered IDs and aliases remain available for
switching; additional sources require explicit paths or local file URLs.

## Tagdurl autoload

The selected profile's `_has _config:tagdurl_autoload = _true` enables discovery
of named tagspaces under home. The default is `_false`: names and aliases must
already be registered, though closed databases can reopen. Local runtime edits
take effect on the next operation. The same option governs embedded tagdurls
and HTTP, including HTTP SET `_tagspace` and `_use`. HTTP cannot enable it,
supply filesystem paths, or execute closed `.tagl` sources. Direct-file mode
disables home discovery.

A tagdurl starts with its target, for example `/animals/donkey` or `/_/donkey`.
`_` denotes the current selection; explicit targets in embedded TAGL last for
one statement. In HTTP, each request starts from the fixed server default.
See [tagdurl addressing](TAGL-spec.md#tagdurl-addressing) for description reads,
request restrictions, and migration.

## Filepile home

TAGL `>>+ URL;` uses the active persistent
tagspace and effective home. Content is shared at `<home>/filepile/`; it does
not live below an individual tagspace directory. Physical home aliases resolve
to one root, and generated URLs percent-encode path bytes.

`--db` disables filepile storage with `TS_MISUSE`, explaining the unavailable
home. `_config`, `_default`, and all other volatile targets also reject storage,
even if a home is available. These restrictions do not change ordinary tagspace
operations. See [Filepile storage](filepile.md) for the sequential-process
contract, diagnostics, metadata, and publication guarantees.
