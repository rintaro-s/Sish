//! Shortcuts module
//!
//! Handles command shortcuts and expansions.

use std::collections::HashMap;

/// Built-in shortcuts
pub fn get_default_shortcuts() -> HashMap<String, ShortcutEntry> {
    let mut shortcuts = HashMap::new();
    
    // Git shortcuts
    shortcuts.insert("g".to_string(), ShortcutEntry {
        expansion: "git".to_string(),
        description: "Git version control".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gs".to_string(), ShortcutEntry {
        expansion: "git status".to_string(),
        description: "Show git status".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("ga".to_string(), ShortcutEntry {
        expansion: "git add".to_string(),
        description: "Stage changes".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gaa".to_string(), ShortcutEntry {
        expansion: "git add --all".to_string(),
        description: "Stage all changes".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gc".to_string(), ShortcutEntry {
        expansion: "git commit".to_string(),
        description: "Commit changes".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gcm".to_string(), ShortcutEntry {
        expansion: "git commit -m".to_string(),
        description: "Commit with message".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gp".to_string(), ShortcutEntry {
        expansion: "git push".to_string(),
        description: "Push to remote".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gpl".to_string(), ShortcutEntry {
        expansion: "git pull".to_string(),
        description: "Pull from remote".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gl".to_string(), ShortcutEntry {
        expansion: "git log --oneline".to_string(),
        description: "Show log".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gd".to_string(), ShortcutEntry {
        expansion: "git diff".to_string(),
        description: "Show diff".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gco".to_string(), ShortcutEntry {
        expansion: "git checkout".to_string(),
        description: "Checkout branch".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gcb".to_string(), ShortcutEntry {
        expansion: "git checkout -b".to_string(),
        description: "Create and checkout branch".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gbr".to_string(), ShortcutEntry {
        expansion: "git branch".to_string(),
        description: "List branches".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gst".to_string(), ShortcutEntry {
        expansion: "git stash".to_string(),
        description: "Stash changes".to_string(),
        category: "Git".to_string(),
    });
    shortcuts.insert("gstp".to_string(), ShortcutEntry {
        expansion: "git stash pop".to_string(),
        description: "Pop stashed changes".to_string(),
        category: "Git".to_string(),
    });
    
    // Package managers
    shortcuts.insert("a-ins".to_string(), ShortcutEntry {
        expansion: "sudo apt install".to_string(),
        description: "Install package (apt)".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("a-up".to_string(), ShortcutEntry {
        expansion: "sudo apt update && sudo apt upgrade".to_string(),
        description: "Update system (apt)".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("a-rm".to_string(), ShortcutEntry {
        expansion: "sudo apt remove".to_string(),
        description: "Remove package (apt)".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("a-search".to_string(), ShortcutEntry {
        expansion: "apt search".to_string(),
        description: "Search packages (apt)".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("p-ins".to_string(), ShortcutEntry {
        expansion: "pip install".to_string(),
        description: "Install Python package".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("p-up".to_string(), ShortcutEntry {
        expansion: "pip install --upgrade".to_string(),
        description: "Upgrade Python package".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("n-ins".to_string(), ShortcutEntry {
        expansion: "npm install".to_string(),
        description: "Install Node package".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("n-insg".to_string(), ShortcutEntry {
        expansion: "npm install -g".to_string(),
        description: "Install Node package globally".to_string(),
        category: "Package Manager".to_string(),
    });
    shortcuts.insert("c-ins".to_string(), ShortcutEntry {
        expansion: "cargo install".to_string(),
        description: "Install Rust crate".to_string(),
        category: "Package Manager".to_string(),
    });
    
    // Docker
    shortcuts.insert("dk".to_string(), ShortcutEntry {
        expansion: "docker".to_string(),
        description: "Docker command".to_string(),
        category: "Docker".to_string(),
    });
    shortcuts.insert("dkc".to_string(), ShortcutEntry {
        expansion: "docker-compose".to_string(),
        description: "Docker Compose".to_string(),
        category: "Docker".to_string(),
    });
    shortcuts.insert("dkps".to_string(), ShortcutEntry {
        expansion: "docker ps".to_string(),
        description: "List containers".to_string(),
        category: "Docker".to_string(),
    });
    shortcuts.insert("dkpsa".to_string(), ShortcutEntry {
        expansion: "docker ps -a".to_string(),
        description: "List all containers".to_string(),
        category: "Docker".to_string(),
    });
    shortcuts.insert("dki".to_string(), ShortcutEntry {
        expansion: "docker images".to_string(),
        description: "List images".to_string(),
        category: "Docker".to_string(),
    });
    shortcuts.insert("dkrm".to_string(), ShortcutEntry {
        expansion: "docker rm".to_string(),
        description: "Remove container".to_string(),
        category: "Docker".to_string(),
    });
    shortcuts.insert("dkrmi".to_string(), ShortcutEntry {
        expansion: "docker rmi".to_string(),
        description: "Remove image".to_string(),
        category: "Docker".to_string(),
    });
    shortcuts.insert("dkex".to_string(), ShortcutEntry {
        expansion: "docker exec -it".to_string(),
        description: "Execute in container".to_string(),
        category: "Docker".to_string(),
    });
    
    // Navigation
    shortcuts.insert("..".to_string(), ShortcutEntry {
        expansion: "cd ..".to_string(),
        description: "Go up one directory".to_string(),
        category: "Navigation".to_string(),
    });
    shortcuts.insert("...".to_string(), ShortcutEntry {
        expansion: "cd ../..".to_string(),
        description: "Go up two directories".to_string(),
        category: "Navigation".to_string(),
    });
    shortcuts.insert("....".to_string(), ShortcutEntry {
        expansion: "cd ../../..".to_string(),
        description: "Go up three directories".to_string(),
        category: "Navigation".to_string(),
    });
    shortcuts.insert("-".to_string(), ShortcutEntry {
        expansion: "cd -".to_string(),
        description: "Go to previous directory".to_string(),
        category: "Navigation".to_string(),
    });
    
    // Listing
    shortcuts.insert("l".to_string(), ShortcutEntry {
        expansion: "ls -la".to_string(),
        description: "List all files (long)".to_string(),
        category: "Files".to_string(),
    });
    shortcuts.insert("ll".to_string(), ShortcutEntry {
        expansion: "ls -l".to_string(),
        description: "List files (long)".to_string(),
        category: "Files".to_string(),
    });
    shortcuts.insert("la".to_string(), ShortcutEntry {
        expansion: "ls -la".to_string(),
        description: "List all files".to_string(),
        category: "Files".to_string(),
    });
    shortcuts.insert("lt".to_string(), ShortcutEntry {
        expansion: "ls -lt".to_string(),
        description: "List by time".to_string(),
        category: "Files".to_string(),
    });
    shortcuts.insert("ltr".to_string(), ShortcutEntry {
        expansion: "ls -ltr".to_string(),
        description: "List by time (reverse)".to_string(),
        category: "Files".to_string(),
    });
    
    // Common commands
    shortcuts.insert("cls".to_string(), ShortcutEntry {
        expansion: "clear".to_string(),
        description: "Clear screen".to_string(),
        category: "Common".to_string(),
    });
    shortcuts.insert("py".to_string(), ShortcutEntry {
        expansion: "python3".to_string(),
        description: "Python 3".to_string(),
        category: "Common".to_string(),
    });
    shortcuts.insert("py2".to_string(), ShortcutEntry {
        expansion: "python2".to_string(),
        description: "Python 2".to_string(),
        category: "Common".to_string(),
    });
    shortcuts.insert("v".to_string(), ShortcutEntry {
        expansion: "nvim".to_string(),
        description: "Neovim".to_string(),
        category: "Common".to_string(),
    });
    shortcuts.insert("vi".to_string(), ShortcutEntry {
        expansion: "nvim".to_string(),
        description: "Neovim".to_string(),
        category: "Common".to_string(),
    });
    
    // Systemd
    shortcuts.insert("sc".to_string(), ShortcutEntry {
        expansion: "sudo systemctl".to_string(),
        description: "Systemctl".to_string(),
        category: "System".to_string(),
    });
    shortcuts.insert("scr".to_string(), ShortcutEntry {
        expansion: "sudo systemctl restart".to_string(),
        description: "Restart service".to_string(),
        category: "System".to_string(),
    });
    shortcuts.insert("scs".to_string(), ShortcutEntry {
        expansion: "sudo systemctl status".to_string(),
        description: "Service status".to_string(),
        category: "System".to_string(),
    });
    shortcuts.insert("sce".to_string(), ShortcutEntry {
        expansion: "sudo systemctl enable".to_string(),
        description: "Enable service".to_string(),
        category: "System".to_string(),
    });
    shortcuts.insert("scd".to_string(), ShortcutEntry {
        expansion: "sudo systemctl disable".to_string(),
        description: "Disable service".to_string(),
        category: "System".to_string(),
    });
    
    shortcuts
}

/// Shortcut entry with metadata
#[derive(Debug, Clone)]
pub struct ShortcutEntry {
    pub expansion: String,
    pub description: String,
    pub category: String,
}

/// Shortcut manager
pub struct ShortcutManager {
    shortcuts: HashMap<String, ShortcutEntry>,
}

impl ShortcutManager {
    /// Create a new shortcut manager with default shortcuts
    pub fn new() -> Self {
        Self {
            shortcuts: get_default_shortcuts(),
        }
    }
    
    /// Add a custom shortcut
    pub fn add(&mut self, trigger: &str, expansion: &str, description: &str, category: &str) {
        self.shortcuts.insert(trigger.to_string(), ShortcutEntry {
            expansion: expansion.to_string(),
            description: description.to_string(),
            category: category.to_string(),
        });
    }
    
    /// Remove a shortcut
    pub fn remove(&mut self, trigger: &str) -> Option<ShortcutEntry> {
        self.shortcuts.remove(trigger)
    }
    
    /// Get expansion for a trigger
    pub fn expand(&self, trigger: &str) -> Option<&str> {
        self.shortcuts.get(trigger).map(|e| e.expansion.as_str())
    }
    
    /// Get all shortcuts
    pub fn all(&self) -> &HashMap<String, ShortcutEntry> {
        &self.shortcuts
    }
    
    /// Get shortcuts by category
    pub fn by_category(&self, category: &str) -> Vec<(&String, &ShortcutEntry)> {
        self.shortcuts
            .iter()
            .filter(|(_, entry)| entry.category == category)
            .collect()
    }
    
    /// Get all categories
    pub fn categories(&self) -> Vec<String> {
        let mut cats: Vec<String> = self.shortcuts
            .values()
            .map(|e| e.category.clone())
            .collect();
        cats.sort();
        cats.dedup();
        cats
    }
    
    /// Search shortcuts
    pub fn search(&self, query: &str) -> Vec<(&String, &ShortcutEntry)> {
        let query_lower = query.to_lowercase();
        self.shortcuts
            .iter()
            .filter(|(trigger, entry)| {
                trigger.to_lowercase().contains(&query_lower) ||
                entry.expansion.to_lowercase().contains(&query_lower) ||
                entry.description.to_lowercase().contains(&query_lower)
            })
            .collect()
    }
}

impl Default for ShortcutManager {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_expand() {
        let manager = ShortcutManager::new();
        assert_eq!(manager.expand("gs"), Some("git status"));
        assert_eq!(manager.expand("nonexistent"), None);
    }
    
    #[test]
    fn test_search() {
        let manager = ShortcutManager::new();
        let results = manager.search("git");
        assert!(!results.is_empty());
    }
}
