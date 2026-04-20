use anyhow::Context;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fs;
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    pub shell: String,
    pub history_limit: usize,
    pub output_limit: usize,
    pub wallpaper_enabled: bool,
    pub keybinds: Keybinds,
    pub llm: LlmConfig,
    pub shortcuts: Vec<Shortcut>,
    pub macros: Vec<MacroEntry>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct Keybinds {
    pub palette: String,
    pub copy_input: String,
    pub copy_output: String,
    pub paste: String,
    pub clear_output: String,
    pub history_prev: String,
    pub history_next: String,
    pub quit: String,
    pub focus_toggle: String,
    pub passthrough_toggle: String,
    pub explorer_up: String,
    pub explorer_down: String,
    pub explorer_parent: String,
    pub explorer_open: String,
    pub explorer_refresh: String,
    pub explorer_toggle_hidden: String,
    pub explorer_top: String,
    pub explorer_bottom: String,
    pub explorer_page_up: String,
    pub explorer_page_down: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct LlmConfig {
    pub enabled: bool,
    pub endpoint: String,
    pub model: String,
    pub max_tokens: usize,
    pub auto_explain: bool,
}

impl Default for Keybinds {
    fn default() -> Self {
        Self {
            palette: "f1,alt+space".to_string(),
            copy_input: "alt+y,ctrl+shift+c".to_string(),
            copy_output: "alt+o,alt+c".to_string(),
            paste: "ctrl+shift+v,shift+insert".to_string(),
            clear_output: "ctrl+l".to_string(),
            history_prev: "up".to_string(),
            history_next: "down".to_string(),
            quit: "alt+q,f12".to_string(),
            focus_toggle: "alt+e,f1".to_string(),
            passthrough_toggle: "alt+t".to_string(),
            explorer_up: "k,up".to_string(),
            explorer_down: "j,down".to_string(),
            explorer_parent: "h,left,backspace,-".to_string(),
            explorer_open: "l,right,enter".to_string(),
            explorer_refresh: "r".to_string(),
            explorer_toggle_hidden: ".".to_string(),
            explorer_top: "g,home".to_string(),
            explorer_bottom: "shift+g,end".to_string(),
            explorer_page_up: "ctrl+u,pageup".to_string(),
            explorer_page_down: "ctrl+d,pagedown".to_string(),
        }
    }
}

impl Default for LlmConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            endpoint: String::new(),
            model: String::new(),
            max_tokens: 2000,
            auto_explain: false,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Shortcut {
    pub name: String,
    pub trigger: String,
    pub expansion: String,
    pub description: String,
    pub bind: Option<String>,
    pub execute: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MacroEntry {
    pub name: String,
    pub template: String,
    pub description: String,
    pub bind: Option<String>,
    pub execute: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            shell: "sish".to_string(),
            history_limit: 1000,
            output_limit: 4000,
            wallpaper_enabled: true,
            keybinds: Keybinds::default(),
            llm: LlmConfig::default(),
            shortcuts: vec![
                Shortcut {
                    name: "git-status".to_string(),
                    trigger: "gs".to_string(),
                    expansion: "git status".to_string(),
                    description: "Show git status".to_string(),
                    bind: None,
                    execute: true,
                },
                Shortcut {
                    name: "git-log".to_string(),
                    trigger: "gl".to_string(),
                    expansion: "git log --oneline --decorate -n 20".to_string(),
                    description: "Show a compact git log".to_string(),
                    bind: None,
                    execute: true,
                },
                Shortcut {
                    name: "readable-ls".to_string(),
                    trigger: "ll".to_string(),
                    expansion: "ls -lah".to_string(),
                    description: "Readable listing".to_string(),
                    bind: None,
                    execute: true,
                },
            ],
            macros: vec![
                MacroEntry {
                    name: "timestamp".to_string(),
                    template: "date '+%F %T'".to_string(),
                    description: "Insert a timestamp command".to_string(),
                    bind: Some("ctrl+1".to_string()),
                    execute: true,
                },
                MacroEntry {
                    name: "list-tree".to_string(),
                    template: "ls -lahF".to_string(),
                    description: "Insert a readable listing command".to_string(),
                    bind: Some("ctrl+2".to_string()),
                    execute: false,
                },
            ],
        }
    }
}

#[allow(dead_code)]
impl Config {
    pub fn path() -> PathBuf {
        dirs::config_dir()
            .unwrap_or_else(|| PathBuf::from("."))
            .join("nicu")
            .join("config.toml")
    }

