#include <sstream> 
#include <exception> // std::terminate
#include <ctemplate/template.h>
#include "httagd.h"

namespace httagd {

void tagd_template::set_tag_link (
		const tpl_t& tpl,
		ctemplate::TemplateDictionary* d,
		const std::string& k,
		const std::string& v,
		const std::string *c) {

	if (v == "*") { // wildcard relator
		// empty link	
		d->SetValue(k, v);
		return;
	}

	std::string context_lnk;
	if (c!=nullptr && !c->empty()) {
		context_lnk.append("&c=").append(tagd::uri_encode(*c));
	} else if (!_context.empty())
		context_lnk.append("&c=").append(tagd::uri_encode(_context));

	if ( looks_like_hduri(v) ) {
		tagd::url u;
		u.init_hduri(v);
		d->SetValue( std::string(k).append("_lnk"),
			std::string("/").append(tagd::uri_encode(u.hduri())).append("?t=").append(tpl.val).append(context_lnk)
		);
		d->SetValue(k, u.id());
	} else if ( looks_like_url(v) ) {
		tagd::url u;
		u.init(v);
		d->SetValue( std::string(k).append("_lnk"),
			std::string("/").append(tagd::uri_encode(u.hduri())).append("?t=").append(tpl.val).append(context_lnk)
		);
		d->SetValue(k, u.id());
	} else {
		d->SetValue( std::string(k).append("_lnk"),
			std::string("/").append(tagd::util::esc_and_quote(v)).append("?t=").append(tpl.val).append(context_lnk) );
		d->SetValue(k, v);
	}
}

void tagd_template::set_relator_link (
		const tpl_t& tpl,
		ctemplate::TemplateDictionary* d,
		const std::string& k,
		const std::string& v,
		const std::string *c) {
	if (v.empty())  // wildcard relator
		set_tag_link(tpl, d, k, "*", c);
	else
		set_tag_link(tpl, d, k, v, c);
}

void tagd_template::set_query_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v) {
	d->SetValue( std::string(k).append("_lnk"),
		std::string("/").append(tagd::uri_encode(v)).append("/?t=query.html") );
}

void tagd_template::set_query_related_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v) {
	d->SetValue( std::string(k).append("_lnk"),
		std::string("/*/").append(tagd::uri_encode(v)).append("?t=query.html") );
}

void tagd_template::expand_template(const tpl_t& tpl, const ctemplate::TemplateDictionary& D) {

	if (tpl.type == TPL_UNKNOWN) {
		this->ferror( tagd::TAGD_ERR, "unknown template value: %s" , tpl.val.c_str() );
		return;
	}

	if (!ctemplate::LoadTemplate(fpath(tpl), ctemplate::DO_NOT_STRIP)) {
		this->ferror( tagd::TAGD_ERR, "load template failed: %s" , fpath(tpl).c_str() );
		return;
	}

	std::string output; 
	ctemplate::ExpandTemplate(fpath(tpl), ctemplate::DO_NOT_STRIP, &D, &output); 
	_res->add(output);
}

void tagd_template::expand_header(const std::string& title) {
	ctemplate::TemplateDictionary D("header");
	D.SetValue("title", title);
	if ( _req->query_map().size() > 0 ) {
		D.ShowSection("query_options");
		auto d = D.AddSectionDictionary("query_option");
		for ( auto &kv : _req->query_map() ) {
			d->SetValue("query_key", kv.first);
			d->SetValue("query_value", kv.second);	
		}
	}

	this->expand_template(this->get(HEADER_TPL_VAL), D);
}

void tagd_template::expand_footer() {
	ctemplate::TemplateDictionary D("footer");
	this->expand_template(this->get(FOOTER_TPL_VAL), D);
}

int tagd_template::expand_home(const tpl_t& tpl, ctemplate::TemplateDictionary& D) {
	// TODO messages should be internationalized
	// a _home_page tag might suffice for these type of definitions
	D.SetValue("msg", "Welcome to tagd!");

	return EVHTP_RES_OK;
}

