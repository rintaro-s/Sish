//! Minimal i18n helpers for Sish-Console.
//!
//! Currently supports Japanese (default) and English, selected via `SISH_LANG`.

/// Returns true if `SISH_LANG` indicates English.
pub fn is_en() -> bool {
    match std::env::var("SISH_LANG") {
        Ok(v) => {
            let v = v.trim();
            if v.is_empty() {
                return false;
            }
            let v = v.to_ascii_lowercase();
            v == "en" || v.starts_with("en_") || v.starts_with("en-")
        }
        Err(_) => false,
    }
}

/// Translate a static UI string.
#[inline]
pub fn tr<'a>(ja: &'a str, en: &'a str) -> &'a str {
    if is_en() { en } else { ja }
}
