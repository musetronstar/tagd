#include "tagd/magic.h"
#include "tagd/sha256.h"
#include <magic.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iterator>

static std::string read(const std::string& path) {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		throw std::runtime_error("cannot read fixture/database: " + path);
	}
	return {std::istreambuf_iterator<char>(stream), {}};
}

int main() {
	try {
		std::istringstream manifest(read("fixtures/magic/manifest.tsv"));
		std::cout << "fixture\texpected\tdetected\tsample_bytes\tfalse_positive\tfalse_negative"
			<< "\tlibmagic\tdatabase_sha256\n";
		std::string row;
		while (std::getline(manifest, row)) {
			std::istringstream fields(row);
			std::string name, expected, custom;
			std::getline(fields, name, '\t');
			std::getline(fields, expected, '\t');
			std::getline(fields, custom, '\t');
			const auto database = custom.empty() ? "/usr/share/misc/magic.mgc"
				: "fixtures/magic/" + custom;
			tagd::sha256 hash;
			std::string identity;
			if (hash.update(read(database)) != tagd::TAGD_OK
				|| hash.finish(identity) != tagd::TAGD_OK) {
				return 1;
			}
			tagd::magic detector(database);
			const auto input = read("fixtures/magic/" + name);
			for (size_t kib : {8u, 16u, 32u, 64u, 128u, 256u}) {
				tagd::magic_result result;
				if (detector.detect(result, std::string_view(input).substr(0, kib * 1024))
					!= tagd::TAGD_OK) {
					detector.print_errors();
					return 1;
				}
				const bool wrong = result.media_type != expected;
				std::cout << name << '\t' << expected << '\t' << result.media_type << '\t'
					<< result.bytes_examined << '\t'
					<< (wrong && result.media_type != "application/octet-stream") << '\t'
					<< (wrong && expected != "application/octet-stream") << '\t'
					<< magic_version() << '\t' << identity << '\n';
			}
		}
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
