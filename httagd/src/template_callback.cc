#include <sstream> 
#include <exception> // std::terminate
#include <ctemplate/template.h>
#include "httagd.h"

namespace httagd {

typedef enum {
	TPL_HOME,
	TPL_TAG,
	TPL_TREE,
	TPL_RELATIONS,
	TPL_BROWSE,
	TPL_QUERY,
	TPL_NOT_FOUND,
	TPL_HEADER,
	TPL_FOOTER,
	TPL_ERROR
} tpl_file;

struct tpl_t {
	std::string opt_t;
	std::string fname;
	tpl_file file;
};

const tpl_t HOME_TPL{"tag.html", "home.html.tpl", TPL_HOME};
const tpl_t TAG_TPL{"tag.html", "tag.html.tpl", TPL_TAG};
const tpl_t TREE_TPL{"tree.html", "tree.html.tpl", TPL_TREE};
const tpl_t BROWSE_TPL{"browse.html", "browse.html.tpl", TPL_BROWSE};
const tpl_t RELATIONS_TPL{"relations.html", "relations.html.tpl", TPL_RELATIONS};
const tpl_t QUERY_TPL{"query.html", "query.html.tpl", TPL_QUERY};
const tpl_t ERROR_TPL{"error.html", "error.html.tpl", TPL_ERROR};
const tpl_t HEADER_TPL{"header.html", "header.html.tpl", TPL_HEADER};
const tpl_t FOOTER_TPL{"footer.html", "footer.html.tpl", TPL_FOOTER};

class tagd_template : public tagd::errorable {
		tagspace::tagspace *_TS;
		request *_request;
		evhtp_request_t *_ev_req;
		std::string _tpl_dir;
		std::string _context;

	public: 
		tagd_template(tagspace::tagspace* ts, request *r, evhtp_request_t *v, const std::string &tpl_dir) :
			_TS{ts}, _request{r}, _ev_req{v}, _tpl_dir{tpl_dir}, _context{r->query_opt("c")}
		{
			if (_tpl_dir.empty()) _tpl_dir = "./tpl/";
		}

		std::string fpath(const tpl_t &tpl) {
			return std::string(_tpl_dir)
					.append( (_tpl_dir.size() > 0 && _tpl_dir[_tpl_dir.size()-1] == '/' ? "" : "/") )
					.append( tpl.fname );
		}

