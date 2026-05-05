#!/usr/bin/env perl

use strict;
use warnings;
use utf8;

use open qw(:std :encoding(UTF-8));

my $FNV1A32_OFFSET_BASIS = 2166136261;
my $FNV1A32_PRIME = 16777619;
my $UINT32_MASK = 0xffffffff;

# Mirror the generated C++ hash exactly so slot placement can be computed
# ahead of time and then re-verified by static_assert in the output.
sub fnv1a32 {
    my ($s) = @_;
    my $h = $FNV1A32_OFFSET_BASIS;

    for my $c (split //, $s) {
        $h ^= ord($c);
        # Perl integers are not fixed-width uint32_t values, so we explicitly
        # truncate after multiplying by the standard FNV-1a 32-bit prime.
        $h = ($h * $FNV1A32_PRIME) & $UINT32_MASK;
    }

    return $h;
}

# Hash tables use a power-of-two size so modulo becomes bit masking.
sub next_power_of_two {
    my ($n) = @_;
    my $p = 1;
    $p <<= 1 while $p < $n;
    return $p;
}

# Find the smallest table whose linear-probe layout keeps every hard tag
# within one step of its home slot. That bound becomes a generated invariant.
sub choose_hash_layout {
    my ($axioms) = @_;
    my $count = scalar @$axioms;
    my $start = next_power_of_two($count * 2);
    $start = 128 if $start < 128;

    for (my $size = $start; $size <= 8192; $size <<= 1) {
        my @table;
        my $max_probe_depth = 0;
        my $ok = 1;

        for my $axiom_index (0 .. $#$axioms) {
            my $slot = fnv1a32($axioms->[$axiom_index]{id}) & ($size - 1);
            my $probe_depth = 0;

            while (defined $table[$slot]) {
                $slot = ($slot + 1) & ($size - 1);
                ++$probe_depth;
                if ($probe_depth > 16) {
                    $ok = 0;
                    last;
                }
            }

            last unless $ok;

            $table[$slot] = $axiom_index;
            $max_probe_depth = $probe_depth if $probe_depth > $max_probe_depth;
        }

        next unless $ok;
        next if $max_probe_depth > 1;

        return ($size, \@table, $max_probe_depth);
    }

    die "could not place hard tag hash table with probe depth <= 1\n";
}

# A rank is stored as big-endian path bytes packed into a uint64_t.
# Prefix order on those bytes is the tagspace containment relation.
sub packed_rank_hex {
    my ($bytes) = @_;
    my $packed = 0;

    for my $i (0 .. $#$bytes) {
        $packed |= ($bytes->[$i] << (56 - ($i * 8)));
    }

    return sprintf '0x%016x', $packed;
}

# Parse only the canonical declaration form. The header is the source of truth;
# this generator should fail if that declarative shape drifts.
sub read_hard_tags {
    my ($source) = @_;

    open my $fh, '<', $source
        or die "cannot open $source: $!\n";

    my @tags;
    my @all_names;
    my %seen_names;

    while (my $line = <$fh>) {
        next if $line =~ /^\s*\/\//;
        next unless $line =~ /inline constexpr std::string_view\s+(HARD_TAG_[A-Z0-9_]+)\{"([^"]+)"\};\s*\/\/\/\s*(HARD_TAG_[A-Z0-9_]+)\s+(HARD_TAG_[A-Z0-9_]+)\s+(tagd::[A-Z0-9_]+)\s*$/;

        my ($name, $id, $sub_relator, $super_object, $pos) = ($1, $2, $3, $4, $5);

        die "duplicate hard tag constant: $name\n" if $seen_names{$name}++;

        push @all_names, $name;
        push @tags, {
            name         => $name,
            id           => $id,
            sub_relator  => $sub_relator,
            super_object => $super_object,
            pos          => $pos,
        };
    }

    die "no hard tags found in $source\n" unless @tags;

    return (\@tags, \@all_names);
}

