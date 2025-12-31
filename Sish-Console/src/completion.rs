//! Command completion UI module

use gtk4::prelude::*;
use gtk4::{Popover, ListBox, ListBoxRow, Label, ScrolledWindow, gdk};
use vte4::Terminal;

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
    
    /// Show completion popup with candidates
    pub fn show(&mut self, candidates: Vec<String>, x: f64, y: f64) {
        // Clear existing items
        while let Some(child) = self.listbox.first_child() {
            self.listbox.remove(&child);
        }
        
        if candidates.is_empty() {
            self.hide();
            return;
        }
        
        self.candidates = candidates;
        
        // Add candidates to list
        for candidate in &self.candidates {
            let row = ListBoxRow::new();
            let label = Label::builder()
                .label(candidate)
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
    // Try to get completions from shell
    // For now, return mock data
    // TODO: Implement actual shell completion via compgen or zsh completion
    
    if command.is_empty() {
        return Vec::new();
    }
    
    // Mock completions for common commands
    let parts: Vec<&str> = command.split_whitespace().collect();
    let last_word = parts.last().unwrap_or(&"");
    
    let common_commands = vec![
        "ls", "cd", "pwd", "cat", "grep", "find", "echo", "mkdir", "rm", "cp", "mv",
        "touch", "vim", "nano", "less", "head", "tail", "sort", "uniq", "wc",
        "git", "cargo", "python", "node", "npm", "ssh", "scp", "rsync",
    ];
    
    if parts.len() == 1 {
        // Complete command names
        common_commands
            .iter()
            .filter(|cmd| cmd.starts_with(last_word))
            .map(|s| s.to_string())
            .collect()
    } else {
        // Complete file paths
        // TODO: Implement directory completion
        Vec::new()
    }
}