		// lookup a tpl_t given the tpl query string option 
		// doesn't allow looking up error template
		static tpl_t lookup_tpl(const std::string& opt_tpl) {
			switch (opt_tpl[0]) {
				case 'b':
					if (opt_tpl == "browse.html")
						return BROWSE_TPL;
					break;
				case 't':
					if (opt_tpl == "tag.html")
						return TAG_TPL;
					if (opt_tpl == "tree.html")
						return TREE_TPL;
					break;	
				case 'q':
					if (opt_tpl == "query.html")
						return QUERY_TPL;
					break;	
				case 'r':
					if (opt_tpl == "relations.html")
						return RELATIONS_TPL;
					break;	
				default:
					;
			}

			return {opt_tpl, std::string(), TPL_NOT_FOUND};
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
				const tpl_t& tpl,
				ctemplate::TemplateDictionary* d,
				const std::string& k,
				const std::string& v,
				const std::string *c = nullptr) {

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
					std::string("/").append(tagd::uri_encode(u.hduri())).append("?t=").append(tpl.opt_t).append(context_lnk)
				);
				d->SetValue(k, u.id());
			} else if ( looks_like_url(v) ) {
				tagd::url u;
				u.init(v);
				d->SetValue( std::string(k).append("_lnk"),
					std::string("/").append(tagd::uri_encode(u.hduri())).append("?t=").append(tpl.opt_t).append(context_lnk)
				);
				d->SetValue(k, u.id());
			} else {
				d->SetValue( std::string(k).append("_lnk"),
					std::string("/").append(tagd::util::esc_and_quote(v)).append("?t=").append(tpl.opt_t).append(context_lnk) );
				d->SetValue(k, v);
			}
		}

		void set_relator_link (
				const tpl_t& tpl,
				ctemplate::TemplateDictionary* d,
				const std::string& k,
				const std::string& v,
				const std::string *c = nullptr) {
			if (v.empty())  // wildcard relator
				set_tag_link(tpl, d, k, "*", c);
			else
				set_tag_link(tpl, d, k, v, c);
		}

		void set_query_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v) {
			d->SetValue( std::string(k).append("_lnk"),
				std::string("/").append(tagd::uri_encode(v)).append("/?t=query.html") );
		}

		void set_query_related_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v) {
			d->SetValue( std::string(k).append("_lnk"),
				std::string("/*/").append(tagd::uri_encode(v)).append("?t=query.html") );
		}

		void expand_template(const tpl_t& tpl, const ctemplate::TemplateDictionary& D) {
			if (!ctemplate::LoadTemplate(fpath(tpl), ctemplate::DO_NOT_STRIP)) {
				this->ferror( tagd::TAGD_ERR, "load template failed: %s" , fpath(tpl).c_str() );
				return;
			}

			std::string output; 
			ctemplate::ExpandTemplate(fpath(tpl), ctemplate::DO_NOT_STRIP, &D, &output); 
			evbuffer_add(_ev_req->buffer_out, output.c_str(), output.size());
		}

		void expand_header(const std::string& title) {
			ctemplate::TemplateDictionary D("header");
			D.SetValue("title", title);

			this->expand_template(HEADER_TPL, D);
		}

		void expand_footer() {
			ctemplate::TemplateDictionary D("footer");
			this->expand_template(FOOTER_TPL, D);
		}

		int expand_home(const tpl_t& tpl, ctemplate::TemplateDictionary& D) {
			// TODO messages should be internationalized
			// a _home_page tag might suffice for these type of definitions
			D.SetValue("msg", "Welcome to tagd!");

			return EVHTP_RES_OK;
		}

		int expand_tag(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D) {
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

			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0)
					this->expand_query(QUERY_TPL, q_refers_to, S, D);
			}

			/*
			S.clear();
			tagd::interrogator q_refers(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
			q_refers.relation(HARD_TAG_REFERS, t.id());
			tc = _TS->query(S, q_refers);

			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0)
					this->expand_query(QUERY_TPL, q_refers, S, D);
			}

			S.clear();
			tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
			q_related.relation("", t.id());
			tc = _TS->query(S, q_related);

			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0) {
					set_query_related_link(&D, "query_related", t.id());
					this->expand_query(QUERY_TPL, q_related, S, D);
				}
			}

			S.clear();
			tagd::interrogator q_siblings(HARD_TAG_INTERROGATOR, t.super_object());
			tc = _TS->query(S, q_siblings);

			set_query_link(&D, "query_siblings", t.super_object());
			if (tc == tagd::TAGD_OK && S.size() > 1) {
				if (S.size() > 0)
					this->expand_query(QUERY_TPL, q_siblings, S, D);
			}

			S.clear();
			tagd::interrogator q_children(HARD_TAG_INTERROGATOR, t.id());
			tc = _TS->query(S, q_children);

			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0)
					this->expand_query(QUERY_TPL, q_children, S, D);
			}
  */
			if (_TS->has_error()) {
				std::stringstream ss;
				_TS->print_errors(ss);
				D.SetValue("errors", ss.str());
			}

			return EVHTP_RES_OK;
		}
		
		int expand_browse(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D) {
			// ctemplate::TemplateDictionary D("browse");
			if (t.pos() == tagd::POS_URL) {
				tagd::url u;
				u.init_hduri(t.id());
				D.SetValue("id", u.id());
			} else {
				D.SetValue("id", t.id());
			}

			ctemplate::TemplateDictionary* tree = D.AddIncludeDictionary("tree_html_tpl");
			tree->SetFilename(fpath(TREE_TPL));
			size_t num_children = 0;
			this->expand_tree(tpl, t, *tree, &num_children);
			// std::cerr << "num_children: " << num_children << std::endl;

			ctemplate::TemplateDictionary* tag = D.AddIncludeDictionary("tag_html_tpl");
			tag->SetFilename(fpath(TAG_TPL));
			this->expand_tag(BROWSE_TPL, t, *tag);

			tagd::tag_set S;
			tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
			if (t.pos() == tagd::POS_RELATOR)	
				q_related.relation(t.id(), "");
			else
				q_related.relation("", t.id());
			tagd::code tc = _TS->query(S, q_related);

			// std::cerr << "q_related(" << S.size() << "): " << q_related << std::endl;

			ctemplate::TemplateDictionary* results = D.AddIncludeDictionary("results_html_tpl");
			results->SetFilename(fpath(QUERY_TPL));
			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0) {
					this->expand_query(QUERY_TPL, q_related, S, *results);
				}
			}

			if (_TS->has_error()) {
				std::stringstream ss;
				_TS->print_errors(ss);
				D.SetValue("errors", ss.str());
			}

			return EVHTP_RES_OK;
		}

		int expand_relations(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D) {
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
					this->expand_query(QUERY_TPL, q_refers_to, S, D);
			}

			S.clear();
			tagd::interrogator q_refers(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
			q_refers.relation(HARD_TAG_REFERS, t.id());
			tc = _TS->query(S, q_refers);

			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0)
					this->expand_query(QUERY_TPL, q_refers, S, D);
			}

			S.clear();
			tagd::interrogator q_related(HARD_TAG_INTERROGATOR);
			q_related.relation("", t.id());
			tc = _TS->query(S, q_related);

			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0) {
					set_query_related_link(&D, "query_related", t.id());
					this->expand_query(QUERY_TPL, q_related, S, D);
				}
			}

			S.clear();
			tagd::interrogator q_siblings(HARD_TAG_INTERROGATOR, t.super_object());
			tc = _TS->query(S, q_siblings);

			set_query_link(&D, "query_siblings", t.super_object());
			if (tc == tagd::TAGD_OK && S.size() > 1) {
				if (S.size() > 0)
					this->expand_query(QUERY_TPL, q_siblings, S, D);
			}

			S.clear();
			tagd::interrogator q_children(HARD_TAG_INTERROGATOR, t.id());
			tc = _TS->query(S, q_children);

			if (tc == tagd::TAGD_OK) {
				if (S.size() > 0)
					this->expand_query(QUERY_TPL, q_children, S, D);
			}
  */
			if (_TS->has_error()) {
				std::stringstream ss;
				_TS->print_errors(ss);
				D.SetValue("errors", ss.str());
			}

			return EVHTP_RES_OK;
		}
		
		int expand_tree(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D, size_t *num_children=nullptr) {
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

		int expand_query(const tpl_t& tpl, const tagd::interrogator& q, const tagd::tag_set& R, ctemplate::TemplateDictionary& D) {
			// ctemplate::TemplateDictionary D("query");

			D.SetValue("interrogator", q.id());
			if (!q.super_object().empty()) {
				D.ShowSection("super_relations");
				ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("super_relation");
				set_tag_link(TAG_TPL, s1, "super_relator", q.super_relator());
				set_tag_link(TAG_TPL, s1, "super_object", q.super_object());
			}

			if (q.relations.size() > 0) {
				D.ShowSection("relations");
				for (auto p : q.relations) {
					ctemplate::TemplateDictionary* s1 = D.AddSectionDictionary("relation");
					set_relator_link(TAG_TPL, s1, "relator", p.relator);
					set_tag_link(TAG_TPL, s1, "object", p.object);
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
						set_tag_link(BROWSE_TPL, s1, "res_id", r.id(), &context);
					} else {
						set_tag_link(BROWSE_TPL, s1, "res_id", r.id());
					}
					set_tag_link(TAG_TPL, s1, "res_super_relator", r.super_relator());
					set_tag_link(TAG_TPL, s1, "res_super_object", r.super_object());
					if (r.relations.size() > 0) {
						s1->ShowSection("res_relations");
						for (auto p : r.relations) {
							ctemplate::TemplateDictionary* s2 = s1->AddSectionDictionary("res_relation");
							set_relator_link(BROWSE_TPL, s2, "res_relator", p.relator);
							set_tag_link(BROWSE_TPL, s2, "res_object", p.object);
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

		void expand_error(const tpl_t& tpl, const tagd::errorable& E) {
			ctemplate::TemplateDictionary D("error");
			D.SetValue("err_ids", tagd::tag_ids_str(E.errors()));
			const tagd::errors_t& R	= E.errors();
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
					set_tag_link(TAG_TPL, s1, "err_super_relator", r.super_relator());
					set_tag_link(TAG_TPL, s1, "err_super_object", r.super_object());
					if (r.relations.size() > 0) {
						s1->ShowSection("err_relations");
						for (auto p : r.relations) {
							ctemplate::TemplateDictionary* s2 = s1->AddSectionDictionary("err_relation");
							set_tag_link(TAG_TPL, s2, "err_relator", p.relator);
							set_tag_link(TAG_TPL, s2, "err_object", p.object);
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
};

void ev_send_error_str(evhtp_request_t *evreq, const tagd::errorable &err) {
	std::stringstream ss;
	err.print_errors(ss);
	evbuffer_add(evreq->buffer_out, ss.str().c_str(), ss.str().size());
	evhtp_send_reply(evreq, EVHTP_RES_SERVERR);
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
	tagd_template tt(_TS, _request, _ev_req, _request->_server->_args->tpl_dir);
	this->add_http_headers();
	tt.expand_header(t.id());

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	ctemplate::TemplateDictionary D("get");
	if (ts_rc == tagd::TAGD_OK) {
		tpl_t tpl = tagd_template::lookup_tpl(_request->query_opt("t"));
		switch (tpl.file) {
			case TPL_BROWSE:
				res = tt.expand_browse(tpl, T, D);
				tt.expand_template(tpl, D);
				break;
			case TPL_TAG:
				res = tt.expand_tag(tpl, T, D);
				tt.expand_template(tpl, D);
				break;
			case TPL_TREE:
				res = tt.expand_tree(tpl, T, D);
				tt.expand_template(tpl, D);
				break;
			case TPL_RELATIONS:
				res = tt.expand_relations(tpl, T, D);
				tt.expand_template(tpl, D);
				break;
			default: // TPL_NOT_FOUND:
				this->ferror(tagd::TS_NOT_FOUND, "resource not found: %s", _request->query_opt("t").c_str());
				tt.expand_error(ERROR_TPL, *this);
				res = EVHTP_RES_NOTFOUND;
				break;
		}
	} else {
		// TODO TS_NOT_FOUND does not set error
		tt.expand_error(ERROR_TPL, *_TS);
		res = EVHTP_RES_SERVERR;
	}

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	tt.expand_footer();

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	evhtp_send_reply(_ev_req, res);

	if (_trace_on) std::cerr << "cmd_get(" << tagd_code_str(ts_rc) << ") : " <<  res << std::endl;
}

void template_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q);

	int res;
	tagd_template tt(_TS, _request, _ev_req, _request->_server->_args->tpl_dir);
	this->add_http_headers();
	tt.expand_header(q.id());

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	ctemplate::TemplateDictionary D("query");
	if (ts_rc == tagd::TAGD_OK || ts_rc == tagd::TS_NOT_FOUND) {
		tpl_t tpl = tagd_template::lookup_tpl(_request->query_opt("t"));
		switch (tpl.file) {
			case TPL_NOT_FOUND:
				this->ferror(tagd::TS_NOT_FOUND, "resource not found: %s", _request->query_opt("t").c_str());
				tt.expand_error(ERROR_TPL, *this);
				res = EVHTP_RES_NOTFOUND;
				break;
			default:
				res = tt.expand_query(tpl, q, T, D);
				tt.expand_template(tpl, D);
		}
	} else {
		tt.expand_error(ERROR_TPL, *_TS);
		res = EVHTP_RES_SERVERR;
	}

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	tt.expand_footer();

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	evhtp_send_reply(_ev_req, res);

	if (_trace_on) std::cerr << "cmd_query(" << tagd_code_str(ts_rc) << ") : " <<  res << std::endl;
}

void template_callback::cmd_error() {
	tagd_template tt(_TS, _request, _ev_req, _request->_server->_args->tpl_dir);
	this->add_http_headers();
	tt.expand_header(_driver->last_error().id());
	tt.expand_error(ERROR_TPL, *_driver);
	tt.expand_footer();

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, *_driver);
		ev_send_error_str(_ev_req, tt);
		return;
	}

	std::stringstream ss;
	if (_trace_on)
		_driver->print_errors(ss);

	switch (_driver->code()) {
		case tagd::TS_NOT_FOUND:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(_driver->code()) << "): " << EVHTP_RES_NOTFOUND << " EVHTP_RES_NOTFOUND" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_ev_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(_driver->code()) << "): " << EVHTP_RES_SERVERR << " EVHTP_RES_SERVERR" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_ev_req, EVHTP_RES_SERVERR);
	}
}

void template_callback::empty() {
	tagd_template tt(_TS, _request, _ev_req, _request->_server->_args->tpl_dir);
	this->add_http_headers();
	tt.expand_header("Welcome to tagd");

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	ctemplate::TemplateDictionary D("get");
	int res = tt.expand_home(HOME_TPL, D);

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	tt.expand_template(HOME_TPL, D);

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	tt.expand_footer();

	if (tt.has_error()) {
		ev_send_error_str(_ev_req, tt);
		return;
	}

	evhtp_send_reply(_ev_req, res);

	if (_trace_on) std::cerr << "empty: " <<  res << std::endl;
}

}
