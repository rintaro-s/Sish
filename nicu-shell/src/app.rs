use crate::config::{key_event_name, Config, MacroEntry, Shortcut};
use crate::shell::ShellSession;
use anyhow::Context;
use arboard::Clipboard;
use crossterm::cursor;
use crossterm::event::{self, Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use crossterm::execute;
use crossterm::terminal::{
    self, disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen,
};
use ratatui::backend::CrosstermBackend;
use ratatui::layout::{Constraint, Direction, Layout};
use ratatui::style::{Color, Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Borders, List, ListItem, Paragraph, Wrap};
use ratatui::Terminal;
use std::fs;
use std::io::{self, Stdout};
use std::path::{Path, PathBuf};
use std::sync::mpsc::Receiver;
use std::time::{Duration, Instant};

const NICU_PASSTHROUGH_ON: &[u8] = b"\x1b]9;nicu-passthrough=on\x07";
const NICU_PASSTHROUGH_OFF: &[u8] = b"\x1b]9;nicu-passthrough=off\x07";
const NICU_OSC_PREFIX: &[u8] = b"\x1b]9;";
const NICU_OSC_TERMINATOR: u8 = 0x07;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Focus {
    Terminal,
    Explorer,
}

#[derive(Clone)]
struct ExplorerEntry {
    name: String,
    path: PathBuf,
    is_dir: bool,
}

struct Wallpaper {
    lines: Vec<String>,
    width: usize,
    height: usize,
}

struct App {
    config: Config,
    shell: ShellSession,
    rx: Receiver<Vec<u8>>,
    parser: vt100::Parser,
    parser_cols: u16,
    parser_rows: u16,
    focus: Focus,
    explorer_path: PathBuf,
    explorer_entries: Vec<ExplorerEntry>,
    explorer_selected: usize,
    show_hidden: bool,
    tui_passthrough: bool,
    auto_tui_until: Option<Instant>,
    shell_control_tail: Vec<u8>,
    status: String,
    llm_preview: String,
    clipboard: Option<Clipboard>,
    wallpaper: Wallpaper,
}

pub fn run() -> anyhow::Result<()> {
    let config = Config::load()?;
    let (cols, rows) = terminal::size().unwrap_or((120, 40));
    let (shell, rx) = ShellSession::spawn(&config.shell, cols, rows.saturating_sub(3))?;

    enable_raw_mode().context("failed to enable raw mode")?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, cursor::Hide)
        .context("failed to enter alternate screen")?;

    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend).context("failed to create terminal")?;

    let result = run_app(&mut terminal, App::new(config, shell, rx, cols, rows));

    disable_raw_mode().ok();
    let mut stdout = io::stdout();
    execute!(stdout, LeaveAlternateScreen, cursor::Show).ok();

    result
}

fn run_app(terminal: &mut Terminal<CrosstermBackend<Stdout>>, mut app: App) -> anyhow::Result<()> {
    loop {
        app.drain_shell_output();
        terminal.draw(|frame| app.draw(frame))?;

        if event::poll(Duration::from_millis(16)).context("event poll failed")? {
            match event::read().context("event read failed")? {
                Event::Key(key) => {
                    if app.handle_key(key)? {
                        break;
                    }
                }
                Event::Resize(cols, rows) => {
                    app.resize(cols, rows)?;
                }
                _ => {}
            }
        }
    }

    Ok(())
}

impl App {
    fn new(config: Config, shell: ShellSession, rx: Receiver<Vec<u8>>, cols: u16, rows: u16) -> Self {
        let explorer_path = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        let parser_rows = rows.saturating_sub(3).max(1);
        let parser_cols = cols.saturating_sub(36).max(20);

        let mut app = Self {
            config,
            shell,
            rx,
            parser: vt100::Parser::new(parser_rows, parser_cols, 5000),
            parser_cols,
            parser_rows,
            focus: Focus::Terminal,
            explorer_path,
            explorer_entries: Vec::new(),
            explorer_selected: 0,
            show_hidden: false,
            tui_passthrough: false,
            auto_tui_until: None,
            shell_control_tail: Vec::new(),
            status: String::new(),
            llm_preview: String::new(),
            clipboard: try_init_clipboard(),
            wallpaper: Wallpaper::embedded(),
        };

        app.refresh_explorer();
        app.status = app.default_status();
        app
    }

