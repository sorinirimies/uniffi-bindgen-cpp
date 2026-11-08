//! Mapping UniFFI types to C++: type names, FFI types, lifting, lowering and
//! default values.

use std::collections::BTreeSet;

use uniffi_bindgen::ComponentInterface;
use uniffi_bindgen::interface::{Callable, DefaultValue, FfiType, Literal, Radix, Type};

use super::names::{class, member, string_literal};
use crate::config::{Config, CustomTypeConfig};

/// Everything the emitters need to know about the component being generated.
#[derive(Debug)]
pub(crate) struct Gen<'a> {
    pub(crate) ci: &'a ComponentInterface,
    pub(crate) config: &'a Config,
    /// C++ namespace of the generated code.
    pub(crate) ns: String,
    /// Names of the error types of the other components generated alongside this one:
    /// an external type is an error if its own component says so.
    external_errors: &'a BTreeSet<String>,
    /// Names of the types this component's callables throw. UniFFI does not count a
    /// trait object (`Arc<dyn Trait>`) as an error, so collect them here.
    thrown: BTreeSet<String>,
}

impl<'a> Gen<'a> {
    pub(crate) fn new(
        ci: &'a ComponentInterface,
        config: &'a Config,
        external_errors: &'a BTreeSet<String>,
    ) -> Self {
        Self {
            ci,
            config,
            ns: config.namespace_for(ci.namespace()),
            external_errors,
            thrown: thrown_type_names(ci),
        }
    }

    /// `::other_ns::` for types defined in another component, empty otherwise.
    pub(crate) fn qualifier(&self, t: &Type) -> String {
        if !self.ci.is_external(t) {
            return String::new();
        }
        match self.ci.namespace_for_type(t) {
            Ok(ns) => format!("::{ns}::"),
            Err(_) => match module_path_of(t) {
                Some(path) => format!("::{}::", path.split("::").next().unwrap_or(path)),
                None => String::new(),
            },
        }
    }

    /// The fully qualified name of a user-defined type. Always qualified, so that a
    /// member or variant with the same name cannot hide it.
    pub(crate) fn named(&self, t: &Type, name: &str) -> String {
        let name = self.renamed_type(name);
        let q = self.qualifier(t);
        if q.is_empty() {
            self.local(name)
        } else {
            format!("{q}{}", class(name))
        }
    }

    /// `name` after the `rename` table. UniFFI renames definitions and most references,
    /// but not the type a constructor returns, so references go through this too.
    fn renamed_type<'n>(&'n self, name: &'n str) -> &'n str {
        self.config
            .rename
            .get(name)
            .and_then(|v| v.as_str())
            .unwrap_or(name)
    }

    /// The fully qualified name of a type this component defines.
    pub(crate) fn local(&self, name: &str) -> String {
        format!("::{}::{}", self.ns, class(name))
    }

    pub(crate) fn custom_config(&self, name: &str) -> Option<&CustomTypeConfig> {
        self.config.custom_types.get(name)
    }

