#include <cstring>  // tolower, strcmp (for gperf)
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>  // find_last_of

#include "tagd/domain.h"

#include "public-suffix.gperf.h"

namespace tagd {

tld_code domain::process_icann(const std::string &domain, size_t *offset) {
    assert (_tld_code == TLD_ICANN);

    // check if next lable makes it a private tld
    _tld_code = lookup_next_label(domain, offset);

    switch (_tld_code) {
        case TLD_UNKNOWN:
            // have a registrable domain
            _tld_code = TLD_ICANN_REG;
            break;
        case TLD_ICANN:
            _tld_offset = *offset;
            if (_tld_offset == 0) return _tld_code;
            return this->process_icann(domain, offset);
        case TLD_WILDCARD:
            _tld_offset = *offset;
            // only wildcard placeholder, not a tld
            if (*offset == 0) return _tld_code;
            *offset = next_label(domain, *offset);
            _tld_code = TLD_WILDCARD_REG;
            break;
        case TLD_PRIVATE:
            _tld_offset = *offset;

            // not a registrable domain if there is not another label
            // before the private tld label one
            if (*offset == 0) return _tld_code;

            *offset = next_label(domain, *offset);
            _tld_code = TLD_PRIVATE_REG;
            break;
        default:
            assert( false );
            _tld_code = TLD_ERR;
            return _tld_code;  // unknown state;
    }
    
    _reg_offset = *offset;

    assert( this->is_registrable() );

    return _tld_code;
}

tld_code domain::process_wildcard(const std::string &domain, size_t *offset) {
    assert ( _tld_code == TLD_WILDCARD );
    assert( *offset > 0 );

    _tld_code = lookup_next_label(domain, offset);

    switch (_tld_code) {
        case TLD_UNKNOWN:
            _tld_offset = *offset;
            if (*offset == 0) {
                _tld_code = TLD_WILDCARD;
                return _tld_code;
            }
            *offset = next_label(domain, *offset);
            _tld_code = TLD_WILDCARD_REG;
            break;
       case TLD_EXCEPTION_REG:
//            _tld_offset = *offset;
//            if (*offset == 0) return _tld_code;
//            *offset = next_label(domain, *offset);
//            _tld_code = TLD_EXCEPTION_REG;
            break;
        default:
            assert( false );
            _tld_code = TLD_ERR;
            return _tld_code;
    }
    
    _reg_offset = *offset;
    assert( this->is_registrable() );

    return _tld_code;
}

tld_code domain::init(const std::string &domain) {
    // clear upfront since this is an init
    if (!this->empty()) this->clear();

    size_t offset = domain.size();
    if (offset == 0) return _tld_code;  // TLD_UNKNOWN

    if (offset > TLD_MAX_LEN) {
        _tld_code = TLD_MAX_LEN;
        return _tld_code;
    }

    // eat leading dots
    size_t i = 0;
    for (; i < offset && domain[i] == '.'; i++);

    if (i == 0) {
        _domain = domain;
    } else {
        _domain = domain.substr(i);
        offset = _domain.size();
        if (offset == 0) return _tld_code;
    }

    // lowercase
    std::transform(_domain.begin(), _domain.end(), _domain.begin(), tolower);

    // lookup rightmost label then advance offset leftward to next lable
    _tld_code = lookup_next_label(_domain, &offset);

    switch (_tld_code) {
        case TLD_ICANN:
            _tld_offset = offset;
            if (offset == 0) return _tld_code;
            return this->process_icann(_domain, &offset);
        case TLD_UNKNOWN:
            return TLD_UNKNOWN;
        case TLD_WILDCARD:
            _tld_offset = offset;
            // no label to match wildcard
            if (offset == 0) {
                _tld_code = TLD_ICANN; 
                return _tld_code;
            }
            return this->process_wildcard(_domain, &offset);
        default:
            // unknown return value for rightmost label
            assert(false);
            _tld_code = TLD_ERR;
            return TLD_ERR;
    }
}

bool domain::is_registrable() const {
    return (
        !_domain.empty() &&
        _tld_offset >= 0 &&
        _reg_offset >= 0 &&
        (
            _tld_code == TLD_ICANN_REG ||
            _tld_code == TLD_PRIVATE_REG ||
            _tld_code == TLD_WILDCARD_REG ||
            _tld_code == TLD_EXCEPTION_REG
        )
    );
}

// the public suffix of a domain
std::string domain::pub() const {
    if (_tld_offset == -1)
        return std::string();
    else
        return _domain.substr(_tld_offset); 
}

// the private portion of a domain (domain - tld)
// ex: given www.example.com, www.example would be the private portion
std::string domain::priv() const {
    if (_tld_code == TLD_UNKNOWN)
        return _domain;

    if (_tld_offset == -1 || _reg_offset == -1)
        return std::string();

    if (_tld_offset < 2) // shortest ex: a.com 
        return _domain;
    else
        return _domain.substr(0, (_tld_offset - 1));
}

// the subdomain(s) portion minus the registerable domain
// ex: given www.news.example.com, www.news, would be the subdomain portion
std::string domain::sub() const {
    std::string priv = this->priv();
    if (priv.empty()) return std::string();

    size_t last_dot_pos =  priv.find_last_of('.');
    if (last_dot_pos == std::string::npos) return std::string();

    return _domain.substr(0, last_dot_pos);
}

// only the private label of a domain
std::string domain::priv_label() const {
    std::string priv = this->priv();
    if (priv.empty()) return std::string();

    size_t last_dot_pos =  priv.find_last_of('.');
    if (last_dot_pos == std::string::npos) return priv;

    return priv.substr(last_dot_pos + 1);
}

// registrable domain
std::string domain::reg() const {
    if (!this->is_registrable())
        return std::string();
    else
        return _domain.substr(_reg_offset); 
}

void domain::clear() {
    _domain.clear();
    _tld_offset = _reg_offset = -1;
    _tld_code = TLD_UNKNOWN;
}

bool domain::empty() const {
    return (
        _domain.empty() &&
        _tld_offset == -1 &&
        _reg_offset == -1 &&
        _tld_code == TLD_UNKNOWN
    );    
}

// sets offset to beginning of next label (from the right)
size_t domain::next_label(const std::string &domain, size_t offset) {
    if (offset == 0) return 0;

    // offset may be pointing to a label after a dot (say 'com'):
    // www.example.com
    //             ^
    // we must 'jump over' the dot
    if (offset > 1 && domain[offset-1] == '.')
        offset--;

    while (--offset > 0) {
        if (domain[offset] == '.') {
            offset++;
            break;
        }
    }

    return offset;
}

// looks up next label (starting from right) and returns lookup code
// sets offset to beginning of label looked up
tld_code domain::lookup_next_label(const std::string &domain, size_t *offset) {
    *offset = next_label(domain, *offset);

    std::string label(domain.substr(*offset));
    hash_value *val = tld_hash::lookup(label.c_str(), label.size());

    //std::cout << "tld_hash::lookup( " << label.c_str() << " ) => "
    //          << (val == NULL ? "TLD_UNKNOWN" : tld_code_str(val->code)) << std::endl;

    if (val == NULL)
        return TLD_UNKNOWN;
    else
        return val->code;
}

} // namespace tagd