    fn draw(&mut self, frame: &mut ratatui::Frame<'_>) {
        let area = frame.area();
        let panel_bg = Color::Rgb(11, 14, 20);
        let panel_fg = Color::Rgb(228, 234, 242);
        let panel_title_fg = Color::Rgb(248, 250, 252);
        let auto_tui = self.auto_tui_active();
        let passthrough_active = self.effective_passthrough();

        // Always clear full frame to avoid stale UI artifacts when layout switches.
        frame.render_widget(Block::default().style(Style::default().bg(panel_bg)), area);

        if self.wallpaper_visible() {
            self.draw_wallpaper(frame, area);
        }

        // Native TUI mode: give the child app the full viewport for correct bottom/help lines.
        if passthrough_active {
            let terminal_area = area;
            let terminal_cols = terminal_area.width.max(1);
            let terminal_rows = terminal_area.height.max(1);
            if terminal_cols != self.parser_cols || terminal_rows != self.parser_rows {
                self.parser_cols = terminal_cols;
                self.parser_rows = terminal_rows;
                self.parser.set_size(terminal_rows, terminal_cols);
                let _ = self.shell.resize(terminal_cols, terminal_rows);
            }

            self.render_terminal_cells(frame, terminal_area, terminal_rows, terminal_cols, false, panel_fg, panel_bg);
            self.render_terminal_cursor(frame, terminal_area, terminal_rows, terminal_cols, false);
            return;
        }

        let vertical = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Length(2), Constraint::Min(4), Constraint::Length(1)])
            .split(area);

        let body = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Percentage(72), Constraint::Percentage(28)])
            .split(vertical[1]);

        let terminal_area = body[0];
        let right_area = body[1];

        // Split right panel: explorer on top, LLM panel on bottom (only when LLM enabled)
        let llm_panel_height: u16 = if self.config.llm.enabled { 8 } else { 0 };
        let right_split = Layout::default()
            .direction(Direction::Vertical)
            .constraints([
                Constraint::Min(4),
                Constraint::Length(llm_panel_height),
            ])
            .split(right_area);
        let explorer_area = right_split[0];
        let llm_area = right_split[1];

        let terminal_cols = terminal_area.width.saturating_sub(2).max(1);
        let terminal_rows = terminal_area.height.saturating_sub(2).max(1);
        if terminal_cols != self.parser_cols || terminal_rows != self.parser_rows {
            self.parser_cols = terminal_cols;
            self.parser_rows = terminal_rows;
            self.parser.set_size(terminal_rows, terminal_cols);
            let _ = self.shell.resize(terminal_cols, terminal_rows);
        }

        let focus_name = match self.focus {
            Focus::Terminal => "terminal",
            Focus::Explorer => "explorer",
        };
        let mode_name = if self.tui_passthrough {
            "passthrough(manual)"
        } else if auto_tui {
            "passthrough(auto)"
        } else {
            "normal"
        };
        let llm_status = self.llm_status();

        let header = Paragraph::new(Line::from(vec![
            Span::styled(
                " nicu ",
                Style::default()
                    .bg(Color::Cyan)
                    .fg(Color::Black)
                    .add_modifier(Modifier::BOLD),
            ),
            Span::raw("  "),
            Span::styled(
                format!("embedded shell: {}", self.config.shell),
                Style::default().fg(panel_fg),
            ),
            Span::raw("  "),
            Span::styled(
                format!("focus: {focus_name}"),
                Style::default().fg(Color::Yellow),
            ),
            Span::raw("  "),
            Span::styled(
                format!("mode: {mode_name}"),
                Style::default().fg(Color::Rgb(255, 179, 71)),
            ),
            Span::raw("  "),
            Span::styled(llm_status, Style::default().fg(Color::LightMagenta)),
        ]))
        .style(Style::default().bg(panel_bg).fg(panel_fg))
        .block(
            Block::default()
                .style(Style::default().bg(panel_bg))
                .borders(Borders::ALL)
                .title(Line::from(Span::styled(
                    "Nicu Workspace",
                    Style::default().fg(panel_title_fg).add_modifier(Modifier::BOLD),
                ))),
        );
        frame.render_widget(header, vertical[0]);

        let terminal_border = if self.focus == Focus::Terminal {
            Style::default().fg(Color::Cyan)
        } else {
            Style::default().fg(Color::DarkGray)
        };

        frame.render_widget(Block::default().style(Style::default().bg(panel_bg)), terminal_area);

        let terminal_block = Block::default()
            .style(Style::default().bg(panel_bg))
            .borders(Borders::ALL)
            .title(Line::from(Span::styled(
                "Sish Terminal",
                Style::default().fg(panel_title_fg).add_modifier(Modifier::BOLD),
            )))
            .border_style(terminal_border);
        frame.render_widget(terminal_block, terminal_area);
        self.render_terminal_cells(frame, terminal_area, terminal_rows, terminal_cols, true, panel_fg, panel_bg);
        self.render_terminal_cursor(frame, terminal_area, terminal_rows, terminal_cols, true);

        let explorer_border = if self.focus == Focus::Explorer {
            Style::default().fg(Color::LightGreen)
        } else {
            Style::default().fg(Color::DarkGray)
        };

        let mut items = Vec::new();
        items.push(ListItem::new(Line::from(Span::styled(
            format!("Path: {}", self.explorer_path.display()),
            Style::default().fg(panel_fg),
        ))));
        items.push(ListItem::new(""));

        for (index, entry) in self.explorer_entries.iter().enumerate() {
            let marker = if entry.is_dir { "d" } else { "f" };
            let base_style = if self.focus == Focus::Explorer && self.explorer_selected == index {
                Style::default().fg(Color::Black).bg(Color::Green)
            } else if entry.is_dir {
                Style::default().fg(Color::Rgb(141, 218, 255))
            } else {
                Style::default().fg(panel_fg)
            };
            items.push(ListItem::new(Line::from(Span::styled(
                format!("{marker} {}", entry.name),
                base_style,
            ))));
        }

        if self.explorer_entries.is_empty() {
            items.push(ListItem::new(Line::from(Span::styled(
                "(empty)",
                Style::default().fg(Color::DarkGray),
            ))));
        }

        let explorer = List::new(items).block(
            Block::default()
                .style(Style::default().bg(panel_bg))
                .borders(Borders::ALL)
                .title(Line::from(Span::styled(
                    self.explorer_title(),
                    Style::default().fg(panel_title_fg).add_modifier(Modifier::BOLD),
                )))
                .border_style(explorer_border),
        );
        frame.render_widget(explorer, explorer_area);

        // LLM panel (only when enabled)
        if self.config.llm.enabled {
            let llm_text = if self.llm_preview.is_empty() {
                "(waiting for LLM activity...)".to_string()
            } else {
                self.llm_preview.clone()
            };
            let llm_color = if llm_text.starts_with("error:") || llm_text.contains("エラー") {
                Color::LightRed
            } else if llm_text.starts_with("LLM") || llm_text.contains("解析中") || llm_text.contains("analyzing") {
                Color::Yellow
            } else if llm_text.starts_with("(") {
                Color::DarkGray
            } else {
                Color::Rgb(200, 230, 255)
            };
            let llm_widget = Paragraph::new(llm_text)
                .style(Style::default().fg(llm_color).bg(panel_bg))
                .wrap(Wrap { trim: false })
                .block(
                    Block::default()
                        .style(Style::default().bg(panel_bg))
                        .borders(Borders::ALL)
                        .title(Line::from(Span::styled(
                            "LLM Assist",
                            Style::default().fg(Color::LightMagenta).add_modifier(Modifier::BOLD),
                        )))
                        .border_style(Style::default().fg(Color::Rgb(120, 80, 180))),
                );
            frame.render_widget(llm_widget, llm_area);
        }

        let footer_text = self.status.clone();
        let footer = Paragraph::new(footer_text)
            .style(Style::default().bg(panel_bg).fg(panel_fg));
        frame.render_widget(footer, vertical[2]);
    }

    fn draw_wallpaper(&self, frame: &mut ratatui::Frame<'_>, area: ratatui::layout::Rect) {
        let wallpaper_lines = self.wallpaper.render(area.width, area.height);
        if wallpaper_lines.is_empty() {
            return;
        }

        let wallpaper = Paragraph::new(wallpaper_lines)
            .style(Style::default().fg(Color::Rgb(30, 36, 48)));
        frame.render_widget(wallpaper, area);
    }

    fn wallpaper_visible(&self) -> bool {
        if !self.config.wallpaper_enabled {
            return false;
        }

        if self.effective_passthrough() {
            return false;
        }

        true
    }

    fn tui_detected(&self) -> bool {
        let screen = self.parser.screen();
        screen.alternate_screen()
            || screen.application_cursor()
            || !matches!(screen.mouse_protocol_mode(), vt100::MouseProtocolMode::None)
    }

    fn auto_tui_active(&self) -> bool {
        if self.tui_detected() {
            return true;
        }

        self.auto_tui_until
            .map(|until| Instant::now() <= until)
            .unwrap_or(false)
    }

    fn effective_passthrough(&self) -> bool {
        self.tui_passthrough || self.auto_tui_active()
    }

    fn render_terminal_cursor(
        &self,
        frame: &mut ratatui::Frame<'_>,
        terminal_area: ratatui::layout::Rect,
        rows: u16,
        cols: u16,
        with_border: bool,
    ) {
        let screen = self.parser.screen();
        if screen.hide_cursor() || rows == 0 || cols == 0 {
            return;
        }

        let (row, mut col) = screen.cursor_position();
        if row >= rows {
            return;
        }

        col = col.min(cols.saturating_sub(1));
        if col > 0
            && screen
                .cell(row, col)
                .map(vt100::Cell::is_wide_continuation)
                .unwrap_or(false)
        {
            col -= 1;
        }

        let border = u16::from(with_border);
        let x = terminal_area.x.saturating_add(border).saturating_add(col);
        let y = terminal_area.y.saturating_add(border).saturating_add(row);
        let max_x = terminal_area
            .x
            .saturating_add(terminal_area.width.saturating_sub(1 + border));
        let max_y = terminal_area
            .y
            .saturating_add(terminal_area.height.saturating_sub(1 + border));

        if x <= max_x && y <= max_y {
            frame.set_cursor_position((x, y));
        }
    }

    fn resize(&mut self, cols: u16, rows: u16) -> anyhow::Result<()> {
        // Actual PTY sizing is handled in draw() from the active layout.
        // Doing it here causes conflicting resize events and TUI glitches.
        let _ = (cols, rows);
        Ok(())
    }

    fn drain_shell_output(&mut self) {
        while let Ok(chunk) = self.rx.try_recv() {
            let cleaned = self.consume_shell_controls(&chunk);
            if !cleaned.is_empty() {
                self.parser.process(&cleaned);
                if self.tui_detected() {
                    self.auto_tui_until = Some(Instant::now() + Duration::from_millis(800));
                }
            }
        }

        if !self.tui_detected()
            && self
                .auto_tui_until
                .map(|until| Instant::now() > until)
                .unwrap_or(false)
        {
            self.auto_tui_until = None;
        }
    }

    fn consume_shell_controls(&mut self, chunk: &[u8]) -> Vec<u8> {
        self.shell_control_tail.extend_from_slice(chunk);

        let keep = marker_suffix_len(&self.shell_control_tail);
        let process_len = self.shell_control_tail.len().saturating_sub(keep);

        let mut out = Vec::with_capacity(process_len);
        let mut i = 0;
        while i < process_len {
            if self.shell_control_tail[i..].starts_with(NICU_OSC_PREFIX) {
                if let Some(relative_end) = self.shell_control_tail[i..process_len]
                    .iter()
                    .position(|byte| *byte == NICU_OSC_TERMINATOR)
                {
                    let payload_start = i + NICU_OSC_PREFIX.len();
                    let payload_end = i + relative_end;
                    let payload = self.shell_control_tail[payload_start..payload_end].to_vec();
                    self.handle_shell_control_payload(&payload);
                    i = payload_end + 1;
                    continue;
                }
            }

            if i + NICU_PASSTHROUGH_ON.len() <= process_len
                && self.shell_control_tail[i..].starts_with(NICU_PASSTHROUGH_ON)
            {
                self.tui_passthrough = true;
                self.focus = Focus::Terminal;
                self.status = "TUI直通: ON (sish-config) | Ctrl+Tで解除".to_string();
                i += NICU_PASSTHROUGH_ON.len();
                continue;
            }

            if i + NICU_PASSTHROUGH_OFF.len() <= process_len
                && self.shell_control_tail[i..].starts_with(NICU_PASSTHROUGH_OFF)
            {
                self.tui_passthrough = false;
                self.status = "TUI直通: OFF | Ctrl+Tで再開".to_string();
                i += NICU_PASSTHROUGH_OFF.len();
                continue;
            }

            out.push(self.shell_control_tail[i]);
            i += 1;
        }

        self.shell_control_tail.drain(0..process_len);
        out
    }

    fn handle_shell_control_payload(&mut self, payload: &[u8]) {
        let Ok(text) = std::str::from_utf8(payload) else {
            return;
        };

        if let Some(value) = text.strip_prefix("nicu-llm-status=") {
            let normalized = value.trim();
            if !normalized.is_empty() {
                self.llm_preview = normalized.to_string();
                self.status = format!("llm: {}", truncate_for_status(normalized, 96));
            }
            return;
        }

        if text == "nicu-passthrough=on" {
            self.tui_passthrough = true;
            self.focus = Focus::Terminal;
            self.status = "TUI直通: ON (sish-config) | Alt+Tで解除".to_string();
        } else if text == "nicu-passthrough=off" {
            self.tui_passthrough = false;
            self.status = "TUI直通: OFF | Alt+Tで再開".to_string();
        }
    }

    fn handle_key(&mut self, key: KeyEvent) -> anyhow::Result<bool> {
        if !matches!(key.kind, KeyEventKind::Press | KeyEventKind::Repeat) {
            return Ok(false);
        }

        if self.should_swallow_repeat(key) {
            return Ok(false);
        }

        let auto_tui = self.auto_tui_active();

        if self.config.bind_matches(&self.config.keybinds.passthrough_toggle, &key) && !auto_tui {
            self.focus = Focus::Terminal;
            self.tui_passthrough = !self.tui_passthrough;
            self.status = if self.tui_passthrough {
                "TUI直通: ON(manual) | 同じキーで解除".to_string()
            } else {
                "TUI直通: OFF(manual) | 自動判定は継続".to_string()
            };
            return Ok(false);
        }

        let passthrough_active = self.effective_passthrough();

        if passthrough_active {
            if let Some(bytes) = self.key_to_pty_bytes(key) {
                self.shell.send_bytes(&bytes)?;
            }
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.quit, &key) {
            return Ok(true);
        }

        if self.config.bind_matches(&self.config.keybinds.focus_toggle, &key) {
            self.toggle_focus();
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.copy_output, &key) {
            self.copy_terminal_screen();
            return Ok(false);
        }

        if self.handle_bound_action(key)? {
            return Ok(false);
        }

        match self.focus {
            Focus::Explorer => self.handle_explorer_key(key),
            Focus::Terminal => self.handle_terminal_key(key),
        }
    }

    fn handle_terminal_key(&mut self, key: KeyEvent) -> anyhow::Result<bool> {
        if let Some(bytes) = self.key_to_pty_bytes(key) {
            self.shell.send_bytes(&bytes)?;
        }
        Ok(false)
    }

    fn handle_explorer_key(&mut self, key: KeyEvent) -> anyhow::Result<bool> {
        if matches!(key.code, KeyCode::Esc) || matches!(key.code, KeyCode::Char('q')) {
            self.focus = Focus::Terminal;
            self.status = "focus: terminal".to_string();
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_up, &key) {
            self.move_explorer_selection(-1);
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_down, &key) {
            self.move_explorer_selection(1);
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_page_up, &key) {
            self.move_explorer_selection(-(self.explorer_page_step() as isize));
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_page_down, &key) {
            self.move_explorer_selection(self.explorer_page_step() as isize);
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_top, &key) {
            self.explorer_selected = 0;
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_bottom, &key) {
            self.explorer_selected = self.explorer_entries.len().saturating_sub(1);
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_parent, &key) {
            if let Some(parent) = self.explorer_path.parent() {
                self.explorer_path = parent.to_path_buf();
                self.refresh_explorer();
                self.status = format!("explorer: {}", self.explorer_path.display());
            }
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_refresh, &key) {
            self.refresh_explorer();
            self.status = "explorer refreshed".to_string();
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_toggle_hidden, &key) {
            self.show_hidden = !self.show_hidden;
            self.refresh_explorer();
            self.status = if self.show_hidden {
                "show hidden: on".to_string()
            } else {
                "show hidden: off".to_string()
            };
            return Ok(false);
        }

        if self.config.bind_matches(&self.config.keybinds.explorer_open, &key) {
            if let Some(entry) = self.explorer_entries.get(self.explorer_selected).cloned() {
                if entry.is_dir {
                    self.explorer_path = entry.path.clone();
                    self.refresh_explorer();
                    self.send_cd(&entry.path)?;
                    self.status = format!("cd {}", entry.path.display());
                } else {
                    self.send_open_file(&entry.path)?;
                    self.status = format!("open {}", entry.path.display());
                }
            }
            return Ok(false);
        }

        if let Some(bytes) = self.key_to_pty_bytes(key) {
            self.shell.send_bytes(&bytes)?;
        }

        Ok(false)
    }

    fn toggle_focus(&mut self) {
        self.focus = match self.focus {
            Focus::Terminal => Focus::Explorer,
            Focus::Explorer => Focus::Terminal,
        };
        self.status = match self.focus {
            Focus::Terminal => "focus: terminal".to_string(),
            Focus::Explorer => "focus: explorer (Enter: open/cd, Backspace: up, .: hidden)".to_string(),
        };
    }

    fn refresh_explorer(&mut self) {
        self.explorer_entries.clear();

        let read_result = fs::read_dir(&self.explorer_path)
            .or_else(|_| fs::read_dir("."));
        let Ok(entries) = read_result else {
            self.status = format!("cannot read {}", self.explorer_path.display());
            return;
        };

        for entry in entries.flatten() {
            let file_name = entry.file_name().to_string_lossy().to_string();
            if !self.show_hidden && file_name.starts_with('.') {
                continue;
            }

            let path = entry.path();
            let is_dir = path.is_dir();
            self.explorer_entries.push(ExplorerEntry {
                name: file_name,
                path,
                is_dir,
            });
        }

        self.explorer_entries.sort_by(|a, b| {
            b.is_dir
                .cmp(&a.is_dir)
                .then_with(|| a.name.to_lowercase().cmp(&b.name.to_lowercase()))
        });

        if self.explorer_selected >= self.explorer_entries.len() {
            self.explorer_selected = self.explorer_entries.len().saturating_sub(1);
        }
    }

    fn should_swallow_repeat(&self, key: KeyEvent) -> bool {
        if key.kind != KeyEventKind::Repeat {
            return false;
        }

        self.config.bind_matches(&self.config.keybinds.quit, &key)
            || self.config.bind_matches(&self.config.keybinds.focus_toggle, &key)
            || self.config.bind_matches(&self.config.keybinds.passthrough_toggle, &key)
            || self.config.bind_matches(&self.config.keybinds.copy_output, &key)
            || self.config.bind_matches(&self.config.keybinds.explorer_open, &key)
            || self.config.bind_matches(&self.config.keybinds.explorer_parent, &key)
            || self.config.bind_matches(&self.config.keybinds.explorer_toggle_hidden, &key)
    }

    fn handle_bound_action(&mut self, key: KeyEvent) -> anyhow::Result<bool> {
        let event_name = key_event_name(&key);

        if let Some(shortcut) = self.config.shortcut_by_bind(&event_name).cloned() {
            self.run_shortcut(shortcut)?;
            return Ok(true);
        }

        if let Some(entry) = self.config.macro_by_bind(&event_name).cloned() {
            self.run_macro(entry)?;
            return Ok(true);
        }

        Ok(false)
    }

    fn run_shortcut(&mut self, shortcut: Shortcut) -> anyhow::Result<()> {
        self.shell.send(&shortcut.expansion)?;
        if shortcut.execute {
            self.shell.send("\r")?;
        }
        self.focus = Focus::Terminal;
        self.status = format!("shortcut: {}", shortcut.name);
        Ok(())
    }

    fn run_macro(&mut self, entry: MacroEntry) -> anyhow::Result<()> {
        self.shell.send(&entry.template)?;
        if entry.execute {
            self.shell.send("\r")?;
        }
        self.focus = Focus::Terminal;
        self.status = format!("macro: {}", entry.name);
        Ok(())
    }

    fn move_explorer_selection(&mut self, delta: isize) {
        if self.explorer_entries.is_empty() {
            self.explorer_selected = 0;
            return;
        }

        let max = self.explorer_entries.len().saturating_sub(1) as isize;
        let next = (self.explorer_selected as isize + delta).clamp(0, max);
        self.explorer_selected = next as usize;
    }

    fn explorer_page_step(&self) -> usize {
        (self.parser_rows as usize / 2).clamp(5, 20)
    }

    fn default_status(&self) -> String {
        format!(
            "{}: quit | {}: focus | {}: passthrough | explorer: j/k h/l g G ^u ^d . | q/esc: terminal",
            primary_bind(&self.config.keybinds.quit),
            primary_bind(&self.config.keybinds.focus_toggle),
            primary_bind(&self.config.keybinds.passthrough_toggle)
        )
    }

    fn llm_status(&self) -> String {
        if !self.config.llm.enabled {
            return "llm: off".to_string();
        }

        if self.config.llm_ready() {
            let label = if self.config.llm.model.trim().is_empty() {
                "default".to_string()
            } else {
                self.config.llm.model.clone()
            };
            format!("llm: ready ({label})")
        } else {
            "llm: endpoint missing".to_string()
        }
    }

    fn explorer_title(&self) -> String {
        if self.llm_preview.is_empty() {
            return "TUI Explorer [j/k h/l g G ^u ^d .]".to_string();
        }

        format!(
            "LLM Assist | {}",
            truncate_for_status(&self.llm_preview, 34)
        )
    }

    fn send_cd(&self, path: &Path) -> anyhow::Result<()> {
        let quoted = shell_quote(path);
        self.shell.send(&format!("cd -- {quoted}\r"))
    }

    fn send_open_file(&self, path: &Path) -> anyhow::Result<()> {
        let quoted = shell_quote(path);
        self.shell.send(&format!(
            "if [[ -n \"${{EDITOR:-}}\" ]] && command -v \"${{EDITOR}}\" >/dev/null 2>&1; then \"${{EDITOR}}\" -- {quoted}; elif command -v nvim >/dev/null 2>&1; then nvim -- {quoted}; elif command -v vim >/dev/null 2>&1; then vim -- {quoted}; else less -- {quoted}; fi\r"
        ))
    }

    fn copy_terminal_screen(&mut self) {
        let Some(clipboard) = self.clipboard.as_mut() else {
            self.status = "clipboard unavailable".to_string();
            return;
        };

        let text = self.parser.screen().contents();
        if clipboard.set_text(text).is_ok() {
            self.status = "copied terminal screen".to_string();
        } else {
            self.status = "clipboard copy failed".to_string();
        }
    }

    fn render_terminal_cells(
        &self,
        frame: &mut ratatui::Frame<'_>,
        terminal_area: ratatui::layout::Rect,
        rows: u16,
        cols: u16,
        with_border: bool,
        default_fg: Color,
        default_bg: Color,
    ) {
        let screen = self.parser.screen();
        let border = u16::from(with_border);
        let x0 = terminal_area.x.saturating_add(border);
        let y0 = terminal_area.y.saturating_add(border);
        let draw_cols = cols.min(terminal_area.width.saturating_sub(border * 2));
        let draw_rows = rows.min(terminal_area.height.saturating_sub(border * 2));
        let buf = frame.buffer_mut();

        for row in 0..draw_rows {
            for col in 0..draw_cols {
                let Some(cell) = screen.cell(row, col) else {
                    continue;
                };

                if cell.is_wide_continuation() {
                    continue;
                }

                let style = style_from_vt_cell(cell, default_fg, default_bg);
                let text = cell.contents();
                let symbol = if text.is_empty() { " " } else { text.as_str() };
                let x = x0 + col;
                let y = y0 + row;
                buf[(x, y)].set_symbol(symbol).set_style(style);
            }
        }
    }

    fn key_to_pty_bytes(&self, key: KeyEvent) -> Option<Vec<u8>> {
        key_to_pty_bytes(key, self.parser.screen().application_cursor())
    }
}