    /// The C++ type that represents `t` to the caller.
    pub(crate) fn cpp_type(&self, t: &Type) -> String {
        let rec = |t: &Type| self.cpp_type(t);
        match t {
            Type::UInt8 => "uint8_t".into(),
            Type::Int8 => "int8_t".into(),
            Type::UInt16 => "uint16_t".into(),
            Type::Int16 => "int16_t".into(),
            Type::UInt32 => "uint32_t".into(),
            Type::Int32 => "int32_t".into(),
            Type::UInt64 => "uint64_t".into(),
            Type::Int64 => "int64_t".into(),
            Type::Float32 => "float".into(),
            Type::Float64 => "double".into(),
            Type::Boolean => "bool".into(),
            Type::String => "std::string".into(),
            Type::Bytes => "std::vector<uint8_t>".into(),
            Type::Timestamp => "uniffi::Timestamp".into(),
            Type::Duration => "uniffi::Duration".into(),
            Type::Object { name, .. } | Type::CallbackInterface { name, .. } => {
                format!("std::shared_ptr<{}>", self.named(t, name))
            }
            // Errors are class hierarchies, held by pointer to keep the variant.
            Type::Enum { name, .. } if self.is_error_enum(name) => {
                format!("std::shared_ptr<{}>", self.named(t, name))
            }
            Type::Record { name, .. } | Type::Enum { name, .. } => self.named(t, name),
            Type::Optional { inner_type } => format!("std::optional<{}>", rec(inner_type)),
            Type::Sequence { inner_type } => format!("std::vector<{}>", rec(inner_type)),
            Type::Set { inner_type } => format!("std::set<{}>", rec(inner_type)),
            Type::Box { inner_type } => format!("uniffi::Box<{}>", rec(inner_type)),
            Type::Map {
                key_type,
                value_type,
            } => {
                format!("std::map<{}, {}>", rec(key_type), rec(value_type))
            }
            Type::Custom { name, .. } => {
                match self.custom_config(name).and_then(|c| c.type_name.clone()) {
                    Some(type_name) if !self.ci.is_external(t) => type_name,
                    _ => self.named(t, name),
                }
            }
        }
    }

    /// How a parameter of type `t` is declared. `view`: a string is taken as a
    /// `std::string_view` (calls into Rust, which copy it straight into a buffer);
    /// virtual methods implemented in C++ take `const std::string&`.
    pub(crate) fn param_type(&self, t: &Type, view: bool) -> String {
        match t {
            Type::String if view => "std::string_view".to_owned(),
            Type::UInt8
            | Type::Int8
            | Type::UInt16
            | Type::Int16
            | Type::UInt32
            | Type::Int32
            | Type::UInt64
            | Type::Int64
            | Type::Float32
            | Type::Float64
            | Type::Boolean => self.cpp_type(t),
            Type::Enum { .. } if self.is_flat_enum(t) => self.cpp_type(t),
            _ => format!("const {}&", self.cpp_type(t)),
        }
    }

    fn is_flat_enum(&self, t: &Type) -> bool {
        match t {
            Type::Enum { name, .. } if !self.ci.is_external(t) => self
                .ci
                .get_enum_definition(name)
                .is_some_and(|e| e.is_flat() && !self.ci.is_name_used_as_error(name)),
            _ => false,
        }
    }

    pub(crate) fn is_error_enum(&self, name: &str) -> bool {
        self.ci.get_object_definition(name).is_none()
            && self.ci.get_record_definition(name).is_none()
            && (self.ci.is_name_used_as_error(name) || self.external_errors.contains(name))
    }

    /// An object that Rust functions return as their error.
    pub(crate) fn is_error_object(&self, name: &str) -> bool {
        self.ci.get_object_definition(name).is_some()
            && (self.ci.is_name_used_as_error(name) || self.thrown.contains(name))
    }

    /// The C++ spelling of an FFI-level type, for code outside the FFI namespace.
    pub(crate) fn ffi_type(&self, t: &FfiType) -> String {
        self.ffi_type_in(t, true)
    }

    /// The C++ spelling of an FFI-level type, for the FFI namespace's own declarations.
    pub(crate) fn ffi_type_unqualified(&self, t: &FfiType) -> String {
        self.ffi_type_in(t, false)
    }

