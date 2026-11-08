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
                    // `&[u8]` arguments are lent to Rust for the call, not copied.
                    args.push(if a.is_borrowed_bytes() {
                        format!("uniffi::borrow_bytes({})", var(a.name()))
                    } else {
                        self.lower(&a.as_type(), &var(a.name()))
                    });
                }
            }
        }
        let thrower = self.thrower(c);
        let result = self.result_type_of(c);
        // Without exceptions a Rust error returns early, as the `Err` of the result.
        let on_error = |pad: &str| match (
            self.result_error(c),
            c.throws_type().and_then(|t| self.error_lifter(t)),
        ) {
            (Some(_), Some(lifter)) => format!(
                "{pad}if (uniffi_status.code == uniffi::CALL_ERROR) {{\n{pad}    return {result}(uniffi::Err{{{lifter}(uniffi_status.error_buf)}});\n{pad}}}\n"
            ),
            _ => String::new(),
        };
        // The value a successful call returns, if any.
        let success = |lifted: Option<String>| -> Option<String> {
            if self.result_error(c).is_some() {
                Some(format!(
                    "{result}(uniffi::Ok{{{}}})",
                    lifted.unwrap_or_default()
                ))
            } else {
                lifted
            }
        };
        let lifted = c.return_type().map(|t| self.lift(t, "uniffi_result"));
        let assign = if c.return_type().is_some() {
            "auto uniffi_result = "
        } else {
            ""
        };
        let mut out = String::new();
        let _ = writeln!(out, "{indent}uniffi_detail::ensure_init();");
        if c.is_async() {
            let slot = if result == "void" {
                "std::monostate".to_owned()
            } else {
                result.clone()
            };
            let value = success(lifted).unwrap_or_else(|| "std::monostate{}".to_owned());
            let inner = format!("{indent}        ");
            let _ = writeln!(
                out,
                "{indent}uint64_t uniffi_future = ffi::{ffi_name}({});",
                args.join(", ")
            );
            let _ = writeln!(
                out,
                "{indent}return uniffi::detail::start_future<{result}>(\n{indent}    uniffi_future, &ffi::{poll}, &ffi::{cancel}, &ffi::{free},\n{indent}    [](uint64_t uniffi_handle) -> std::optional<{slot}> {{\n{inner}uniffi::RustCallStatus uniffi_status{{}};\n{inner}{assign}ffi::{complete}(uniffi_handle, &uniffi_status);\n{inner}if (uniffi_status.code == uniffi::CALL_CANCELLED) {{\n{inner}    return std::nullopt;\n{inner}}}\n{on_error}{inner}uniffi_detail::check_status(uniffi_status{thrower});\n{inner}return {value};\n{indent}    }});",
                poll = c.ffi_rust_future_poll(self.ci),
                cancel = c.ffi_rust_future_cancel(self.ci),
                free = c.ffi_rust_future_free(self.ci),
                complete = c.ffi_rust_future_complete(self.ci),
                on_error = on_error(&inner),
            );
        } else {
            args.push("&uniffi_status".to_owned());
            let call = format!("ffi::{ffi_name}({})", args.join(", "));
            let _ = writeln!(out, "{indent}uniffi::RustCallStatus uniffi_status{{}};");
            let _ = writeln!(out, "{indent}{assign}{call};");
            out.push_str(&on_error(&indent));
            let _ = writeln!(
                out,
                "{indent}uniffi_detail::check_status(uniffi_status{thrower});"
            );
            if let Some(value) = success(lifted) {
                let _ = writeln!(out, "{indent}return {value};");
            }
        }
        out
    }

    /// `, &throw_buf_Err` for callables that throw an error enum.
    fn thrower(&self, c: &dyn Callable) -> String {
        if !self.exceptions() {
            return String::new();
        }
        c.throws_type()
            .map(|t| self.thrower_for(t))
            .unwrap_or_default()
    }

    /// A custom type is thrown as the error type it wraps.
    fn thrower_for(&self, t: &Type) -> String {
        match t {
            Type::Enum { name, .. } | Type::Object { name, .. } => format!(
                ", &{}uniffi_detail::throw_buf_{}",
                self.qualifier(t),
                class(name)
            ),
            Type::Custom { builtin, .. } => self.thrower_for(builtin),
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