impl Wallpaper {
    fn embedded() -> Self {
        let raw = include_str!("AA/umaru.txt");
        let lines: Vec<String> = raw.lines().map(|line| line.to_string()).collect();
        let width = lines.iter().map(|line| line.len()).max().unwrap_or(0);
        let height = lines.len();

        Self {
            lines,
            width,
            height,
        }
    }

    fn render(&self, width: u16, height: u16) -> Vec<Line<'static>> {
        if width == 0 || height == 0 || self.height == 0 || self.width == 0 {
            return Vec::new();
        }

        let width = width as usize;
        let height = height as usize;
        let x_offset = self.width.saturating_sub(width) / 2;
        let y_offset = self.height.saturating_sub(height) / 2;

        let mut rows = Vec::with_capacity(height);
        for row_index in 0..height {
            let source_row = if self.height > height {
                row_index + y_offset
            } else {
                row_index
            };

            let mut row = String::with_capacity(width);
            if let Some(source_line) = self.lines.get(source_row) {
                if self.width > width {
                    let start = x_offset.min(source_line.len());
                    let end = (start + width).min(source_line.len());
                    row.push_str(&source_line[start..end]);
                } else {
                    row.push_str(source_line);
                }
            }

            if row.len() < width {
                row.extend(std::iter::repeat(' ').take(width - row.len()));
            } else if row.len() > width {
                row.truncate(width);
            }

            rows.push(Line::from(Span::styled(
                row,
                Style::default().fg(Color::Rgb(72, 78, 94)),
            )));
        }