int tagd_template::expand_tag(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D) {
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.SetValue("id", u.id());
		if (t.related("type", "image_jpeg"))
			D.SetValueAndShowSection("img_src", u.id(), "has_img");
	} else {
		D.SetValue("id", t.id());
	}

	set_tag_link(tpl, &D, "super_relator", t.super_relator());
	set_tag_link(tpl, &D, "super_object", t.super_object());

	tagd::tag_set img_urls;
	if (t.relations.size() > 0) {
		D.ShowSection("relations");
		tagd::id_type last_relator;
		ctemplate::TemplateDictionary* relator_dict = nullptr;
		for (auto p : t.relations) {
			if (p.relator == "img_src" && looks_like_hduri(p.object)) {
				tagd::url u;
				u.init_hduri(p.object);
				img_urls.insert(u);
				continue;
			}

			if (p.relator != last_relator) {
				last_relator = p.relator;
				relator_dict = D.AddSectionDictionary("relation_set");
				set_relator_link(tpl, relator_dict, "relator", p.relator);
			}

			if (!relator_dict) continue;

			ctemplate::TemplateDictionary* sub_dict = relator_dict->AddSectionDictionary("relation");
			set_tag_link(tpl, sub_dict, "object", p.object);
			if (!p.modifier.empty()) {
				sub_dict->ShowSection("has_modifier");
				sub_dict->SetValue("modifier", p.modifier);
			}
		}
	}

	if (img_urls.size() > 0) {
		D.ShowSection("gallery");
		for (auto u : img_urls) {
			D.SetValueAndShowSection("rel_img_src", u.id(), "gallery_item");
		}
	}

	tagd::tag_set S;
	tagd::interrogator q_refers_to(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
	q_refers_to.relation(HARD_TAG_REFERS_TO, t.id());
	tagd::code tc = _TS->query(S, q_refers_to);

	/*
	std::cerr << "q_refers_to: " << q_refers_to << std::endl;
	for (auto s : S) {
		std::cerr << "\t" << s << std::endl;
	}
	*/

	tpl_t query_tpl = this->get(QUERY_TPL_VAL);
	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_refers_to, S, D);
	}

	/*
	S.clear();
	tagd::interrogator q_refers(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
	q_refers.relation(HARD_TAG_REFERS, t.id());
	tc = _TS->query(S, q_refers);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_refers, S, D);
	}

	S.clear();
	tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
	q_related.relation("", t.id());
	tc = _TS->query(S, q_related);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0) {
			set_query_related_link(&D, "query_related", t.id());
			this->expand_query(query_tpl, q_related, S, D);
		}
	}

	S.clear();
	tagd::interrogator q_siblings(HARD_TAG_INTERROGATOR, t.super_object());
	tc = _TS->query(S, q_siblings);

	set_query_link(&D, "query_siblings", t.super_object());
	if (tc == tagd::TAGD_OK && S.size() > 1) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_siblings, S, D);
	}

	S.clear();
	tagd::interrogator q_children(HARD_TAG_INTERROGATOR, t.id());
	tc = _TS->query(S, q_children);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_children, S, D);
	}
*/
	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		D.SetValue("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int tagd_template::expand_browse(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D) {
	// ctemplate::TemplateDictionary D("browse");
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.SetValue("id", u.id());
	} else {
		D.SetValue("id", t.id());
	}

	tpl_t tag_tpl = this->get(TAG_TPL_VAL);
	tpl_t tree_tpl = this->get(TREE_TPL_VAL);
	tpl_t browse_tpl = this->get(BROWSE_TPL_VAL);
	tpl_t query_tpl = this->get(QUERY_TPL_VAL);

	ctemplate::TemplateDictionary* tree = D.AddIncludeDictionary("tree_html_tpl");
	tree->SetFilename(fpath(tree_tpl));
	size_t num_children = 0;
	this->expand_tree(tpl, t, *tree, &num_children);
	// std::cerr << "num_children: " << num_children << std::endl;

	ctemplate::TemplateDictionary* tag = D.AddIncludeDictionary("tag_html_tpl");
	tag->SetFilename(fpath(tag_tpl));
	this->expand_tag(browse_tpl, t, *tag);

	tagd::tag_set S;
	tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
	if (t.pos() == tagd::POS_RELATOR)	
		q_related.relation(t.id(), "");
	else
		q_related.relation("", t.id());
	tagd::code tc = _TS->query(S, q_related);

	// std::cerr << "q_related(" << S.size() << "): " << q_related << std::endl;

	ctemplate::TemplateDictionary* results = D.AddIncludeDictionary("results_html_tpl");
	results->SetFilename(fpath(query_tpl));
	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0) {
			this->expand_query(query_tpl, q_related, S, *results);
		}
	}

	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		D.SetValue("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int tagd_template::expand_relations(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D) {
/*
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.SetValue("id", u.id());
		if (t.related("type", "image_jpeg")) {
			std::cerr << "related(type, jpeg): " << u.id() << std::endl;
			D.SetValueAndShowSection("img_src", u.id(), "has_img");
		} else {
			std::cerr << "not related(type, jpeg): " << t << std::endl;
		}
	} else {
		D.SetValue("id", t.id());
	}
	set_tag_link(tpl, &D, "super_relator", t.super_relator());
	set_tag_link(tpl, &D, "super_object", t.super_object());
*/	
	if (t.relations.size() > 0) {
		D.ShowSection("relations");
		for (auto p : t.relations) {
			ctemplate::TemplateDictionary* sub_dict = D.AddSectionDictionary("relation");
			set_relator_link(tpl, sub_dict, "relator", p.relator);
			set_tag_link(tpl, sub_dict, "object", p.object);
			if (!p.modifier.empty()) {
				sub_dict->ShowSection("has_modifier");
				sub_dict->SetValue("modifier", p.modifier);
			}
		}
	}
/*
	tagd::interrogator q_refers_to(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
	q_refers_to.relation(HARD_TAG_REFERS_TO, t.id());
	tagd::code tc = _TS->query(S, q_refers_to);
	
	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_refers_to, S, D);
	}

	S.clear();
	tagd::interrogator q_refers(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
	q_refers.relation(HARD_TAG_REFERS, t.id());
	tc = _TS->query(S, q_refers);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_refers, S, D);
	}

	S.clear();
	tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
	q_related.relation("", t.id());
	tc = _TS->query(S, q_related);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0) {
			set_query_related_link(&D, "query_related", t.id());
			this->expand_query(query_tpl, q_related, S, D);
		}
	}

	S.clear();
	tagd::interrogator q_siblings(HARD_TAG_INTERROGATOR, t.super_object());
	tc = _TS->query(S, q_siblings);

	set_query_link(&D, "query_siblings", t.super_object());
	if (tc == tagd::TAGD_OK && S.size() > 1) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_siblings, S, D);
	}

	S.clear();
	tagd::interrogator q_children(HARD_TAG_INTERROGATOR, t.id());
	tc = _TS->query(S, q_children);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_tpl, q_children, S, D);
	}
*/

	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		D.SetValue("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int tagd_template::expand_tree(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D, size_t *num_children) {
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.SetValue("id", u.id());
	} else {
		D.SetValue("id", t.id());
	}
	set_tag_link(tpl, &D, "super_relator", t.super_relator());
	set_tag_link(tpl, &D, "super_object", t.super_object());

	tagd::tag_set S;
	tagd::code tc = _TS->query(S,
			tagd::interrogator(HARD_TAG_INTERROGATOR, t.super_object()));

	if (tc == tagd::TAGD_OK) {
		for (auto it=S.begin(); it != S.end(); ++it) {
			if (it->id() == t.id()) {
				if (it != S.begin()) {
					--it;
					ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("has_prev");
					set_tag_link(tpl, s1, "prev", it->id());
					++it;
				}
				++it;
				if (it != S.end()) {
					ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("has_next");
					set_tag_link(tpl, s1, "next", it->id());
				}
				break;
			}
		}
	}

	// query children
	S.clear();
	tc = _TS->query(S,
			tagd::interrogator(HARD_TAG_INTERROGATOR, t.id()));

	if (num_children != nullptr)
		*num_children = S.size();

	if (tc == tagd::TAGD_OK) {
		set_query_link(&D, "query_children", t.id());
		if (S.size() > 0) {
			ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("has_children");
			for (auto s : S) {
				ctemplate::TemplateDictionary* s2 = s1->AddSectionDictionary("children");
				if (s.id() != t.id()) {
					set_tag_link(tpl, s2, "child", s.id());
				}
			}
		}
	}

	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		D.SetValue("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int tagd_template::expand_query(const tpl_t& tpl, const tagd::interrogator& q, const tagd::tag_set& R, ctemplate::TemplateDictionary& D) {
	// ctemplate::TemplateDictionary D("query");

	D.SetValue("interrogator", q.id());
	tpl_t tag_tpl = this->get(TAG_TPL_VAL);
	tpl_t browse_tpl = this->get(BROWSE_TPL_VAL);
	if (!q.super_object().empty()) {
		D.ShowSection("super_relations");
		ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("super_relation");
		set_tag_link(tag_tpl, s1, "super_relator", q.super_relator());
		set_tag_link(tag_tpl, s1, "super_object", q.super_object());
	}

	if (q.relations.size() > 0) {
		D.ShowSection("relations");
		for (auto p : q.relations) {
			ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("relation");
			set_relator_link(tag_tpl, s1, "relator", p.relator);
			set_tag_link(tag_tpl, s1, "object", p.object);
			if (!p.modifier.empty()) {
				s1->ShowSection("has_modifier");
				s1->SetValue("modifier", p.modifier);
			}
		}
	}

	D.SetIntValue("num_results", R.size());

	if (R.size() > 0) {
		D.ShowSection("results");
		for (auto r : R) {
			ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("result");
			if (r.has_relator(HARD_TAG_CONTEXT)) {
				tagd::referent ref(r);
				std::string context{ref.context()};
				set_tag_link(browse_tpl, s1, "res_id", r.id(), &context);
			} else {
				set_tag_link(browse_tpl, s1, "res_id", r.id());
			}
			set_tag_link(tag_tpl, s1, "res_super_relator", r.super_relator());
			set_tag_link(tag_tpl, s1, "res_super_object", r.super_object());
			if (r.relations.size() > 0) {
				s1->ShowSection("res_relations");
				for (auto p : r.relations) {
					ctemplate::TemplateDictionary* s2 = s1->AddSectionDictionary("res_relation");
					set_relator_link(browse_tpl, s2, "res_relator", p.relator);
					set_tag_link(browse_tpl, s2, "res_object", p.object);
					if (!p.modifier.empty()) {
						s2->ShowSection("res_has_modifier");
						s2->SetValue("res_modifier", p.modifier);
					}
				}
			}
		}
	}

	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		D.SetValue("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

void tagd_template::expand_error(const tpl_t& tpl, const tagd::errorable& E) {
	ctemplate::TemplateDictionary D("error");
	D.SetValue("err_ids", tagd::tag_ids_str(E.errors()));
	const tagd::errors_t& R	= E.errors();
	tpl_t tag_tpl = this->get(TAG_TPL_VAL);

	if (R.size() > 0) {
		D.ShowSection("errors");
		for (auto r : R) {
			ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("error");
			/*
			if (r.has_relator(HARD_TAG_CONTEXT)) {
				tagd::referent ref(r);
				std::string context{ref.context()};
				set_tag_link(tpl, s1, "err_id", r.id(), &context);
			} else {
				set_tag_link(tpl, s1, "err_id", r.id());
			}
			*/
			s1->SetValue("err_id", r.id());
			set_tag_link(tag_tpl, s1, "err_super_relator", r.super_relator());
			set_tag_link(tag_tpl, s1, "err_super_object", r.super_object());
			if (r.relations.size() > 0) {
				s1->ShowSection("err_relations");
				for (auto p : r.relations) {
					ctemplate::TemplateDictionary* s2 = s1->AddSectionDictionary("err_relation");
					set_tag_link(tag_tpl, s2, "err_relator", p.relator);
					set_tag_link(tag_tpl, s2, "err_object", p.object);
					if (!p.modifier.empty()) {
						s2->ShowSection("err_has_modifier");
						s2->SetValue("err_modifier", p.modifier);
					}
				}
			}
		}
	}

	this->expand_template(tpl, D);
}


void template_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc;
	if (t.pos() == tagd::POS_URL) {
		ts_rc = _TS->get((tagd::url&)T, t.id());
	} else {
		ts_rc = _TS->get(T, t.id());
	}

	int res;
	_template.expand_header(t.id());

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	ctemplate::TemplateDictionary D("get");
	if (ts_rc == tagd::TAGD_OK) {
		tpl_t tpl = _template.get(_req->query_opt("t"));
		switch (tpl.type) {
			case TPL_BROWSE:
				res = _template.expand_browse(tpl, T, D);
				_template.expand_template(tpl, D);
				break;
			case TPL_TAG:
				res = _template.expand_tag(tpl, T, D);
				_template.expand_template(tpl, D);
				break;
			case TPL_TREE:
				res = _template.expand_tree(tpl, T, D);
				_template.expand_template(tpl, D);
				break;
			case TPL_RELATIONS:
				res = _template.expand_relations(tpl, T, D);
				_template.expand_template(tpl, D);
				break;
			default:
				this->ferror(tagd::TS_NOT_FOUND, "resource not found: %s", _req->query_opt("t").c_str());
				_template.expand_error(_template.get(ERROR_TPL_VAL), *this);
				res = EVHTP_RES_NOTFOUND;
				break;
		}
	} else {
		// TODO TS_NOT_FOUND does not set error
		_template.expand_error(_template.get(ERROR_TPL_VAL), *_TS);
		res = EVHTP_RES_SERVERR;
	}

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	_template.expand_footer();

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	_res->send_reply(res);

	if (_trace_on) std::cerr << "cmd_get(" << tagd_code_str(ts_rc) << ") : " <<  res << std::endl;
}

void template_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q);

	int res;
	_template.expand_header(q.id());

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	ctemplate::TemplateDictionary D("query");
	if (ts_rc == tagd::TAGD_OK || ts_rc == tagd::TS_NOT_FOUND) {
		tpl_t tpl = _template.get(_req->query_opt("t"));
		switch (tpl.type) {
			case TPL_BROWSE:
				tpl = _template.get(QUERY_TPL_VAL);
				res = _template.expand_query(tpl, q, T, D);
				_template.expand_template(tpl, D);
				break;
			case TPL_UNKNOWN:
				this->ferror(tagd::TS_NOT_FOUND, "resource not found: %s", _req->query_opt("t").c_str());
				_template.expand_error(_template.get(ERROR_TPL_VAL), *this);
				res = EVHTP_RES_NOTFOUND;
				break;
			default:
				res = _template.expand_query(tpl, q, T, D);
				_template.expand_template(tpl, D);
		}
	} else {
		_template.expand_error(_template.get(ERROR_TPL_VAL), *_TS);
		res = EVHTP_RES_SERVERR;
	}

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	_template.expand_footer();

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	_res->send_reply(res);

	if (_trace_on) std::cerr << "cmd_query(" << tagd_code_str(ts_rc) << ") : " <<  res << std::endl;
}

