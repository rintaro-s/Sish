//! Command completion UI module

use gtk4::prelude::*;
use gtk4::{Popover, ListBox, ListBoxRow, Label, ScrolledWindow, gdk};
use vte4::Terminal;
use std::fs;

/// Completion popup widget
pub struct CompletionPopup {
    popover: Popover,
    listbox: ListBox,
    candidates: Vec<String>,
}

impl CompletionPopup {
    /// Create new completion popup attached to terminal
    pub fn new(terminal: &Terminal) -> Self {
        let popover = Popover::builder()
            .autohide(true)
            .has_arrow(false)
            .build();
        
        popover.set_parent(terminal);
        
        let scrolled = ScrolledWindow::builder()
            .min_content_width(300)
            .max_content_height(200)
            .hscrollbar_policy(gtk4::PolicyType::Never)
            .vscrollbar_policy(gtk4::PolicyType::Automatic)
            .build();
        
        let listbox = ListBox::builder()
            .selection_mode(gtk4::SelectionMode::Single)
            .build();
        
        // Style the listbox
        listbox.add_css_class("completion-list");
        
        scrolled.set_child(Some(&listbox));
        popover.set_child(Some(&scrolled));
        
        Self {
            popover,
            listbox,
            candidates: Vec::new(),
        }
    }
    
    /// Show completion popup with candidates (limited to top 5)
    pub fn show(&mut self, candidates: Vec<String>, x: f64, y: f64) {
        // Clear existing items
        while let Some(child) = self.listbox.first_child() {
            self.listbox.remove(&child);
        }
        
        if candidates.is_empty() {
            self.hide();
            return;
        }
        
        // Limit to top 5 candidates
        let limited_candidates: Vec<String> = candidates.iter()
            .take(5)
            .cloned()
            .collect();
        
        self.candidates = limited_candidates;
        
        // Add candidates to list
        for (idx, candidate) in self.candidates.iter().enumerate() {
            let row = ListBoxRow::new();
            // Add numbering for quick reference (1-5)
            let label_text = format!("{}. {}", idx + 1, candidate);
            let label = Label::builder()
                .label(&label_text)
                .xalign(0.0)
                .margin_start(8)
                .margin_end(8)
                .margin_top(4)
                .margin_bottom(4)
                .build();
            
            row.set_child(Some(&label));
            self.listbox.append(&row);
        }
        
        // Select first item
        self.listbox.select_row(self.listbox.row_at_index(0).as_ref());
        
        // Position and show popup
        let rect = gdk::Rectangle::new(x as i32, y as i32, 1, 1);
        self.popover.set_pointing_to(Some(&rect));
        self.popover.popup();
    }
    
    /// Hide completion popup
    pub fn hide(&self) {
        self.popover.popdown();
    }
    
    /// Get currently selected candidate
    pub fn selected_candidate(&self) -> Option<String> {
        self.listbox.selected_row()
            .and_then(|row| {
                let idx = row.index();
                self.candidates.get(idx as usize).cloned()
            })
    }
    
    /// Move selection up
    pub fn select_previous(&self) {
        if let Some(row) = self.listbox.selected_row() {
            let idx = row.index();
            if idx > 0 {
                self.listbox.select_row(self.listbox.row_at_index(idx - 1).as_ref());
            }
        }
    }
    
    /// Move selection down
    pub fn select_next(&self) {
        if let Some(row) = self.listbox.selected_row() {
            let idx = row.index();
            let n_items = self.listbox.observe_children().n_items();
            if idx < (n_items as i32) - 1 {
                self.listbox.select_row(self.listbox.row_at_index(idx + 1).as_ref());
            }
        }
    }
    
    /// Check if popup is visible
    pub fn is_visible(&self) -> bool {
        self.popover.is_visible()
    }
}

/// Get completion candidates from shell command
pub fn get_completions(command: &str) -> Vec<String> {
    if command.is_empty() {
        return Vec::new();
    }
    
    let parts: Vec<&str> = command.split_whitespace().collect();
    let last_word = parts.last().unwrap_or(&"");
    
    let common_commands = vec![
        "ls", "cd", "pwd", "cat", "grep", "find", "echo", "mkdir", "rm", "cp", "mv",
        "touch", "vim", "nano", "less", "head", "tail", "sort", "uniq", "wc",
        "git", "cargo", "python", "node", "npm", "ssh", "scp", "rsync",
        "history", "help", "man", "apt", "sudo", "make", "chmod", "chown",
    ];
    
    if parts.len() == 1 {
        // Complete command names (limited to top 5)
        common_commands
            .iter()
            .filter(|cmd| cmd.starts_with(last_word))
            .map(|s| s.to_string())
            .take(5)
            .collect()
    } else {
        // Complete file paths
        get_file_completions(last_word)
    }
}

/// Get file/directory completion candidates
pub fn get_file_completions(partial_path: &str) -> Vec<String> {
    let mut completions = Vec::new();
    
    // Determine the directory and prefix to match
    let (dir_path, prefix) = if partial_path.contains('/') {
        let parts: Vec<&str> = partial_path.rsplitn(2, '/').collect();
        let prefix = parts.get(0).map(|s| *s).unwrap_or("");
        let dir = if parts.len() > 1 {
            parts[1].to_string()
        } else {
            ".".to_string()
        };
        (dir, prefix.to_string())
    } else {
        (".".to_string(), partial_path.to_string())
    };
    
    // List directory contents
    if let Ok(entries) = fs::read_dir(&dir_path) {
        for entry in entries.flatten() {
            if let Ok(name) = entry.file_name().into_string() {
                if name.starts_with(&prefix) {
                    // Add trailing slash for directories
                    let is_dir = entry.metadata()
                        .map(|m| m.is_dir())
                        .unwrap_or(false);
                    let completion = if is_dir {
                        format!("{}/", name)
                    } else {
                        name
                    };
                    completions.push(completion);
                }
            }
        }
    }
    
    // Sort and limit to top 5
    completions.sort();
    completions.iter().take(5).cloned().collect()
}
