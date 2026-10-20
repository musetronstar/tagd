# Application Configuration

Each `tagsh` or `httagd` process owns one volatile `_config` tagspace.
`tagd::config` implements the tagspace interface and owns private memory storage.
Applications and TAGL drivers borrow this object; they cannot access its backing
store directly. Selecting another command target does not change the source of
application configuration.

## Startup

Configuration is loaded in this order:

1. Application defaults on `default_config`.
2. `<tagd-home>/_config/_config.tagl`, or the one file supplied by `--config FILE`.
3. Select `--config-id` and supply defaults for any missing effective settings.
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
future direct-file storage need not share that requirement. A home value inside the file can configure subsequent
persistent storage selection but never redirects configuration loading. An
explicit `--home` also overrides that value at the final CLI layer.

```sh
tagsh --tagspace _config --dump
tagsh --config ./development.tagl --tagspace _config --dump
httagd --config ./development.tagl --bind-port 8080
```

`--tagspace _config` selects the initialized config object. Without that option,
the existing default active memory tagspace remains separate. All names beginning
with `_` are reserved: users cannot create or open persistent storage under those
names. `_config` cannot be destroyed or replaced by TAGL operations.

## TAGL Schema

The default mutable subject is `default_config _sub _config`, created with
application defaults before loading the file. Immutable `_config:*` hard tags
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

Both applications accept `--config-id ID`; it defaults to `default_config`.
The subject must exist beneath `_config` after the file is loaded. Unknown IDs,
empty IDs, and hard-tag IDs fail startup. Option order does not matter, and the
selected subject cannot change during runtime.

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
| `_config:default_view` | Absent | View name, or the existing HTTP default view |
| `_config:prompt` | `tagd> ` | Prompt text; empty suppresses the prompt |
| `_config:echo_result_code` | `_true` | `_true` or `_false`; editable |

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
application behavior. URL creation is not a configuration operation.

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

The config tagspace is read-only over HTTP: GET and HEAD remain available;
other methods receive HTTP 403 before request bodies are parsed. This protects
all config subjects, descendants, and referent definitions. Local startup actions
and shell operations still use validated mutations. TODO: authenticated,
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

Runtime tagspace switching and HTTP routing across multiple tagspaces remain
future work. This implementation supplies startup selection and stable config
ownership without defining that future selection protocol.

Dumps retain predicates with hard-tag objects. Hierarchy declarations precede
custom subordinate-relator restoration and predicate output, so a relator may
appear later than a tag that uses it. Config reads remain uncached; any future
optimization must preserve visibility of ancestor and referent changes.
