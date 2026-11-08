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
        if !self.exceptions() {
            return self.sync_wrapper_without_exceptions(name, i, callback, m);
        }
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

    /// The error check of a `uniffi::Result` named `result`: sets `status` and returns.
    fn result_error_check(&self, m: &Method, result: &str, status: &str, pad: &str) -> String {
        let lowering = m
            .throws_type()
            .and_then(|t| self.error_lowering(t, &format!("{result}.error()")));
        match (self.result_error(m), lowering) {
            (Some(_), Some(lowering)) => format!(
                "{pad}if (!{result}) {{\n{pad}    {status}.code = uniffi::CALL_ERROR;\n{pad}    {status}.error_buf = {lowering};\n{pad}}} else {{\n"
            ),
            _ => String::new(),
        }
    }

    /// Without exceptions: the implementation returns a `uniffi::Result` when the
    /// method has a Rust error, and nothing can throw.
    fn sync_wrapper_without_exceptions(
        &self,
        name: &str,
        i: usize,
        callback: &FfiCallbackFunction,
        m: &Method,
    ) -> String {
        let args = self.lifted_args(m);
        let method = member(m.name(), name);
        let has_result = self.result_error(m).is_some();
        let check = self.result_error_check(m, "result", "(*uniffi_call_status)", "    ");
        let (pad, close) = if check.is_empty() {
            ("    ", String::new())
        } else {
            ("        ", "    }\n".to_owned())
        };
        let value = if has_result {
            "result.value()"
        } else {
            "result"
        };
        let write = match m.return_type() {
            Some(t) => format!("{pad}*uniffi_out_return = {};\n", self.lower(t, value)),
            None => String::new(),
        };
        let call = if has_result || m.return_type().is_some() {
            format!("    auto result = object->{method}({args});\n")
        } else {
            format!("    object->{method}({args});\n")
        };
        let mut unused = String::new();
        if m.return_type().is_none() {
            unused.push_str("    (void)uniffi_out_return;\n");
        }
        if !has_result && callback.has_rust_call_status_arg() {
            unused.push_str("    (void)uniffi_call_status;\n");
        }
        format!(
            "void cb_{name}_m{i}({params}) {{\n{unused}    auto object = handle_map_{name}().get(uniffi_handle);\n{call}{check}{write}{close}}}\n\n",
            params = self.vtable_params(callback, m),
        )
    }

    /// Without exceptions: like `async_wrapper`, with the error in a `uniffi::Result`.
    fn async_wrapper_without_exceptions(
        &self,
        name: &str,
        i: usize,
        callback: &FfiCallbackFunction,
        m: &Method,
    ) -> String {
        let args = self.lifted_args(m);
        let method = member(m.name(), name);
        let result_struct = m.foreign_future_ffi_result_struct().name().to_owned();
        let ret_cpp = self.result_type_of(m);
        let has_result = self.result_error(m).is_some();
        let check = self.result_error_check(m, "value", "result.call_status", "        ");
        let (pad, close) = if check.is_empty() {
            ("        ", String::new())
        } else {
            ("            ", "        }\n".to_owned())
        };
        let get = if ret_cpp == "void" {
            "        done.get();\n".to_owned()
        } else {
            "        auto value = done.get();\n".to_owned()
        };
        let store = match m.return_type() {
            Some(t) => format!(
                "{pad}result.return_value = {};\n",
                self.lower(t, if has_result { "value.value()" } else { "value" })
            ),
            None => String::new(),
        };
        format!(
            "void cb_{name}_m{i}({params}) {{\n    uniffi_out_dropped_callback->handle = 0;\n    uniffi_out_dropped_callback->free = &foreign_future_dropped;\n    auto object = handle_map_{name}().get(uniffi_handle);\n    uniffi::Future<{ret_cpp}> pending = object->{method}({args});\n    if (!pending.valid()) {{\n        ffi::{result_struct} result{{}};\n        result.call_status.code = uniffi::CALL_UNEXPECTED_ERROR;\n        result.call_status.error_buf = uniffi_detail::lower_string(\"an async callback returned an empty future\");\n        uniffi_future_callback(uniffi_callback_data, result);\n        return;\n    }}\n    // Rust drops the future when it no longer wants the result: cancel the C++ side.\n    auto state = pending.uniffi_state();\n    uniffi_out_dropped_callback->handle = foreign_futures().insert([state] {{ state->request_cancel(); }});\n    pending.then([uniffi_future_callback, uniffi_callback_data](uniffi::Future<{ret_cpp}> done) {{\n        ffi::{result_struct} result{{}};\n{get}{check}{store}{close}        uniffi_future_callback(uniffi_callback_data, result);\n    }});\n}}\n\n",
            params = self.vtable_params(callback, m),
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
        if !self.exceptions() {
            return self.async_wrapper_without_exceptions(name, i, callback, m);
        }
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
        let clone = if self.exceptions() {
            format!(
                "    try {{\n        return handle_map_{name}().clone(handle);\n    }} catch (...) {{\n        return 0;\n    }}\n"
            )
        } else {
            format!("    return handle_map_{name}().clone(handle);\n")
        };
        let _ = writeln!(
            out,
            "void cb_{name}_free(uint64_t handle) {{\n    (void)handle_map_{name}().try_remove(handle);\n}}\n\nuint64_t cb_{name}_clone(uint64_t handle) {{\n{clone}}}\n"
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
    if (auto cancel = foreign_futures().try_remove(handle)) {
        (*cancel)();
    }
}

"#;
