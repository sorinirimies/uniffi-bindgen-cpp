//! Methods, constructors and uniffi traits of value types (records and enums),
//! and the trait members objects share with them.

use std::fmt::Write;

use uniffi_bindgen::interface::{Constructor, Enum, Method, Record, UniffiTraitMethods};

use super::names::{class, field, member};
use super::types::Gen;

/// How a value type is spelled in C++, which decides where its members go.
#[derive(Clone, Copy, PartialEq, Eq)]
pub(crate) enum ValueKind {
    /// A `struct` with fields.
    Record,
    /// A `struct` holding a `std::variant`.
    DataEnum,
    /// An `enum class`: it cannot have members, so its methods are free functions.
    FlatEnum,
    /// An exception class hierarchy; members go on the base class.
    Error,
}

/// A record or enum, with what it exports besides its data.
pub(crate) struct ValueType<'a> {
    pub(crate) name: &'a str,
    pub(crate) kind: ValueKind,
    pub(crate) constructors: &'a [Constructor],
    pub(crate) methods: &'a [Method],
    pub(crate) traits: UniffiTraitMethods,
    /// Member names the data already uses; methods must not clash with them.
    data_members: Vec<String>,
}

impl ValueType<'_> {
    fn has_members(&self) -> bool {
        !self.constructors.is_empty() || !self.methods.is_empty() || has_traits(&self.traits)
    }
}

fn has_traits(t: &UniffiTraitMethods) -> bool {
    t.display_fmt.is_some()
        || t.debug_fmt.is_some()
        || t.hash_hash.is_some()
        || t.eq_eq.is_some()
        || t.ord_cmp.is_some()
}

/// Declarations of the trait members of class `name` (`to_string`, `==`, ...).
pub(crate) fn trait_declarations(name: &str, traits: &UniffiTraitMethods) -> String {
    let mut out = String::new();
    if traits.display_fmt.is_some() {
        out.push_str(
            "    /// The Rust `Display` implementation.\n    std::string to_string() const;\n",
        );
    }
    if traits.debug_fmt.is_some() {
        out.push_str(
            "    /// The Rust `Debug` implementation.\n    std::string debug_string() const;\n",
        );
    }
    if traits.hash_hash.is_some() {
        out.push_str("    /// The Rust `Hash` implementation.\n    uint64_t hash() const;\n");
    }
    if traits.eq_eq.is_some() {
        let _ = writeln!(
            out,
            "    /// The Rust `Eq` implementation.\n    bool operator==(const {name}& other) const;\n    bool operator!=(const {name}& other) const {{ return !(*this == other); }}"
        );
    }
    if traits.ord_cmp.is_some() {
        let _ = writeln!(
            out,
            "    /// The Rust `Ord` implementation: negative, zero or positive.\n    int8_t compare(const {name}& other) const;\n    bool operator<(const {name}& other) const {{ return compare(other) < 0; }}\n    bool operator>(const {name}& other) const {{ return compare(other) > 0; }}\n    bool operator<=(const {name}& other) const {{ return compare(other) <= 0; }}\n    bool operator>=(const {name}& other) const {{ return compare(other) >= 0; }}"
        );
    }
    out
}

