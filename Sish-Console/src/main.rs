//! Sish Console - A friendly terminal emulator for Sish shell
//!
//! This is the main entry point for the Sish Console application.
//! It provides a GTK4-based terminal emulator with:
//! - Character layer (Sish mascot with emotions)
//! - Command visualization
//! - Smart snippets
//! - Theme support

mod app;
mod terminal;
mod character;
mod socket;
mod config;
mod shortcuts;
mod snippets;
mod completion;

use gtk4::prelude::*;
use gtk4::{Application, glib};

const APP_ID: &str = "com.sish.console";

fn main() -> glib::ExitCode {
    // Initialize logging
    env_logger::init();
    log::info!("Starting Sish Console v{}", env!("CARGO_PKG_VERSION"));

    // Create the GTK application
    let app = Application::builder()
        .application_id(APP_ID)
        .build();

    // Connect to the activate signal
    app.connect_activate(|app| {
        app::build_ui(app);
    });

    // Run the application
    app.run()
}
