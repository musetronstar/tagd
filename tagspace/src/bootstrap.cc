#include <cassert>

#include "tagspace/bootstrap.h"

namespace tagspace {

ts_res_code bootstrap::put_hard_tag(
		tagspace& TS, const tagd::id_type& id,
		const tagd::id_type& super, const tagd::part_of_speech& pos) {
	tagd::abstract_tag t(id, super, pos);
	ts_res_code rs = TS.put(t);
	assert((rs == TS_OK)||(rs == TS_DUPLICATE));
	return rs;
}

ts_res_code bootstrap::init_hard_tags(tagspace& TS) {
	// _entity should be inserted at database creation time,
	// so that it can enforce its circular referential constraints

	// IMPORTANT: updates to tagd/hard-tags.h must be reflected here
	ts_res_code rs;

	rs = put_hard_tag(TS, HARD_TAG_SUPER, "_entity", tagd::POS_TAG);
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_IS_A, HARD_TAG_SUPER, tagd::POS_RELATOR);
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_TYPE_OF, HARD_TAG_SUPER, tagd::POS_RELATOR);
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_RELATOR, HARD_TAG_SUPER, tagd::POS_RELATOR);
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_HAS, HARD_TAG_RELATOR, tagd::POS_RELATOR);
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_URL, "_entity", tagd::POS_URL);
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_INTERROGATOR, "_entity", tagd::POS_INTERROGATOR); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_URL_PART, "_entity", tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_HOST, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_PRIV_LABEL, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_PUB, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_SUB, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_PATH, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_QUERY, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_FRAGMENT, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_PORT, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_USER, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_PASS, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	rs = put_hard_tag(TS, HARD_TAG_SCHEME, HARD_TAG_URL_PART, tagd::POS_TAG); 
	if (rs != TS_OK) return TS_INTERNAL_ERR;

	return rs;
}

} // namespace tagspace
