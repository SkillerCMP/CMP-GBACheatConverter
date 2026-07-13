#include "test_support.hpp"

namespace gba::tests {

void test_release_direct_write_matrix() {
    gba::CheatDocument source;
    gba::CheatEntry entry;
    entry.name = "Release Matrix";

    const auto append_write = [&](std::uint32_t address,
                                  std::uint32_t value,
                                  std::uint8_t width) {
        gba::Operation operation;
        operation.kind = gba::OperationKind::Write;
        operation.address = address;
        operation.value = value;
        operation.width = width;
        entry.operations.push_back(operation);
    };

    append_write(0x02001000U, 0x12U, 1U);
    append_write(0x02001002U, 0x3456U, 2U);
    append_write(0x02001004U, 0x89ABCDEFU, 4U);
    source.entries.push_back(entry);

    const ByteMap expected = direct_write_bytes(source);
    const auto verify = [&](const gba::CheatDocument& reparsed,
                            const std::string& format) {
        require(direct_write_bytes(reparsed) == expected,
                format + " direct-write semantic roundtrip failed");
    };

    {
        gba::codebreaker::ExportOptions options;
        options.encrypted = false;
        const auto output = gba::codebreaker::export_document(source, options);
        require(output.success, "CodeBreaker raw matrix export failed");
        verify(gba::codebreaker::parse(output.text, {false}),
               "CodeBreaker raw");
    }
    {
        gba::codebreaker::ExportOptions options;
        options.encrypted = true;
        options.seed = gba::codebreaker::Seed{0x9ABCDEF0U, 0x1234U};
        const auto output = gba::codebreaker::export_document(source, options);
        require(output.success, "CodeBreaker encrypted matrix export failed");
        verify(gba::codebreaker::parse(output.text, {true}),
               "CodeBreaker encrypted");
    }
    {
        gba::xploder::ExportOptions options;
        options.encrypted = false;
        const auto output = gba::xploder::export_document(source, options);
        require(output.success, "Xploder raw matrix export failed");
        verify(gba::xploder::parse(output.text, {false}), "Xploder raw");
    }
    {
        gba::xploder::ExportOptions options;
        options.encrypted = true;
        options.seed = gba::xploder::Seed{0x9ABCDEF0U, 0x1234U};
        const auto output = gba::xploder::export_document(source, options);
        require(output.success, "Xploder encrypted matrix export failed");
        verify(gba::xploder::parse(output.text, {true}),
               "Xploder encrypted");
    }
    {
        const auto output = gba::gameshark::export_document(source, {false});
        require(output.success, "GameShark raw matrix export failed");
        verify(gba::gameshark::parse(output.text, {false}),
               "GameShark raw");
    }
    {
        const auto output = gba::gameshark::export_document(source, {true});
        require(output.success, "GameShark encrypted matrix export failed");
        verify(gba::gameshark::parse(output.text, {true}),
               "GameShark encrypted");
    }
    {
        const auto output = gba::armax::export_document(source, {false});
        require(output.success, "AR MAX raw matrix export failed");
        verify(gba::armax::parse(output.text, {false}), "AR MAX raw");
    }
    {
        const auto output = gba::armax::export_document(source, {true});
        require(output.success, "AR MAX encrypted matrix export failed");
        verify(gba::armax::parse(output.text, {true}),
               "AR MAX encrypted");
    }
    {
        gba::ezflash::Options options;
        options.mode = gba::ezflash::Mode::Original;
        const auto output = gba::ezflash::export_document(source, options);
        require(output.success, "EZ-Flash Original matrix export failed");
        verify(gba::ezflash::parse(output.text), "EZ-Flash Original");
    }
    {
        gba::ezflash::Options options;
        options.mode = gba::ezflash::Mode::Enhanced;
        const auto output = gba::ezflash::export_document(source, options);
        require(output.success, "EZ-Flash Cheat MOD matrix export failed");
        verify(gba::ezflash::parse(output.text), "EZ-Flash Cheat MOD");
    }
}

void test_tea_roundtrip() {
    const auto encrypted = gba::crypto::tea_encrypt(
        0x04224EA4U, 0x13371337U,
        gba::crypto::ProActionReplayV3Key);
    const auto decrypted = gba::crypto::tea_decrypt(
        encrypted.first, encrypted.second,
        gba::crypto::ProActionReplayV3Key);

    require(decrypted.first == 0x04224EA4U &&
            decrypted.second == 0x13371337U,
            "PARv3 TEA roundtrip failed");
}

} // namespace gba::tests
