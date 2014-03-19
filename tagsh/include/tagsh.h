#pragma once

// TODO use a template
typedef tagspace::sqlite space_type;

class tagsh_callback : public TAGL::callback {
		space_type *_TS;

	public:
		tagsh_callback(space_type*);
		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error();
};


class tagsh {
	protected:
		space_type *_TS;
		TAGL::callback *_CB;
		TAGL::driver _driver;
	public:
		std::string prompt;
		tagsh(space_type*, TAGL::callback*);
		void command(const std::string&);
		int interpret(std::istream&);
		int interpret(const std::string&);  // tagl statement
		int interpret_fname(const std::string&);  // filename
		void dump_file(const std::string&, bool = true);
		static int error(const char *errfmt, ...);
		static bool file_exists(const std::string&);
		static void cmd_show();
};

