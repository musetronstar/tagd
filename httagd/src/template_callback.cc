#include <sstream>
#include <ctemplate/template.h>
#include "httagd.h"

namespace httagd {

const empty_view_id HOME_VIEW_ID{"browse.html"};
const std::string HOME_TPL{"home.html.tpl"};

const get_view_id BROWSE_VIEW_ID{"browse.html"};
const std::string BROWSE_TPL{"browse.html.tpl"};

const query_view_id QUERY_VIEW_ID{"browse.html"};
const std::string QUERY_TPL{"query.html.tpl"};

const error_view_id ERROR_VIEW_ID{"browse.html"};
const std::string ERROR_TPL{"error.html.tpl"};

const get_view_id TAG_VIEW_ID{"tag.html"};
const std::string TAG_TPL{"tag.html.tpl"};

const error_view_id TAG_ERROR_VIEW_ID{"tag.html"};

const get_view_id TREE_VIEW_ID{"tree.html"};
const std::string TREE_TPL{"tree.html.tpl"};

const error_view_id TREE_ERROR_VIEW_ID{"tree.html"};

const std::string LAYOUT_TPL{"layout.html.tpl"};


void fill_header(transaction& tx, tagd_template& tpl, const std::string& title) {
	tpl.set_value("title", title);
	if ( tx.req->query_map().size() > 0 ) {
		tpl.show_section("query_options");
		auto t = tpl.add_section("query_option");
		for ( auto &kv : tx.req->query_map() ) {
			t->set_value("query_key", kv.first);
			t->set_value("query_value", kv.second);
		}
	}
}

empty_handler_t home_handler(
	[](transaction& tx, const view& vw) -> tagd::code {
		// TODO don't do this
		// messages should be internationalized
		// a _home_page tag might suffice for these type of definitions
		tagd::abstract_tag t("Welcomd to tagd!");
		tagd_template tpl("home", tx.res->output_buffer());
		fill_header(tx, tpl, t.id());

		auto home_tpl = tpl.include("main_html_tpl", tx.VS->fpath(vw));
		home_tpl->set_value("msg", t.id());

		return tpl.expand(*tx.VS, LAYOUT_TPL);
	}
);

tagd::code fill_query(transaction&, const view&, tagd_template&, const tagd::interrogator&, const tagd::tag_set&);

tagd::code fill_tag(transaction& tx, const view& vw, tagd_template& tpl, const tagd::abstract_tag& t) {
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		tpl.set_value("id", u.id());
		if (t.related("type", "image_jpeg"))
			tpl.set_value_show_section("img_src", u.id(), "has_img");
	} else {
		tpl.set_value("id", t.id());
	}

	const std::string context = tx.req->query_opt_context();
	tpl.set_tag_link("super_relator", t.super_relator(), vw.name(), context);
	tpl.set_tag_link("super_object", t.super_object(), vw.name(), context);

	tagd::tag_set img_urls;
	if (t.relations.size() > 0) {
		tpl.show_section("relations");
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
				relator_dict = tpl.add_section("relation_set");
				relator_dict->set_relator_link("relator", p.relator, vw.name(), context);
			}

			if (!relator_dict) continue;

