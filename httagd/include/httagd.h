#pragma once

#include <evhtp.h>

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"
#include "tagsh.h"
#include <ctemplate/template.h>

const char* evhtp_res_str(int);

namespace httagd {

class httagd_args : public cmd_args {
	public:
		std::string tpl_dir;
		std::string bind_addr;
		uint16_t bind_port;

		httagd_args () : bind_port{0} {
			_cmds["--tpl-dir"] = {
				[this](char *val) {
						this->tpl_dir = val;
				},
				true
			};

			_cmds["--bind-addr"] = {
				[this](char *val) {
						this->bind_addr = val;
				},
				true
			};

			_cmds["--bind-port"] = {
				[this](char *val) {
						this->bind_port = static_cast<uint16_t>(
								atoi( static_cast<const char*>(val) )
							);
				},
				true
			};
		}
};

void main_cb(evhtp_request_t *, void *);

class tagl_callback;
class request;

class server : public tagsh, public tagd::errorable {
	friend void main_cb(evhtp_request_t *, void *);
	friend class tagl_callback;
	friend class template_callback;
	friend class request;

	protected:
		httagd_args *_args;
		std::string _bind_addr;
		uint16_t _bind_port;	
		evbase_t *_evbase;
		evhtp_t  *_htp;

		void init() {
			_evbase = event_base_new();
			_htp = evhtp_new(_evbase, NULL);	
		}
	public:
		server(tagspace::sqlite *ts)
			: tagsh(ts), _args{nullptr}, _bind_addr{"localhost"}, _bind_port{2112}
		{ this->init(); }

		server(tagspace::sqlite *ts, httagd_args *args)
			: tagsh(ts), _args{args}, _bind_addr{"localhost"}, _bind_port{2112} 
		{
			_bind_addr = (!args->bind_addr.empty() ? args->bind_addr : "localhost");
			_bind_port = (args->bind_port ? args->bind_port : 2112);
			this->init();
		}

		tagd::code start();
};

class tagd_template;

class request {
	friend class tagl_callback;
	friend class template_callback;
	friend class tagd_template;

	protected:
		server *_server;
		evhtp_request_t *_ev_req;
		url_query_map_t _query_map;
		std::string _path; // for testing

	public:
		request(server* S, evhtp_request_t *req)
			: _server{S}, _ev_req{req}
		{
			if ( req->uri->query_raw != NULL ) {
				tagd::url::parse_query(
					_query_map, (char*)req->uri->query_raw );
			}

			_path = _ev_req->uri->path->full;
		}

		request(const std::string &path)	// for testing
			: _server{nullptr}, _ev_req{nullptr}, _path{path} {}

		std::string query_opt(const std::string &opt) const {
			std::string val;
			tagd::url::query_find(_query_map, val, opt);
			return val;
		}

		std::string path() const {
			return _path;
		}

		std::string url() const {

			std::string url;
			switch ( _ev_req->uri->scheme ) {
				case htp_scheme_ftp:
					url.append("ftp://");
					break;
				case htp_scheme_https:
					url.append("https://");
					break;
				case htp_scheme_nfs:
					url.append("nfs://");
					break;
				case htp_scheme_http:
				case htp_scheme_none:
				case htp_scheme_unknown:
				default:
					url.append("http://");
			}

			if ( _ev_req->uri->authority ) {
				if ( _ev_req->uri->authority->username )
					url.append( _ev_req->uri->authority->username );

				if ( _ev_req->uri->authority->password )
					url.append(":").append( _ev_req->uri->authority->password );

				if ( _ev_req->uri->authority->username )
					url.append("@");

				if ( _ev_req->uri->authority->hostname )
					url.append( _ev_req->uri->authority->hostname );

				if ( _ev_req->uri->authority->port && _ev_req->uri->authority->port != 80 )
					url.append( std::to_string(_ev_req->uri->authority->port) );
			} else {
				// TODO not sure this is a good way to do it
				url.append( _server->_bind_addr );
				url.append(":").append( std::to_string(_server->_bind_port) );
			}

			if ( _ev_req->uri->path->full )
				url.append( _ev_req->uri->path->full );

			if ( _ev_req->uri->query_raw )
				url.append("?").append( reinterpret_cast<const char *>(_ev_req->uri->query_raw) );

			if ( _ev_req->uri->fragment )
				url.append("#").append( reinterpret_cast<const char *>(_ev_req->uri->fragment) );

			return url;
		}
};

class httagl;

class htscanner : public TAGL::scanner {
	public:
		htscanner(httagl *d) : TAGL::scanner((TAGL::driver*)d) {}
		void scan_tagdurl_path(int cmd, const request&);
};

class httagl : public TAGL::driver {
	public:
		httagl(tagspace::tagspace *ts)
			: TAGL::driver(ts, new htscanner(this)) {}
		httagl(tagspace::tagspace *ts, TAGL::callback *cb)
			: TAGL::driver(ts, new htscanner(this), cb) {}
		~httagl() { delete _scanner; }
		tagd_code tagdurl_get(const request&);
		tagd_code tagdurl_put(const request&);
		tagd_code tagdurl_del(const request&);
};

class tagl_callback : public TAGL::callback {
	protected:
		tagspace::tagspace *_TS;
		request *_request;
		evhtp_request_t *_ev_req;
		httagd_args *_args;
		bool _trace_on;
		std::string _content_type;