impl Gen<'_> {
    /// Every record and enum this component defines.
    pub(crate) fn value_types(&self) -> Vec<ValueType<'_>> {
        let records = self
            .ci
            .record_definitions()
            .iter()
            .map(|r| self.record_value(r));
        let enums = self
            .ci
            .enum_definitions()
            .iter()
            .map(|e| self.enum_value(e));
        records.chain(enums).collect()
    }

    fn record_value<'r>(&self, r: &'r Record) -> ValueType<'r> {
        ValueType {
            name: r.name(),
            kind: ValueKind::Record,
            constructors: r.constructors(),
            methods: r.methods(),
            traits: r.uniffi_trait_methods(),
            data_members: r
                .fields()
                .iter()
                .map(|f| member(f.name(), r.name()))
                .collect(),
        }
    }

    fn enum_value<'e>(&self, e: &'e Enum) -> ValueType<'e> {
        let kind = if self.is_error_enum(e.name()) {
            ValueKind::Error
        } else if e.is_flat() {
            ValueKind::FlatEnum
        } else {
            ValueKind::DataEnum
        };
        let data_members = match kind {
            ValueKind::DataEnum => {
                let mut names: Vec<String> = e
                    .variants()
                    .iter()
                    .map(|v| member(v.name(), e.name()))
                    .collect();
                names.push("variant".into());
                names
            }
            _ => Vec::new(),
        };
        ValueType {
            name: e.name(),
            kind,
            constructors: e.constructors(),
            methods: e.methods(),
            traits: e.uniffi_trait_methods(),
            data_members,
        }
    }

    pub(crate) fn value_type(&self, name: &str) -> Option<ValueType<'_>> {
        self.value_types().into_iter().find(|v| v.name == name)
    }

    /// The C++ name of a method or constructor of `v`.
    fn value_method_name(&self, v: &ValueType<'_>, name: &str) -> String {
        let n = member(name, v.name);
        if v.data_members.contains(&n) {
            format!("{n}_")
        } else {
            n
        }
    }

    /// Lowers the value `expr` of `v` for passing to Rust.
    fn lower_value(&self, v: &ValueType<'_>, expr: &str) -> String {
        match v.kind {
            ValueKind::Error => format!("uniffi_detail::lower_error({expr})"),
            _ => format!("uniffi_detail::lower_buf<{}>({expr})", self.local(v.name)),
        }
    }

    /// Member declarations to put in the class of `v` (not for flat enums).
    pub(crate) fn value_member_declarations(&self, v: &ValueType<'_>) -> String {
        let name = class(v.name);
        let mut out = String::new();
        if v.has_members() {
            out.push('\n');
        }
        for c in v.constructors {
            let _ = writeln!(
                out,
                "    static {};",
                self.signature(c, &self.value_method_name(v, c.name()), true)
            );
        }
        for m in v.methods {
            let _ = writeln!(
                out,
                "    {} const;",
                self.signature(m, &self.value_method_name(v, m.name()), true)
            );
        }
        out.push_str(&trait_declarations(&name, &v.traits));
        if v.traits.eq_eq.is_none() && matches!(v.kind, ValueKind::Record | ValueKind::DataEnum) {
            let _ = writeln!(
                out,
                "\n    /// Compares the values field by field.\n    bool operator==(const {name}& other) const;\n    bool operator!=(const {name}& other) const {{ return !(*this == other); }}"
            );
        }
        out
    }

    /// Free functions standing in for the members of a flat enum.
    pub(crate) fn flat_enum_function_declarations(&self, v: &ValueType<'_>) -> String {
        let mut out = String::new();
        let e = self.local(v.name);
        for c in v.constructors {
            let name = format!("{}_{}", class(v.name), member(c.name(), v.name));
            let _ = writeln!(out, "{};", self.signature(c, &name, true));
        }
        for m in v.methods {
            let _ = writeln!(out, "{};", self.flat_method_signature(m, &e, true));
        }
        if v.traits.display_fmt.is_some() {
            let _ = writeln!(
                out,
                "/// The Rust `Display` implementation.\nstd::string to_string({e} value);"
            );
        }
        if v.traits.debug_fmt.is_some() {
            let _ = writeln!(
                out,
                "/// The Rust `Debug` implementation.\nstd::string debug_string({e} value);"
            );
        }
        if v.traits.hash_hash.is_some() {
            let _ = writeln!(
                out,
                "/// The Rust `Hash` implementation.\nuint64_t hash({e} value);"
            );
        }
        if !out.is_empty() {
            out.push('\n');
        }
        out
    }

    /// `R method(E self_, args)`: a flat enum's method as a free function.
    fn flat_method_signature(&self, m: &Method, e: &str, with_defaults: bool) -> String {
        let params = self.param_list(m, with_defaults);
        let sep = if params.is_empty() { "" } else { ", " };
        format!(
            "{} {}({e} self_{sep}{params})",
            self.return_type_of(m),
            member(m.name(), "")
        )
    }

    /// Inline definitions that need every type complete: field-wise `==` and
    /// `operator<<` for types with `Display`.
    pub(crate) fn value_inline_definitions(&self) -> String {
        let mut out = String::new();
        for v in self.value_types() {
            let name = class(v.name);
            if v.traits.eq_eq.is_none() {
                match v.kind {
                    ValueKind::Record => {
                        let r = self.ci.get_record_definition(v.name);
                        let fields: Vec<String> = r
                            .map(|r| {
                                r.fields()
                                    .iter()
                                    .map(|f| member(f.name(), r.name()))
                                    .collect()
                            })
                            .unwrap_or_default();
                        let _ = writeln!(
                            out,
                            "inline bool {name}::operator==(const {name}& uniffi_other) const {{\n    {}\n}}\n",
                            fieldwise(&fields)
                        );
                    }
                    ValueKind::DataEnum => {
                        if let Some(e) = self.ci.get_enum_definition(v.name) {
                            for variant in e.variants() {
                                let vname = member(variant.name(), e.name());
                                let fields: Vec<String> = variant
                                    .fields()
                                    .iter()
                                    .enumerate()
                                    .map(|(i, f)| member(&field(f.name(), i), &vname))
                                    .collect();
                                let _ = writeln!(
                                    out,
                                    "inline bool {name}::{vname}::operator==(const {vname}& uniffi_other) const {{\n    {}\n}}\n",
                                    fieldwise(&fields)
                                );
                            }
                        }
                        let _ = writeln!(
                            out,
                            "inline bool {name}::operator==(const {name}& uniffi_other) const {{\n    return variant == uniffi_other.variant;\n}}\n"
                        );
                    }
                    _ => {}
                }
            }
            if v.traits.display_fmt.is_some() {
                let param = if v.kind == ValueKind::FlatEnum {
                    name.clone()
                } else {
                    format!("const {name}&")
                };
                let call = if v.kind == ValueKind::FlatEnum {
                    "to_string(value)"
                } else {
                    "value.to_string()"
                };
                let _ = writeln!(
                    out,
                    "inline std::ostream& operator<<(std::ostream& stream, {param} value) {{\n    return stream << {call};\n}}\n"
                );
            }
        }
        for o in self.ci.object_definitions() {
            if o.uniffi_trait_methods().display_fmt.is_some()
                && !(o.is_trait_interface() || o.has_callback_interface())
            {
                let name = class(o.name());
                let _ = writeln!(
                    out,
                    "inline std::ostream& operator<<(std::ostream& stream, const {name}& value) {{\n    return stream << value.to_string();\n}}\n"
                );
            }
        }
        out
    }

    /// `std::hash` for records and enums that export `Hash`.
    pub(crate) fn value_std_hash(&self) -> String {
        let mut out = String::new();
        for v in self.value_types() {
            if v.traits.hash_hash.is_none()
                || !matches!(v.kind, ValueKind::Record | ValueKind::DataEnum)
            {
                continue;
            }
            let full = self.local(v.name);
            let _ = writeln!(
                out,
                "template <>\nstruct hash<{full}> {{\n    size_t operator()(const {full}& value) const {{ return static_cast<size_t>(value.hash()); }}\n}};\n"
            );
        }
        if out.is_empty() {
            return out;
        }
        format!("namespace std {{\n\n{out}}}  // namespace std\n\n")
    }

    /// Out-of-class definitions of the members of every value type.
    pub(crate) fn value_member_definitions(&self) -> String {
        let mut out = String::new();
        for v in self.value_types() {
            if v.kind == ValueKind::FlatEnum {
                out.push_str(&self.flat_enum_definitions(&v));
                continue;
            }
            let name = class(v.name);
            let this = self.lower_value(&v, "*this");
            for c in v.constructors {
                let _ = writeln!(
                    out,
                    "{} {{\n{}}}\n",
                    self.qualified_signature(c, &name, &self.value_method_name(&v, c.name())),
                    self.call_body(c, None, None, 4)
                );
            }
            for m in v.methods {
                let _ = writeln!(
                    out,
                    "{} const {{\n{}}}\n",
                    self.qualified_signature(m, &name, &self.value_method_name(&v, m.name())),
                    self.call_body(m, Some(&this), None, 4)
                );
            }
            out.push_str(&self.trait_definitions(
                &name,
                &v.traits,
                &this,
                &self.lower_value(&v, "other"),
            ));
            if v.kind == ValueKind::Error && v.traits.display_fmt.is_some() {
                out.push_str(&what_from_display(&name));
            }
        }
        out
    }

    fn flat_enum_definitions(&self, v: &ValueType<'_>) -> String {
        let mut out = String::new();
        let e = self.local(v.name);
        let this = self.lower_value(v, "self_");
        for c in v.constructors {
            let name = format!("{}_{}", class(v.name), member(c.name(), v.name));
            let _ = writeln!(
                out,
                "{} {{\n{}}}\n",
                self.signature(c, &name, false),
                self.call_body(c, None, None, 4)
            );
        }
        for m in v.methods {
            let _ = writeln!(
                out,
                "{} {{\n{}}}\n",
                self.flat_method_signature(m, &e, false),
                self.call_body(m, Some(&this), None, 4)
            );
        }
        let value = self.lower_value(v, "value");
        if let Some(fmt) = &v.traits.display_fmt {
            let _ = writeln!(
                out,
                "std::string to_string({e} value) {{\n{}}}\n",
                self.call_body(fmt, Some(&value), None, 4)
            );
        }
        if let Some(fmt) = &v.traits.debug_fmt {
            let _ = writeln!(
                out,
                "std::string debug_string({e} value) {{\n{}}}\n",
                self.call_body(fmt, Some(&value), None, 4)
            );
        }
        if let Some(hash) = &v.traits.hash_hash {
            let _ = writeln!(
                out,
                "uint64_t hash({e} value) {{\n{}}}\n",
                self.call_body(hash, Some(&value), None, 4)
            );
        }
        out
    }

    /// Definitions of the trait members of class `name`; `this` and `other` are the
    /// lowered receiver and argument.
    pub(crate) fn trait_definitions(
        &self,
        name: &str,
        traits: &UniffiTraitMethods,
        this: &str,
        other: &str,
    ) -> String {
        let mut out = String::new();
        if let Some(fmt) = &traits.display_fmt {
            let _ = writeln!(
                out,
                "std::string {name}::to_string() const {{\n{}}}\n",
                self.call_body(fmt, Some(this), None, 4)
            );
        }
        if let Some(fmt) = &traits.debug_fmt {
            let _ = writeln!(
                out,
                "std::string {name}::debug_string() const {{\n{}}}\n",
                self.call_body(fmt, Some(this), None, 4)
            );
        }
        if let Some(hash) = &traits.hash_hash {
            let _ = writeln!(
                out,
                "uint64_t {name}::hash() const {{\n{}}}\n",
                self.call_body(hash, Some(this), None, 4)
            );
        }
        if let Some(eq) = &traits.eq_eq {
            let _ = writeln!(
                out,
                "bool {name}::operator==(const {name}& other) const {{\n{}}}\n",
                self.call_body(eq, Some(this), Some(vec![other.to_owned()]), 4)
            );
        }
        if let Some(cmp) = &traits.ord_cmp {
            let _ = writeln!(
                out,
                "int8_t {name}::compare(const {name}& other) const {{\n{}}}\n",
                self.call_body(cmp, Some(this), Some(vec![other.to_owned()]), 4)
            );
        }
        out
    }
}

/// `what()` of an exception class: its `Display` text, computed on first use.
pub(crate) fn what_from_display(name: &str) -> String {
    format!(
        "const char* {name}::what() const noexcept {{\n    if (what_.empty()) {{\n        try {{\n            what_ = to_string();\n        }} catch (...) {{\n            what_ = \"{name}\";\n        }}\n    }}\n    return what_.c_str();\n}}\n\n"
    )
}

/// `return uniffi::equal(a, uniffi_other.a) && ...;`, or `true` without fields. The
/// parameter is prefixed so a field called `other` cannot hide it.
fn fieldwise(fields: &[String]) -> String {
    if fields.is_empty() {
        return "(void)uniffi_other;\n    return true;".to_owned();
    }
    let checks: Vec<String> = fields
        .iter()
        .map(|f| format!("uniffi::equal(this->{f}, uniffi_other.{f})"))
        .collect();
    format!("return {};", checks.join("\n        && "))
}
