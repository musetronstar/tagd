#include <sstream> 
#include <ctemplate/template.h>
#include "httagd.h"

namespace httagd {

typedef enum {
	TPL_TAG,
	TPL_QUERY,
	TPL_NOT_FOUND,
	TPL_ERR
} tpl_file;

typedef std::pair<std::string, tpl_file> tpl_pair_t;

class tagd_template {
		tagspace::tagspace *_TS;
		std::string _context;

	public: 
		tagd_template(tagspace::tagspace* ts, std::string c) : _TS{ts}, _context{c} {}

		static tpl_pair_t tpl_file(const std::string& name) {
			tpl_pair_t tpl{std::string(), TPL_NOT_FOUND};
			switch (name[0]) {
				case 't':
					if (name == "tag.html") {
						tpl.first = "tpl/tag.html.tpl";
						tpl.second = TPL_TAG;
					}
					break;	
				case 'q':
					if (name == "query.html") {
						tpl.first = "tpl/query.html.tpl";
						tpl.second = TPL_QUERY;
					}
					break;	
				default:
					; // TPL_NOT_FOUND
			}

			if (tpl.second != TPL_NOT_FOUND) {
				if (!ctemplate::LoadTemplate(tpl.first, ctemplate::DO_NOT_STRIP))
					tpl.second = TPL_ERR;
			}

			return tpl;
		}

		bool looks_like_url(const std::string& s) {
			return (s.find("://") != std::string::npos);
		}

		// if looks like hduri
		bool looks_like_hduri(const std::string& str) {
			size_t i = str.rfind(':');
			if (i == std::string::npos) return false;

			int sz = str.size() - i;
			switch (sz) {
				case 4:  // ":ftp"
					if (str.substr((i+1), sz-1) == "ftp")
						return true;
					break;
				case 5:
					// ":http"
					if (str.substr((i+1), sz-1) == "http")
						return true;
					// ":file"
					if (str.substr((i+1), sz-1) == "file")
						return true;
					break;
				case 6:  // ":https"
					if (str.substr((i+1), sz-1) == "https")
						return true;
					break;
				default:
					return false;
			}

			return false;
		}

		void set_tag_link (
				ctemplate::TemplateDictionary* d,
				const std::string& k,
				const std::string& v,
				const std::string *c = nullptr) {

			std::string context_lnk;
			if (c!=nullptr && !c->empty()) {
				context_lnk.append("&c=").append(tagd::uri_encode(*c));
			} else if (!_context.empty())
				context_lnk.append("&c=").append(tagd::uri_encode(_context));

			if ( looks_like_hduri(v) ) {
				tagd::url u;
				u.init_hduri(v);
				d->SetValue( std::string(k).append("_lnk"),
					std::string("/").append(tagd::uri_encode(u.hduri())).append("?t=tag.html").append(context_lnk)
				);
				d->SetValue(k, u.id());
			} else if ( looks_like_url(v) ) {
				tagd::url u;
				u.init(v);
				d->SetValue( std::string(k).append("_lnk"),
					std::string("/").append(tagd::uri_encode(u.hduri())).append("?t=tag.html").append(context_lnk)
				);
				d->SetValue(k, u.id());
			} else {
				d->SetValue( std::string(k).append("_lnk"),
					std::string("/").append(tagd::uri_encode(v)).append("?t=tag.html").append(context_lnk) );
				d->SetValue(k, v);
			}
		};

		void set_query_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v) {
			d->SetValue( std::string(k).append("_lnk"),
				std::string("/").append(tagd::uri_encode(v)).append("/?t=query.html") );
		};

