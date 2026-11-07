//! Callback interfaces and foreign implementations of traits: the vtables
//! Rust calls back through.

use std::fmt::Write;

use uniffi_bindgen::interface::{AsType, FfiCallbackFunction, Method, Type};

use super::names::member;
use super::types::Gen;

impl Gen<'_> {
    /// The `catch` clauses that turn a C++ exception into a call status.
    fn catch_clauses(&self, m: &Method, status: &str, pad: &str) -> String {
        let mut out = String::new();
        match m.throws_type() {
            Some(t @ Type::Enum { name, .. }) => {
                let err = self.named(t, name);
                let _ = writeln!(
                    out,
                    "{pad}}} catch (const {err}& error) {{\n{pad}    {status}.code = uniffi::CALL_ERROR;\n{pad}    {status}.error_buf = uniffi_detail::lower_error(error);"
                );
            }
            Some(t @ Type::Object { name, .. }) => {
                let err = self.named(t, name);
                let _ = writeln!(
                    out,
                    "{pad}}} catch (const {err}& error) {{\n{pad}    {status}.code = uniffi::CALL_ERROR;\n{pad}    {status}.error_buf = uniffi_detail::lower_buf<std::shared_ptr<{err}>>(std::make_shared<{err}>(error));"
                );
            }
            _ => {}
        }
        let _ = writeln!(
            out,
            "{pad}}} catch (const std::exception& error) {{\n{pad}    {status}.code = uniffi::CALL_UNEXPECTED_ERROR;\n{pad}    {status}.error_buf = uniffi_detail::lower_string(error.what());\n{pad}}} catch (...) {{\n{pad}    {status}.code = uniffi::CALL_UNEXPECTED_ERROR;\n{pad}    {status}.error_buf = uniffi_detail::lower_string(\"unknown exception\");"
        );
        out
    }

    fn vtable_params(&self, callback: &FfiCallbackFunction, method: &Method) -> String {
        let n_args = method.arguments().len();
        let mut params: Vec<String> = callback
            .arguments()
            .iter()
            .enumerate()
            .map(|(i, a)| {
                let name = match i {
                    0 => "uniffi_handle".to_owned(),
                    i if i <= n_args => format!("arg{}", i - 1),
                    _ => a.name().to_owned(),
                };
                format!("{} {name}", self.ffi_type(&a.type_()))
            })
            .collect();
        if callback.has_rust_call_status_arg() {
            params.push("uniffi::RustCallStatus* uniffi_call_status".into());
        }
        params.join(", ")
    }

    fn lifted_args(&self, method: &Method) -> String {
        method
            .arguments()
            .iter()
            .enumerate()
            .map(|(i, a)| self.lift(&a.as_type(), &format!("arg{i}")))
            .collect::<Vec<_>>()
            .join(", ")
    }

    fn sync_wrapper(
        &self,
        name: &str,
        i: usize,
        callback: &FfiCallbackFunction,
        m: &Method,
    ) -> String {
        let args = self.lifted_args(m);
        let method = member(m.name(), name);
        let (call, write) = match m.return_type() {
            Some(t) => (
                format!("auto result = object->{method}({args});"),
                format!(
                    "\n        *uniffi_out_return = {};",
                    self.lower(t, "result")
                ),
            ),
            None => (
                format!("object->{method}({args});"),
                "\n        (void)uniffi_out_return;".to_owned(),
            ),
        };
        format!(
            "void cb_{name}_m{i}({params}) {{\n    try {{\n        auto object = handle_map_{name}().get(uniffi_handle);\n        {call}{write}\n{catches}    }}\n}}\n\n",
            params = self.vtable_params(callback, m),
            catches = self.catch_clauses(m, "(*uniffi_call_status)", "    "),
        )
    }

    fn async_wrapper(
        &self,
        name: &str,
        i: usize,
        callback: &FfiCallbackFunction,
        m: &Method,
    ) -> String {
        let args = self.lifted_args(m);
        let method = member(m.name(), name);
        let result_struct = m.foreign_future_ffi_result_struct().name().to_owned();
        let ret_cpp = m
            .return_type()
            .map_or_else(|| "void".to_owned(), |t| self.cpp_type(t));
        let (get, store) = match m.return_type() {
            Some(t) => (
                "auto value = done.get();".to_owned(),
                format!(
                    "\n            result.return_value = {};",
                    self.lower(t, "value")
                ),
            ),
            None => ("done.get();".to_owned(), String::new()),
        };
        format!(
            "void cb_{name}_m{i}({params}) {{\n    uniffi_out_dropped_callback->handle = 0;\n    uniffi_out_dropped_callback->free = &foreign_future_dropped;\n    uniffi::Future<{ret_cpp}> pending;\n    try {{\n        auto object = handle_map_{name}().get(uniffi_handle);\n        pending = object->{method}({args});\n        if (!pending.valid()) {{\n            throw uniffi::InternalError(\"an async callback returned an empty future\");\n        }}\n    }} catch (const std::exception& error) {{\n        ffi::{result_struct} result{{}};\n        result.call_status.code = uniffi::CALL_UNEXPECTED_ERROR;\n        result.call_status.error_buf = uniffi_detail::lower_string(error.what());\n        uniffi_future_callback(uniffi_callback_data, result);\n        return;\n    }}\n    // Rust drops the future when it no longer wants the result: cancel the C++ side.\n    auto state = pending.uniffi_state();\n    uniffi_out_dropped_callback->handle = foreign_futures().insert([state] {{ state->request_cancel(); }});\n    pending.then([uniffi_future_callback, uniffi_callback_data](uniffi::Future<{ret_cpp}> done) {{\n        ffi::{result_struct} result{{}};\n        try {{\n            {get}{store}\n{catches}        }}\n        uniffi_future_callback(uniffi_callback_data, result);\n    }});\n}}\n\n",
            params = self.vtable_params(callback, m),
            catches = self.catch_clauses(m, "result.call_status", "        "),
        )
    }

    /// Wrappers for every method plus the function that registers the vtable with Rust.
    pub(crate) fn emit_callback_vtable(
        &self,
        name: &str,
        methods: &[(FfiCallbackFunction, Method)],
        init_fn: &str,
        vtable_type: &str,
    ) -> String {
        let mut out = String::new();
        let _ = writeln!(
            out,
            "void cb_{name}_free(uint64_t handle) {{\n    try {{\n        handle_map_{name}().remove(handle);\n    }} catch (...) {{\n    }}\n}}\n\nuint64_t cb_{name}_clone(uint64_t handle) {{\n    try {{\n        return handle_map_{name}().clone(handle);\n    }} catch (...) {{\n        return 0;\n    }}\n}}\n"
        );
        let mut entries = vec![format!("&cb_{name}_free"), format!("&cb_{name}_clone")];
        for (i, (callback, m)) in methods.iter().enumerate() {
            if m.is_async() {
                out.push_str(&self.async_wrapper(name, i, callback, m));
            } else {
                out.push_str(&self.sync_wrapper(name, i, callback, m));
            }
            entries.push(format!("&cb_{name}_m{i}"));
        }
        let _ = writeln!(
            out,
            "void init_{name}() {{\n    static {vtable_type} vtable = {{{}}};\n    ffi::{init_fn}(&vtable);\n}}\n",
            entries.join(", ")
        );
        out
    }
}

/// The cancel functions of the futures that async callbacks returned, by the handle
/// Rust was given; Rust calls `foreign_future_dropped` with it when it drops the future.
pub(crate) const FOREIGN_FUTURE_DROPPED: &str = r#"[[maybe_unused]] uniffi::HandleMap<std::function<void()>>& foreign_futures() {
    static auto* futures = new uniffi::HandleMap<std::function<void()>>();
    return *futures;
}

[[maybe_unused]] void foreign_future_dropped(uint64_t handle) {
    if (handle == 0) {
        return;
    }
    std::function<void()> cancel;
    try {
        cancel = foreign_futures().remove(handle);
    } catch (const std::exception&) {
        return;
    }
    cancel();
}

"#;
