//! Configuration module for Sish Console
//!
//! Handles loading and saving user preferences.

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

/// Main configuration structure
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    /// Window settings
    pub window: WindowConfig,
    /// Terminal settings
    pub terminal: TerminalConfig,
    /// Character settings
    pub character: CharacterConfig,
    /// Shortcut settings
    pub shortcuts: ShortcutsConfig,
    /// LLM integration settings
    pub llm: LlmConfig,
    /// Theme settings
    pub theme: ThemeConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WindowConfig {
    pub width: i32,
    pub height: i32,
    pub maximized: bool,
    pub opacity: f64,
    pub background_image: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TerminalConfig {
    pub shell: String,
    pub font_family: String,
    pub font_size: u32,
    pub scrollback_lines: u32,
    pub cursor_blink: bool,
    pub cursor_shape: String,
    pub bell_audible: bool,
    pub background_opacity: f64,
    pub letter_spacing: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CharacterConfig {
    pub enabled: bool,
    pub position: String, // "bottom-right", "bottom-left", etc.
    pub size: u32,
    pub opacity: f64,
    pub animations_enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShortcutsConfig {
    pub enabled: bool,
    pub custom_shortcuts: Vec<CustomShortcut>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CustomShortcut {
    pub trigger: String,
    pub expansion: String,
    pub description: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LlmConfig {
    pub enabled: bool,
    pub endpoint: String,
    pub model: String,
    pub max_tokens: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ThemeConfig {
    pub name: String,
    pub background_color: String,
    pub foreground_color: String,
    pub cursor_color: String,
    pub selection_color: String,
    /// ANSI color palette (16 colors)
    pub palette: Vec<String>,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            window: WindowConfig {
                width: 1200,
                height: 800,
                maximized: false,
                opacity: 0.95,
                background_image: None,
            },
            terminal: TerminalConfig {
                shell: "sish".to_string(),
                font_family: "Cascadia Code, JetBrains Mono, Fira Code, Consolas, monospace".to_string(),
                font_size: 13,
                scrollback_lines: 10000,
                cursor_blink: true,
                cursor_shape: "block".to_string(),
                bell_audible: false,
                background_opacity: 0.2,
                letter_spacing: 0.0,
            },
            character: CharacterConfig {
                enabled: false,
                position: "bottom-right".to_string(),
                size: 150,
                opacity: 0.9,
                animations_enabled: true,
            },
            shortcuts: ShortcutsConfig {
                enabled: true,
                custom_shortcuts: vec![
                    CustomShortcut {
                        trigger: "g".to_string(),
                        expansion: "git".to_string(),
                        description: "Git command".to_string(),
                    },
                    CustomShortcut {
                        trigger: "gs".to_string(),
                        expansion: "git status".to_string(),
                        description: "Git status".to_string(),
                    },
                    CustomShortcut {
                        trigger: "a-ins".to_string(),
                        expansion: "sudo apt install".to_string(),
                        description: "APT install".to_string(),
                    },
                ],
            },
            llm: LlmConfig {
                enabled: false,
                endpoint: "http://localhost:1234/v1".to_string(),
                model: "local-model".to_string(),
                max_tokens: 500,
            },
            theme: ThemeConfig {
                name: "Sish Default".to_string(),
                // Modern blue/gray theme with good readability
                background_color: "rgba(26, 32, 44, 0.85)".to_string(),
                foreground_color: "#e2e8f0".to_string(),
                cursor_color: "#60a5fa".to_string(),
                selection_color: "rgba(96, 165, 250, 0.3)".to_string(),
                palette: vec![
                    "#111827".to_string(), // Black
                    "#ef4444".to_string(), // Red
                    "#22c55e".to_string(), // Green
                    "#eab308".to_string(), // Yellow
                    "#3b82f6".to_string(), // Blue
                    "#d946ef".to_string(), // Magenta
                    "#06b6d4".to_string(), // Cyan
                    "#f3f4f6".to_string(), // White
                    "#374151".to_string(), // Bright Black
                    "#ef4444".to_string(), // Bright Red
                    "#22c55e".to_string(), // Bright Green
                    "#eab308".to_string(), // Bright Yellow
                    "#3b82f6".to_string(), // Bright Blue
                    "#d946ef".to_string(), // Bright Magenta
                    "#06b6d4".to_string(), // Bright Cyan
                    "#111827".to_string(), // Bright White
                ],
            },
        }
    }
}

impl Config {
    /// Get the config file path
    pub fn config_path() -> PathBuf {
        dirs::config_dir()
            .unwrap_or_else(|| PathBuf::from("."))
            .join("sish-console")
            .join("config.toml")
    }

    /// Load configuration from file
    pub fn load() -> Self {
        let path = Self::config_path();
        
        if path.exists() {
            match fs::read_to_string(&path) {
                Ok(content) => {
                    match toml::from_str(&content) {
                        Ok(config) => {
                            log::info!("Loaded config from {:?}", path);
                            return config;
                        }
                        Err(e) => {
                            log::warn!("Failed to parse config: {}", e);
                        }
                    }
                }
                Err(e) => {
                    log::warn!("Failed to read config file: {}", e);
                }
            }
        }
        
        log::info!("Using default configuration");
        Self::default()
    }

    /// Save configuration to file
    pub fn save(&self) -> Result<(), Box<dyn std::error::Error>> {
        let path = Self::config_path();
        
        // Create parent directories if they don't exist
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        
        let content = toml::to_string_pretty(self)?;
        fs::write(&path, content)?;
        
        log::info!("Saved config to {:?}", path);
        Ok(())
    }
}