# Compute each tag's path from the unique root. The root itself is the lone
# byte 0x00; every other rank starts at 0x01 and extends its non-root parent
# path with one more byte: the 1-based ordinal among that parent's children.
sub annotate_ranks {
    my ($tags) = @_;

    my %by_name = map { $tags->[$_]{name} => $_ } 0 .. $#$tags;
    my @roots = grep { $tags->[$_]{name} eq $tags->[$_]{super_object} } 0 .. $#$tags;

    die "expected exactly one root hard tag\n" unless @roots == 1;

    my $root_index = $roots[0];
    my $root_name = $tags->[$root_index]{name};
    my %rank_bytes;
    my %child_ordinal;

    for my $tag (@$tags) {
        die "hard tag id must begin with _: $tag->{name}\n"
            unless $tag->{id} =~ /^_/;
        die "unknown sub_relator $tag->{sub_relator} for $tag->{name}\n"
            unless exists $by_name{$tag->{sub_relator}};
        die "unknown super_object $tag->{super_object} for $tag->{name}\n"
            unless exists $by_name{$tag->{super_object}};
    }

    for my $tag (@$tags) {
        if ($tag->{name} eq $root_name) {
            $rank_bytes{$tag->{name}} = [0];
            next;
        }

        my $parent = $tag->{super_object};
        die "parent $parent must appear before $tag->{name}\n"
            unless exists $rank_bytes{$parent};

        my @parent_rank = $parent eq $root_name ? () : @{ $rank_bytes{$parent} };
        die "rank depth exceeds 8 bytes at $tag->{name}\n"
            if @parent_rank >= 8;

        my $ordinal = ++$child_ordinal{$parent};
        die "too many direct children for $parent\n"
            if $ordinal > 255;

        $rank_bytes{$tag->{name}} = [ @parent_rank, $ordinal ];
    }

    for my $tag (@$tags) {
        $tag->{packed_rank} = packed_rank_hex($rank_bytes{$tag->{name}});
        $tag->{rank_size} = scalar @{ $rank_bytes{$tag->{name}} };
    }

    return $root_name;
}

# These render_* helpers keep the C++ template in __DATA__ readable and keep
# Perl responsible only for computed facts, not handwritten C++ structure.
sub render_axioms_block {
    my ($axioms) = @_;
    return join "\n", map {
        sprintf '    {%s, %s, %s, %s, %s, %d},',
            $_->{name}, $_->{sub_relator}, $_->{super_object}, $_->{pos},
            $_->{packed_rank}, $_->{rank_size}
    } @$axioms;
}

sub render_id_index_block {
    my ($sorted_by_id, $axiom_index_by_name) = @_;
    return join "\n", map {
        sprintf '    {%s, %d},', $_->{name}, $axiom_index_by_name->{ $_->{name} }
    } @$sorted_by_id;
}

sub render_hash_table_block {
    my ($table, $axioms) = @_;
    my @lines;

    for my $slot (0 .. $#$table) {
        next unless defined $table->[$slot];
        push @lines, sprintf '    table[%3d] = &HARD_TAG_AXIOMS[%3d];  // %s',
            $slot, $table->[$slot], $axioms->[ $table->[$slot] ]{name};
    }

    return join "\n", @lines;
}

sub render_static_asserts_block {
    my ($axioms, $table, $root_name, $axiom_index_by_name, $mask) = @_;

    my @lines = (
        'static_assert(HARD_TAG_AXIOM_COUNT == HARD_TAG_AXIOMS.size());',
        'static_assert(hard_tag_ids_are_prefixed());',
        'static_assert(hard_tag_id_index_is_sorted());',
        'static_assert(hard_tag_id_index_matches_axioms());',
        'static_assert(check_hierarchy());',
        sprintf('static_assert(HARD_TAG_AXIOMS[%d].id == %s);',
            $axiom_index_by_name->{$root_name}, $root_name),
        sprintf('static_assert(HARD_TAG_AXIOMS[%d].super_object == %s);',
            $axiom_index_by_name->{$root_name}, $root_name),
        sprintf('static_assert(HARD_TAG_AXIOMS[%d].packed_rank == 0x0000000000000000);',
            $axiom_index_by_name->{$root_name}),
        sprintf('static_assert(HARD_TAG_AXIOMS[%d].rank_size == 1);',
            $axiom_index_by_name->{$root_name}),
        'static_assert(hard_tag_axiom_for_fast(std::string_view{}) == nullptr);',
    );

    for my $axiom_index (0 .. $#$axioms) {
        my $tag = $axioms->[$axiom_index];
        my $base_slot = fnv1a32($tag->{id}) & $mask;
        my ($resolved_slot) = grep {
            defined $table->[$_] && $table->[$_] == $axiom_index
        } 0 .. $#$table;

        die "missing resolved slot for $tag->{name}\n"
            unless defined $resolved_slot;

        push @lines,
            sprintf('static_assert(HARD_TAG_AXIOMS[%d].rank_size > 0);',
                $axiom_index),
            sprintf('static_assert((hard_tag_fnv1a(%s) & HARD_TAG_TABLE_MASK) == %d);',
                $tag->{name}, $base_slot),
            sprintf('static_assert(HARD_TAG_HASH_TABLE[%d] == &HARD_TAG_AXIOMS[%d]);',
                $resolved_slot, $axiom_index),
            sprintf('static_assert(hard_tag_axiom_for_fast(%s) == &HARD_TAG_AXIOMS[%d]);',
                $tag->{name}, $axiom_index),
            sprintf('static_assert(hard_tag_axiom_for(%s) == hard_tag_axiom_for_sorted(%s));',
                $tag->{name}, $tag->{name});

        if ($tag->{name} ne $root_name) {
            push @lines,
                sprintf('static_assert(((HARD_TAG_AXIOMS[%d].packed_rank >> 56) & 0xff) >= 0x01);',
                    $axiom_index);
        }
    }

    return join "\n", @lines;
}