	public:
		tagl_callback(
				tagspace::tagspace* ts,
				request* req
			) : _TS{ts}, _request{req}, _ev_req{req->_ev_req}, _args{req->_server->_args},
				_content_type{"text/plain; charset=utf-8"}
			{
				_trace_on = req->_server->_args->opt_trace;
			}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error();
        virtual void empty();  // welcome message or home page
        void finish();
		void add_http_headers();
};

class template_callback : public tagl_callback, public tagd::errorable {
	public:
		template_callback( tagspace::tagspace* ts, request* req )
				: tagl_callback(ts, req) 
			{
				_content_type = "text/html; charset=utf-8";
			}

		void cmd_get(const tagd::abstract_tag&);
		// void cmd_put(const tagd::abstract_tag&);
		// void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error();
		void empty();
};


/*
 evhtp_request_t structure 

{
    evhtp_t            * htp;         // the parent evhtp_t structure
    evhtp_connection_t * conn;        // the associated connection
    evhtp_hooks_t      * hooks;       // request specific hooks
    evhtp_uri_t        * uri;         // request URI information
	// evhtp_uri_s => evhtp_uri_t URI strucutre
	struct evhtp_uri_s {
		evhtp_authority_t * authority;
		// evhtp_authority_s => evhtp_authority_t
		struct evhtp_authority_s {
			char   * username;                // the username in URI (scheme://USER:..
			char   * password;                // the password in URI (scheme://...:PASS..
			char   * hostname;                // hostname if present in URI
			uint16_t port;                    // port if present in URI
		};

		evhtp_path_t      * path;
		// evhtp_path_s => evhtp_path_t 
		struct evhtp_path_s {
			char       * full;                // the full path+file (/a/b/c.html)
			char       * path;                // the path (/a/b/)
			char       * file;                // the filename if present (c.html)
			char       * match_start;
			char       * match_end;
			unsigned int matched_soff;        // offset of where the uri sta
											//   mainly used for regex matching
											
			unsigned int matched_eoff;        // offset of where the uri e
											//   mainly used for regex matching
		};

		unsigned char     * fragment;     // data after '#' in uri
		unsigned char     * query_raw;    // the unparsed query arguments
		evhtp_query_t     * query;        // list of k/v for query arguments
		htp_scheme          scheme;       // set if a scheme is found
	};


    evbuf_t            * buffer_in;   // buffer containing data from client
    evbuf_t            * buffer_out;  // buffer containing data to client
    evhtp_headers_t    * headers_in;  // headers from client
    evhtp_headers_t    * headers_out; // headers to client
    evhtp_proto          proto;       // HTTP protocol used
    htp_method           method;      // HTTP method used
    evhtp_res            status;      // The HTTP response code or other error conditions
    int                  keepalive;   // set to 1 if the connection is keep-alive
    int                  finished;    // set to 1 if the request is fully processed
    int                  chunked;     // set to 1 if the request is chunked

    evhtp_callback_cb cb;             // the function to call when fully processed
    void            * cbarg;          // argument which is passed to the cb function
    int               error;

    TAILQ_ENTRY(evhtp_request_s) next;
};
*/

typedef enum {
	TPL_HTML,  // general html template
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
				case 'h':
					if (opt_tpl == "html")
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
				const std::string *c = nullptr);

		void set_relator_link (
				const tpl_t& tpl,
				ctemplate::TemplateDictionary* d,
				const std::string& k,
				const std::string& v,
				const std::string *c = nullptr);

		void set_query_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v);
		void set_query_related_link (ctemplate::TemplateDictionary* d, const std::string& k, const std::string& v);
		void expand_template(const tpl_t& tpl, const ctemplate::TemplateDictionary& D);
		void expand_header(const std::string& title);
		void expand_footer();
		int expand_home(const tpl_t& tpl, ctemplate::TemplateDictionary& D);
		int expand_tag(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D);
		int expand_browse(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D);
		int expand_relations(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D);
		int expand_tree(const tpl_t& tpl, const tagd::abstract_tag& t, ctemplate::TemplateDictionary& D, size_t *num_children=nullptr);
		int expand_query(const tpl_t& tpl, const tagd::interrogator& q, const tagd::tag_set& R, ctemplate::TemplateDictionary& D);
		void expand_error(const tpl_t& tpl, const tagd::errorable& E);
};

} // namespace httagd
