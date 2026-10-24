#pragma once
#include <cxxtest/TestSuite.h>
#include "tagd/magic.h"
#include "tagd/sha256.h"
#include <cstdlib>

class MagicTester : public CxxTest::TestSuite {
public:
	void test_hash_vectors_and_chunks() {
		for (size_t chunk : {1u, 2u, 3u}) {
			tagd::sha256 hash;
			std::string data = "abc", digest;
			for (size_t at = 0; at < data.size(); at += chunk) {
				TS_ASSERT_EQUALS(hash.update(std::string_view(data).substr(at, chunk)), tagd::TAGD_OK);
			}
			TS_ASSERT_EQUALS(hash.finish(digest), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
			TS_ASSERT_EQUALS(hash.update("x"), tagd::TS_MISUSE);
		}
		tagd::sha256 hash;
		std::string digest;
		TS_ASSERT_EQUALS(hash.finish(digest), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(digest, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	}

	void test_bounded_sample_and_chunk_independence() {
		tagd::magic detector("fixtures/magic/late.magic");
		const std::string input = std::string(70000, '\0') + "TAGD_LATE";
		for (size_t chunk : {1u, 4096u, 65536u}) {
			std::string sample;
			for (size_t at = 0; at < input.size() && sample.size() < tagd::filepile_sample_limit;
				at += chunk) {
				sample.append(input, at, std::min(chunk, tagd::filepile_sample_limit - sample.size()));
			}
			tagd::magic_result result;
			TS_ASSERT_EQUALS(detector.detect(result, sample), tagd::TAGD_OK);
			TS_ASSERT_DIFFERS(result.media_type, "application/x-tagd-late");
			TS_ASSERT_EQUALS(result.bytes_examined, tagd::filepile_sample_limit);
		}
		tagd::magic_result full;
		TS_ASSERT_EQUALS(detector.detect(full, input), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(full.media_type, "application/x-tagd-late");
	}

	void test_detection_and_mapping() {
		setenv("MAGIC", "/nonexistent/environment-magic", 1);
		tagd::magic detector;
		tagd::magic_result result;
		TS_ASSERT_EQUALS(detector.detect(result, ""), tagd::TAGD_OK);
		TS_ASSERT(result.empty);
		TS_ASSERT_EQUALS(result.media_type, "application/octet-stream");
		TS_ASSERT_EQUALS(detector.detect(result, "plain ascii text\n"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(result.media_type, "text/plain");
		TS_ASSERT_EQUALS(result.bytes_examined, 17u);
		TS_ASSERT_EQUALS(tagd::canonical_extension("image/jpeg"), "jpg");
		TS_ASSERT_EQUALS(tagd::canonical_extension("application/octet-stream"), "bin");
		TS_ASSERT_EQUALS(tagd::canonical_extension("unknown/type"), "bin");
		TS_ASSERT_EQUALS(tagd::canonical_extension("IMAGE/SVG+XML"), "svg");
		TS_ASSERT(tagd::valid_media_type("application/vnd.test+json"));
		TS_ASSERT(!tagd::valid_media_type("text/plain\n_has injected"));
		TS_ASSERT(!tagd::valid_media_type("text/plain; charset=utf-8"));
		unsetenv("MAGIC");
		tagd::magic missing("/nonexistent/magic.mgc");
		TS_ASSERT_EQUALS(missing.detect(result, "abc"), tagd::TS_INTERNAL_ERR);
		TS_ASSERT(result.media_type.empty());
	}
};