my ($tags, $names) = read_hard_tags($ARGV[0] // die "usage: $0 <hard-tags.h>\n");
my $root_name = annotate_ranks($tags);

my %axiom_index_by_name = map { $tags->[$_]{name} => $_ } 0 .. $#$tags;
my @sorted_by_id = sort { $a->{id} cmp $b->{id} } @$tags;
my ($table_size, $hash_table, $max_probe_depth) = choose_hash_layout($tags);
my $table_mask = $table_size - 1;

my $axioms_block = render_axioms_block($tags);
my $id_index_block = render_id_index_block(\@sorted_by_id, \%axiom_index_by_name);
my $hash_table_block = render_hash_table_block($hash_table, $tags);
my $static_asserts_block = render_static_asserts_block(
    $tags,
    $hash_table,
    $root_name,
    \%axiom_index_by_name,
    $table_mask
);

my $template = do { local $/; <DATA> };
$template =~ s/\@\@AXIOM_COUNT\@\@/scalar(@$tags)/gex;
$template =~ s/\@\@AXIOMS_BLOCK\@\@/$axioms_block/g;
$template =~ s/\@\@ID_INDEX_BLOCK\@\@/$id_index_block/g;
$template =~ s/\@\@TABLE_SIZE\@\@/$table_size/g;
$template =~ s/\@\@TABLE_MASK\@\@/$table_mask/g;
$template =~ s/\@\@MAX_PROBE_DEPTH\@\@/$max_probe_depth/g;
$template =~ s/\@\@HASH_TABLE_BLOCK\@\@/$hash_table_block/g;
$template =~ s/\@\@STATIC_ASSERTS_BLOCK\@\@/$static_asserts_block/g;

print $template;

__DATA__
#pragma once

// Generated by src/gen-hard-tags-lookups.pl from ../include/tagd/hard-tags.h.
// Do not edit this file by hand.

namespace tagd {

inline constexpr std::size_t HARD_TAG_AXIOM_COUNT = @@AXIOM_COUNT@@;

inline constexpr std::array<hard_tag_axiom, HARD_TAG_AXIOM_COUNT>
HARD_TAG_AXIOMS{{
@@AXIOMS_BLOCK@@
}};

inline constexpr std::array<hard_tag_id_index, HARD_TAG_AXIOM_COUNT>
HARD_TAG_ID_INDEX{{
@@ID_INDEX_BLOCK@@
}};

[[nodiscard]] inline constexpr bool hard_tag_id_index_is_sorted() {
    for (size_t i = 1; i < HARD_TAG_ID_INDEX.size(); ++i) {
        if (!(HARD_TAG_ID_INDEX[i - 1].id < HARD_TAG_ID_INDEX[i].id)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline constexpr bool hard_tag_id_index_matches_axioms() {
    for (const hard_tag_id_index& index : HARD_TAG_ID_INDEX) {
        if (index.axiom_index >= HARD_TAG_AXIOMS.size()) {
            return false;
        }

        if (HARD_TAG_AXIOMS[index.axiom_index].id != index.id) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline constexpr bool hard_tag_ids_are_prefixed() {
    // Hard tags form a distinguished subspace H: every id begins with '_'.
    for (const hard_tag_axiom& axiom : HARD_TAG_AXIOMS) {
        if (axiom.id.empty() || axiom.id[0] != '_') {
            return false;
        }
    }

    return true;
}

[[nodiscard]] constexpr uint32_t hard_tag_fnv1a(std::string_view s) noexcept {
    // FNV-1a over raw bytes. std::string_view comparison is also byte-wise,
    // so this remains valid for future UTF-8 hard tag ids.
    uint32_t h = 2166136261u;

    for (unsigned char c : s) {
        h = (h ^ c) * 16777619u;
    }

    return h;
}

[[nodiscard]] constexpr bool rank_prefix_of(
    uint64_t parent,
    uint8_t parent_size,
    uint64_t child,
    uint8_t child_size
) noexcept {
    // In the packed big-endian encoding, parenthood is prefix containment.
    if (parent_size == 0 || child_size == 0 || parent_size > child_size) {
        return false;
    }

    const bool parent_is_root =
        parent_size == 1 && ((parent >> 56) & 0xff) == 0x00;
    if (parent_is_root) {
        return true;
    }

    if (parent == child && parent_size == child_size) {
        return true;
    }

    for (uint8_t byte = 0; byte < parent_size; ++byte) {
        uint8_t pb = (parent >> (56 - (byte * 8))) & 0xff;
        uint8_t cb = (child >> (56 - (byte * 8))) & 0xff;

        if (pb != cb) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline constexpr bool check_hierarchy() noexcept {
    // The declared super_object tree and the packed ranks must describe
    // the same partial order.
    for (const hard_tag_axiom& axiom : HARD_TAG_AXIOMS) {
        if (axiom.id == HARD_TAG_ENTITY) {
            continue;
        }

        bool found_parent = false;

        for (const hard_tag_axiom& parent : HARD_TAG_AXIOMS) {
            if (parent.id == axiom.super_object) {
                found_parent = true;

                if (!rank_prefix_of(
                    parent.packed_rank,
                    parent.rank_size,
                    axiom.packed_rank,
                    axiom.rank_size
                )) {
                    return false;
                }

                break;
            }
        }

        if (!found_parent) {
            return false;
        }
    }

    return true;
}

inline constexpr std::size_t HARD_TAG_TABLE_SIZE = @@TABLE_SIZE@@;
inline constexpr std::size_t HARD_TAG_TABLE_MASK = @@TABLE_MASK@@;
inline constexpr std::size_t HARD_TAG_MAX_PROBE_DEPTH = @@MAX_PROBE_DEPTH@@;

[[nodiscard]] constexpr std::array<const hard_tag_axiom*, HARD_TAG_TABLE_SIZE>
make_hard_tag_hash_table() noexcept {
    // Precomputed open-addressed table: no runtime setup, allocation, or cache.
    std::array<const hard_tag_axiom*, HARD_TAG_TABLE_SIZE> table{};

@@HASH_TABLE_BLOCK@@

    return table;
}

inline constexpr std::array<const hard_tag_axiom*, HARD_TAG_TABLE_SIZE>
HARD_TAG_HASH_TABLE = make_hard_tag_hash_table();

[[nodiscard]] inline constexpr const hard_tag_axiom*
hard_tag_axiom_for_sorted(std::string_view id) {
    const auto it = std::lower_bound(
        HARD_TAG_ID_INDEX.begin(),
        HARD_TAG_ID_INDEX.end(),
        id,
        [](const hard_tag_id_index& entry, std::string_view needle) {
            return entry.id < needle;
        }
    );

    if (it == HARD_TAG_ID_INDEX.end() || it->id != id) {
        return nullptr;
    }

    return &HARD_TAG_AXIOMS[it->axiom_index];
}

[[nodiscard]] inline constexpr const hard_tag_axiom*
hard_tag_axiom_for_fast(std::string_view id) noexcept {
    // Non-hard tags are rejected before hashing by the subspace prefix test.
    if (id.empty() || id[0] != '_') {
        return nullptr;
    }

    const std::size_t slot = hard_tag_fnv1a(id) & HARD_TAG_TABLE_MASK;

    // Probe bound is generated from the concrete table and enforced by static_asserts.
    for (std::size_t probe = 0; probe <= HARD_TAG_MAX_PROBE_DEPTH; ++probe) {
        const hard_tag_axiom* entry =
            HARD_TAG_HASH_TABLE[(slot + probe) & HARD_TAG_TABLE_MASK];

        if (entry == nullptr) {
            return nullptr;
        }

        if (entry->id == id) {
            return entry;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr const hard_tag_axiom*
hard_tag_axiom_for(std::string_view id) {
    return hard_tag_axiom_for_fast(id);
}

@@STATIC_ASSERTS_BLOCK@@

} // namespace tagd
