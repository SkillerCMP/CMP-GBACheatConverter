#include "test_support.hpp"

namespace gba::tests {

void test_xploder_encrypted_roundtrip() {
    const std::string input =
        "Xploder Round Trip:\n"
        "82025BC4 423F\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";

    gba::xploder::ExportOptions encrypted_options;
    encrypted_options.encrypted = true;
    encrypted_options.seed = gba::xploder::Seed{0x9ABCDEF0U, 0x1234U};

    const auto raw_document = gba::xploder::parse(input, {false});
    const auto encrypted =
        gba::xploder::export_document(raw_document, encrypted_options);
    require(encrypted.success,
            "Xploder encrypted export unexpectedly failed");
    require(encrypted.text.find("9ABCDEF0 1234") != std::string::npos,
            "Xploder encrypted output did not include its seed line");

    const auto decrypted_document =
        gba::xploder::parse(encrypted.text, {true});
    const auto raw_again =
        gba::xploder::export_document(decrypted_document, {false, std::nullopt});

    require(raw_again.text.find("82025BC4 423F") != std::string::npos &&
            raw_again.text.find("72024C10 9EB7") != std::string::npos &&
            raw_again.text.find("32024C10 00C2") != std::string::npos,
            "Xploder encrypted roundtrip failed");
}

void test_codebreaker_to_xploder_raw() {
    const std::string input =
        "Shared FCD Format:\n"
        "82025BC4 423F\n"
        "82025BC6 000F\n";

    const auto document = gba::codebreaker::parse(input, {false});
    const auto output = gba::xploder::export_document(document, {false, std::nullopt});

    require(output.text.find("82025BC4 423F") != std::string::npos &&
            output.text.find("82025BC6 000F") != std::string::npos,
            "CodeBreaker operations did not encode as Xploder raw");
}

} // namespace gba::tests