    fn ffi_type_in(&self, t: &FfiType, qualify: bool) -> String {
        match t {
            FfiType::UInt8 => "uint8_t".into(),
            FfiType::Int8 => "int8_t".into(),
            FfiType::UInt16 => "uint16_t".into(),
            FfiType::Int16 => "int16_t".into(),
            FfiType::UInt32 => "uint32_t".into(),
            FfiType::Int32 => "int32_t".into(),
            FfiType::UInt64 | FfiType::Handle => "uint64_t".into(),
            FfiType::Int64 => "int64_t".into(),
            FfiType::Float32 => "float".into(),
            FfiType::Float64 => "double".into(),
            FfiType::RustBuffer(_) => "uniffi::RustBuffer".into(),
            FfiType::ForeignBytes => "uniffi::ForeignBytes".into(),
            FfiType::Callback(name) | FfiType::Struct(name) if qualify => format!("ffi::{name}"),
            FfiType::Callback(name) | FfiType::Struct(name) => name.clone(),
            FfiType::RustCallStatus => "uniffi::RustCallStatus".into(),
            FfiType::Reference(inner) => format!("const {}*", self.ffi_type_in(inner, qualify)),
            FfiType::MutReference(inner) => format!("{}*", self.ffi_type_in(inner, qualify)),
            FfiType::VoidPointer => "void*".into(),
        }
    }

    /// Expression lowering the C++ value `expr` of type `t` to its FFI value.
    pub(crate) fn lower(&self, t: &Type, expr: &str) -> String {
        match t {
            Type::Boolean => format!("static_cast<int8_t>({expr})"),
            Type::UInt8
            | Type::Int8
            | Type::UInt16
            | Type::Int16
            | Type::UInt32
            | Type::Int32
            | Type::UInt64
            | Type::Int64
            | Type::Float32
            | Type::Float64 => expr.to_owned(),
            Type::String => format!("uniffi_detail::lower_string({expr})"),
            Type::Object { name, .. } | Type::CallbackInterface { name, .. } => {
                format!(
                    "{}uniffi_detail::lower_{}({expr})",
                    self.qualifier(t),
                    class(self.renamed_type(name))
                )
            }
            Type::Custom { name, builtin, .. } => {
                let inner = self
                    .custom_config(name)
                    .map_or_else(|| expr.to_owned(), |c| c.lower(expr));
                self.lower(builtin, &inner)
            }
            _ => format!("uniffi_detail::lower_buf<{}>({expr})", self.cpp_type(t)),
        }
    }

    /// Expression lifting the FFI value `expr` to the C++ type of `t`.
    pub(crate) fn lift(&self, t: &Type, expr: &str) -> String {
        match t {
            Type::Boolean => format!("({expr} != 0)"),
            Type::UInt8
            | Type::Int8
            | Type::UInt16
            | Type::Int16
            | Type::UInt32
            | Type::Int32
            | Type::UInt64
            | Type::Int64
            | Type::Float32
            | Type::Float64 => expr.to_owned(),
            Type::String => format!("uniffi_detail::lift_string({expr})"),
            Type::Object { name, .. } | Type::CallbackInterface { name, .. } => {
                format!(
                    "{}uniffi_detail::lift_{}({expr})",
                    self.qualifier(t),
                    class(self.renamed_type(name))
                )
            }
            Type::Custom { name, builtin, .. } => {
                let inner = self.lift(builtin, expr);
                self.custom_config(name)
                    .map_or(inner.clone(), |c| c.lift(&inner))
            }
            _ => format!("uniffi_detail::lift_buf<{}>({expr})", self.cpp_type(t)),
        }
    }

    /// Expression for a default argument or field value, or `None` when the type has no
    /// literal form here.
    pub(crate) fn default_value(&self, value: &DefaultValue, t: &Type) -> Option<String> {
        match value {
            DefaultValue::Default => Some(format!("{}{{}}", self.cpp_type(t))),
            DefaultValue::Literal(lit) => self.literal(lit, t),
        }
    }

