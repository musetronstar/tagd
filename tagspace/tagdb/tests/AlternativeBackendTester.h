#pragma once

#include <cxxtest/TestSuite.h>
#include "tagdb.h"
#include "tagdb/canonical-cache.h"
#include "tagdb/read-snapshot.h"

/*
 * A deliberately limited test backend proves cache composition without SQL.
 * It accepts flat tags and direct predicates; unsupported semantics fail explicitly.
 * Copying the map at the outermost read models an immutable backend snapshot.
 */
class MapBackend final : public ::tagdb::tagdb, private ::tagdb::snapshot_source {
	std::map<std::string, tagd::abstract_tag, std::less<>> _tags, _view;
	unsigned _depth = 0;
	bool _open = false;
	size_t _version = 0, _cached_version = 0;
	::tagdb::canonical_cache _cache;

	tagd::code begin_read() override {
		if (_depth++ != 0)
			return tagd::TAGD_OK;
		if (!_open)
			return tagd::TS_MISUSE;

		_view = _tags;
		if (_cached_version != _version)
			_cache.clear();
		_cached_version = _version;
		_cache.begin(true);
		_cache.expire();
		return tagd::TAGD_OK;
	}

	void end_read() override {
		if (--_depth != 0)
			return;

		_cache.end();
		_view.clear();
		if (_cached_version != _version)
			_cache.clear();
	}

public:
	tagd::code open(const ::tagdb::storage_location& location) override {
		if (_open || location.kind != ::tagdb::storage_kind::memory)
			return tagd::TS_MISUSE;

		_open = true;
		return tagd::TAGD_OK;
	}

	tagd::code open_named(const std::string&, const std::string&, ::tagdb::open_mode) override {
		return tagd::TS_NOT_IMPLEMENTED;
	}

	void close() override {
		_cache.clear();
		_tags.clear();
		_open = false;
	}

	cache_metrics cache_status() const override {
		return _cache.cache_status();
	}

	void configure_cache(size_t bytes, size_t entries,
		::tagdb::canonical_cache::cache_clock::duration idle,
		std::function<::tagdb::canonical_cache::cache_clock::time_point()> now) override {
		_cache.configure_cache(bytes, entries, idle, std::move(now));
	}

	tagd::code lookup_rank(tagd::rank& result, tagd::id_view id) override {
		::tagdb::read_scope read(*this);
		if (read.status != tagd::TAGD_OK)
			return read.status;

		return _cache.rank(result, id, true, [&](auto& value) {
			auto found = _view.find(id);
			if (found == _view.end())
				return tagd::TS_NOT_FOUND;

			value = found->second.rank();
			return tagd::TAGD_OK;
		});
	}

	tagd::code order_predicates(tagd::predicate_set& values) override {
		::tagdb::read_scope read(*this);
		if (read.status != tagd::TAGD_OK)
			return read.status;

		return values.order([&](auto& rank, auto id) {
			return lookup_rank(rank, id);
		});
	}

	tagd::code resolve_referents(tagd::abstract_tag& result, const tagd::abstract_tag& input,
		::tagdb::session* session) override {
		if (session && !session->context().empty())
			return tagd::TS_NOT_IMPLEMENTED;

		result = input;
		return tagd::TAGD_OK;
	}

	tagd::code put(const tagd::abstract_tag& value, ::tagdb::session*, ::tagdb::flags_t = 0) override {
		_cache.clear();
		if (!_open)
			return tagd::TS_MISUSE;
		if (value.super_object() != HARD_TAG_ENTITY)
			return tagd::TS_NOT_IMPLEMENTED;

		// Test ranks are distinct siblings beneath _entity, independent of ID spelling.
		tagd::rank rank;
		const char ordinal[] = {static_cast<char>(32 + ++_version), '\0'};
		auto rc = rank.init(ordinal);
		if (rc != tagd::TAGD_OK)
			return rc;

		_tags.insert_or_assign(value.id(), tagd::abstract_tag(value, rank));
		_cache.clear();
		return tagd::TAGD_OK;
	}

