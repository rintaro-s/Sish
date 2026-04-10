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
    pub shortcuts: Vec<Shortcut>,
    pub macros: Vec<MacroEntry>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Keybinds {
    pub palette: String,
    pub copy_input: String,
    pub copy_output: String,
    pub paste: String,
    pub clear_output: String,
    pub history_prev: String,
    pub history_next: String,
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
            keybinds: Keybinds {
                palette: "f1,ctrl+g".to_string(),
                copy_input: "ctrl+y,ctrl+shift+c".to_string(),
                copy_output: "ctrl+o,alt+c".to_string(),
                paste: "ctrl+v,shift+insert".to_string(),
                clear_output: "ctrl+l".to_string(),
                history_prev: "up".to_string(),
                history_next: "down".to_string(),
            },
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
            let config = Self::default();
            config.save()?;
            return Ok(config);
        }

        let content = fs::read_to_string(&path)
            .with_context(|| format!("failed to read config: {}", path.display()))?;
        let mut config: Self = toml::from_str(&content)
            .with_context(|| format!("failed to parse config: {}", path.display()))?;
        let original_shell = config.shell.clone();
        config.merge_defaults();

        // Historical configs used /bin/zsh. For nicu, default should be Sish.
        if should_migrate_shell_to_sish(&config.shell) {
            config.shell = "sish".to_string();
        }

        if config.shell != original_shell {
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