    pub fn load() -> anyhow::Result<Self> {
        let path = Self::path();
        if !path.exists() {
            let mut config = Self::default();
            config.apply_sish_overrides();
            config.save()?;
            return Ok(config);
        }

        let content = fs::read_to_string(&path)
            .with_context(|| format!("failed to read config: {}", path.display()))?;
        let mut config: Self = toml::from_str(&content)
            .with_context(|| format!("failed to parse config: {}", path.display()))?;
        let original_shell = config.shell.clone();
        config.merge_defaults();
        let migrated_legacy_keybinds = config.migrate_legacy_keybinds();

        // Historical configs used /bin/zsh. For nicu, default should be Sish.
        if should_migrate_shell_to_sish(&config.shell) {
            config.shell = "sish".to_string();
        }

        config.apply_sish_overrides();

        if config.shell != original_shell || migrated_legacy_keybinds {
            let _ = config.save();
        }
        Ok(config)
    }

    pub fn save(&self) -> anyhow::Result<()> {
        let path = Self::path();
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)
                .with_context(|| format!("failed to create {}", parent.display()))?;
        }
        let content = toml::to_string_pretty(self)?;
        fs::write(&path, content)
            .with_context(|| format!("failed to write config: {}", path.display()))?;
        Ok(())
    }

    pub fn shortcut_by_trigger(&self, trigger: &str) -> Option<&Shortcut> {
        self.shortcuts.iter().find(|shortcut| shortcut.trigger == trigger)
    }

    pub fn shortcut_by_bind(&self, bind: &str) -> Option<&Shortcut> {
        let bind = normalize_key(bind);
        self.shortcuts.iter().find(|shortcut| {
            shortcut
                .bind
                .as_ref()
                .map(|value| normalize_key(value) == bind)
                .unwrap_or(false)
        })
    }

    pub fn macro_by_bind(&self, bind: &str) -> Option<&MacroEntry> {
        let bind = normalize_key(bind);
        self.macros.iter().find(|entry| {
            entry
                .bind
                .as_ref()
                .map(|value| normalize_key(value) == bind)
                .unwrap_or(false)
        })
    }

    pub fn bind_matches(&self, configured: &str, event: &crossterm::event::KeyEvent) -> bool {
        let event_name = key_event_name(event);
        configured
            .split([',', '|'])
            .map(normalize_key)
            .filter(|value| !value.is_empty())
            .any(|value| value == event_name)
    }

    pub fn llm_ready(&self) -> bool {
        self.llm.enabled && !self.llm.endpoint.trim().is_empty()
    }

    fn merge_defaults(&mut self) {
        let defaults = Self::default();

        if self.shell.trim().is_empty() {
            self.shell = defaults.shell;
        }
        if self.history_limit == 0 {
            self.history_limit = defaults.history_limit;
        }
        if self.output_limit == 0 {
            self.output_limit = defaults.output_limit;
        }
        if self.keybinds.palette.trim().is_empty() {
            self.keybinds.palette = defaults.keybinds.palette;
        }
        if self.keybinds.copy_input.trim().is_empty() {
            self.keybinds.copy_input = defaults.keybinds.copy_input;
        }
        if self.keybinds.copy_output.trim().is_empty() {
            self.keybinds.copy_output = defaults.keybinds.copy_output;
        }
        if self.keybinds.paste.trim().is_empty() {
            self.keybinds.paste = defaults.keybinds.paste;
        }
        if self.keybinds.clear_output.trim().is_empty() {
            self.keybinds.clear_output = defaults.keybinds.clear_output;
        }
        if self.keybinds.history_prev.trim().is_empty() {
            self.keybinds.history_prev = defaults.keybinds.history_prev;
        }
        if self.keybinds.history_next.trim().is_empty() {
            self.keybinds.history_next = defaults.keybinds.history_next;
        }
        if self.keybinds.quit.trim().is_empty() {
            self.keybinds.quit = defaults.keybinds.quit;
        }
        if self.keybinds.focus_toggle.trim().is_empty() {
            self.keybinds.focus_toggle = defaults.keybinds.focus_toggle;
        }
        if self.keybinds.passthrough_toggle.trim().is_empty() {
            self.keybinds.passthrough_toggle = defaults.keybinds.passthrough_toggle;
        }
        if self.keybinds.explorer_up.trim().is_empty() {
            self.keybinds.explorer_up = defaults.keybinds.explorer_up;
        }
        if self.keybinds.explorer_down.trim().is_empty() {
            self.keybinds.explorer_down = defaults.keybinds.explorer_down;
        }
        if self.keybinds.explorer_parent.trim().is_empty() {
            self.keybinds.explorer_parent = defaults.keybinds.explorer_parent;
        }
        if self.keybinds.explorer_open.trim().is_empty() {
            self.keybinds.explorer_open = defaults.keybinds.explorer_open;
        }
        if self.keybinds.explorer_refresh.trim().is_empty() {
            self.keybinds.explorer_refresh = defaults.keybinds.explorer_refresh;
        }
        if self.keybinds.explorer_toggle_hidden.trim().is_empty() {
            self.keybinds.explorer_toggle_hidden = defaults.keybinds.explorer_toggle_hidden;
        }
        if self.keybinds.explorer_top.trim().is_empty() {
            self.keybinds.explorer_top = defaults.keybinds.explorer_top;
        }
        if self.keybinds.explorer_bottom.trim().is_empty() {
            self.keybinds.explorer_bottom = defaults.keybinds.explorer_bottom;
        }
        if self.keybinds.explorer_page_up.trim().is_empty() {
            self.keybinds.explorer_page_up = defaults.keybinds.explorer_page_up;
        }
        if self.keybinds.explorer_page_down.trim().is_empty() {
            self.keybinds.explorer_page_down = defaults.keybinds.explorer_page_down;
        }
        if self.llm.endpoint.trim().is_empty() {
            self.llm.endpoint = defaults.llm.endpoint;
        }
        if self.llm.model.trim().is_empty() {
            self.llm.model = defaults.llm.model;
        }
        if self.llm.max_tokens == 0 {
            self.llm.max_tokens = defaults.llm.max_tokens;
        }

        let mut shortcut_map = BTreeMap::new();
        for shortcut in defaults.shortcuts {
            shortcut_map.insert(shortcut.name.clone(), shortcut);
        }
        for shortcut in &self.shortcuts {
            shortcut_map.insert(shortcut.name.clone(), shortcut.clone());
        }
        self.shortcuts = shortcut_map.into_values().collect();

        let mut macro_map = BTreeMap::new();
        for entry in defaults.macros {
            macro_map.insert(entry.name.clone(), entry);
        }
        for entry in &self.macros {
            macro_map.insert(entry.name.clone(), entry.clone());
        }
        self.macros = macro_map.into_values().collect();
    }

    fn apply_sish_overrides(&mut self) {
        let exports = load_sish_exports();

        if let Some(value) = env_or_export("SISH_LLM_ENABLE", &exports) {
            self.llm.enabled = parse_bool_like(&value);
        }
        if let Some(value) = env_or_export("SISH_LLM_ENDPOINT", &exports) {
            self.llm.endpoint = value;
        }
        if let Some(value) = env_or_export("SISH_LLM_MODEL", &exports) {
            self.llm.model = value;
        }
        if let Some(value) = env_or_export("SISH_LLM_MAX_TOKENS", &exports) {
            if let Ok(parsed) = value.parse::<usize>() {
                self.llm.max_tokens = parsed.max(1);
            }
        }
        if let Some(value) = env_or_export("SISH_LLM_AUTO_EXPLAIN", &exports) {
            self.llm.auto_explain = parse_bool_like(&value);
        }
    }

    fn migrate_legacy_keybinds(&mut self) -> bool {
        let mut changed = false;

        changed |= migrate_keybind(&mut self.keybinds.palette, "f1,ctrl+g", "f1,alt+space");
        changed |= migrate_keybind(
            &mut self.keybinds.copy_input,
            "ctrl+y,ctrl+shift+c",
            "alt+y,ctrl+shift+c",
        );
        changed |= migrate_keybind(&mut self.keybinds.copy_output, "ctrl+o,alt+c", "alt+o,alt+c");
        changed |= migrate_keybind(
            &mut self.keybinds.paste,
            "ctrl+v,shift+insert",
            "ctrl+shift+v,shift+insert",
        );
        changed |= migrate_keybind(&mut self.keybinds.quit, "ctrl+q", "alt+q,f12");
        changed |= migrate_keybind(&mut self.keybinds.focus_toggle, "ctrl+e", "alt+e,f1");
        changed |= migrate_keybind(&mut self.keybinds.passthrough_toggle, "ctrl+p", "alt+t");
        changed |= migrate_keybind(&mut self.keybinds.explorer_up, "ctrl+k", "k,up");
        changed |= migrate_keybind(&mut self.keybinds.explorer_down, "ctrl+j", "j,down");
        changed |= migrate_keybind(
            &mut self.keybinds.explorer_parent,
            "ctrl+shift+h",
            "h,left,backspace,-",
        );
        changed |= migrate_keybind(&mut self.keybinds.explorer_open, "ctrl+o", "l,right,enter");
        changed |= migrate_keybind(&mut self.keybinds.explorer_refresh, "ctrl+r", "r");
        changed |= migrate_keybind(&mut self.keybinds.explorer_toggle_hidden, "ctrl+shift+.", ".");
        changed |= migrate_keybind(&mut self.keybinds.explorer_top, "ctrl+shift+k", "g,home");
        changed |= migrate_keybind(&mut self.keybinds.explorer_bottom, "ctrl+shift+j", "shift+g,end");
        changed |= migrate_keybind(
            &mut self.keybinds.explorer_page_up,
            "ctrl+shift+up",
            "ctrl+u,pageup",
        );
        changed |= migrate_keybind(
            &mut self.keybinds.explorer_page_down,
            "ctrl+shift+down",
            "ctrl+d,pagedown",
        );

        changed
    }
}

