//! Shortcuts module
//!
//! Handles command shortcuts and expansions with environment variable support.

use std::collections::HashMap;
use std::env;

/// Built-in shortcuts (empty by default - users configure via sish-config)
pub fn get_default_shortcuts() -> HashMap<String, ShortcutEntry> {
    HashMap::new()
}

/// Shortcut entry with metadata
#[derive(Debug, Clone)]
pub struct ShortcutEntry {
    pub expansion: String,
    pub description: String,
    pub category: String,
}

/// Environment variable in shortcut
#[derive(Debug, Clone)]
pub struct EnvVar {
    pub name: String,
    pub value: String,
    pub description: String,
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
    
    /// Expand shortcut with environment variable substitution
    pub fn expand_with_env(&self, trigger: &str) -> Option<String> {
        self.shortcuts.get(trigger).map(|e| {
            let mut expansion = e.expansion.clone();
            
            // Replace environment variables in format $VAR or ${VAR}
            for (key, value) in env::vars() {
                let pattern_braces = format!("${{{}}}", key);
                let pattern_dollar = format!("${}", key);
                expansion = expansion.replace(&pattern_braces, &value);
                expansion = expansion.replace(&pattern_dollar, &value);
            }
            
            expansion
        })
    }
    
    /// Get environment variables used in shortcuts
    pub fn get_env_vars(&self) -> Vec<EnvVar> {
        let mut vars = Vec::new();
        let env_vars: HashMap<String, String> = env::vars().collect();
        
        for (_, entry) in &self.shortcuts {
            for (key, value) in &env_vars {
                let pattern_braces = format!("${{{}}}", key);
                let pattern_dollar = format!("${}", key);
                if entry.expansion.contains(&pattern_braces) || 
                   entry.expansion.contains(&pattern_dollar) {
                    if !vars.iter().any(|v: &EnvVar| v.name == *key) {
                        vars.push(EnvVar {
                            name: key.clone(),
                            value: value.clone(),
                            description: format!("Environment variable: {}", key),
                        });
                    }
                }
            }
        }
        
        vars
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
    fn test_manager_creation() {
        let manager = ShortcutManager::new();
        // Default manager should be empty (users configure via sish-config)
        assert_eq!(manager.all().len(), 0);
    }
    
    #[test]
    fn test_add_and_expand() {
        let mut manager = ShortcutManager::new();
        manager.add("gs", "git status", "Git status", "Git");
        assert_eq!(manager.expand("gs"), Some("git status"));
        assert_eq!(manager.expand("nonexistent"), None);
    }
}
