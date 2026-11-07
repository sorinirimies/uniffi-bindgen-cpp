//! Identifier handling: C++ keywords, casing, templating helpers.

const KEYWORDS: &[&str] = &[
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "atomic_cancel",
    "atomic_commit",
    "atomic_noexcept",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char8_t",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const",
    "consteval",
    "constexpr",
    "constinit",
    "const_cast",
    "continue",
    "co_await",
    "co_return",
    "co_yield",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "reflexpr",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "synchronized",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
    // Macros and names the generated code relies on.
    "NULL",
    "EOF",
    "errno",
    "assert",
    "major",
    "minor",
    "uniffi",
];

/// Appends `_` to names that are C++ keywords (or would clash with generated code).
pub(crate) fn escape(name: &str) -> String {
    if KEYWORDS.contains(&name) {
        format!("{name}_")
    } else {
        name.to_owned()
    }
}

/// A function, method, argument or field name. Names are kept as written in the
/// Rust source (or UDL); only C++ keywords are escaped.
pub(crate) fn var(name: &str) -> String {
    escape(name)
}

/// A type, enumerator or variant name. Kept as written, like [`var`].
pub(crate) fn class(name: &str) -> String {
    escape(name)
}

/// Field name inside a variant: nameless fields become `v0`, `v1`, ...
pub(crate) fn field(name: &str, index: usize) -> String {
    if name.is_empty() {
        format!("v{index}")
    } else {
        var(name)
    }
}

/// Substitute `@key@` tokens in `template`.
pub(crate) fn fill(template: &str, values: &[(&str, &str)]) -> String {
    let mut out = template.to_owned();
    for (key, value) in values {
        out = out.replace(&format!("@{key}@"), value);
    }
    out
}

/// Quote `s` as a C++ string literal.
pub(crate) fn string_literal(s: &str) -> String {
    let mut out = String::from("\"");
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\x{:02x}\"\"", c as u32)),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

/// Format a doc comment, indented by `indent` spaces.
pub(crate) fn doc(docstring: Option<&str>, indent: usize) -> String {
    let Some(text) = docstring else {
        return String::new();
    };
    let pad = " ".repeat(indent);
    let mut out = String::new();
    for line in text.trim_end().lines() {
        let line = line.strip_prefix(' ').unwrap_or(line);
        out.push_str(&format!("{pad}/// {}\n", line.trim_end()));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn var_keyword_gets_trailing_underscore() {
        assert_eq!(var("new"), "new_");
        assert_eq!(var("delete"), "delete_");
        assert_eq!(var("connect"), "connect");
        assert_eq!(class("Class"), "Class");
        assert_eq!(var("uniffi"), "uniffi_");
    }

    #[test]
    fn field_without_name_uses_its_index() {
        assert_eq!(field("", 0), "v0");
        assert_eq!(field("client_id", 3), "client_id");
    }

    #[test]
    fn string_literal_escapes_quotes_backslashes_and_newlines() {
        assert_eq!(string_literal("a\"b\\c\n"), r#""a\"b\\c\n""#);
    }

    #[test]
    fn fill_replaces_every_key() {
        assert_eq!(fill("a @x@ b @y@", &[("x", "1"), ("y", "2")]), "a 1 b 2");
    }
}
