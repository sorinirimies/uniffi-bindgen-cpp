//! Calling scaffolding functions from C++.

use std::fmt::Write;

use uniffi_bindgen::interface::{AsType, Callable, Type};

use super::names::{class, var};
use super::types::Gen;

impl Gen<'_> {
    /// The body of a function or method that calls into Rust.
    ///
    /// `self_handle` is the expression for the receiver's handle and `arg_exprs`
    /// replaces the lowering of the arguments.
    pub(crate) fn call_body(
        &self,
        c: &dyn Callable,
        self_handle: Option<&str>,
        arg_exprs: Option<Vec<String>>,
        pad: usize,
    ) -> String {
        let indent = " ".repeat(pad);
        let ffi_name = c.ffi_func().name().to_owned();
        let mut args: Vec<String> = Vec::new();
        if let Some(handle) = self_handle {
            args.push(handle.to_owned());
        }
        match arg_exprs {
            Some(exprs) => args.extend(exprs),
            None => {
                for a in c.arguments() {
                    args.push(self.lower(&a.as_type(), &var(a.name())));
                }
            }
        }
        let thrower = self.thrower(c);
        let mut out = String::new();
        let _ = writeln!(out, "{indent}uniffi_detail::ensure_init();");
        if c.is_async() {
            let ret_cpp = c
                .return_type()
                .map_or_else(|| "void".to_owned(), |t| self.cpp_type(t));
            let (assign, ret_line) = match c.return_type() {
                Some(t) => (
                    "auto uniffi_result = ",
                    format!(
                        "{indent}        return {};\n",
                        self.lift(t, "uniffi_result")
                    ),
                ),
                None => ("", String::new()),
            };
            let _ = writeln!(
                out,
                "{indent}uint64_t uniffi_future = ffi::{ffi_name}({});",
                args.join(", ")
            );
            let _ = writeln!(
                out,
                "{indent}return uniffi::detail::start_future<{ret_cpp}>(\n{indent}    uniffi_future, &ffi::{poll}, &ffi::{free},\n{indent}    [](uint64_t uniffi_handle) -> {ret_cpp} {{\n{indent}        uniffi::RustCallStatus uniffi_status{{}};\n{indent}        {assign}ffi::{complete}(uniffi_handle, &uniffi_status);\n{indent}        uniffi_detail::check_status(uniffi_status{thrower});\n{ret_line}{indent}    }});",
                poll = c.ffi_rust_future_poll(self.ci),
                free = c.ffi_rust_future_free(self.ci),
                complete = c.ffi_rust_future_complete(self.ci),
            );
        } else {
            args.push("&uniffi_status".to_owned());
            let call = format!("ffi::{ffi_name}({})", args.join(", "));
            let _ = writeln!(out, "{indent}uniffi::RustCallStatus uniffi_status{{}};");
            match c.return_type() {
                Some(t) => {
                    let _ = writeln!(out, "{indent}auto uniffi_result = {call};");
                    let _ = writeln!(
                        out,
                        "{indent}uniffi_detail::check_status(uniffi_status{thrower});"
                    );
                    let _ = writeln!(out, "{indent}return {};", self.lift(t, "uniffi_result"));
                }
                None => {
                    let _ = writeln!(out, "{indent}{call};");
                    let _ = writeln!(
                        out,
                        "{indent}uniffi_detail::check_status(uniffi_status{thrower});"
                    );
                }
            }
        }
        out
    }

    /// `, &throw_buf_Err` for callables that throw an error enum.
    fn thrower(&self, c: &dyn Callable) -> String {
        match c.throws_type() {
            Some(t @ (Type::Enum { name, .. } | Type::Object { name, .. })) => format!(
                ", &{}uniffi_detail::throw_buf_{}",
                self.qualifier(t),
                class(name)
            ),
            _ => String::new(),
        }
    }

    /// `RET Class::name(params)` for out-of-class definitions.
    pub(crate) fn qualified_signature(
        &self,
        c: &dyn Callable,
        class_name: &str,
        name: &str,
    ) -> String {
        format!(
            "{} {class_name}::{name}({})",
            self.return_type_of(c),
            self.param_list(c, false)
        )
    }
}