fn should_migrate_shell_to_sish(shell: &str) -> bool {
    if std::env::var("SISH_NICU_ALLOW_NON_SISH")
        .ok()
        .map(|v| v == "1")
        .unwrap_or(false)
    {
        return false;
    }

    let trimmed = shell.trim();
    if trimmed.is_empty() {
        return false;
    }

    let program = shell_words::split(trimmed)
        .ok()
        .and_then(|tokens| tokens.first().cloned())
        .unwrap_or_else(|| trimmed.to_string());

    matches!(
        program.as_str(),
        "zsh" | "/bin/zsh" | "/usr/bin/zsh"
    )
}

fn load_sish_exports() -> BTreeMap<String, String> {
    let path = dirs::home_dir()
        .unwrap_or_else(|| PathBuf::from("."))
        .join(".sishrc");

    let Ok(content) = fs::read_to_string(path) else {
        return BTreeMap::new();
    };

    content
        .lines()
        .filter_map(parse_export_line)
        .collect()
}

fn parse_export_line(line: &str) -> Option<(String, String)> {
    let trimmed = line.trim();
    if trimmed.is_empty() || trimmed.starts_with('#') {
        return None;
    }

    let rest = trimmed.strip_prefix("export ").unwrap_or(trimmed);
    let (key, value) = rest.split_once('=')?;
    let key = key.trim();
    if key.is_empty() || !key.starts_with("SISH_") {
        return None;
    }

    Some((key.to_string(), unquote_shell_value(value.trim())))
}

