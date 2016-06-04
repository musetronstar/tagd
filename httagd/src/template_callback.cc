#include <sstream> 
#include <exception> // std::terminate
#include <ctemplate/template.h>
#include "httagd.h"

namespace httagd {

void expand_header(transaction& tx, const view& vw, tagd_template& tpl, const std::string& title) {
	tpl.set_value("title", title);
	if ( tx.req->query_map().size() > 0 ) {
		tpl.show_section("query_options");
		auto t = tpl.add_section("query_option");
		for ( auto &kv : tx.req->query_map() ) {
			t->set_value("query_key", kv.first);
			t->set_value("query_value", kv.second);
		}
	}

	tpl.expand(tx.router->fpath(vw));
}

home_handler_t f_home_handler =
[](transaction& T, const view& V, tagd_template& L) -> int {

	// TODO messages should be internationalized
	// a _home_page tag might suffice for these type of definitions
	L.set_value("msg", "Welcome to tagd!");

	return EVHTP_RES_OK;
};

int router::expand_tag(const view& tpl, const tagd::abstract_tag& t, tagd_template& D) {
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.set_value("id", u.id());
		if (t.related("type", "image_jpeg"))
			D.set_value_show_section("img_src", u.id(), "has_img");
	} else {
		D.set_value("id", t.id());
	}

	D.set_tag_link("super_relator", t.super_relator(), tpl, &_context);
	D.set_tag_link("super_object", t.super_object(), tpl, &_context);

	tagd::tag_set img_urls;
	if (t.relations.size() > 0) {
		D.show_section("relations");
		tagd::id_type last_relator;
		tagd_template *relator_dict = nullptr;
		for (auto p : t.relations) {
			if (p.relator == "img_src" && tagd::url::looks_like_hduri(p.object)) {
				tagd::url u;
				u.init_hduri(p.object);
				img_urls.insert(u);
				continue;
			}

			if (p.relator != last_relator) {
				last_relator = p.relator;
				relator_dict = D.add_section("relation_set");
				relator_dict->set_relator_link("relator", p.relator, tpl, &_context);
			}

			if (!relator_dict) continue;

			auto sub_dict = relator_dict->add_section("relation");
			sub_dict->set_tag_link("object", p.object, tpl, &_context);
			if (!p.modifier.empty()) {
				sub_dict->show_section("has_modifier");
				sub_dict->set_value("modifier", p.modifier);
			}
		}
	}

