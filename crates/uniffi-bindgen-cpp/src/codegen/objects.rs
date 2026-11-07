//! Objects: handles, lifting/lowering, methods and the Rust-backed
//! implementations of traits.

use std::fmt::Write;

use uniffi_bindgen::interface::Object;

use super::names::{class, member};
use super::types::Gen;

impl Gen<'_> {
    pub(crate) fn emit_object_lift_lower(&self, obj: &Object) -> String {
        let name = class(obj.name());
        if obj.is_trait_interface() || obj.has_callback_interface() {
            let impl_name = format!("{name}RustImpl");
            let (lift_foreign, lower_foreign) = if obj.has_callback_interface() {
                (
                    format!("return handle_map_{name}().remove(handle);"),
                    format!("return handle_map_{name}().insert(value);"),
                )
            } else {
                (
                    "throw uniffi::InternalError(\"unexpected foreign handle for a Rust trait\");"
                        .to_owned(),
                    "throw uniffi::InternalError(\"a Rust trait object cannot be implemented in C++\");"
                        .to_owned(),
                )
            };
            format!(
                "std::shared_ptr<{name}> lift_{name}(uint64_t handle) {{\n    if ((handle & 1) == 0) {{\n        return std::make_shared<{impl_name}>(handle);\n    }}\n    {lift_foreign}\n}}\n\nuint64_t lower_{name}(const std::shared_ptr<{name}>& value) {{\n    if (auto* rust = dynamic_cast<{impl_name}*>(value.get())) {{\n        return rust->uniffi_clone_handle();\n    }}\n    {lower_foreign}\n}}\n\n"
            )
        } else {
            format!(
                "std::shared_ptr<{name}> lift_{name}(uint64_t handle) {{\n    return std::make_shared<{name}>(uniffi::HandleTag{{}}, handle);\n}}\n\nuint64_t lower_{name}(const std::shared_ptr<{name}>& value) {{\n    return value->uniffi_clone_handle();\n}}\n\n"
            )
        }
    }

    pub(crate) fn emit_callback_lift_lower(&self, name: &str) -> String {
        format!(
            "std::shared_ptr<{name}> lift_{name}(uint64_t handle) {{\n    return handle_map_{name}().get(handle);\n}}\n\nuint64_t lower_{name}(const std::shared_ptr<{name}>& value) {{\n    return handle_map_{name}().insert(value);\n}}\n\n"
        )
    }

    fn free_and_clone(&self, obj: &Object, class_name: &str, pad: usize) -> (String, String) {
        let pad = " ".repeat(pad);
        let free = format!(
            "{pad}if (handle_ != 0) {{\n{pad}    uniffi::RustCallStatus status{{}};\n{pad}    ffi::{}(handle_, &status);\n{pad}}}\n",
            obj.ffi_object_free().name()
        );
        let clone = format!(
            "{pad}uniffi_detail::ensure_init();\n{pad}uniffi::RustCallStatus status{{}};\n{pad}uint64_t handle = ffi::{}(handle_, &status);\n{pad}uniffi_detail::check_status(status);\n{pad}return handle;\n",
            obj.ffi_object_clone().name()
        );
        let _ = class_name;
        (free, clone)
    }

    /// Out-of-class definitions for an object backed by a Rust struct.
    pub(crate) fn emit_object_methods(&self, obj: &Object) -> String {
        let name = class(obj.name());
        let (free, clone) = self.free_and_clone(obj, &name, 4);
        let mut out = String::new();
        let _ = writeln!(out, "{name}::~{name}() {{\n{free}}}\n");
        let _ = writeln!(
            out,
            "uint64_t {name}::uniffi_clone_handle() const {{\n{clone}}}\n"
        );
        for c in obj.constructors() {
            let _ = writeln!(
                out,
                "{} {{\n{}}}\n",
                self.qualified_signature(c, &name, &member(c.name(), obj.name())),
                self.call_body(c, None, None, 4)
            );
        }
        for m in obj.methods() {
            let _ = writeln!(
                out,
                "{} const {{\n{}}}\n",
                self.qualified_signature(m, &name, &member(m.name(), obj.name())),
                self.call_body(m, Some("uniffi_clone_handle()"), None, 4)
            );
        }
        if self.is_error_object(obj.name()) && obj.uniffi_trait_methods().display_fmt.is_some() {
            out.push_str(&super::values::what_from_display(&name));
        }
        out.push_str(&self.trait_definitions(
            &name,
            &obj.uniffi_trait_methods(),
            "uniffi_clone_handle()",
            "other.uniffi_clone_handle()",
        ));
        out
    }

    /// The class that forwards a trait object's methods to Rust.
    pub(crate) fn emit_rust_impl_class(&self, obj: &Object) -> String {
        let name = class(obj.name());
        let impl_name = format!("{name}RustImpl");
        let (free, clone) = self.free_and_clone(obj, &name, 8);
        let mut out = String::new();
        // Thrown when the trait is an error type, so then a copy is a new reference.
        let copy = if self.is_error_object(obj.name()) {
            format!(
                "{impl_name}(const {impl_name}& other) : {name}(other), handle_(other.uniffi_clone_handle()) {{}}"
            )
        } else {
            format!("{impl_name}(const {impl_name}&) = delete;")
        };
        let _ = writeln!(
            out,
            "class {impl_name} final : public {name} {{\npublic:\n    explicit {impl_name}(uint64_t handle) : handle_(handle) {{}}\n    {copy}\n    {impl_name}& operator=(const {impl_name}&) = delete;\n    ~{impl_name}() override {{\n{free}    }}\n\n    uint64_t uniffi_clone_handle() const {{\n{clone}    }}\n"
        );
        for m in obj.methods() {
            let _ = writeln!(
                out,
                "    {} override {{\n{}    }}\n",
                self.signature(m, &member(m.name(), obj.name()), false),
                self.call_body(m, Some("uniffi_clone_handle()"), None, 8)
            );
        }
        let _ = writeln!(out, "private:\n    uint64_t handle_;\n}};\n");
        out
    }
}
