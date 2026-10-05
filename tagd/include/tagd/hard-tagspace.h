#pragma once

#include "tagd/hard-tags.h"
#include "tagd/tagspace.h"

#include <functional>
#include <map>
#include <ostream>

namespace tagd {

/*\
|*| hard_tagspace reads the hard tags generated from hard-tags.h.
|*| These tags and their ranks are shared by every tagspace and cannot be changed
|*| at runtime. This class therefore implements only the read-only interface.
\*/
class hard_tagspace final : public const_tagspace {
public:
	hard_tagspace() = default;
	~hard_tagspace() override = default;

	tagd::code put(const abstract_tag&, tagspace_session* = nullptr, flags_t = 0) = delete;
	tagd::code put(const url&, tagspace_session* = nullptr, flags_t = 0) = delete;
	tagd::code put(const referent&, tagspace_session* = nullptr, flags_t = 0) = delete;

	tagd::code del(const abstract_tag&, tagspace_session* = nullptr, flags_t = 0) = delete;
	tagd::code del(const url&, tagspace_session* = nullptr, flags_t = 0) = delete;
	tagd::code del(const referent&, tagspace_session* = nullptr, flags_t = 0) = delete;

	size_t merge(tag_set&, const tag_set&) = delete;

	[[nodiscard]] rank lookup_rank(id_view id) const override;
	[[nodiscard]] part_of_speech lookup_pos(id_view id) const override;
	[[nodiscard]] bool contains(id_view ancestor, id_view descendant) const override;

	[[nodiscard]] tagd::code get(abstract_tag& t, id_view id, tagspace_session* ssn = nullptr, flags_t flags = 0) override;

	[[nodiscard]] tagd::code query(tag_set& result, const interrogator& q, tagspace_session* ssn = nullptr, flags_t flags = 0) override;

	[[nodiscard]] bool exists(id_view id, flags_t flags = 0) const override;
	[[nodiscard]] tagd::code dump(std::ostream& os = std::cout) const override;
};

} // namespace tagd