	tagd::code get(tagd::abstract_tag& result, tagd::id_view id, ::tagdb::session*,
		::tagdb::flags_t flags = 0) override {
		if (flags)
			return tagd::TS_NOT_IMPLEMENTED;

		::tagdb::read_scope read(*this);
		if (read.status != tagd::TAGD_OK)
			return read.status;
		auto found = _view.find(id);
		if (found == _view.end())
			return tagd::TS_NOT_FOUND;

		::tagdb::canonical_cache::canonical_identity identity;
		tagd::rank rank;
		const std::string key(id);
		auto rc = _cache.identity(identity, rank, key, [&](auto& value, auto& r) {
			const auto& tag = found->second;
			value = {tag.sub_relator(), tag.super_object(), tag.pos()};
			r = tag.rank();
			return tagd::TAGD_OK;
		});
		if (rc != tagd::TAGD_OK)
			return rc;

		result = tagd::abstract_tag(tagd::abstract_tag(id, identity.sub_relator,
			identity.super_object, identity.pos), rank);
		return _cache.predicates(result.relations, key, true, [&](auto& rows) {
			_cache.record_predicate_read();
			for (const auto& p : found->second.relations) {
				rows.push_back({p.relator, p.object, p.modifier});
			}
			return tagd::TAGD_OK;
		}, [&](auto& values) {
			return order_predicates(values);
		});
	}

	tagd::code del(const tagd::abstract_tag& tag, ::tagdb::session*, ::tagdb::flags_t = 0) override {
		_cache.clear();
		++_version;
		return _tags.erase(tag.id()) ? tagd::TAGD_OK : tagd::TS_NOT_FOUND;
	}

	tagd::part_of_speech pos(tagd::id_view id, ::tagdb::session*, ::tagdb::flags_t = 0) override {
		auto found = _tags.find(id);
		return found == _tags.end() ? tagd::POS_UNKNOWN : found->second.pos();
	}

	bool exists(tagd::id_view id, ::tagdb::flags_t = 0) override {
		return _open && _tags.contains(id);
	}

	tagd::code query(tagd::tag_set&, const tagd::interrogator&, ::tagdb::session*,
		::tagdb::flags_t = 0) override {
		return tagd::TS_NOT_IMPLEMENTED;
	}

	tagd::code dump(std::ostream&) override {
		return tagd::TS_NOT_IMPLEMENTED;
	}
};

class AlternativeBackendTester : public CxxTest::TestSuite {
public:
	void test_unsupported_atomic_put_does_not_mutate() {
		MapBackend db;
		TS_ASSERT_EQUALS(db.open({tagdb::storage_kind::memory}), tagd::TAGD_OK);
		TS_ASSERT(!db.supports_atomic_put());
		tagd::abstract_tag tag("absent", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		const tagd::abstract_tag* tags[] = {&tag};
		TS_ASSERT_EQUALS(db.put_batch(tags, nullptr), tagd::TS_NOT_IMPLEMENTED);
		TS_ASSERT(!db.exists("absent"));
	}

	void test_owned_backend_cache_ordering_and_invalidation() {
		auto owner = std::make_unique<MapBackend>();
		auto* concrete = owner.get();
		std::unique_ptr<::tagdb::tagdb> backend = std::move(owner);
		auto& db = *backend;
		TS_ASSERT_EQUALS(db.open({::tagdb::storage_kind::memory}), tagd::TAGD_OK);
		for (const auto id : {"z", "a"}) {
			TS_ASSERT_EQUALS(db.put(tagd::abstract_tag(id, HARD_TAG_SUB, HARD_TAG_ENTITY,
				tagd::POS_TAG), nullptr), tagd::TAGD_OK);
		}

		tagd::abstract_tag subject("subject", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(subject.relation(HARD_TAG_HAS, "a"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(subject.relation(HARD_TAG_HAS, "z"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db.put(subject, nullptr), tagd::TAGD_OK);

		tagd::abstract_tag first, second;
		TS_ASSERT_EQUALS(db.get(first, "subject", nullptr), tagd::TAGD_OK);
		const auto loaded = concrete->cache_status();
		TS_ASSERT_EQUALS(db.get(second, "subject", nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(concrete->cache_status().identity_reads, loaded.identity_reads);
		TS_ASSERT_EQUALS(concrete->cache_status().predicate_reads, loaded.predicate_reads);
		TS_ASSERT_EQUALS(first.relations.begin()->object, "z");

		TS_ASSERT_EQUALS(db.del(subject, nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db.get(second, "subject", nullptr), tagd::TS_NOT_FOUND);
		db.close();
		TS_ASSERT_EQUALS(first.relations.begin()->object, "z");
		TS_ASSERT_EQUALS(concrete->cache_status().entries, 0);
	}
};
