#pragma once

#include "tagd/tagspace.h"
#include "tagd/logger.h"
#include <memory>

void TAGSPACE_SET_LOGGER(tagd::logger*);

namespace tagd {

void tagspace_install_logger_validator();

/*\
|*| stored_tagspace owns the storage used by persistent and memory tagspaces.
|*| Its private implementation keeps backend headers out of application code.
|*| Applications use the tagspace interface; storage handles stay in this module.
\*/
class stored_tagspace : public tagspace {
	class impl;
	std::unique_ptr<impl> _impl;
	bool prepare() const;
	tagd::code finish(tagd::code) const;

protected:
	stored_tagspace();
	[[nodiscard]] tagd::code initialize_memory();
	[[nodiscard]] tagd::code initialize(
		const std::string&,
		const std::string&,
		bool create
	);

public:
	~stored_tagspace() override;
	// Sessions borrow this object's address; open storage has a single owner.
	stored_tagspace(const stored_tagspace&) = delete;
	stored_tagspace& operator=(const stored_tagspace&) = delete;
	stored_tagspace(stored_tagspace&&) = delete;
	stored_tagspace& operator=(stored_tagspace&&) = delete;

	[[nodiscard]] tagd::code get(abstract_tag&, id_view, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code get(url&, id_view, tagspace_session* = nullptr, flags_t = 0) override;
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
	// Check the return code before use; errors are recorded on this object.
	[[nodiscard]] tagd::code init(const std::string& name, const std::string& home);
	[[nodiscard]] tagd::code init(const std::string& name);
	[[nodiscard]] tagd::code create(const std::string& name, const std::string& home);
	[[nodiscard]] tagd::code create(const std::string& name);
};

/*\
|*| memory provides an independent tagspace that lasts as long as this object.
|*| It uses SQLite :memory: so queries, including full-text search, behave like
|*| queries on a persistent tagspace. No database file is created.
\*/
class tagspace::memory final : public stored_tagspace {
public:
	[[nodiscard]] tagd::code init() {
		return initialize_memory();
	}
};

} // namespace tagd