        rows
    }
}

fn key_to_pty_bytes(key: KeyEvent, application_cursor: bool) -> Option<Vec<u8>> {
    let mut out = Vec::new();

    match key.code {
        KeyCode::Enter => out.push(b'\r'),
        KeyCode::Tab => out.push(b'\t'),
        KeyCode::Backspace => out.push(0x7f),
        KeyCode::Esc => out.push(0x1b),
        KeyCode::Insert => out.extend_from_slice(b"\x1b[2~"),
        KeyCode::Left => {
            if application_cursor {
                out.extend_from_slice(b"\x1bOD");
            } else {
                out.extend_from_slice(b"\x1b[D");
            }
        }
        KeyCode::Right => {
            if application_cursor {
                out.extend_from_slice(b"\x1bOC");
            } else {
                out.extend_from_slice(b"\x1b[C");
            }
        }
        KeyCode::Up => {
            if application_cursor {
                out.extend_from_slice(b"\x1bOA");
            } else {
                out.extend_from_slice(b"\x1b[A");
            }
        }
        KeyCode::Down => {
            if application_cursor {
                out.extend_from_slice(b"\x1bOB");
            } else {
                out.extend_from_slice(b"\x1b[B");
            }
        }
        KeyCode::Home => {
            if application_cursor {
                out.extend_from_slice(b"\x1bOH");
            } else {
                out.extend_from_slice(b"\x1b[H");
            }
        }
        KeyCode::End => {
            if application_cursor {
                out.extend_from_slice(b"\x1bOF");
            } else {
                out.extend_from_slice(b"\x1b[F");
            }
        }
        KeyCode::Delete => out.extend_from_slice(b"\x1b[3~"),
        KeyCode::PageUp => out.extend_from_slice(b"\x1b[5~"),
        KeyCode::PageDown => out.extend_from_slice(b"\x1b[6~"),
        KeyCode::F(n) => {
            let seq = match n {
                1 => "\x1bOP",
                2 => "\x1bOQ",
                3 => "\x1bOR",
                4 => "\x1bOS",
                5 => "\x1b[15~",
                6 => "\x1b[17~",
                7 => "\x1b[18~",
                8 => "\x1b[19~",
                9 => "\x1b[20~",
                10 => "\x1b[21~",
                11 => "\x1b[23~",
                12 => "\x1b[24~",
                _ => "",
            };
            if seq.is_empty() {
                return None;
            }
            out.extend_from_slice(seq.as_bytes());
        }
        KeyCode::Char(c) => {
            if key.modifiers.contains(KeyModifiers::CONTROL) {
                let lower = c.to_ascii_lowercase();
                if lower.is_ascii_alphabetic() {
                    out.push((lower as u8) & 0x1f);
                } else if lower == ' ' {
                    out.push(0);
                } else {
                    return None;
                }
            } else if key.modifiers.contains(KeyModifiers::ALT) {
                out.push(0x1b);
                let mut bytes = [0_u8; 4];
                let text = c.encode_utf8(&mut bytes);
                out.extend_from_slice(text.as_bytes());
            } else {
                let mut bytes = [0_u8; 4];
                let text = c.encode_utf8(&mut bytes);
                out.extend_from_slice(text.as_bytes());
            }
        }
        _ => return None,
    }

    Some(out)
}