			auto sub_dict = relator_dict->add_section("relation");
			sub_dict->set_tag_link("object", p.object, vw.name(), context);
			if (!p.modifier.empty()) {
				sub_dict->show_section("has_modifier");
				sub_dict->set_value("modifier", p.modifier);
			}
		}
	}

	if (img_urls.size() > 0) {
		tpl.show_section("gallery");
		for (auto u : img_urls) {
			tpl.set_value_show_section("rel_img_src", u.id(), "gallery_item");
		}
	}

	/* TODO implement code to fill the referents_refers_to section
	 * fill query won't cut it
	tagd::tag_set S;
	tagd::interrogator q_refers_to(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
	q_refers_to.relation(HARD_TAG_REFERS_TO, t.id());
	tagd::code tc = tx.TS->query(S, q_refers_to);
	if (tc != tagd::TAGD_OK) return tc;

	if (S.size() > 0) {
		tagd_template refers_tpl("refers_html_tpl", tx.res->output_buffer());
		tc = fill_query(tx, vw, refers_tpl, q_refers_to, S);
		if (tc != tagd::TAGD_OK) return tc;
	}
	*/

	/*
	S.clear();
	tagd::interrogator q_refers(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
	q_refers.relation(HARD_TAG_REFERS, t.id());
	tc = tx.TS->query(S, q_refers);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_view, q_refers, S, tpl);
	}

	S.clear();
	tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
	q_related.relation("", t.id());
	tc = tx.TS->query(S, q_related);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0) {
			set_query_related_link(&tpl, "query_related", t.id());
			this->expand_query(query_view, q_related, S, tpl);
		}
	}

	S.clear();
	tagd::interrogator q_siblings(HARD_TAG_INTERROGATOR, t.super_object());
	tc = tx.TS->query(S, q_siblings);

	set_query_link(&tpl, "query_siblings", t.super_object());
	if (tc == tagd::TAGD_OK && S.size() > 1) {
		if (S.size() > 0)
			this->expand_query(query_view, q_siblings, S, tpl);
	}

	S.clear();
	tagd::interrogator q_children(HARD_TAG_INTERROGATOR, t.id());
	tc = tx.TS->query(S, q_children);

	if (tc == tagd::TAGD_OK) {
		if (S.size() > 0)
			this->expand_query(query_view, q_children, S, tpl);
	}
	*/

	return tagd::TAGD_OK;
}

get_handler_t tag_handler(
	[](transaction& tx, const view& vw, const tagd::abstract_tag& t) -> tagd::code {
		tagd_template tpl("tag", tx.res->output_buffer());
		fill_tag(tx, vw, tpl, t);
		return tpl.expand(*tx.VS, vw);
	}
);

tagd::code fill_tree(transaction& tx, const view& vw, tagd_template& tpl, const tagd::abstract_tag& t) {
	const std::string context = tx.req->query_opt_context();
	if (t.pos() == tagd::POS_URL) {
		tagd::url u;
		u.init_hduri(t.id());
		tpl.set_value("id", u.id());
	} else {
		tpl.set_value("id", t.id());
	}
	tpl.set_tag_link("super_relator", t.super_relator(), vw.name(), context);
	tpl.set_tag_link("super_object", t.super_object(), vw.name(), context);

	tagd::tag_set S;
	tagd::code tc = tx.TS->query(S,
			tagd::interrogator(HARD_TAG_INTERROGATOR, t.super_object()));

	if (tc == tagd::TAGD_OK) {
		for (auto it=S.begin(); it != S.end(); ++it) {
			if (it->id() == t.id()) {
				if (it != S.begin()) {
					--it;
					auto s1 = tpl.add_section("has_prev");
					s1->set_tag_link("prev", it->id(), vw.name(), context);
					++it;
				}
				++it;
				if (it != S.end()) {
					auto s1 = tpl.add_section("has_next");
					s1->set_tag_link("next", it->id(), vw.name(), context);
				}
				break;
			}
		}
	}

	// query children
	S.clear();
	tc = tx.TS->query(S,
			tagd::interrogator(HARD_TAG_INTERROGATOR, t.id()));

	if (tc != tagd::TAGD_OK && tc != tagd::TS_NOT_FOUND)
		return tx.TS->code();

	if (S.size() > 0) {
		auto s1 = tpl.add_section("has_children");
		for (auto s : S) {
			auto s2 = s1->add_section("children");
			if (s.id() != t.id()) {
				s2->set_tag_link("child", s.id(), vw.name(), context);
			}
		}
	}

	return tagd::TAGD_OK;
}

get_handler_t tree_handler(
	[](transaction& tx, const view& vw, const tagd::abstract_tag& t) -> tagd::code {
		tagd_template tpl("tree", tx.res->output_buffer());
		tagd::code tc = fill_tree(tx, vw, tpl, t);
		if (tc != tagd::TAGD_OK) return tc;
		return tpl.expand(*tx.VS, vw);
	}
);