		void set_query_related_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v) {
			d->SetValue( std::string(k).append("_lnk"),
				std::string("/*/").append(tagd::uri_encode(v)).append("?t=query.html") );
		};

		void expand_tag(std::string& output, const std::string& tpl_file, const tagd::abstract_tag& t) {


			ctemplate::TemplateDictionary dict("tag");
			if (t.pos() == tagd::POS_URL) {
				tagd::url u;
				u.init_hduri(t.id());
				dict.SetValue("id", u.id());
			} else {
				dict.SetValue("id", t.id());
			}
			set_tag_link(&dict, "super_relator", t.super_relator());
			set_tag_link(&dict, "super_object", t.super_object());

			if (t.relations.size() > 0) {
				dict.ShowSection("relations");
				for (auto p : t.relations) {

					ctemplate::TemplateDictionary* sub_dict = dict.AddSectionDictionary("relation");
					set_tag_link(sub_dict, "relator", p.relator);
					set_tag_link(sub_dict, "object", p.object);
					if (!p.modifier.empty()) {
						sub_dict->ShowSection("has_modifier");
						sub_dict->SetValue("modifier", p.modifier);
					}
				}
			}

			tagd::tag_set S;
			tagd::interrogator q_refers_to(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
			q_refers_to.relation(HARD_TAG_REFERS_TO, t.id());
			tagd::code tc = _TS->query(S, q_refers_to);
			
			if (tc == tagd::TAGD_OK) {
				for (auto r : S) {
					ctemplate::TemplateDictionary* s1 = dict.AddSectionDictionary("referents_refers_to");

					if (r.has_relator(HARD_TAG_CONTEXT)) {
						tagd::referent ref(r);
						std::string context{ref.context()};
						set_tag_link(s1, "refers", r.id(), &context);
					} else {
						set_tag_link(s1, "refers", r.id());
					}

					set_tag_link(s1, "refers_to_relator", r.super_relator());
					set_tag_link(s1, "refers_to", ((const tagd::referent)r).refers_to());

					std::string context{((const tagd::referent)r).context()};
					if (!context.empty()) {
						s1->ShowSection("has_context");
						set_tag_link(s1, "context_relator", HARD_TAG_CONTEXT);
						set_tag_link(s1, "context", context);
					}
				}
			}

			S.clear();
			tagd::interrogator q_refers(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
			q_refers.relation(HARD_TAG_REFERS, t.id());
			tc = _TS->query(S, q_refers);

			if (tc == tagd::TAGD_OK) {
				for (auto r : S) {
					ctemplate::TemplateDictionary* s1 = dict.AddSectionDictionary("referents_refers");
					set_tag_link(s1, "refers", ((const tagd::referent)r).refers());
					set_tag_link(s1, "refers_to_relator", r.super_relator());
					set_tag_link(s1, "refers_to", ((const tagd::referent)r).refers_to());
					
					std::string context{((const tagd::referent)r).context()};
					if (!context.empty()) {
						s1->ShowSection("has_context");
						set_tag_link(s1, "context_relator", HARD_TAG_CONTEXT);
						set_tag_link(s1, "context", context);
					}
				}
			}

			S.clear();
			tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
			q_related.relation("", t.id());
			tc = _TS->query(S, q_related);

			set_query_related_link(&dict, "query_related", t.id());
			if (tc == tagd::TAGD_OK) {
				for (auto s : S) {
					ctemplate::TemplateDictionary* s1 = dict.AddSectionDictionary("objects_related");

					set_tag_link(s1, "related", s.id());

					tagd::predicate_set how;
					size_t n = s.related(t.id(), how);
					for (auto p : how) {
						ctemplate::TemplateDictionary* s2 = s1->AddSectionDictionary("related_predicate");
						set_tag_link(s2, "related_relator", p.relator);
						set_tag_link(s2, "related_object", p.object);

						if (!p.modifier.empty()) {
							s2->ShowSection("has_related_modifier");
							s2->SetValue("related_modifier", p.modifier);
						}
					}
				}
			}

			S.clear();
			tc = _TS->query(S,
					tagd::interrogator(HARD_TAG_INTERROGATOR, t.super_object()));

			set_query_link(&dict, "query_siblings", t.super_object());
			if (tc == tagd::TAGD_OK && S.size() > 1) {
				for (auto s : S) {
					if (s.id() != t.id()) {
						ctemplate::TemplateDictionary* s1 = dict.AddSectionDictionary("siblings");

						set_tag_link(s1, "sibling", s.id());
					}
				}
			}

			S.clear();
			tc = _TS->query(S,
					tagd::interrogator(HARD_TAG_INTERROGATOR, t.id()));

			if (tc == tagd::TAGD_OK) {
				set_query_link(&dict, "query_children", t.id());
				for (auto s : S) {
					ctemplate::TemplateDictionary* s1 = dict.AddSectionDictionary("children");
					if (s.id() != t.id()) {
						set_tag_link(s1, "child", s.id());
					}
				}
			}

			if (_TS->has_errors()) {
				std::stringstream ss;
				_TS->print_errors(ss);
				dict.SetValue("errors", ss.str());
			}

			ctemplate::ExpandTemplate(tpl_file,
							ctemplate::DO_NOT_STRIP, &dict, &output);
		}

		void expand_query(std::string& output, const std::string& tpl_file, const tagd::interrogator& q, const tagd::tag_set R) {
			ctemplate::TemplateDictionary dict("query");
			std::stringstream ss;
			ss << q;
			dict.SetValue("query", ss.str());
			dict.SetValue("interrogator", q.id());
			set_tag_link(&dict, "super_relator", q.super_relator());
			set_tag_link(&dict, "super_object", q.super_object());

			if (q.relations.size() > 0) {
				dict.ShowSection("relations");
				for (auto p : q.relations) {
					ctemplate::TemplateDictionary* s1 = dict.AddSectionDictionary("relation");
					set_tag_link(s1, "relator", p.relator);
					set_tag_link(s1, "object", p.object);
					if (!p.modifier.empty()) {
						s1->ShowSection("has_modifier");
						s1->SetValue("modifier", p.modifier);
					}
				}
			}

			if (R.size() > 0) {
				dict.ShowSection("results");
				for (auto r : R) {
					ctemplate::TemplateDictionary* s1 = dict.AddSectionDictionary("result");
					if (r.has_relator(HARD_TAG_CONTEXT)) {
						tagd::referent ref(r);
						std::string context{ref.context()};
						set_tag_link(s1, "res_id", r.id(), &context);
					} else {
						set_tag_link(s1, "res_id", r.id());
					}
					set_tag_link(s1, "res_super_relator", r.super_relator());
					set_tag_link(s1, "res_super_object", r.super_object());
					if (r.relations.size() > 0) {
						s1->ShowSection("res_relations");
						for (auto p : r.relations) {
							ctemplate::TemplateDictionary* s2 = s1->AddSectionDictionary("res_relation");
							set_tag_link(s1, "res_relator", p.relator);
							set_tag_link(s1, "res_object", p.object);
							if (!p.modifier.empty()) {
								s2->ShowSection("res_has_modifier");
								s2->SetValue("res_modifier", p.modifier);
							}
						}
					}
				}
			}

			if (_TS->has_errors()) {
				std::stringstream ss;
				_TS->print_errors(ss);
				dict.SetValue("errors", ss.str());
			}

			ctemplate::ExpandTemplate(tpl_file,
							ctemplate::DO_NOT_STRIP, &dict, &output);
		}
};

void template_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc;
	if (t.pos() == tagd::POS_URL) {
		ts_rc = _TS->get((tagd::url&)T, t.id());
	} else {
		ts_rc = _TS->get(T, t.id());
	}
	int res;
	std::string xpnd;
	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		tagd_template tt(_TS, _context);
		tpl_pair_t tpl = tagd_template::tpl_file(_tpl_file);
		switch (tpl.second) {
			case TPL_NOT_FOUND:
				// TODO create a not found html template
				xpnd = "resource not found: ";
				xpnd.append(_tpl_file).append("\n");
				res = EVHTP_RES_NOTFOUND;
				break;
			case TPL_ERR:
				// TODO create an error html template
				ss <<  "error expanding template: ";
				ss << _tpl_file << std::endl;
				_TS->print_errors(ss);
				xpnd = ss.str();
				res = EVHTP_RES_SERVERR;
				break;
			default:
				tt.expand_tag(xpnd, tpl.first, T);
				res = EVHTP_RES_OK;
		}

		evhtp_headers_add_header(_req->headers_out,
				evhtp_header_new("Content-Type", "text/html", 0, 0));
	} else {
		// TODO create and err template
		std::stringstream ss;
		_TS->print_errors(ss);
		xpnd = ss.str();
		res = EVHTP_RES_SERVERR;
	}

	evbuffer_add(_req->buffer_out, xpnd.c_str(), xpnd.size());
	evhtp_send_reply(_req, res);
}

