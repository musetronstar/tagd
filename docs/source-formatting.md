# Source formatting

Use tabs with width four. `.editorconfig` configures cooperating editors;
`.clang-format` defines the C++ layout. Run `make check-formatting` explicitly
to check formatting and whitespace without rewriting files. `make tests` does
not run formatting checks. `make format` formats the files listed in
`tools/format-files.txt`. Add files to that list when deliberately converting
their formatting; do not run a blanket formatter over borrowed code or generated
files. Whitespace checks cover tracked and new project text files, including the
generated domain tables. Markdown line breaks are kept.

Use clang-format 18 for consistent results. No formatter rewrites files during
tests. Existing handwritten files outside the list keep their local layout;
new or revised code uses the project's tab and readability rules.

Git has no automatically loaded, repository-shared setting for how every diff
viewer displays tabs. For terminal diffs viewed with `less`, this repository-local
setting displays tab stops every four columns:

```sh
git config --local core.pager 'less -R -x4'
```

This changes only the pager, not file contents. Other diff viewers need their
own tab-width setting. Git's `core.whitespace` tab width affects whitespace
checks; it does not set a viewer's tab display.

