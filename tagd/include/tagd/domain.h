#pragma once

#include <iostream>
#include <sstream>

/*
Public Suffix List TLD Code Examples:

rule        domain          tld_code
----        ------          --------
com         com             TLD_ICANN
com         example.com     TLD_ICANN_REG

*.uk        uk              TLD_ICANN
*.uk        co.uk           TLD_WILDCARD
*.uk        example.co.uk   TLD_WILDCARD_REG

-- exceptions are registered by definition
!nhs.uk     nhs.uk          TLD_EXCEPTION_REG 
!nhs.uk     example.nhs.uk  TLD_EXCEPTION_REG

uk.com      uk.com          TLD_PRIVATE
uk.com      example.uk.com  TLD_PRIVATE_REG

<NULL>      local           TLD_UNKNOWN
<NULL>      example.local   TLD_UNKNOWN
*/
typedef enum {
    TLD_ICANN,
    TLD_ICANN_REG,
    TLD_WILDCARD,  // tld requires a wildcard, but domain given does not match it
    TLD_WILDCARD_REG,  // wildcard tld and matching label and registrable
    TLD_EXCEPTION_REG,
    TLD_PRIVATE,
    TLD_PRIVATE_REG,
    TLD_UNKNOWN,
    TLD_ERR,  // internal error - unexpected state

// borrowed from http://www.iss.net/security_center/reference/vuln/DNS_Hostname_Overflow_Verylong.htm
    TLD_MAX_LEN = 900
} tld_code;
// changes to enum need to be reflected in tld_code_str

namespace tagd {

class domain {
    private:
        int _tld_offset;
        int _reg_offset; // registrable domain (not saying it is)
        std::string _domain;
        tld_code _tld_code; // code from init

        tld_code process_icann(const std::string &domain, size_t *offset);
        tld_code process_wildcard(const std::string &domain, size_t *offset);

/*
        void debug_stat(const char *entry) {
            std::cout << ">> " << entry << ": " << std::endl;
            std::cout << "    _domain: " << _domain << std::endl
                      << "_tld_offset: " << _tld_offset << std::endl
                      << "_reg_offset: " << _reg_offset << std::endl
                      << "  _tld_code: " << tld_code_str(_tld_code) << std::endl;
        }
*/

    public:
        domain() : _tld_offset(-1), _reg_offset(-1), _domain(), _tld_code(TLD_UNKNOWN) {}

        domain(const std::string& domain)
            : _tld_offset(-1), _reg_offset(-1), _domain(), _tld_code(TLD_UNKNOWN)
        { this->init(domain); }

        tld_code init(const std::string &domain);

        tld_code code() const { return _tld_code; }

        bool is_registrable() const;

        // will be empty after an unsuccessful init
        // regardless of the domain passed into init
        std::string str() const { return _domain; }

        // the public suffix of a domain
        std::string pub() const;

        // the private portion of a domain (domain - tld)
        // ex: given www.example.com, www.example would be the private portion
        std::string priv() const;

        // only the topmost private label
        // ex: given www.example.com, example would be the private label
        std::string priv_label() const;

        // the subdomain(s) portion minus the registerable label
        // ex: given www.news.example.com, www.news, would be the subdomain portion
        std::string sub() const;

        // registrable domain
        std::string reg() const;

        void clear();

        bool empty() const;

        // sets offset to beginning of next label (from the right)
        static size_t next_label(const std::string &domain, size_t offset);

        // looks up next label (starting from right) and returns lookup code
        // sets offset to beginning of label looked up
        static tld_code lookup_next_label(const std::string &domain, size_t *offset);

    public:
        static std::string tld_code_str(tld_code c) {
            switch (c) {
                case TLD_ICANN:     return "TLD_ICANN";
                case TLD_ICANN_REG: return "TLD_ICANN_REG";
                case TLD_WILDCARD:  return "TLD_WILDCARD";
                case TLD_WILDCARD_REG: return "TLD_WILDCARD_REG";
                case TLD_EXCEPTION_REG: return "TLD_EXCEPTION_REG";
                case TLD_PRIVATE:   return "TLD_PRIVATE";
                case TLD_PRIVATE_REG: return "TLD_PRIVATE_REG";
                case TLD_UNKNOWN:   return "TLD_UNKNOWN";
                case TLD_ERR:       return "TLD_ERR";
                case TLD_MAX_LEN:   return "TLD_MAX_LEN";
                default:            return "STR_UNKNOWN";
            }
        }

        // reverses the labels in a domain (not the chars)
        // ex: www.example.com  => com.example.www
        // ex: com.example.www  => www.example.com
        static std::string reverse_labels(const std::string& labels) {
            if (labels.empty()) return std::string();

            std::stringstream ss;

            size_t j = labels.size();
            for (size_t i = labels.size(); i > 0; i--) {
                if (labels[i] == '.') {
                    ss << labels.substr((i + 1), (j - (i + 1))) << '.';
                    j = i;
                }
            }

            if (j == labels.size())  // no dots
                return labels;

            ss << labels.substr(0, j);
            return ss.str();
        }
};

#define tld_code_str(c) tagd::domain::tld_code_str(c)
#define reverse_labels(l) tagd::domain::reverse_labels(l)

} // namespace tagd