	if (img_urls.size() > 0) {
		D.show_section("gallery");
		for (auto u : img_urls) {
			D.set_value_show_section("rel_img_src", u.id(), "gallery_item");
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

	view query_tpl = this->get(QUERY_VIEW);
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
		D.set_value("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int router::expand_browse(const view& tpl, const tagd::abstract_tag& t, tagd_template& D) {
	// tagd_template D("browse");
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.set_value("id", u.id());
	} else {
		D.set_value("id", t.id());
	}

	view tag_tpl = this->get(TAG_VIEW);
	view tree_tpl = this->get(TREE_VIEW);
	view browse_tpl = this->get(BROWSE_VIEW);
	view query_tpl = this->get(QUERY_VIEW);

	auto tree = D.include("tree_html_tpl", fpath(tree_tpl));
	size_t num_children = 0;
	this->expand_tree(tpl, t, *tree, &num_children);
	//std::cerr << "num_children: " << num_children << std::endl;

	auto tag = D.include("tag_html_tpl", fpath(tag_tpl));
	this->expand_tag(browse_tpl, t, *tag);

	tagd::tag_set S;
	tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
	if (t.pos() == tagd::POS_RELATOR)	
		q_related.relation(t.id(), "");
	else
		q_related.relation("", t.id());
	tagd::code tc = _TS->query(S, q_related);

	// std::cerr << "q_related(" << S.size() << "): " << q_related << std::endl;

	auto results = D.include("results_html_tpl", fpath(query_tpl));
	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0) {
			this->expand_query(query_tpl, q_related, S, *results);
		}
	}

	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		D.set_value("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int router::expand_relations(const view& tpl, const tagd::abstract_tag& t, tagd_template& D) {
/*
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.set_value("id", u.id());
		if (t.related("type", "image_jpeg")) {
			std::cerr << "related(type, jpeg): " << u.id() << std::endl;
			D.set_valueAndShowSection("img_src", u.id(), "has_img");
		} else {
			std::cerr << "not related(type, jpeg): " << t << std::endl;
		}
	} else {
		D.set_value("id", t.id());
	}
	set_tag_link(&D, "super_relator", tpl, t.super_relator());
	set_tag_link(&D, "super_object", tpl, t.super_object());
*/	
	if (t.relations.size() > 0) {
		D.show_section("relations");
		for (auto p : t.relations) {
			auto sub_dict = D.add_section("relation");
			sub_dict->set_relator_link("relator", p.relator, tpl, &_context);
			sub_dict->set_tag_link("object", p.object, tpl, &_context);
			if (!p.modifier.empty()) {
				sub_dict->show_section("has_modifier");
				sub_dict->set_value("modifier", p.modifier);
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
		D.set_value("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int router::expand_tree(const view& tpl, const tagd::abstract_tag& t, tagd_template& D, size_t *num_children) {
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		D.set_value("id", u.id());
	} else {
		D.set_value("id", t.id());
	}
	D.set_tag_link("super_relator", t.super_relator(), tpl, &_context);
	D.set_tag_link("super_object", t.super_object(), tpl, &_context);

	tagd::tag_set S;
	tagd::code tc = _TS->query(S,
			tagd::interrogator(HARD_TAG_INTERROGATOR, t.super_object()));

	if (tc == tagd::TAGD_OK) {
		for (auto it=S.begin(); it != S.end(); ++it) {
			if (it->id() == t.id()) {
				if (it != S.begin()) {
					--it;
					auto s1 = D.add_section("has_prev");
					s1->set_tag_link("prev", it->id(), tpl, &_context);
					++it;
				}
				++it;
				if (it != S.end()) {
					auto s1 = D.add_section("has_next");
					s1->set_tag_link("next", it->id(), tpl, &_context);
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
		D.set_query_link("query_children", t.id());
		if (S.size() > 0) {
			auto s1 = D.add_section("has_children");
			for (auto s : S) {
				auto s2 = s1->add_section("children");
				if (s.id() != t.id()) {
					s2->set_tag_link("child", s.id(), tpl, &_context);
				}
			}
		}
	}

	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		D.set_value("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

int router::expand_query(const view& vw, const tagd::interrogator& q, const tagd::tag_set& R, tagd_template& tpl) {
	// tagd_template tpl("query");

	tpl.set_value("interrogator", q.id());
	view tag_tpl = this->get(TAG_VIEW);
	view browse_tpl = this->get(BROWSE_VIEW);
	if (!q.super_object().empty()) {
		tpl.show_section("super_relations");
		auto s1 = tpl.add_section("super_relation");
		s1->set_tag_link("super_relator", q.super_relator(), tag_tpl, &_context);
		s1->set_tag_link("super_object", q.super_object(), tag_tpl, &_context);
	}

	if (q.relations.size() > 0) {
		tpl.show_section("relations");
		for (auto p : q.relations) {
			auto s1 = tpl.add_section("relation");
			s1->set_relator_link("relator", p.relator, tag_tpl, &_context);
			s1->set_tag_link("object", p.object, tag_tpl, &_context);
			if (!p.modifier.empty()) {
				s1->show_section("has_modifier");
				s1->set_value("modifier", p.modifier);
			}
		}
	}

	tpl.set_int_value("num_results", R.size());

	if (R.size() > 0) {
		tpl.show_section("results");
		for (auto r : R) {
			auto s1 = tpl.add_section("result");
			if (r.has_relator(HARD_TAG_CONTEXT)) {
				tagd::referent ref(r);
				std::string context{ref.context()};
				s1->set_tag_link("res_id", r.id(), browse_tpl, &context);
			} else {
				s1->set_tag_link("res_id", r.id(), browse_tpl, &_context);
			}
			s1->set_tag_link("res_super_relator", r.super_relator(), tag_tpl, &_context);
			s1->set_tag_link("res_super_object", r.super_object(), tag_tpl, &_context);
			if (r.relations.size() > 0) {
				s1->show_section("res_relations");
				for (auto p : r.relations) {
					auto s2 = s1->add_section("res_relation");
					s2->set_relator_link("res_relator", p.relator, browse_tpl, &_context);
					s2->set_tag_link("res_object", p.object, browse_tpl, &_context);
					if (!p.modifier.empty()) {
						s2->show_section("res_has_modifier");
						s2->set_value("res_modifier", p.modifier);
					}
				}
			}
		}
	}

	if (_TS->has_error()) {
		std::stringstream ss;
		_TS->print_errors(ss);
		tpl.set_value("errors", ss.str());
	}

	return EVHTP_RES_OK;
}

void router::expand_error(const view& vw, const tagd::errorable& E) {
	tagd_template D("error", _res->output_buffer());
	D.set_value("err_ids", tagd::tag_ids_str(E.errors()));
	const tagd::errors_t& R	= E.errors();
	view tag_vw = this->get(TAG_VIEW);

	if (R.size() > 0) {
		D.show_section("errors");
		for (auto r : R) {
			auto s1 = D.add_section("error");
			/*
			if (r.has_relator(HARD_TAG_CONTEXT)) {
				tagd::referent ref(r);
				std::string context{ref.context()};
				set_tag_link(s1, "err_id", vw, r.id(), &context);
			} else {
				set_tag_link(s1, "err_id", vw, r.id());
			}
			*/
			s1->set_value("err_id", r.id());
			s1->set_tag_link("err_super_relator", r.super_relator(), tag_vw, &_context);
			s1->set_tag_link("err_super_object", r.super_object(), tag_vw, &_context);
			if (r.relations.size() > 0) {
				s1->show_section("err_relations");
				for (auto p : r.relations) {
					auto s2 = s1->add_section("err_relation");
					s2->set_tag_link("err_relator", p.relator, tag_vw, &_context);
					s2->set_tag_link("err_object", p.object, tag_vw, &_context);
					if (!p.modifier.empty()) {
						s2->show_section("err_has_modifier");
						s2->set_value("err_modifier", p.modifier);
					}
				}
			}
		}
	}

	D.expand(this->fpath(vw));
}


void html_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc;
	if (t.pos() == tagd::POS_URL) {
		ts_rc = _TS->get((tagd::url&)T, t.id());
	} else {
		ts_rc = _TS->get(T, t.id());
	}

	int res;
	auto tx = this->tx();
	tagd_template hdr_tpl("header", _res->output_buffer());
	expand_header(tx, tx.router->get(HEADER_VIEW), hdr_tpl, t.id());

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	tagd_template get_tpl("get", _res->output_buffer());
	if (ts_rc == tagd::TAGD_OK) {
		auto opt_view = _req->query_opt(QUERY_OPT_VIEW);
		if (opt_view.empty())
			opt_view = _req->svr()->args()->default_view;

		view tpl = _router.get(opt_view);
		switch (tpl.type) {
			case view::BROWSE:
				res = _router.expand_browse(tpl, T, get_tpl);
				get_tpl.expand(_router.fpath(tpl));
				break;
			case view::TAG:
				res = _router.expand_tag(tpl, T, get_tpl);
				get_tpl.expand(_router.fpath(tpl));
				break;
			case view::TREE:
				res = _router.expand_tree(tpl, T, get_tpl);
				get_tpl.expand(_router.fpath(tpl));
				break;
			case view::RELATIONS:
				res = _router.expand_relations(tpl, T, get_tpl);
				get_tpl.expand(_router.fpath(tpl));
				break;
			default:
				this->ferror(tagd::TS_NOT_FOUND, "resource not found: %s", opt_view.c_str());
				_router.expand_error(_router.get(ERROR_VIEW), *this);
				res = EVHTP_RES_NOTFOUND;
				break;
		}
	} else {
		// TODO TS_NOT_FOUND does not set error
		_router.expand_error(_router.get(ERROR_VIEW), *_TS);
		res = EVHTP_RES_SERVERR;
	}

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	_router.expand_footer();

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	_res->send_reply(res);

	if (_trace_on) std::cerr << "cmd_get(" << tagd_code_str(ts_rc) << ") : " <<  res << std::endl;
}

void html_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q);

	int res;
	auto tx = this->tx();
	tagd_template hdr_tpl("header", _res->output_buffer());
	expand_header(tx, tx.router->get(HEADER_VIEW), hdr_tpl, q.id());

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	tagd_template D("query", _res->output_buffer());
	if (ts_rc == tagd::TAGD_OK || ts_rc == tagd::TS_NOT_FOUND) {
		auto opt_view = _req->query_opt(QUERY_OPT_VIEW);
		if (opt_view.empty())
			opt_view = _req->svr()->args()->default_view;

		view tpl = _router.get(opt_view);
		switch (tpl.type) {
			case view::BROWSE:
				tpl = _router.get(QUERY_VIEW);
				res = _router.expand_query(tpl, q, T, D);
				D.expand(_router.fpath(tpl));
				break;
			case view::UNKNOWN:
				this->ferror(tagd::TS_NOT_FOUND, "resource not found: %s", opt_view.c_str());
				_router.expand_error(_router.get(ERROR_VIEW), *this);
				res = EVHTP_RES_NOTFOUND;
				break;
			default:
				res = _router.expand_query(tpl, q, T, D);
				D.expand(_router.fpath(tpl));
		}
	} else {
		_router.expand_error(_router.get(ERROR_VIEW), *_TS);
		res = EVHTP_RES_SERVERR;
	}

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	_router.expand_footer();

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	_res->send_reply(res);

	if (_trace_on) std::cerr << "cmd_query(" << tagd_code_str(ts_rc) << ") : " <<  res << std::endl;
}

void html_callback::cmd_error() {
	auto tx = this->tx();
	tagd_template hdr_tpl("header", _res->output_buffer());
	expand_header(tx, tx.router->get(HEADER_VIEW), hdr_tpl, _driver->last_error().id());
	_router.expand_error(_router.get(ERROR_VIEW), *_driver);
	_router.expand_footer();

	if (_router.has_error()) {
		_res->send_error_str(*_driver);
		_res->send_error_str(_router);
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

void html_callback::empty() {
	auto tx = this->tx();
	tagd_template tpl("header", tx.res->output_buffer());
	expand_header(tx, tx.router->get(HEADER_VIEW), tpl, "Welcome to tagd");

	if (tx.has_error()) {
		tx.send_errors_res();
		return;
	}

	view home_view = _router.get(HOME_VIEW);
	tagd_template D("get", _res->output_buffer());
	int res = f_home_handler(tx, home_view, D);

	if (tx.has_error()) {
		tx.send_errors_res();
		return;
	}

	D.expand(tx.router->fpath(home_view));

	if (tx.has_error()) {
		tx.send_errors_res();
		return;
	}

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	_router.expand_footer();

	if (_router.has_error()) {
		_res->send_error_str(_router);
		return;
	}

	_res->send_reply(res);

	if (_trace_on) std::cerr << "empty: " <<  res << std::endl;
}

void router::expand_footer() {
	tagd_template D("footer", _res->output_buffer());
	D.expand(this->fpath(this->get(FOOTER_VIEW)));
}

view router::get (const std::string& key) {
	auto it = _views.find(key);
	if (it == _views.end()) {
		view t = UNKNOWN_VIEW;
		t.name = key;
		return t;
	}

	return it->second;
}


tagd_code tagd_template::expand(const std::string& fname) {
	if (fname.empty()) {
		this->ferror( tagd::TAGD_ERR, "template file required" );
		return tagd::TAGD_ERR;
	}

	if (!ctemplate::LoadTemplate(fname, ctemplate::DO_NOT_STRIP)) {
		this->ferror( tagd::TAGD_ERR, "load template failed: %s" , fname.c_str() );
		return tagd::TAGD_ERR;
	}

	// this also would load (if not already loaded), but lets load and expand in seperate
	// steps so we can better trace the source of errors
	if (!ctemplate::ExpandTemplate(fname, ctemplate::DO_NOT_STRIP, _dict, _output)) {
		this->ferror( tagd::TAGD_ERR, "load template failed: %s" , fname.c_str() );
		return tagd::TAGD_ERR;
	}

	return tagd::TAGD_OK;
}

inline tagd_template* tagd_template::add_section(const std::string &id) {
	return this->new_sub_template(_dict->AddSectionDictionary(id));
}

inline void tagd_template::show_section(const std::string &id) {
	_dict->ShowSection(id);
}

inline void tagd_template::set_value(const std::string &k, const std::string &v) {
	_dict->SetValue(k, v);
}

inline void tagd_template::set_int_value(const std::string &k, long v) {
	_dict->SetIntValue(k, v);
}

inline void tagd_template::set_value_show_section(const std::string &k, const std::string &v, const std::string &id) {
	_dict->SetValueAndShowSection(k, v, id);
}

tagd_template* tagd_template::include(const std::string &id, const std::string &fname) {
	auto d = _dict->AddIncludeDictionary(id);
	d->SetFilename(fname);
	return this->new_sub_template(d);
}

void tagd_template::set_tag_link (
		const std::string& k,
		const std::string& v,
		const view& vw,
		const std::string *c) {

	if (v == "*") { // wildcard relator
		// empty link
		this->set_value(k, v);
		return;
	}

	std::string context_lnk;
	if (c!=nullptr && !c->empty()) {
		context_lnk.append("&c=").append(tagd::uri_encode(*c));
	}

	if ( tagd::url::looks_like_hduri(v) ) {
		tagd::url u;
		u.init_hduri(v);
		this->set_value( std::string(k).append("_lnk"),
			std::string("/").append(tagd::uri_encode(u.hduri())).append("?v=").append(vw.name).append(context_lnk)
		);
		this->set_value(k, u.id());
	} else if ( tagd::url::looks_like_url(v) ) {
		tagd::url u;
		u.init(v);
		this->set_value( std::string(k).append("_lnk"),
			std::string("/").append(tagd::uri_encode(u.hduri())).append("?v=").append(vw.name).append(context_lnk)
		);
		this->set_value(k, u.id());
	} else {
		this->set_value( std::string(k).append("_lnk"),
			std::string("/").append(tagd::util::esc_and_quote(v)).append("?v=").append(vw.name).append(context_lnk) );
		this->set_value(k, v);
	}
}

void tagd_template::set_relator_link (
		const std::string& k,
		const std::string& v,
		const view& vw,
		const std::string *c) {
	if (v.empty())  // wildcard relator
		set_tag_link(k, "*", vw, c);
	else
		set_tag_link(k, v, vw, c);
}

void tagd_template::set_query_link (const std::string& k, const std::string& v) {
	this->set_value( std::string(k).append("_lnk"),
		std::string("/").append(tagd::uri_encode(v)).append("/?v=query.html") );
}

void tagd_template::set_query_related_link (const std::string& k, const std::string& v) {
	this->set_value( std::string(k).append("_lnk"),
		std::string("/*/").append(tagd::uri_encode(v)).append("?v=query.html") );
}


} // namespace httagd
