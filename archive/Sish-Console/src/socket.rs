//! Unix Domain Socket communication module
//!
//! Handles communication between Sish shell and Sish Console.

use serde::{Deserialize, Serialize};
use std::io::{BufRead, BufReader};
use std::os::unix::net::{UnixListener, UnixStream};
use std::sync::{Arc, Mutex};
use std::fs;

const SOCKET_PATH: &str = "/tmp/sish-console.sock";

/// Event received from Sish shell
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SishEvent {
    #[serde(rename = "type")]
    pub event_type: String,
    pub data: String,
    #[serde(default)]
    pub timestamp: i64,
}

/// Callback type for handling events
type EventCallback = Box<dyn Fn(&SishEvent) + Send>;

/// Socket server for receiving events from Sish
pub struct SocketServer {
    callbacks: Arc<Mutex<Vec<EventCallback>>>,
}

impl SocketServer {
    /// Create a new socket server
    pub fn new() -> Self {
        Self {
            callbacks: Arc::new(Mutex::new(Vec::new())),
        }
    }
    
    /// Register a callback for events
    pub fn on_event<F>(&self, callback: F)
    where
        F: Fn(&SishEvent) + Send + 'static,
    {
        let mut callbacks = self.callbacks.lock().unwrap();
        callbacks.push(Box::new(callback));
    }
    
    /// Start the socket server
    pub fn start(&self) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        // Remove existing socket file if it exists
        let _ = fs::remove_file(SOCKET_PATH);
        
        // Create the Unix socket listener
        let listener = UnixListener::bind(SOCKET_PATH)?;
        log::info!("Socket server listening on {}", SOCKET_PATH);
        
        // Set socket permissions to allow Sish to connect
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let perms = fs::Permissions::from_mode(0o666);
            fs::set_permissions(SOCKET_PATH, perms)?;
        }
        
        let callbacks = self.callbacks.clone();
        
        // Handle connections in a loop
        for stream in listener.incoming() {
            match stream {
                Ok(stream) => {
                    let callbacks = callbacks.clone();
                    std::thread::spawn(move || {
                        if let Err(e) = handle_client(stream, callbacks) {
                            log::error!("Client error: {}", e);
                        }
                    });
                }
                Err(e) => {
                    log::error!("Connection error: {}", e);
                }
            }
        }
        
        Ok(())
    }
    
    /// Stop the socket server
    pub fn stop(&self) {
        let _ = fs::remove_file(SOCKET_PATH);
    }
}

impl Drop for SocketServer {
    fn drop(&mut self) {
        self.stop();
    }
}

/// Handle a client connection
fn handle_client(
    stream: UnixStream,
    callbacks: Arc<Mutex<Vec<EventCallback>>>,
) -> Result<(), Box<dyn std::error::Error>> {
    let reader = BufReader::new(&stream);
    
    for line in reader.lines() {
        let line = line?;
        
        if line.is_empty() {
            continue;
        }
        
        log::debug!("Received: {}", line);
        
        // Parse JSON event
        match serde_json::from_str::<SishEvent>(&line) {
            Ok(event) => {
                // Call all registered callbacks
                let callbacks = callbacks.lock().unwrap();
                for callback in callbacks.iter() {
                    callback(&event);
                }
            }
            Err(e) => {
                log::warn!("Failed to parse event: {} - {}", e, line);
            }
        }
    }
    
    Ok(())
}

/// Client for sending events to Sish Console (used by Sish shell)
pub struct SocketClient {
    stream: Option<UnixStream>,
}

impl SocketClient {
    /// Create a new socket client
    pub fn new() -> Self {
        Self { stream: None }
    }
    
    /// Connect to the socket server
    pub fn connect(&mut self) -> Result<(), std::io::Error> {
        self.stream = Some(UnixStream::connect(SOCKET_PATH)?);
        Ok(())
    }
    
    /// Check if connected
    pub fn is_connected(&self) -> bool {
        self.stream.is_some()
    }
    
    /// Send an event
    pub fn send_event(&mut self, event: &SishEvent) -> Result<(), Box<dyn std::error::Error>> {
        use std::io::Write;
        
        if self.stream.is_none() {
            self.connect()?;
        }
        
        if let Some(ref mut stream) = self.stream {
            let json = serde_json::to_string(event)?;
            writeln!(stream, "{}", json)?;
            stream.flush()?;
        }
        
        Ok(())
    }
    
    /// Disconnect
    pub fn disconnect(&mut self) {
        self.stream = None;
    }
}

impl Drop for SocketClient {
    fn drop(&mut self) {
        self.disconnect();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_event_serialization() {
        let event = SishEvent {
            event_type: "test".to_string(),
            data: "hello".to_string(),
            timestamp: 12345,
        };
        
        let json = serde_json::to_string(&event).unwrap();
        let parsed: SishEvent = serde_json::from_str(&json).unwrap();
        
        assert_eq!(parsed.event_type, "test");
        assert_eq!(parsed.data, "hello");
    }
}
