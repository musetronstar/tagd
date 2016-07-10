#include <sstream>
#include "httagd.h"

using namespace httagd;

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
