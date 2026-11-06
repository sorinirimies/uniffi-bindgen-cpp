//! Objects: handles, lifting/lowering, methods and the Rust-backed
//! implementations of traits.

use std::fmt::Write;

use uniffi_bindgen::interface::Object;

use super::names::{class, var};
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

    /// The members that give an object access to its uniffi traits.
    fn trait_methods(&self, obj: &Object, name: &str) -> String {
        let traits = obj.uniffi_trait_methods();
        let handle = Some("uniffi_clone_handle()");
        let mut out = String::new();
        if let Some(fmt) = &traits.display_fmt {
            let _ = writeln!(
                out,
                "std::string {name}::to_string() const {{\n{}}}\n",
                self.call_body(fmt, handle, None, 4)
            );
        }
        if let Some(fmt) = &traits.debug_fmt {
            let _ = writeln!(
                out,
                "std::string {name}::debug_string() const {{\n{}}}\n",
                self.call_body(fmt, handle, None, 4)
            );
        }
        if let Some(hash) = &traits.hash_hash {
            let _ = writeln!(
                out,
                "uint64_t {name}::hash() const {{\n{}}}\n",
                self.call_body(hash, handle, None, 4)
            );
        }
        if let Some(eq) = &traits.eq_eq {
            let _ = writeln!(
                out,
                "bool {name}::operator==(const {name}& other) const {{\n{}}}\n",
                self.call_body(eq, handle, Some(vec!["other.uniffi_clone_handle()".into()]), 4)
            );
        }
        if let Some(cmp) = &traits.ord_cmp {
            let body = self
                .call_body(cmp, handle, Some(vec!["other.uniffi_clone_handle()".into()]), 4)
                .replace("return result;", "return result < 0;");
            let _ = writeln!(
                out,
                "bool {name}::operator<(const {name}& other) const {{\n{body}}}\n"
            );
        }
        out
    }

    /// Out-of-class definitions for an object backed by a Rust struct.
    pub(crate) fn emit_object_methods(&self, obj: &Object) -> String {
        let name = class(obj.name());
        let (free, clone) = self.free_and_clone(obj, &name, 4);
        let mut out = String::new();
        let _ = writeln!(out, "{name}::~{name}() {{\n{free}}}\n");
        let _ = writeln!(out, "uint64_t {name}::uniffi_clone_handle() const {{\n{clone}}}\n");
        for c in obj.constructors() {
            let _ = writeln!(
                out,
                "{} {{\n{}}}\n",
                self.qualified_signature(c, &name, &var(c.name())),
                self.call_body(c, None, None, 4)
            );
        }
        for m in obj.methods() {
            let _ = writeln!(
                out,
                "{} const {{\n{}}}\n",
                self.qualified_signature(m, &name, &var(m.name())),
                self.call_body(m, Some("uniffi_clone_handle()"), None, 4)
            );
        }
        out.push_str(&self.trait_methods(obj, &name));
        out
    }

    /// The class that forwards a trait object's methods to Rust.
    pub(crate) fn emit_rust_impl_class(&self, obj: &Object) -> String {
        let name = class(obj.name());
        let impl_name = format!("{name}RustImpl");
        let (free, clone) = self.free_and_clone(obj, &name, 8);
        let mut out = String::new();
        let _ = writeln!(
            out,
            "class {impl_name} final : public {name} {{\npublic:\n    explicit {impl_name}(uint64_t handle) : handle_(handle) {{}}\n    {impl_name}(const {impl_name}&) = delete;\n    {impl_name}& operator=(const {impl_name}&) = delete;\n    ~{impl_name}() override {{\n{free}    }}\n\n    uint64_t uniffi_clone_handle() const {{\n{clone}    }}\n"
        );
        for m in obj.methods() {
            let _ = writeln!(
                out,
                "    {} override {{\n{}    }}\n",
                self.signature(m, &var(m.name()), false),
                self.call_body(m, Some("uniffi_clone_handle()"), None, 8)
            );
        }
        let _ = writeln!(out, "private:\n    uint64_t handle_;\n}};\n");
        out
    }
}