tagd::code fill_query(transaction& tx, const view& vw, tagd_template& tpl, const tagd::interrogator& q, const tagd::tag_set& R) {
	const std::string context = tx.req->query_opt_context();
	tpl.set_value("interrogator", q.id());
	if (!q.super_object().empty()) {
		tpl.show_section("super_relations");
		auto s1 = tpl.add_section("super_relation");
		s1->set_tag_link("super_relator", q.super_relator(), vw.name(), context);
		s1->set_tag_link("super_object", q.super_object(), vw.name(), context);
	}

	if (q.relations.size() > 0) {
		tpl.show_section("relations");
		for (auto p : q.relations) {
			auto s1 = tpl.add_section("relation");
			s1->set_relator_link("relator", p.relator, vw.name(), context);
			s1->set_tag_link("object", p.object, vw.name(), context);
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
				s1->set_tag_link("res_id", r.id(), vw.name(), ref.context());
			} else {
				s1->set_tag_link("res_id", r.id(), vw.name(), context);
			}
			s1->set_tag_link("res_super_relator", r.super_relator(), vw.name(), context);
			s1->set_tag_link("res_super_object", r.super_object(), vw.name(), context);
			if (r.relations.size() > 0) {
				s1->show_section("res_relations");
				for (auto p : r.relations) {
					auto s2 = s1->add_section("res_relation");
					s2->set_relator_link("res_relator", p.relator, vw.name(), context);
					s2->set_tag_link("res_object", p.object, vw.name(), context);
					if (!p.modifier.empty()) {
						s2->show_section("res_has_modifier");
						s2->set_value("res_modifier", p.modifier);
					}
				}
			}
		}
	}

	if (tx.TS->has_error())
		return tx.TS->code();

	return tagd::TAGD_OK;
}

query_handler_t query_handler(
	[](transaction& tx, const view& vw, const tagd::interrogator& q, const tagd::tag_set& R) -> tagd::code {
		tagd_template tpl("query", tx.res->output_buffer());

		std::stringstream ss;
		ss << q;

		fill_header(tx, tpl, ss.str());

		auto main_tpl = tpl.include("main_html_tpl", tx.VS->fpath(vw));

		tagd::code tc = fill_query(tx, vw, *main_tpl, q, R);
		if (tc != tagd::TAGD_OK) return tc;

		return tpl.expand(*tx.VS, LAYOUT_TPL);
	}
);

get_handler_t browse_handler(
	[](transaction& tx, const view& vw, const tagd::abstract_tag& t) -> tagd::code {
		tagd_template tpl("browse", tx.res->output_buffer());

		fill_header(tx, tpl, t.id());

		auto main_tpl = tpl.include("main_html_tpl", tx.VS->fpath(vw));

		if (t.pos() == tagd::POS_URL) {
			tagd::url u;
			u.init_hduri(t.id());
			main_tpl->set_value("id", u.id());
		} else {
			main_tpl->set_value("id", t.id());
		}

		auto tree_tpl = main_tpl->include("tree_html_tpl", tx.VS->fpath(TREE_TPL));
		tagd::code tc = fill_tree(tx, vw, *tree_tpl, t);
		if (tc != tagd::TAGD_OK) return tc;

		auto tag_tpl = main_tpl->include("tag_html_tpl", tx.VS->fpath(TAG_TPL));
		tc = fill_tag(tx, vw, *tag_tpl, t);
		if (tc != tagd::TAGD_OK) return tc;

		tagd::tag_set S;
		tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
		if (t.pos() == tagd::POS_RELATOR)
			q_related.relation(t.id(), "");
		else
			q_related.relation("", t.id());
		tc = tx.TS->query(S, q_related);

		if (tc != tagd::TAGD_OK && tc != tagd::TS_NOT_FOUND)
			return tc;

		if (S.size() > 0) {
			auto results_tpl = main_tpl->include("results_html_tpl", tx.VS->fpath(QUERY_TPL));
			tc = fill_query(tx, vw, *results_tpl, q_related, S);
			if (tc != tagd::TAGD_OK) return tc;
		}

		return tpl.expand(*tx.VS, LAYOUT_TPL);
	}
);

