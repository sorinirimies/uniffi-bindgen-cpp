//! The `[bindings.cpp]` section of `uniffi.toml`.

use std::collections::BTreeMap;

use serde::Deserialize;

/// Options that customize the generated C++.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(default)]
pub struct Config {
    /// C++ namespace of the generated code. Defaults to the UniFFI namespace.
    pub namespace: Option<String>,
    /// Name of the shared library that holds the scaffolding. Informational:
    /// the generated code links against it, it is not loaded at run time.
    pub cdylib_name: Option<String>,
    /// Skip the API checksum comparison at start-up.
    pub omit_checksums: bool,
    /// How custom types map to C++ types.
    pub custom_types: BTreeMap<String, CustomTypeConfig>,
}

/// Maps a UniFFI custom type to a C++ type.
///
/// Without an entry a custom type is an alias of its builtin type.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(default)]
pub struct CustomTypeConfig {
    /// The C++ type, for example `std::filesystem::path`.
    pub type_name: Option<String>,
    /// Headers to include, for example `<filesystem>`.
    pub includes: Vec<String>,
    /// Expression turning the builtin value `{}` into the custom type.
    pub lift: Option<String>,
    /// Expression turning the custom value `{}` into the builtin type.
    pub lower: Option<String>,
}

impl Config {
    /// Reads `[bindings.cpp]` from the root of a `uniffi.toml`.
    pub fn from_root_toml(root: &toml::Value) -> anyhow::Result<Self> {
        match root.get("bindings").and_then(|b| b.get("cpp")) {
            Some(section) => Ok(section.clone().try_into()?),
            None => Ok(Self::default()),
        }
    }

    pub fn namespace_for(&self, ci_namespace: &str) -> String {
        self.namespace
            .clone()
            .unwrap_or_else(|| ci_namespace.to_owned())
    }
}

impl CustomTypeConfig {
    pub fn lift(&self, value: &str) -> String {
        match &self.lift {
            Some(expr) => expr.replace("{}", value),
            None => value.to_owned(),
        }
    }

    pub fn lower(&self, value: &str) -> String {
        match &self.lower {
            Some(expr) => expr.replace("{}", value),
            None => value.to_owned(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn missing_section_is_default() {
        let root: toml::Value = toml::from_str("[bindings.kotlin]\npackage_name='x'").unwrap();
        let config = Config::from_root_toml(&root).unwrap();
        assert_eq!(config.namespace, None);
        assert!(!config.omit_checksums);
    }

    #[test]
    fn parses_custom_types() {
        let root: toml::Value = toml::from_str(
            r#"
            [bindings.cpp]
            namespace = "mqtt"
            [bindings.cpp.custom_types.Url]
            type_name = "std::string"
            lift = "{}"
            "#,
        )
        .unwrap();
        let config = Config::from_root_toml(&root).unwrap();
        assert_eq!(config.namespace_for("x"), "mqtt");
        let url = &config.custom_types["Url"];
        assert_eq!(url.lift("v"), "v");
        assert_eq!(url.lower("v"), "v");
    }
}