fn style_from_vt_cell(cell: &vt100::Cell, default_fg: Color, default_bg: Color) -> Style {
    let mut fg = vt_color_to_ratatui(cell.fgcolor(), default_fg);
    let mut bg = vt_color_to_ratatui(cell.bgcolor(), default_bg);

    if cell.inverse() {
        std::mem::swap(&mut fg, &mut bg);
    }

    let mut style = Style::default().fg(fg).bg(bg);
    if cell.bold() {
        style = style.add_modifier(Modifier::BOLD);
    }
    if cell.italic() {
        style = style.add_modifier(Modifier::ITALIC);
    }
    if cell.underline() {
        style = style.add_modifier(Modifier::UNDERLINED);
    }
    style
}

fn vt_color_to_ratatui(color: vt100::Color, default: Color) -> Color {
    match color {
        vt100::Color::Default => default,
        vt100::Color::Idx(idx) => Color::Indexed(idx),
        vt100::Color::Rgb(r, g, b) => Color::Rgb(r, g, b),
    }
}

fn shell_quote(path: &Path) -> String {
    let raw = path.to_string_lossy();
    let escaped = raw.replace('\'', "'\\''");
    format!("'{escaped}'")
}

fn primary_bind(bindings: &str) -> String {
    bindings
        .split([',', '|'])
        .map(str::trim)
        .find(|value| !value.is_empty())
        .unwrap_or("unbound")
        .to_string()
}

fn truncate_for_status(value: &str, max_chars: usize) -> String {
    let mut out = String::new();
    for (count, ch) in value.chars().enumerate() {
        if count >= max_chars {
            out.push('…');
            return out;
        }
        out.push(ch);
    }
    out
}

fn try_init_clipboard() -> Option<Clipboard> {
    let has_graphics = std::env::var_os("WAYLAND_DISPLAY").is_some()
        || std::env::var_os("DISPLAY").is_some();
    if !has_graphics {
        return None;
    }
    Clipboard::new().ok()
}

fn marker_suffix_len(buf: &[u8]) -> usize {
    let max = NICU_PASSTHROUGH_ON
        .len()
        .max(NICU_PASSTHROUGH_OFF.len())
        .max(NICU_OSC_PREFIX.len() + 128)
        .saturating_sub(1);
    let limit = max.min(buf.len());

    for n in (1..=limit).rev() {
        if NICU_PASSTHROUGH_ON.starts_with(&buf[buf.len() - n..])
            || NICU_PASSTHROUGH_OFF.starts_with(&buf[buf.len() - n..])
            || NICU_OSC_PREFIX.starts_with(&buf[buf.len() - n..])
        {
            return n;
        }
    }
    0
}