void fill_error(transaction& tx, tagd_template& tpl, const view& vw, const tagd::errorable& E) {
	tpl.set_value("err_ids", tagd::tag_ids_str(E.errors()));
	E.print_errors();
	auto context = tx.req->query_opt_context();

	if (E.size() > 0) {
		tpl.show_section("errors");
		for (auto r : E.errors()) {
			auto s1 = tpl.add_section("error");
			s1->set_value("err_id", r.id());
			s1->set_tag_link("err_super_relator", r.super_relator(), vw.name(), context);
			s1->set_tag_link("err_super_object", r.super_object(), vw.name(), context);
			if (r.relations.size() > 0) {
				s1->show_section("err_relations");
				for (auto p : r.relations) {
					auto s2 = s1->add_section("err_relation");
					s2->set_tag_link("err_relator", p.relator, vw.name(), context);
					s2->set_tag_link("err_object", p.object, vw.name(), context);
					if (!p.modifier.empty()) {
						s2->show_section("err_has_modifier");
						s2->set_value("err_modifier", p.modifier);
					}
				}
			}
		}
	}
}

error_handler_t error_handler(
	[](transaction& tx, const view& vw, const tagd::errorable& E) -> tagd::code {
		tagd_template tpl("error", tx.res->output_buffer());
		fill_header(tx, tpl, tagd::tag_ids_str(E.errors()));

		auto err_tpl = tpl.include("main_html_tpl", tx.VS->fpath(vw));
		fill_error(tx, *err_tpl, vw, E);

		return tpl.expand(*tx.VS, LAYOUT_TPL);
	}
);

error_handler_t partial_error_handler(
	[](transaction& tx, const view& vw, const tagd::errorable& E) -> tagd::code {
		tagd_template tpl("error", tx.res->output_buffer());
		fill_error(tx, tpl, vw, E);
		return tpl.expand(*tx.VS, vw);
	}
);

void init_viewspace(viewspace &VS) {
	// full html using LAYOUT_TPL
	VS.put({HOME_VIEW_ID, HOME_TPL, home_handler});
	VS.put({BROWSE_VIEW_ID, BROWSE_TPL, browse_handler});
	VS.put({QUERY_VIEW_ID, QUERY_TPL, query_handler});
	VS.put({ERROR_VIEW_ID, ERROR_TPL, error_handler});

	// html partials
	VS.put({TAG_VIEW_ID, TAG_TPL, tag_handler});
	VS.put({TAG_ERROR_VIEW_ID, ERROR_TPL, partial_error_handler});
	VS.put({TREE_VIEW_ID, TREE_TPL, tree_handler});
	VS.put({TREE_ERROR_VIEW_ID, ERROR_TPL, partial_error_handler});
}

// TODO add to httagd::callback
void output_errors(transaction& tx, tagd::code ret_tc, const tagd::errorable* E=nullptr) {
	if (!tx.size())
		tx.ferror(tagd::TS_INTERNAL_ERR, "no errors to output, returned: %s", tagd_code_str(ret_tc));

	if (tx.trace_on) tx.print_errors();

	view vw;
	// get the error_function view
	if (tx.VS->get(vw, error_view_id(tx.req->query_opt_view())) != tagd::TAGD_OK) {
		// adds plain text errors to response output buffer
		// for errorable objects contained by transaction
		tx.add_errors();
		return;
	}

	// attempts to add error_function response for the errorable object
	// adds plain text of error_function fails
	errorable_function_t f_err = [&tx, &vw](const tagd::errorable& e) -> tagd::code {
		tagd::code tc = vw.error_function(tx, vw, e);
		if (tc != tagd::TAGD_OK) {
			tx.add_errors();  // add all errors plain text
			return tc;
		}

		return tagd::TAGD_OK;
	};

	if (E == nullptr) {
		// have transaction call f_err on each errorable object it contains
		tx.add_errors(f_err);
	} else {
		// adds error_function response for the errorable object pointed to
		f_err(*E);
	}
}

void html_callback::cmd_get(const tagd::abstract_tag& t) {
	if (_tx->trace_on) std::cerr << "cmd_get()" << std::endl;
	/*
	 * In the case of HEAD requests, we are still adding
	 * content to the buffer just like GET requests.
	 * This is because evhtp will count the bytes and add
	 * a Content-Length header for us, even though the content
	 * will not be sent to the client.
	 * TODO surely, there must be a less wasteful way to do this.
	 */

	tagd::abstract_tag T;
	tagd::code tc;
	if (t.pos() == tagd::POS_URL) {
		tc = _tx->TS->get(static_cast<tagd::url&>(T), t.id());
	} else {
		tc = _tx->TS->get(T, t.id());
	}

	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc, _tx->TS);
		return;
	}

	view vw;
	tc = _tx->VS->get(vw, get_view_id(_tx->req->query_opt_view()));
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc, _tx->VS);
		return;
	}

	tc = vw.get_function(*_tx, vw, T);
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}
}

