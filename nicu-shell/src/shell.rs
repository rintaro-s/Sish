use anyhow::Context;
use portable_pty::{native_pty_system, CommandBuilder, PtySize};
use std::io::{Read, Write};
use std::path::PathBuf;
use std::sync::{mpsc, Arc, Mutex};

pub struct ShellSession {
    writer: Arc<Mutex<Box<dyn Write + Send>>>,
    master: Box<dyn portable_pty::MasterPty + Send>,
    _child: Box<dyn portable_pty::Child + Send>,
}

impl ShellSession {
    pub fn spawn(shell: &str, cols: u16, rows: u16) -> anyhow::Result<(Self, mpsc::Receiver<Vec<u8>>)> {
        let pty_system = native_pty_system();
        let pair = pty_system
            .openpty(PtySize {
                rows,
                cols,
                pixel_width: 0,
                pixel_height: 0,
            })
            .context("failed to open pty")?;

        let tokens = resolve_shell_tokens(shell);
        let program = tokens
            .first()
            .cloned()
            .unwrap_or_else(|| "/bin/bash".to_string());

        let mut command = CommandBuilder::new(program);
        for arg in tokens.iter().skip(1) {
            command.arg(arg);
        }

        let child = pair
            .slave
            .spawn_command(command)
            .context("failed to spawn shell")?;

        let reader = pair
            .master
            .try_clone_reader()
            .context("failed to clone pty reader")?;
        let writer = pair
            .master
            .take_writer()
            .context("failed to take pty writer")?;

        let writer = Arc::new(Mutex::new(writer));
        let (tx, rx) = mpsc::channel::<Vec<u8>>();
        std::thread::spawn(move || read_loop(reader, tx));

        Ok((
            Self {
                writer,
                master: pair.master,
                _child: child,
            },
            rx,
        ))
    }

    pub fn send_bytes(&self, bytes: &[u8]) -> anyhow::Result<()> {
        let mut writer = self.writer.lock().unwrap();
        writer
            .write_all(bytes)
            .context("failed to write to shell")?;
        writer.flush().ok();
        Ok(())
    }

    pub fn send(&self, text: &str) -> anyhow::Result<()> {
        self.send_bytes(text.as_bytes())
    }

    pub fn resize(&self, cols: u16, rows: u16) -> anyhow::Result<()> {
        self.master
            .resize(PtySize {
                rows,
                cols,
                pixel_width: 0,
                pixel_height: 0,
            })
            .context("failed to resize pty")
    }
}

fn read_loop<R: Read + Send + 'static>(mut reader: R, tx: mpsc::Sender<Vec<u8>>) {
    let mut buffer = [0_u8; 8192];
    loop {
        match reader.read(&mut buffer) {
            Ok(0) => break,
            Ok(size) => {
                if tx.send(buffer[..size].to_vec()).is_err() {
                    break;
                }
            }
            Err(_) => break,
        }
    }
}

fn resolve_shell_tokens(shell: &str) -> Vec<String> {
    let shell = shell.trim();
    if shell.is_empty() {
        return detect_sish_or_fallback();
    }

    if shell.eq_ignore_ascii_case("sish") {
        return detect_sish_or_fallback();
    }

    if let Ok(tokens) = shell_words::split(shell) {
        if !tokens.is_empty() {
            return tokens;
        }
    }

    vec![shell.to_string()]
}

fn detect_sish_or_fallback() -> Vec<String> {
    if let Some(path) = std::env::var_os("SISH_NICU_SHELL") {
        let as_string = path.to_string_lossy().to_string();
        if !as_string.trim().is_empty() {
            return vec![as_string];
        }
    }

    if let Some(repo_sish) = detect_repo_sish() {
        return vec![repo_sish];
    }

    if let Some(path_sish) = find_in_path("sish") {
        return vec![path_sish];
    }

    vec![std::env::var("SHELL").unwrap_or_else(|_| "/bin/bash".to_string())]
}

fn detect_repo_sish() -> Option<String> {
    let cwd = std::env::current_dir().ok()?;
    let candidates = [
        cwd.join("sish"),
        cwd.join("..").join("sish"),
        cwd.join("..").join("..").join("sish"),
    ];

    for candidate in candidates {
        if candidate.is_file() {
            return Some(candidate.to_string_lossy().to_string());
        }
    }

    None
}

fn find_in_path(command: &str) -> Option<String> {
    let paths = std::env::var_os("PATH")?;
    for directory in std::env::split_paths(&paths) {
        let mut candidate = PathBuf::from(directory);
        candidate.push(command);
        if candidate.is_file() {
            return Some(candidate.to_string_lossy().to_string());
        }
    }
    None
}
