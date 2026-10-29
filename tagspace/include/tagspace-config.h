#pragma once

#include "tagspace.h"
#include "tagd/hard-tags.h"
#include <map>
#include <functional>
#include <optional>

namespace tagd {

// Find or register the tagspace tag in _config without opening its
// database or executing its TAGL source.
enum class source_action { open, create, register_only };

// Hosts choose source authority; denial is distinct from malformed input or a name conflict.
enum class tagspace_access { local, registered_only, registered_with_config,
	named_only, named_with_config };

/*
 * One application owns this object; drivers and sessions borrow its stable
 * address. Private storage prevents TAGL callers from bypassing validation.
 */
class config final : public tagspace {
	/*
	 * TODO(persistent-config): Add validated, permission-controlled .db storage
	 * before enabling filepile for _config; volatile associations would be lost.
	 * Filepile requires persistent semantic storage for every target; reject this
	 * volatile _config with TS_MISUSE before mutation.
	 * See workspace TASKS.d/2026-10-23-persistent-config-tagspace.md.
	 */
	class profile_store final : public stored_tagspace {
	public:
		profile_store();
		[[nodiscard]] tagd::code init() {
			return initialize_memory();
		}
	};
	mutable profile_store _configspace;
	id_string _subject_id{CONFIG_SUBJECT};
	bool _runtime = false;
	bool _home_disabled = false;
	// File loading defers defaults and selection until all includes have executed.
	bool _file_loading = false;
	bool _file_default_touched = false;
	std::string _bootstrap_home;
	id_string _file_profile;
	id_vec _file_context;

	logger _logger;

	// Descriptions own names; these derived entries own only resources and resolved identity.
	std::map<id_string, std::shared_ptr<tagspace>> _opened;
	std::map<id_string, std::string> _sources;
	// Declared lexical locations key invalidation; resolved sources and observed spellings
	// retain identity after disappearance without changing the authoritative description.
	std::map<id_string, std::string> _locations;
	std::map<std::string, id_string> _spellings;
	// Canonical paths currently being parsed guard recursive TAGL loading.
	std::set<std::string> _loading;
	// The selected profile target is immutable once the server default is established.
	std::optional<id_string> _frozen_target;

	tagd::code validate_collection(const abstract_tag*, bool deleting = false, bool opening = false);
	tagd::code validate_alias(const referent&, bool planned = false);
	tagd::code descriptions(tag_set&);
	tagd::code source_path(const abstract_tag&, std::string&,
		const std::string* home = nullptr, bool canonical = true);
	void refresh_sources();
	tagd::code normalize_source(const std::string&, const std::string&, std::string&, bool canonical = true);

	std::string value(id_view) const;
	tagd::code validate(const abstract_tag&, bool assertions = false);
	// Ancestor edits can change inherited settings; check the selected profile's
	// resulting values before storing the change.
	tagd::code validate_effective(const abstract_tag&);
	void update_logger();
	void prepare();
	tagd::code finish(tagd::code);

public:
	/*
	 * Tagspace collection: config owns opened instances and callers borrow them.
	 * Descriptions and aliases in _config stay authoritative; *denied reports a
	 * policy refusal separately from malformed input or a name conflict.
	 */
	[[nodiscard]] tagd::code startup_target(id_string&, bool aliases = true);
	[[nodiscard]] tagd::code freeze_target();
	[[nodiscard]] tagd::code create_tagspace(const std::string&, std::shared_ptr<tagspace>&);
	using source_loader = std::function<tagd::code(const std::string&, std::shared_ptr<tagspace>)>;
	[[nodiscard]] tagd::code resolve_tagspace(const std::string&, id_string&,
		tagspace_session* = nullptr);
	[[nodiscard]] tagd::code open_tagspace(const std::string&, const std::string&,
		std::shared_ptr<tagspace>&, tagspace_session* = nullptr,
		const std::string& base = {}, tagspace_access = tagspace_access::local,
		source_loader = {}, bool* denied = nullptr,
		source_action = source_action::open, const std::string& expected_id = {});
	[[nodiscard]] tagd::code close_tagspace(const std::string&, const std::string& alias = {},
		tagspace_session* = nullptr, const std::string& base = {});
	[[nodiscard]] tagd::code bind_alias(id_view, const std::string&);
	std::shared_ptr<tagspace> borrow(tagspace*);
	id_string tagspace_id(const tagspace*) const;
	[[nodiscard]] tagd::code list_tagspaces(std::ostream&, const tagspace*);