void html_callback::cmd_query(const tagd::interrogator& q) {
	if (_tx->trace_on) std::cerr << "cmd_query()" << std::endl;

	tagd::tag_set R;
	tagd::code tc = _tx->TS->query(R, q);

	if (tc != tagd::TAGD_OK && tc != tagd::TS_NOT_FOUND) {
		output_errors(*_tx, tc, _tx->TS);
		return;
	}

	view vw;
	tc = _tx->VS->get(vw, query_view_id(_tx->req->query_opt_view()));
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc, _tx->VS);
		return;
	}

	tc = vw.query_function(*_tx, vw, q, R);
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}
}

void html_callback::cmd_error() {
	if (_tx->trace_on) std::cerr << "cmd_error()" << std::endl;

	if (_driver->has_error())
		output_errors(*_tx, _driver->code(), _driver);

	if (_tx->has_error())
		output_errors(*_tx, _tx->code());
}


void html_callback::empty() {
	if (_tx->trace_on) std::cerr << "empty()" <<  std::endl;

	view vw;
	tagd::code tc = _tx->VS->get(vw, empty_view_id(_tx->req->query_opt_view()));
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc, _tx->VS);
		return;
	}

	tc = vw.empty_function(*_tx, vw);
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}
}

tagd::code tagd_template::expand(const std::string& fname) {
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
		this->ferror( tagd::TAGD_ERR, "expand template failed: %s" , fname.c_str() );
		return tagd::TAGD_ERR;
	}

	return tagd::TAGD_OK;
}

inline tagd::code tagd_template::expand(viewspace& VS, const std::string& fname) {
	if (this->expand(VS.fpath(fname)) != tagd::TAGD_OK)
		return VS.errors(*this);  // add this errors to viewspace and return last code

	return tagd::TAGD_OK;
}

inline tagd::code tagd_template::expand(viewspace& VS, const view& vw) {
	return this->expand(VS, vw.tpl_fname);
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

// sets the value of a template marker to <key> and also its
// corresponding <key>_lnk marker to the hyperlink representation of <key>
void tagd_template::set_tag_link (
		const std::string& key,
		const std::string& val,
		const std::string& view_name,
		const std::string& context) {

	if (val == "*") { // wildcard relator
		// empty link
		this->set_value(key, val);
		return;
	}

	std::string opt_str;
	if (!view_name.empty()) {
		opt_str.push_back(opt_str.empty() ? '?' : '&');
		opt_str.append(QUERY_OPT_VIEW);
		opt_str.push_back(('='));
		opt_str.append(tagd::uri_encode(view_name));
	}

	if (!context.empty()) {
		opt_str.push_back(opt_str.empty() ? '?' : '&');
		opt_str.append(QUERY_OPT_CONTEXT);
		opt_str.push_back(('='));
		opt_str.append(tagd::uri_encode(context));
	}

	// sets template key and key_lnk with their corresponding values
	auto f_set_vals = [this, &key, &opt_str](const std::string& key_val, const std::string& lnk_val) {
		this->set_value(
			std::string(key).append("_lnk"),
			std::string("/").append(lnk_val).append(opt_str)
		);
		this->set_value(key, key_val);
	};

	if ( tagd::url::looks_like_hduri(val) ) {
		tagd::url u;
		u.init_hduri(val);
		f_set_vals(u.id(), tagd::uri_encode(u.hduri()));
	} else if ( tagd::url::looks_like_url(val) ) {
		tagd::url u;
		u.init(val);
		f_set_vals(u.id(), tagd::uri_encode(u.hduri()));
	} else {
		f_set_vals(val, tagd::util::esc_and_quote(val));
	}
}

void tagd_template::set_relator_link (
		const std::string& key,
		const std::string& val,
		const std::string& view_name,
		const std::string& context) {
	if (val.empty())  // wildcard relator
		set_tag_link(key, "*", view_name, context);
	else
		set_tag_link(key, val, view_name, context);
}

} // namespace httagd