fn unquote_shell_value(value: &str) -> String {
    if value.len() >= 2 && value.starts_with('"') && value.ends_with('"') {
        return value[1..value.len() - 1].to_string();
    }

    if value.len() >= 2 && value.starts_with('\'') && value.ends_with('\'') {
        return value[1..value.len() - 1].replace("'\\''", "'");
    }

    value.to_string()
}

fn env_or_export(key: &str, exports: &BTreeMap<String, String>) -> Option<String> {
    std::env::var(key)
        .ok()
        .filter(|value| !value.trim().is_empty())
        .or_else(|| exports.get(key).cloned().filter(|value| !value.trim().is_empty()))
}

fn parse_bool_like(value: &str) -> bool {
    matches!(
        value.trim().to_ascii_lowercase().as_str(),
        "1" | "true" | "yes" | "on"
    )
}

fn migrate_keybind(current: &mut String, legacy: &str, replacement: &str) -> bool {
    if normalize_key(current) == normalize_key(legacy) {
        *current = replacement.to_string();
        return true;
    }

    false
}

#[allow(dead_code)]
pub fn normalize_key(input: &str) -> String {
    input.trim().to_lowercase().replace(' ', "")
}

#[allow(dead_code)]
pub fn key_event_name(event: &crossterm::event::KeyEvent) -> String {
    use crossterm::event::{KeyCode, KeyModifiers};

    let mut parts = Vec::new();
    if event.modifiers.contains(KeyModifiers::CONTROL) {
        parts.push("ctrl".to_string());
    }
    if event.modifiers.contains(KeyModifiers::ALT) {
        parts.push("alt".to_string());
    }
    if event.modifiers.contains(KeyModifiers::SHIFT) {
        parts.push("shift".to_string());
    }

    let key = match event.code {
        KeyCode::Char(' ') => "space".to_string(),
        KeyCode::Char(c) => c.to_string(),
        KeyCode::Enter => "enter".to_string(),
        KeyCode::Esc => "esc".to_string(),
        KeyCode::Backspace => "backspace".to_string(),
        KeyCode::Delete => "delete".to_string(),
        KeyCode::Left => "left".to_string(),
        KeyCode::Right => "right".to_string(),
        KeyCode::Up => "up".to_string(),
        KeyCode::Down => "down".to_string(),
        KeyCode::Home => "home".to_string(),
        KeyCode::End => "end".to_string(),
        KeyCode::PageUp => "pageup".to_string(),
        KeyCode::PageDown => "pagedown".to_string(),
        KeyCode::Tab => "tab".to_string(),
        KeyCode::F(n) => format!("f{n}"),
        other => format!("{:?}", other).to_lowercase(),
    };

    if parts.is_empty() {
        key
    } else {
        parts.push(key);
        parts.join("+")
    }
}