void template_callback::cmd_put(const tagd::abstract_tag& t) {
	tagd::code ts_rc = _TS->put(t);
	if (ts_rc != tagd::TAGD_OK) {
		std::stringstream ss;
		_TS->print_errors(ss);
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	}

	switch (ts_rc) {
		case tagd::TAGD_OK:
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			evhtp_send_reply(_req, EVHTP_RES_SERVERR);
	}
}

void template_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q);

	int res;
	std::string xpnd;
	if (ts_rc == tagd::TAGD_OK) {
		tagd_template tt(_TS, _context);
		tpl_pair_t tpl = tagd_template::tpl_file(_tpl_file);
		switch (tpl.second) {
			case TPL_NOT_FOUND:
				// TODO create a not found html template
				xpnd = "resource not found: ";
				xpnd.append(_tpl_file).append("\n");
				res = EVHTP_RES_NOTFOUND;
				break;
			case TPL_ERR:
				// TODO create an error html template
				xpnd = "error expanding template: ";
				xpnd.append(_tpl_file).append("\n");
				res = EVHTP_RES_SERVERR;
				break;
			default:
				tt.expand_query(xpnd, tpl.first, q, T);
				res = EVHTP_RES_OK;
		}

		evhtp_headers_add_header(_req->headers_out,
				evhtp_header_new("Content-Type", "text/html", 0, 0));
	} else {
		// TODO create and err template
		std::stringstream ss;
		_TS->print_errors(ss);
		xpnd = ss.str();
		res = EVHTP_RES_SERVERR;
	}

	evbuffer_add(_req->buffer_out, xpnd.c_str(), xpnd.size());
	evhtp_send_reply(_req, res);

/*
	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		tagd::print_tag_ids(T, ss);
		ss << std::endl;
	} else {
		_TS->print_errors(ss);
	}
	if (ss.str().size())
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());

	switch (ts_rc) {
		case tagd::TAGD_OK:
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			evhtp_send_reply(_req, EVHTP_RES_SERVERR);
	}
*/
}

void template_callback::error(const TAGL::driver& D) {
	std::stringstream ss;
	D.print_errors(ss);
	evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	evhtp_send_reply(_req, EVHTP_RES_SERVERR);
}

}
