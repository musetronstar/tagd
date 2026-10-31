#pragma once

#include "tagd/tagspace.h"
#include "tagd/logger.h"
#include <memory>

void TAGSPACE_SET_LOGGER(tagd::logger*);
bool TAGSPACE_LOG_ENABLED(tagd::log_level);
void TAGSPACE_LOG(tagd::log_level, const std::string&);

namespace tagdb { struct storage_location; class tagdb; enum class relation_semantics; }

namespace tagd {

void tagspace_install_logger_validator();
std::string default_tagd_home();
bool valid_tagspace_name(const std::string&);
inline constexpr char TAGSPACE_TAGL_SUFFIX[] = ".tagl";
// Creation, source validation, and discovery must agree on the home layout.
std::string tagspaces_directory(const std::string& home);
std::string tagspace_database_path(const std::string& home, const std::string& id);

/*\
|*| stored_tagspace owns the storage used by persistent and memory tagspaces.
|*| Its private implementation keeps backend headers out of application code.
|*| Applications use the tagspace interface; storage handles stay in this module.
\*/
class stored_tagspace : public tagspace {
	class impl;
	std::unique_ptr<impl> _impl;
	bool prepare() const;
	tagdb::tagdb& backend();
	bool _persistent = false;
	std::string _filepile_home;
	std::string _filepile_canonical_home, _filepile_configured_root, _filepile_home_error;
	std::string _home_reason = "no home directory is configured.";
	tagd::code finish(tagd::code) const;
	[[nodiscard]] tagd::code managed_path(bool&, const url&, bool resolve = false);
	[[nodiscard]] tagd::code guard_mutation(const abstract_tag&, bool deleting,
		tagspace_session*, flags_t);

protected:
	stored_tagspace();
	explicit stored_tagspace(tagdb::relation_semantics);
	[[nodiscard]] tagd::code initialize_location(const tagdb::storage_location&);
	[[nodiscard]] tagd::code initialize_memory();
	[[nodiscard]] tagd::code initialize_file(const std::string&, bool create = false);
	[[nodiscard]] tagd::code initialize(
		const std::string&,
		const std::string&,
		bool create
	);

public:
	~stored_tagspace() override;
	// Configuration supplies the selected home, independently of the database location.
	void filepile_home(std::string home, std::string reason = "no home directory is configured.");
	[[nodiscard]] tagd::code store(store_result&, const abstract_tag&,
		tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code delete_storage(filepile_delete_result&, const abstract_tag&,
		tagspace_session* = nullptr, flags_t = 0) override;
	using const_tagspace::lookup_rank;
	[[nodiscard]] tagd::code lookup_rank(rank&, id_view) const override;
	[[nodiscard]] tagd::code order_predicates(predicate_set&) const override;
	// Sessions borrow this object's address; open storage has a single owner.
	stored_tagspace(const stored_tagspace&) = delete;
	stored_tagspace& operator=(const stored_tagspace&) = delete;
	stored_tagspace(stored_tagspace&&) = delete;
	stored_tagspace& operator=(stored_tagspace&&) = delete;

	// Resolve an ordinary tag operation without writing; unknown IDs remain unchanged.
	// Referent declarations keep their own lexeme semantics in put(referent).
	[[nodiscard]] tagd::code resolve_referents(abstract_tag&, const abstract_tag&,
		tagspace_session*);

	[[nodiscard]] tagd::code get(abstract_tag&, id_view, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code get(url&, id_view, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code replace(replace_outcome&, const abstract_tag&,
		tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code put(const abstract_tag&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code put(const url&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code put(const referent&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code del(const abstract_tag&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code del(const url&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code del(const referent&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code query(tag_set&, const interrogator&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] part_of_speech pos(id_view, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] bool exists(id_view, flags_t = 0) const override;
	[[nodiscard]] tagd::code dump(std::ostream& = std::cout) const override;
	[[nodiscard]] tagd::code dump_grid(std::ostream& = std::cout) const override;
	[[nodiscard]] tagd::code dump_terms(std::ostream& = std::cout) const override;
	[[nodiscard]] tagd::code dump_search(std::ostream& = std::cout) const override;
};

/*\
|*| persistent opens or creates a named tagspace under the tagd home directory.
|*| create() rejects an existing database; init() requires an existing database.
|*| Both open the tagspace for use. Neither requires a second initialization call.
\*/
class tagspace::persistent final : public stored_tagspace {
public:
	// Location interpretation stays with the selected backend, including non-file resources.
	[[nodiscard]] tagd::code open_location(const tagdb::storage_location& location) {
		return initialize_location(location);
	}
	// Direct-file opening never creates storage.
	[[nodiscard]] tagd::code open_file(const std::string& path) {
		return initialize_file(path);
	}
	[[nodiscard]] tagd::code create_file(const std::string& path) {
		return initialize_file(path, true);
	}
	// Check the return code before use; errors are recorded on this object.
	[[nodiscard]] tagd::code init(const std::string& name, const std::string& home);
	[[nodiscard]] tagd::code init(const std::string& name);
	[[nodiscard]] tagd::code create(const std::string& name, const std::string& home);
	[[nodiscard]] tagd::code create(const std::string& name);
};

/*\
|*| memory provides an independent tagspace that lasts as long as this object.
|*| The selected backend supplies volatile storage with the same query contract.
|*| Its contents do not survive closing the tagspace.
\*/
class tagspace::memory final : public stored_tagspace {
public:
	[[nodiscard]] tagd::code init() {
		return initialize_memory();
	}
};

} // namespace tagd
