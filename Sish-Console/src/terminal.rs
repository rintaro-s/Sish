//! Terminal widget module
//!
//! Wraps VTE terminal widget with Sish-specific functionality.

use vte4::prelude::*;
use vte4::Terminal;
use std::path::PathBuf;

use crate::config::TerminalConfig;

/// Sish Terminal widget wrapper
pub struct SishTerminal {
    terminal: Terminal,
}

impl SishTerminal {
    /// Create a new terminal widget
    pub fn new(config: &TerminalConfig) -> Self {
        let terminal = Terminal::new();
        
        // Configure terminal appearance
        terminal.set_font_scale(1.0);
        terminal.set_scrollback_lines(config.scrollback_lines as i64);
        terminal.set_cursor_blink_mode(if config.cursor_blink {
            vte4::CursorBlinkMode::On
        } else {
            vte4::CursorBlinkMode::Off
        });
        
        // Set cursor shape
        let cursor_shape = match config.cursor_shape.as_str() {
            "block" => vte4::CursorShape::Block,
            "ibeam" => vte4::CursorShape::Ibeam,
            "underline" => vte4::CursorShape::Underline,
            _ => vte4::CursorShape::Block,
        };
        terminal.set_cursor_shape(cursor_shape);
        
        // Set font with fallback
        let font_desc = pango::FontDescription::from_string(
            &format!("{} {}", config.font_family, config.font_size)
        );
        terminal.set_font(Some(&font_desc));
        
        // Enable transparency
        terminal.set_clear_background(false);
        
        // Spawn the shell
        let shell = resolve_shell(&config.shell);
        
        terminal.spawn_async(
            vte4::PtyFlags::DEFAULT,
            None, // working directory (use current)
            &[shell.as_str()],
            &[], // environment
            glib::SpawnFlags::DEFAULT,
            || {},
            -1, // timeout
            None::<&gio::Cancellable>,
            |result| {
                if let Err(err) = result {
                    log::error!("Failed to spawn shell: {}", err);
                }
            },
        );
        
        // Handle window title changes
        terminal.connect_window_title_changed(|term| {
            if let Some(title) = term.window_title() {
                log::debug!("Terminal title changed: {}", title);
            }
        });
        
        // Handle child exit
        terminal.connect_child_exited(|_term, _status| {
            log::info!("Shell exited");
            // You could restart the shell or close the window here
        });
        
        Self { terminal }
    }
    
    /// Get the underlying widget
    pub fn widget(&self) -> &Terminal {
        &self.terminal
    }
    
    /// Feed text to the terminal
    pub fn feed(&self, text: &str) {
        self.terminal.feed(text.as_bytes());
    }
    
    /// Feed text to the terminal as if typed by user
    pub fn feed_child(&self, text: &str) {
        self.terminal.feed_child(text.as_bytes());
    }
    
    /// Copy selected text to clipboard
    pub fn copy_clipboard(&self) {
        self.terminal.copy_clipboard_format(vte4::Format::Text);
    }
    
    /// Paste from clipboard
    pub fn paste_clipboard(&self) {
        self.terminal.paste_clipboard();
    }
    
    /// Select all text
    pub fn select_all(&self) {
        self.terminal.select_all();
    }
    
    /// Zoom in
    pub fn zoom_in(&self) {
        let scale = self.terminal.font_scale();
        self.terminal.set_font_scale(scale * 1.1);
    }
    
    /// Zoom out
    pub fn zoom_out(&self) {
        let scale = self.terminal.font_scale();
        self.terminal.set_font_scale(scale / 1.1);
    }
    
    /// Reset zoom
    pub fn zoom_reset(&self) {
        self.terminal.set_font_scale(1.0);
    }
}

fn resolve_shell(config_shell: &str) -> String {
    // Empty: use user shell
    if config_shell.trim().is_empty() {
        return std::env::var("SHELL").unwrap_or_else(|_| "/bin/bash".to_string());
    }

    // "sish": try ./sish in workspace first, then PATH, else fallback
    if config_shell.trim() == "sish" {
        // Try workspace ./sish first
        let workspace_sish = std::env::current_dir()
            .ok()
            .and_then(|mut p| {
                p.pop(); // go up from Sish-Console to Sish
                p.push("sish");
                if p.is_file() {
                    Some(p.to_string_lossy().to_string())
                } else {
                    None
                }
            });
        if let Some(path) = workspace_sish {
            log::info!("Using workspace sish: {}", path);
            return path;
        }
        
        // Try PATH
        if let Some(path) = find_in_path("sish") {
            log::info!("Using sish from PATH: {}", path);
            return path;
        }
        
        log::warn!("'sish' not found in workspace or PATH; falling back to $SHELL");
        return std::env::var("SHELL").unwrap_or_else(|_| "/bin/bash".to_string());
    }

    config_shell.to_string()
}

fn find_in_path(cmd: &str) -> Option<String> {
    let paths = std::env::var_os("PATH")?;
    for p in std::env::split_paths(&paths) {
        let mut candidate = PathBuf::from(p);
        candidate.push(cmd);
        if candidate.is_file() {
            return Some(candidate.to_string_lossy().to_string());
        }
    }
    None
}