	// Typed accessors require successful init(); callers receive owned string values.
	// Violating this internal invariant is fatal; input validation returns error codes.
	// Default configuration subject; --config-id selects another tag.
	static constexpr id_view CONFIG_SUBJECT = "default_config";
	[[nodiscard]] tagd::code init(const std::string& home);
	[[nodiscard]] tagd::code begin_config_load();
	void file_profile(const id_string& id, const id_vec& context) {
		_file_profile = id;
		_file_context = context;
	}
	[[nodiscard]] tagd::code finish_config_load(const std::string& cli_id, const std::string& home);
	[[nodiscard]] tagd::code select(id_view subject, const std::string& default_home);
	const id_string& subject_id() const {
		return _subject_id;
	}
	void start_runtime() {
		_runtime = true;
	}
	logger& log() {
		return _logger;
	}
	[[nodiscard]] tagd::code set(id_view setting, const std::string& value);
	void disable_home() { _home_disabled = true; }
	std::string configured_home() const { return value(HARD_TAG_CONFIG_HOME); }
	[[nodiscard]] tagd::code open_database(const std::string&, bool create,
		const std::string& expected_id, std::string& resolved);
	std::string home() const {
		return _home_disabled ? std::string{} : value(HARD_TAG_CONFIG_HOME);
	}
	std::string log_level() const {
		return value(HARD_TAG_CONFIG_LOG_LEVEL);
	}
	std::string bind_addr() const {
		return value(HARD_TAG_CONFIG_BIND_ADDR);
	}
	uint16_t bind_port() const;
	std::string tpl_dir() const {
		return value(HARD_TAG_CONFIG_TPL_DIR);
	}
	std::string www_dir() const {
		return value(HARD_TAG_CONFIG_WWW_DIR);
	}
	std::string favicon() const {
		auto path = value(HARD_TAG_CONFIG_FAVICON);
		return path.empty() ? tagd::io::concat_dir(www_dir(), "favicon.ico") : path;
	}
	std::string default_view() const {
		return value(HARD_TAG_CONFIG_DEFAULT_VIEW);
	}
	std::string fallback_error_view() const {
		return value(HARD_TAG_CONFIG_FALLBACK_ERROR_VIEW);
	}
	std::string prompt() const {
		return value(HARD_TAG_CONFIG_PROMPT);
	}
	bool echo_result_code() const;
	bool tagdurl_autoload() const {
		return value(HARD_TAG_CONFIG_TAGDURL_AUTOLOAD) == HARD_TAG_TRUE;
	}

	using tagspace::get;
	using tagspace::put;
	using tagspace::del;
	[[nodiscard]] tagd::code get(abstract_tag&, id_view, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code query(tag_set&, const interrogator&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code put(const abstract_tag&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] tagd::code del(const abstract_tag&, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] part_of_speech pos(id_view, tagspace_session* = nullptr, flags_t = 0) override;
	[[nodiscard]] bool exists(id_view, flags_t = 0) const override;
	[[nodiscard]] tagd::code dump(std::ostream& = std::cout) const override;
	[[nodiscard]] tagd::code dump_grid(std::ostream& = std::cout) const override;
	[[nodiscard]] tagd::code dump_terms(std::ostream& = std::cout) const override;
	[[nodiscard]] tagd::code dump_search(std::ostream& = std::cout) const override;
};

} // namespace tagd