void template_callback::cmd_error() {
	_template.expand_header(_driver->last_error().id());
	_template.expand_error(_template.get(ERROR_TPL_VAL), *_driver);
	_template.expand_footer();

	if (_template.has_error()) {
		_res->send_error_str(*_driver);
		_res->send_error_str(_template);
		return;
	}

	std::stringstream ss;
	if (_trace_on)
		_driver->print_errors(ss);

	switch (_driver->code()) {
		case tagd::TS_NOT_FOUND:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(_driver->code()) << "): " << EVHTP_RES_NOTFOUND << " EVHTP_RES_NOTFOUND" << std::endl << ss.str() << std::endl;
			_res->send_reply(EVHTP_RES_NOTFOUND);
			break;
		default:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(_driver->code()) << "): " << EVHTP_RES_SERVERR << " EVHTP_RES_SERVERR" << std::endl << ss.str() << std::endl;
			_res->send_reply(EVHTP_RES_SERVERR);
	}
}

void template_callback::empty() {
	_template.expand_header("Welcome to tagd");

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	tpl_t home_tpl = _template.get(HOME_TPL_VAL);
	ctemplate::TemplateDictionary D("get");
	int res = _template.expand_home(home_tpl, D);

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	_template.expand_template(home_tpl, D);

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	_template.expand_footer();

	if (_template.has_error()) {
		_res->send_error_str(_template);
		return;
	}

	_res->send_reply(res);

	if (_trace_on) std::cerr << "empty: " <<  res << std::endl;
}

}
