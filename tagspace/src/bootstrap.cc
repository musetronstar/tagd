#include <cassert>

#include "tagspace/bootstrap.h"

namespace tagspace {

tagd::code bootstrap::put_hard_tag(
		tagspace& TS, const tagd::id_type& id,
		const tagd::id_type& super, const tagd::part_of_speech& pos) {
	tagd::abstract_tag t(id, super, pos);
	tagd::code rs = TS.put(t, NO_POS_CAST);
	assert((rs == tagd::TAGD_OK)||(rs == tagd::TS_DUPLICATE));
	return rs;
}

#define PUT_OR_DIE(id, super, pos)  if(put_hard_tag(TS, id, super, pos) != tagd::TAGD_OK) \
										return TS.error(tagd::TS_INTERNAL_ERR, \
												"put_hard_tag failed (id, super, pos): %s, %s, %s", \
												id, super, pos_str(pos).c_str());

tagd::code bootstrap::init_hard_tags(tagspace& TS) {
	// _entity should be inserted at database creation time,
	// so that it can enforce its circular referential constraints

	// IMPORTANT: updates to tagd/hard-tags.h must be reflected here
	PUT_OR_DIE(HARD_TAG_SUPER, "_entity", tagd::POS_SUPER)
	PUT_OR_DIE(HARD_TAG_IS_A, HARD_TAG_SUPER, tagd::POS_SUPER)
	PUT_OR_DIE(HARD_TAG_TYPE_OF, HARD_TAG_SUPER, tagd::POS_SUPER)
	PUT_OR_DIE(HARD_TAG_RELATOR, "_entity", tagd::POS_RELATOR)
	PUT_OR_DIE(HARD_TAG_HAS, HARD_TAG_RELATOR, tagd::POS_RELATOR)
	PUT_OR_DIE(HARD_TAG_CAN, HARD_TAG_RELATOR, tagd::POS_RELATOR)
	// actual URLs will use POS_URL, using POS_URL for _url will create problems (it won't parse as a url)
	PUT_OR_DIE(HARD_TAG_URL, "_entity", tagd::POS_TAG)
	PUT_OR_DIE(HARD_TAG_INTERROGATOR, "_entity", tagd::POS_INTERROGATOR) 

	PUT_OR_DIE(HARD_TAG_REFERENT,HARD_TAG_SUPER, tagd::POS_REFERENT)	  // super relation for token that refer to a tag in a context
	PUT_OR_DIE(HARD_TAG_REFERS, HARD_TAG_RELATOR, tagd::POS_REFERS)       // relation that refers a _referent
	PUT_OR_DIE(HARD_TAG_REFERS_TO, HARD_TAG_RELATOR, tagd::POS_REFERS_TO) // relation that refers the _referent to the tag
	PUT_OR_DIE(HARD_TAG_CONTEXT, HARD_TAG_RELATOR, tagd::POS_CONTEXT)	  // relation that defines the context of a _referent

	PUT_OR_DIE(HARD_TAG_URL_PART, "_entity", tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_HOST, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_PRIV_LABEL, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_PUB, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_SUB, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_PATH, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_QUERY, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_FRAGMENT, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_PORT, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_USER, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_PASS, HARD_TAG_URL_PART, tagd::POS_TAG) 
	PUT_OR_DIE(HARD_TAG_SCHEME, HARD_TAG_URL_PART, tagd::POS_TAG) 

	return TS.code();
}

} // namespace tagspace
