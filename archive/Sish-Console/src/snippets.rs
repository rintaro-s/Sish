//! Smart Snippets module
//!
//! Allows saving and reusing command templates.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

/// A command snippet with variables
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Snippet {
    /// Unique name for the snippet
    pub name: String,
    /// Description of what the snippet does
    pub description: String,
    /// The command template with variables like ${var_name}
    pub template: String,
    /// Variable definitions
    pub variables: Vec<SnippetVariable>,
    /// Category for organization
    pub category: String,
    /// Tags for searching
    pub tags: Vec<String>,
    /// Number of times this snippet has been used
    pub use_count: u32,
    /// Last used timestamp
    pub last_used: Option<i64>,
}

/// A variable in a snippet
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SnippetVariable {
    /// Variable name (used in template as ${name})
    pub name: String,
    /// Human-readable label
    pub label: String,
    /// Default value
    pub default: String,
    /// Variable type (text, file, directory, choice)
    pub var_type: VariableType,
    /// For choice type, the available options
    pub options: Vec<String>,
    /// Whether this variable is required
    pub required: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum VariableType {
    Text,
    File,
    Directory,
    Choice,
    Number,
}

impl Default for VariableType {
    fn default() -> Self {
        VariableType::Text
    }
}

/// Snippet manager
pub struct SnippetManager {
    snippets: HashMap<String, Snippet>,
    storage_path: PathBuf,
}

impl SnippetManager {
    /// Create a new snippet manager
    pub fn new() -> Self {
        let storage_path = dirs::data_dir()
            .unwrap_or_else(|| PathBuf::from("."))
            .join("sish-console")
            .join("snippets.json");
        
        let mut manager = Self {
            snippets: HashMap::new(),
            storage_path,
        };
        
        manager.load();
        manager.add_default_snippets();
        
        manager
    }
    
    /// Add default snippets
    fn add_default_snippets(&mut self) {
        // Git commit with message
        if !self.snippets.contains_key("git-commit") {
            self.add(Snippet {
                name: "git-commit".to_string(),
                description: "Git commit with message".to_string(),
                template: "git commit -m \"${message}\"".to_string(),
                variables: vec![
                    SnippetVariable {
                        name: "message".to_string(),
                        label: "Commit message".to_string(),
                        default: "".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                ],
                category: "Git".to_string(),
                tags: vec!["git".to_string(), "commit".to_string()],
                use_count: 0,
                last_used: None,
            });
        }
        
        // Create Python virtual environment
        if !self.snippets.contains_key("python-venv") {
            self.add(Snippet {
                name: "python-venv".to_string(),
                description: "Create Python virtual environment".to_string(),
                template: "python3 -m venv ${name} && source ${name}/bin/activate".to_string(),
                variables: vec![
                    SnippetVariable {
                        name: "name".to_string(),
                        label: "Environment name".to_string(),
                        default: "venv".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                ],
                category: "Python".to_string(),
                tags: vec!["python".to_string(), "venv".to_string(), "virtual".to_string()],
                use_count: 0,
                last_used: None,
            });
        }
        
        // Docker run with port mapping
        if !self.snippets.contains_key("docker-run") {
            self.add(Snippet {
                name: "docker-run".to_string(),
                description: "Run Docker container with port mapping".to_string(),
                template: "docker run -d --name ${name} -p ${host_port}:${container_port} ${image}".to_string(),
                variables: vec![
                    SnippetVariable {
                        name: "name".to_string(),
                        label: "Container name".to_string(),
                        default: "my-container".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "host_port".to_string(),
                        label: "Host port".to_string(),
                        default: "8080".to_string(),
                        var_type: VariableType::Number,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "container_port".to_string(),
                        label: "Container port".to_string(),
                        default: "80".to_string(),
                        var_type: VariableType::Number,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "image".to_string(),
                        label: "Image name".to_string(),
                        default: "nginx:latest".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                ],
                category: "Docker".to_string(),
                tags: vec!["docker".to_string(), "container".to_string(), "run".to_string()],
                use_count: 0,
                last_used: None,
            });
        }
        
        // SSH with port forwarding
        if !self.snippets.contains_key("ssh-tunnel") {
            self.add(Snippet {
                name: "ssh-tunnel".to_string(),
                description: "SSH tunnel with local port forwarding".to_string(),
                template: "ssh -L ${local_port}:localhost:${remote_port} ${user}@${host}".to_string(),
                variables: vec![
                    SnippetVariable {
                        name: "user".to_string(),
                        label: "Username".to_string(),
                        default: "".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "host".to_string(),
                        label: "Remote host".to_string(),
                        default: "".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "local_port".to_string(),
                        label: "Local port".to_string(),
                        default: "8080".to_string(),
                        var_type: VariableType::Number,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "remote_port".to_string(),
                        label: "Remote port".to_string(),
                        default: "80".to_string(),
                        var_type: VariableType::Number,
                        options: vec![],
                        required: true,
                    },
                ],
                category: "SSH".to_string(),
                tags: vec!["ssh".to_string(), "tunnel".to_string(), "forward".to_string()],
                use_count: 0,
                last_used: None,
            });
        }
        
        // Find and replace in files
        if !self.snippets.contains_key("find-replace") {
            self.add(Snippet {
                name: "find-replace".to_string(),
                description: "Find and replace text in files".to_string(),
                template: "find ${directory} -type f -name \"${pattern}\" -exec sed -i 's/${search}/${replace}/g' {} +".to_string(),
                variables: vec![
                    SnippetVariable {
                        name: "directory".to_string(),
                        label: "Directory".to_string(),
                        default: ".".to_string(),
                        var_type: VariableType::Directory,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "pattern".to_string(),
                        label: "File pattern".to_string(),
                        default: "*.txt".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "search".to_string(),
                        label: "Search text".to_string(),
                        default: "".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "replace".to_string(),
                        label: "Replace with".to_string(),
                        default: "".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                ],
                category: "Files".to_string(),
                tags: vec!["find".to_string(), "replace".to_string(), "sed".to_string()],
                use_count: 0,
                last_used: None,
            });
        }
        
        // Tar archive
        if !self.snippets.contains_key("tar-create") {
            self.add(Snippet {
                name: "tar-create".to_string(),
                description: "Create a tar.gz archive".to_string(),
                template: "tar -czvf ${archive_name}.tar.gz ${source}".to_string(),
                variables: vec![
                    SnippetVariable {
                        name: "archive_name".to_string(),
                        label: "Archive name".to_string(),
                        default: "archive".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                    SnippetVariable {
                        name: "source".to_string(),
                        label: "Source path".to_string(),
                        default: ".".to_string(),
                        var_type: VariableType::Text,
                        options: vec![],
                        required: true,
                    },
                ],
                category: "Files".to_string(),
                tags: vec!["tar".to_string(), "archive".to_string(), "compress".to_string()],
                use_count: 0,
                last_used: None,
            });
        }
    }
    
    /// Load snippets from storage
    fn load(&mut self) {
        if self.storage_path.exists() {
            if let Ok(content) = fs::read_to_string(&self.storage_path) {
                if let Ok(snippets) = serde_json::from_str(&content) {
                    self.snippets = snippets;
                }
            }
        }
    }
    
    /// Save snippets to storage
    pub fn save(&self) -> Result<(), Box<dyn std::error::Error>> {
        if let Some(parent) = self.storage_path.parent() {
            fs::create_dir_all(parent)?;
        }
        let content = serde_json::to_string_pretty(&self.snippets)?;
        fs::write(&self.storage_path, content)?;
        Ok(())
    }
    
    /// Add a snippet
    pub fn add(&mut self, snippet: Snippet) {
        self.snippets.insert(snippet.name.clone(), snippet);
    }
    
    /// Remove a snippet
    pub fn remove(&mut self, name: &str) -> Option<Snippet> {
        self.snippets.remove(name)
    }
    
    /// Get a snippet by name
    pub fn get(&self, name: &str) -> Option<&Snippet> {
        self.snippets.get(name)
    }
    
    /// Get all snippets
    pub fn all(&self) -> &HashMap<String, Snippet> {
        &self.snippets
    }
    
    /// Get snippets by category
    pub fn by_category(&self, category: &str) -> Vec<&Snippet> {
        self.snippets
            .values()
            .filter(|s| s.category == category)
            .collect()
    }
    
    /// Get all categories
    pub fn categories(&self) -> Vec<String> {
        let mut cats: Vec<String> = self.snippets
            .values()
            .map(|s| s.category.clone())
            .collect();
        cats.sort();
        cats.dedup();
        cats
    }
    
    /// Search snippets
    pub fn search(&self, query: &str) -> Vec<&Snippet> {
        let query_lower = query.to_lowercase();
        self.snippets
            .values()
            .filter(|s| {
                s.name.to_lowercase().contains(&query_lower) ||
                s.description.to_lowercase().contains(&query_lower) ||
                s.template.to_lowercase().contains(&query_lower) ||
                s.tags.iter().any(|t| t.to_lowercase().contains(&query_lower))
            })
            .collect()
    }
    
    /// Execute a snippet with provided variables
    pub fn execute(&mut self, name: &str, values: &HashMap<String, String>) -> Option<String> {
        if let Some(snippet) = self.snippets.get_mut(name) {
            let mut result = snippet.template.clone();
            
            for var in &snippet.variables {
                let value = values.get(&var.name).cloned().unwrap_or(var.default.clone());
                result = result.replace(&format!("${{{}}}", var.name), &value);
            }
            
            // Update usage statistics
            snippet.use_count += 1;
            snippet.last_used = Some(std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_secs() as i64)
                .unwrap_or(0));
            
            let _ = self.save();
            
            Some(result)
        } else {
            None
        }
    }
    
    /// Get most used snippets
    pub fn most_used(&self, limit: usize) -> Vec<&Snippet> {
        let mut snippets: Vec<&Snippet> = self.snippets.values().collect();
        snippets.sort_by(|a, b| b.use_count.cmp(&a.use_count));
        snippets.truncate(limit);
        snippets
    }
    
    /// Get recently used snippets
    pub fn recently_used(&self, limit: usize) -> Vec<&Snippet> {
        let mut snippets: Vec<&Snippet> = self.snippets
            .values()
            .filter(|s| s.last_used.is_some())
            .collect();
        snippets.sort_by(|a, b| b.last_used.cmp(&a.last_used));
        snippets.truncate(limit);
        snippets
    }
}

impl Default for SnippetManager {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_execute_snippet() {
        let mut manager = SnippetManager::new();
        
        let mut values = HashMap::new();
        values.insert("message".to_string(), "Test commit".to_string());
        
        let result = manager.execute("git-commit", &values);
        assert_eq!(result, Some("git commit -m \"Test commit\"".to_string()));
    }
    
    #[test]
    fn test_search() {
        let manager = SnippetManager::new();
        let results = manager.search("docker");
        assert!(!results.is_empty());
    }
}
