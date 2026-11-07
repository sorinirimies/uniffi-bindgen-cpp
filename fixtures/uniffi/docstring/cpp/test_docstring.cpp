// Port of uniffi-rs v0.31.2 fixtures/docstring/tests/bindings/test_docstring.py
// C++ has no run-time docstrings, so this checks the generated header's comments.
#include <fstream>
#include <sstream>

#include "check.hpp"
#include "uniffi_docstring.hpp"

int main() {
    std::ifstream file(UNIFFI_GENERATED_DIR "/uniffi_docstring.hpp");
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string header = buffer.str();
    for (const char* doc : {
             "<docstring-namespace>", "<docstring-function>", "<docstring-multiline-function>",
             "<second-line>", "<docstring-enum>", "<docstring-enum-variant>",
             "<docstring-associated-enum>", "<docstring-associated-enum-variant>", "<docstring-error>",
             "<docstring-error-variant>", "<docstring-associated-error>",
             "<docstring-associated-error-variant>", "<docstring-object>",
             "<docstring-primary-constructor>", "<docstring-alternate-constructor>", "<docstring-method>",
             "<docstring-record>", "<docstring-record-field>", "<docstring-callback>",
             "<docstring-callback-method>",
         }) {
        CHECK(header.find(std::string("/// ") + doc) != std::string::npos);
        if (header.find(doc) == std::string::npos) {
            std::fprintf(stderr, "missing %s\n", doc);
        }
    }
    uniffi_docstring::test();
    return check::report();
}