    fn literal(&self, lit: &Literal, t: &Type) -> Option<String> {
        Some(match lit {
            Literal::Boolean(b) => b.to_string(),
            Literal::String(s) => format!("std::string({})", string_literal(s)),
            Literal::UInt(v, radix, ty) => self.int_literal(*v as i128, *radix, ty),
            Literal::Int(v, radix, ty) => self.int_literal(i128::from(*v), *radix, ty),
            Literal::Float(s, ty) => match ty {
                Type::Float32 => format!("{s}f"),
                _ => s.clone(),
            },
            Literal::Enum(variant, ty) => match ty {
                Type::Enum { name, .. } if self.is_flat_enum(ty) => {
                    format!(
                        "{}::{}",
                        self.named(ty, name),
                        class(&self.variant_named(name, variant))
                    )
                }
                Type::Enum { name, .. } => {
                    let enum_name = self.named(ty, name);
                    let variant = member(&self.variant_named(name, variant), name);
                    format!("{enum_name}{{{enum_name}::{variant}{{}}}}")
                }
                _ => return None,
            },
            Literal::EmptySequence | Literal::EmptyMap | Literal::EmptySet => {
                format!("{}{{}}", self.cpp_type(t))
            }
            Literal::None => "std::nullopt".into(),
            Literal::Some { inner } => {
                let inner_type = match t {
                    Type::Optional { inner_type } => inner_type.as_ref(),
                    other => other,
                };
                let inner = self.default_value(inner, inner_type)?;
                format!("{}({inner})", self.cpp_type(t))
            }
        })
    }

    /// The variant of enum `enum_name` a default value refers to. UDL spells it in any
    /// case (`"DEUX"` for `Deux`), so match ignoring case and underscores.
    fn variant_named(&self, enum_name: &str, literal: &str) -> String {
        let normal = |s: &str| s.replace('_', "").to_ascii_lowercase();
        self.ci
            .get_enum_definition(enum_name)
            .and_then(|e| {
                e.variants()
                    .iter()
                    .find(|v| normal(v.name()) == normal(literal))
            })
            .map_or_else(|| literal.to_owned(), |v| v.name().to_owned())
    }

    fn int_literal(&self, v: i128, radix: Radix, ty: &Type) -> String {
        let cpp = self.cpp_type(ty);
        let digits = match radix {
            Radix::Decimal => v.to_string(),
            Radix::Octal => format!("0{v:o}"),
            Radix::Hexadecimal => format!("0x{v:x}"),
        };
        let suffix = match ty {
            Type::UInt64 => "ULL",
            Type::Int64 => "LL",
            Type::UInt32 => "U",
            _ => "",
        };
        if v < 0 {
            // The minimum of a signed type cannot be written as a literal.
            format!("static_cast<{cpp}>({v}{suffix})")
        } else {
            format!("static_cast<{cpp}>({digits}{suffix})")
        }
    }
}

fn module_path_of(t: &Type) -> Option<&str> {
    match t {
        Type::Object { module_path, .. }
        | Type::Record { module_path, .. }
        | Type::Enum { module_path, .. }
        | Type::CallbackInterface { module_path, .. }
        | Type::Custom { module_path, .. } => Some(module_path),
        _ => None,
    }
}

/// The names of every type a function, method or constructor of `ci` throws.
fn thrown_type_names(ci: &ComponentInterface) -> BTreeSet<String> {
    let mut names = BTreeSet::new();
    let mut add = |c: &dyn Callable| {
        if let Some(Type::Object { name, .. } | Type::Enum { name, .. }) = c.throws_type() {
            names.insert(name.clone());
        }
    };
    ci.function_definitions().iter().for_each(|f| add(f));
    for o in ci.object_definitions() {
        o.constructors().into_iter().for_each(|c| add(c));
        o.methods().into_iter().for_each(|m| add(m));
    }
    for r in ci.record_definitions() {
        r.constructors().iter().for_each(|c| add(c));
        r.methods().iter().for_each(|m| add(m));
    }
    for e in ci.enum_definitions() {
        e.constructors().iter().for_each(|c| add(c));
        e.methods().iter().for_each(|m| add(m));
    }
    for cb in ci.callback_interface_definitions() {
        cb.methods().into_iter().for_each(|m| add(m));
    }
    names
}
