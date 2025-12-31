//! Application window and UI building module
//!
//! This module handles the main application window and UI layout.

use gtk4::prelude::*;
use gtk4::glib;
use gtk4::{
    Application, ApplicationWindow, Box as GtkBox, Button, HeaderBar,
    MenuButton, Orientation, ScrolledWindow, CssProvider,
    Notebook, Label, Picture, gdk,
};
use gio::Menu;
use vte4::prelude::*;
use std::cell::Cell;
use std::cell::RefCell;
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::mpsc;
use std::time::Duration;

use crate::config::Config;
use crate::terminal::SishTerminal;
use crate::character::CharacterLayer;
use crate::socket::SocketServer;

/// Build the main application UI
pub fn build_ui(app: &Application) {
    // Load configuration
    let config = Rc::new(RefCell::new(Config::load()));
    
    // Apply custom CSS
    load_css(&config.borrow());
    
    // Create the main window
    let window = ApplicationWindow::builder()
        .application(app)
        .title("Sish Console")
        .default_width(config.borrow().window.width)
        .default_height(config.borrow().window.height)
        .build();

    // Create header bar
    let header = create_header_bar();
    window.set_titlebar(Some(&header));

    // Create main container (wallpaper + tabs + character overlay)
    let overlay = gtk4::Overlay::new();

    // Wallpaper (under everything)
    let wallpaper = Picture::new();
    wallpaper.set_hexpand(true);
    wallpaper.set_vexpand(true);
    wallpaper.set_can_shrink(true);
    wallpaper.set_keep_aspect_ratio(false);
    wallpaper.set_content_fit(gtk4::ContentFit::Cover);
    apply_wallpaper(&wallpaper, config.borrow().window.background_image.as_deref());
    overlay.set_child(Some(&wallpaper));

    // Tabs (Notebook)
    let notebook = Notebook::new();
    notebook.set_hexpand(true);
    notebook.set_vexpand(true);
    notebook.set_scrollable(true);
    overlay.add_overlay(&notebook);

    // First tab
    add_new_tab(&notebook, &config.borrow());

    // Create character layer
    let character = CharacterLayer::new(&config.borrow().character);

    // Add character layer as overlay
    if config.borrow().character.enabled {
        overlay.add_overlay(character.widget());
    }

    window.set_child(Some(&overlay));
    
    // Start socket server for Sish communication
    let socket_server = SocketServer::new();
    let (event_tx, event_rx) = mpsc::channel::<crate::socket::SishEvent>();

    // UI thread: periodically drain queued events
    let character_for_rx = character.clone();
    glib::timeout_add_local(Duration::from_millis(30), move || {
        while let Ok(event) = event_rx.try_recv() {
            character_for_rx.handle_event(&event);
        }
        glib::ControlFlow::Continue
    });

    // Socket thread(s): forward events to UI via channel
    socket_server.on_event(move |event| {
        let _ = event_tx.send(event.clone());
    });

    // Start the server in a background thread (never block the GTK main loop)
    std::thread::spawn(move || {
        if let Err(e) = socket_server.start() {
            log::error!("Socket server error: {}", e);
        }
    });

    // Register app actions (menu/shortcuts)
    register_actions(app, &window, &notebook, &wallpaper, &config);

    // Show window
    window.present();
}

fn register_actions(
    app: &Application,
    window: &ApplicationWindow,
    notebook: &Notebook,
    wallpaper: &Picture,
    config: &Rc<RefCell<Config>>,
) {
    // Quit
    let quit = gio::SimpleAction::new("quit", None);
    quit.connect_activate(glib::clone!(@weak app => move |_, _| {
        app.quit();
    }));
    app.add_action(&quit);

    // New tab
    let new_tab = gio::SimpleAction::new("new-tab", None);
    new_tab.connect_activate(glib::clone!(@weak notebook, @strong config => move |_, _| {
        add_new_tab(&notebook, &config.borrow());
    }));
    app.add_action(&new_tab);

    // Preferences
    let preferences = gio::SimpleAction::new("preferences", None);
    preferences.connect_activate(glib::clone!(@weak window, @weak notebook, @weak wallpaper, @strong config => move |_, _| {
        open_preferences_dialog(&window, &notebook, &wallpaper, &config);
    }));
    app.add_action(&preferences);

    // Copy / Paste / Select all (current tab)
    let copy = gio::SimpleAction::new("copy", None);
    copy.connect_activate(glib::clone!(@weak notebook => move |_, _| {
        with_current_vte(&notebook, |vte| vte.copy_clipboard_format(vte4::Format::Text));
    }));
    app.add_action(&copy);

    let paste = gio::SimpleAction::new("paste", None);
    paste.connect_activate(glib::clone!(@weak notebook => move |_, _| {
        with_current_vte(&notebook, |vte| vte.paste_clipboard());
    }));
    app.add_action(&paste);

    let select_all = gio::SimpleAction::new("select-all", None);
    select_all.connect_activate(glib::clone!(@weak notebook => move |_, _| {
        with_current_vte(&notebook, |vte| vte.select_all());
    }));
    app.add_action(&select_all);

    // Fullscreen toggle
    let is_fullscreen = Rc::new(Cell::new(false));
    let fullscreen = gio::SimpleAction::new("fullscreen", None);
    fullscreen.connect_activate(glib::clone!(@weak window, @strong is_fullscreen => move |_, _| {
        if is_fullscreen.get() {
            window.unfullscreen();
            is_fullscreen.set(false);
        } else {
            window.fullscreen();
            is_fullscreen.set(true);
        }
    }));
    app.add_action(&fullscreen);

    // Zoom
    let zoom_in = gio::SimpleAction::new("zoom-in", None);
    zoom_in.connect_activate(glib::clone!(@weak notebook => move |_, _| {
        with_current_vte(&notebook, |vte| {
            let scale = vte.font_scale();
            vte.set_font_scale(scale * 1.1);
        });
    }));
    app.add_action(&zoom_in);

    let zoom_out = gio::SimpleAction::new("zoom-out", None);
    zoom_out.connect_activate(glib::clone!(@weak notebook => move |_, _| {
        with_current_vte(&notebook, |vte| {
            let scale = vte.font_scale();
            vte.set_font_scale(scale / 1.1);
        });
    }));
    app.add_action(&zoom_out);
}

/// Create the header bar with menus
fn create_header_bar() -> HeaderBar {
    let header = HeaderBar::new();
    
    // Create menu model
    let menu = Menu::new();
    
    // File submenu
    let file_menu = Menu::new();
    file_menu.append(Some("新しいタブ"), Some("app.new-tab"));
    file_menu.append(Some("設定"), Some("app.preferences"));
    file_menu.append(Some("終了"), Some("app.quit"));
    menu.append_submenu(Some("ファイル"), &file_menu);
    
    // Edit submenu
    let edit_menu = Menu::new();
    edit_menu.append(Some("コピー"), Some("app.copy"));
    edit_menu.append(Some("貼り付け"), Some("app.paste"));
    edit_menu.append(Some("すべて選択"), Some("app.select-all"));
    menu.append_submenu(Some("編集"), &edit_menu);
    
    // View submenu
    let view_menu = Menu::new();
    view_menu.append(Some("フルスクリーン"), Some("app.fullscreen"));
    view_menu.append(Some("ズームイン"), Some("app.zoom-in"));
    view_menu.append(Some("ズームアウト"), Some("app.zoom-out"));
    menu.append_submenu(Some("表示"), &view_menu);
    
    // Create menu button
    let menu_button = MenuButton::builder()
        .icon_name("open-menu-symbolic")
        .menu_model(&menu)
        .build();
    
    header.pack_end(&menu_button);
    
    // New tab button
    let new_tab_button = Button::builder()
        .icon_name("list-add-symbolic")
        .tooltip_text("新しいタブ")
        .build();
    new_tab_button.set_action_name(Some("app.new-tab"));
    header.pack_start(&new_tab_button);

    // Settings button
    let settings_button = Button::builder()
        .icon_name("emblem-system-symbolic")
        .tooltip_text("設定")
        .build();
    settings_button.set_action_name(Some("app.preferences"));
    header.pack_end(&settings_button);
    
    header
}

/// Load custom CSS for theming
fn load_css(config: &Config) {
    let css = format!(r#"
        /* Modern light theme with proper contrast */
        window {{
            background-color: transparent;
            border-radius: 12px;
        }}

        window.background {{
            background-color: transparent;
            border-radius: 12px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.15);
        }}

        headerbar {{
            background: linear-gradient(180deg, rgba(30, 41, 59, 0.95) 0%, rgba(15, 23, 42, 0.97) 100%);
            border-bottom: 1px solid rgba(148, 163, 184, 0.1);
            box-shadow: 0 1px 2px rgba(0, 0, 0, 0.3);
            padding: 4px 8px;
            border-radius: 8px 8px 0 0;
            min-height: 38px;
        }}

        headerbar button {{
            background: rgba(51, 65, 85, 0.6);
            border: 1px solid rgba(100, 116, 139, 0.3);
            border-radius: 6px;
            padding: 6px 10px;
            color: #cbd5e1;
            font-weight: 500;
            font-size: 12px;
            min-width: 32px;
            min-height: 32px;
            transition: all 0.15s ease;
        }}

        headerbar button:hover {{
            background: rgba(59, 130, 246, 0.2);
            border-color: rgba(96, 165, 250, 0.5);
            color: #93c5fd;
            box-shadow: 0 0 8px rgba(59, 130, 246, 0.3);
        }}

        headerbar button:active {{
            background: rgba(37, 99, 235, 0.3);
            transform: scale(0.97);
        }}

        notebook {{
            background: transparent;
            border-radius: 0 0 12px 12px;
        }}

        notebook > header {{
            background: rgba(30, 41, 59, 0.9);
            border-bottom: 1px solid rgba(71, 85, 105, 0.5);
            padding: 4px 8px;
            border-radius: 8px 8px 0 0;
        }}

        notebook > header > tabs {{
            background: transparent;
        }}

        notebook > header > tabs > tab {{
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px 6px 0 0;
            padding: 6px 12px;
            margin: 0 2px;
            color: #94a3b8;
            font-weight: 500;
            font-size: 12px;
            transition: all 0.15s ease;
            min-height: 32px;
        }}

        notebook > header > tabs > tab:hover {{
            background: rgba(59, 130, 246, 0.15);
            color: #93c5fd;
            border-bottom-color: transparent;
        }}

        notebook > header > tabs > tab:checked {{
            background: linear-gradient(180deg, rgba(51, 65, 85, 0.95) 0%, rgba(30, 41, 59, 0.9) 100%);
            border-color: rgba(96, 165, 250, 0.4);
            border-bottom-color: transparent;
            color: #60a5fa;
            font-weight: 600;
            box-shadow: 0 -1px 4px rgba(59, 130, 246, 0.3);
        }}

        notebook > header > tabs > tab button {{
            background: transparent;
            border: none;
            border-radius: 4px;
            padding: 2px 6px;
            margin-left: 8px;
            opacity: 0.6;
            transition: all 0.2s;
        }}

        notebook > header > tabs > tab button:hover {{
            background: rgba(255, 0, 0, 0.1);
            opacity: 1;
        }}

        scrolledwindow {{
            background: rgba(15, 23, 42, {});
            border-radius: 0 0 8px 8px;
        }}

        scrolledwindow > undershoot {{
            background: none;
        }}

        vte-terminal, .terminal, text {{
            background: rgba(15, 23, 42, {});
            background-color: rgba(15, 23, 42, {});
            color: {};
            font-family: 'Cascadia Code', 'JetBrains Mono', 'Fira Code', monospace;
            letter-spacing: {}em;
            line-height: 1.4;
        }}
        
        .character-layer {{
            background-color: transparent;
        }}
        
        /* Modern dialog styling */
        window.dialog {{
            background: linear-gradient(180deg, rgba(30, 41, 59, 0.98) 0%, rgba(15, 23, 42, 0.98) 100%);
            border-radius: 8px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.6);
            border: 1px solid rgba(71, 85, 105, 0.5);
        }}

        .dialog-content {{
            padding: 16px;
        }}

        entry {{
            background: rgba(30, 41, 59, 0.8);
            border: 1px solid rgba(71, 85, 105, 0.5);
            border-radius: 6px;
            padding: 8px 12px;
            color: #e2e8f0;
            font-size: 12px;
            transition: all 0.15s ease;
        }}

        entry:focus {{
            border-color: #60a5fa;
            box-shadow: 0 0 0 3px rgba(96, 165, 250, 0.2);
            outline: none;
            background: rgba(30, 41, 59, 0.95);
        }}

        spinbutton {{
            background: #ffffff;
            border: 1px solid #d0d0d0;
            border-radius: 6px;
            padding: 8px 12px;
        }}

        spinbutton:focus {{
            border-color: #0078d7;
            box-shadow: 0 0 0 3px rgba(0, 120, 215, 0.1);
        }}

        button {{
            background: linear-gradient(135deg, #3b82f6 0%, #2563eb 100%);
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            color: #ffffff;
            font-weight: 600;
            font-size: 12px;
            transition: all 0.2s ease;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.3);
        }}

        button:hover {{
            background: linear-gradient(135deg, #60a5fa 0%, #3b82f6 100%);
            box-shadow: 0 2px 8px rgba(59, 130, 246, 0.5);
            transform: translateY(-1px);
        }}

        button:active {{
            background: linear-gradient(135deg, #2563eb 0%, #1d4ed8 100%);
            box-shadow: 0 1px 2px rgba(0, 0, 0, 0.3);
            transform: translateY(0);
        }}

        button.text-button {{
            background: transparent;
            border: 1px solid rgba(71, 85, 105, 0.5);
            color: #cbd5e1;
        }}

        button.text-button:hover {{
            background: rgba(51, 65, 85, 0.5);
            border-color: rgba(100, 116, 139, 0.7);
        }}

        label {{
            color: #e2e8f0;
        }}

        menubutton > button {{
            background: transparent;
            border: 1px solid transparent;
        }}
    "#,
        config.terminal.background_opacity,
        config.terminal.background_opacity,
        config.terminal.background_opacity,
        config.theme.foreground_color,
        config.terminal.letter_spacing,
    );
    
    let provider = CssProvider::new();
    provider.load_from_string(&css);
    
    gtk4::style_context_add_provider_for_display(
        &gtk4::gdk::Display::default().expect("Could not get display"),
        &provider,
        gtk4::STYLE_PROVIDER_PRIORITY_APPLICATION,
    );
}

fn with_current_vte(notebook: &Notebook, f: impl FnOnce(&vte4::Terminal)) {
    if let Some(idx) = notebook.current_page() {
        if let Some(page) = notebook.nth_page(Some(idx)) {
            if let Some(scrolled) = page.downcast_ref::<ScrolledWindow>() {
                if let Some(child) = scrolled.child() {
                    if let Ok(vte) = child.downcast::<vte4::Terminal>() {
                        f(&vte);
                    }
                }
            }
        }
    }
}

fn add_new_tab(notebook: &Notebook, config: &Config) {
    let terminal = SishTerminal::new(&config.terminal);

    // Right-click context menu
    let gesture = gtk4::GestureClick::new();
    gesture.set_button(3); // Right click

    // Keep a single popover alive; otherwise the menu can become non-interactive.
    let term_for_menu = terminal.widget().clone();
    let popover_holder: Rc<RefCell<Option<gtk4::PopoverMenu>>> = Rc::new(RefCell::new(None));
    let popover_holder_for_cb = popover_holder.clone();
    gesture.connect_pressed(move |_, _, x, y| {
        if popover_holder_for_cb.borrow().is_none() {
            let menu = gio::Menu::new();
            // These actions are defined on the window in register_actions().
            menu.append(Some("コピー"), Some("win.copy"));
            menu.append(Some("ペースト"), Some("win.paste"));
            menu.append(Some("全選択"), Some("win.select-all"));

            let popover = gtk4::PopoverMenu::from_model(Some(&menu));
            popover.set_parent(&term_for_menu);
            *popover_holder_for_cb.borrow_mut() = Some(popover);
        }

        if let Some(popover) = popover_holder_for_cb.borrow().as_ref() {
            let rect = gdk::Rectangle::new(x as i32, y as i32, 1, 1);
            popover.set_pointing_to(Some(&rect));
            popover.popup();
        }
    });
    
    terminal.widget().add_controller(gesture);

    let scrolled = ScrolledWindow::builder()
        .hexpand(true)
        .vexpand(true)
        .child(terminal.widget())
        .build();

    let tab_box = GtkBox::new(Orientation::Horizontal, 6);
    let title = Label::new(Some("Sish"));
    title.set_xalign(0.0);
    tab_box.append(&title);

    let close_btn = Button::builder()
        .icon_name("window-close-symbolic")
        .tooltip_text("タブを閉じる")
        .build();
    close_btn.add_css_class("flat");
    tab_box.append(&close_btn);

    let page_index = notebook.append_page(&scrolled, Some(&tab_box));
    notebook.set_current_page(Some(page_index));
    notebook.set_tab_reorderable(&scrolled, true);

    close_btn.connect_clicked(glib::clone!(@weak notebook, @weak scrolled => move |_| {
        if let Some(idx) = notebook.page_num(&scrolled) {
            notebook.remove_page(Some(idx));
        }
    }));
}

fn apply_wallpaper(picture: &Picture, path: Option<&str>) {
    if let Some(p) = path {
        let file = gio::File::for_path(p);
        picture.set_file(Some(&file));
    } else {
        picture.set_file(None::<&gio::File>);
    }
}

fn apply_terminal_font_to_all_tabs(notebook: &Notebook, cfg: &Config) {
    let font_desc = pango::FontDescription::from_string(
        &format!("{} {}", cfg.terminal.font_family, cfg.terminal.font_size)
    );
    let n = notebook.n_pages();
    for i in 0..n {
        if let Some(page) = notebook.nth_page(Some(i)) {
            if let Some(scrolled) = page.downcast_ref::<ScrolledWindow>() {
                if let Some(child) = scrolled.child() {
                    if let Ok(vte) = child.downcast::<vte4::Terminal>() {
                        vte.set_font(Some(&font_desc));
                    }
                }
            }
        }
    }
}

fn write_sishrc(theme: &str, verbosity: i32) -> std::io::Result<()> {
    let home = std::env::var("HOME").unwrap_or_else(|_| ".".to_string());
    let sishrc_path = PathBuf::from(home).join(".sishrc");
    
    let content = format!(
        "# Sish Configuration\n\
         export SISH_THEME=\"{}\"\n\
         export SISH_ERROR_VERBOSITY={}\n\
         export SISH_FUZZY_THRESHOLD=0.7\n",
        theme, verbosity
    );
    
    std::fs::write(sishrc_path, content)
}

fn read_sishrc() -> Option<(String, i32)> {
    let home = std::env::var("HOME").ok()?;
    let sishrc_path = PathBuf::from(home).join(".sishrc");
    
    if !sishrc_path.exists() {
        return None;
    }
    
    let content = std::fs::read_to_string(sishrc_path).ok()?;
    let mut theme = "pink".to_string();
    let mut verbosity = 1;
    
    for line in content.lines() {
        if line.starts_with("export SISH_THEME=") {
            theme = line.split('=').nth(1)?.trim().trim_matches('"').to_string();
        } else if line.starts_with("export SISH_ERROR_VERBOSITY=") {
            verbosity = line.split('=').nth(1)?.trim().parse().ok()?;
        }
    }
    
    Some((theme, verbosity))
}

fn open_preferences_dialog(window: &ApplicationWindow, notebook: &Notebook, wallpaper: &Picture, config: &Rc<RefCell<Config>>) {
    use gtk4::{Window, Entry, SpinButton};

    let cfg = config.borrow().clone();
    
    // Read existing .sishrc
    let (sish_theme_value, sish_verbosity) = read_sishrc().unwrap_or(("pink".to_string(), 1));

    let dialog = Window::builder()
        .transient_for(window)
        .modal(true)
        .title("設定")
        .default_width(520)
        .default_height(480)
        .build();

    dialog.add_css_class("dialog");

    let main_box = GtkBox::new(Orientation::Vertical, 0);
    let content = GtkBox::new(Orientation::Vertical, 12);
    content.set_margin_top(24);
    content.set_margin_bottom(24);
    content.set_margin_start(24);
    content.set_margin_end(24);

    // Wallpaper
    let wallpaper_row = GtkBox::new(Orientation::Horizontal, 8);
    let wallpaper_label = Label::new(Some("壁紙パス"));
    wallpaper_label.set_xalign(0.0);
    wallpaper_label.set_width_chars(15);
    wallpaper_row.append(&wallpaper_label);
    let wallpaper_entry = Entry::new();
    wallpaper_entry.set_hexpand(true);
    if let Some(p) = cfg.window.background_image.as_deref() {
        wallpaper_entry.set_text(p);
    }
    wallpaper_row.append(&wallpaper_entry);
    let browse_btn = Button::with_label("参照…");
    wallpaper_row.append(&browse_btn);
    content.append(&wallpaper_row);

    // Shell
    let shell_row = GtkBox::new(Orientation::Horizontal, 8);
    let shell_label = Label::new(Some("シェル"));
    shell_label.set_xalign(0.0);
    shell_label.set_width_chars(15);
    shell_row.append(&shell_label);
    let shell_entry = Entry::new();
    shell_entry.set_hexpand(true);
    shell_entry.set_text(&cfg.terminal.shell);
    shell_row.append(&shell_entry);
    content.append(&shell_row);

    // Font size
    let font_row = GtkBox::new(Orientation::Horizontal, 8);
    let font_label = Label::new(Some("フォントサイズ"));
    font_label.set_xalign(0.0);
    font_label.set_width_chars(15);
    font_row.append(&font_label);
    let font_spin = SpinButton::with_range(8.0, 32.0, 1.0);
    font_spin.set_value(cfg.terminal.font_size as f64);
    font_row.append(&font_spin);
    content.append(&font_row);

    // Sish theme
    let sish_theme_row = GtkBox::new(Orientation::Horizontal, 8);
    let sish_theme_label = Label::new(Some("Sishテーマ"));
    sish_theme_label.set_xalign(0.0);
    sish_theme_label.set_width_chars(15);
    sish_theme_row.append(&sish_theme_label);
    let themes = ["pink", "blue", "green", "purple", "orange", "rainbow"];
    let theme_list = gtk4::StringList::new(&themes);
    let sish_theme = gtk4::DropDown::new(Some(theme_list), None::<gtk4::Expression>);
    let theme_idx = themes.iter().position(|&t| t == sish_theme_value.as_str()).unwrap_or(0);
    sish_theme.set_selected(theme_idx as u32);
    sish_theme_row.append(&sish_theme);
    content.append(&sish_theme_row);

    // Error verbosity
    let verb_row = GtkBox::new(Orientation::Horizontal, 8);
    let verb_label = Label::new(Some("エラー詳細度"));
    verb_label.set_xalign(0.0);
    verb_label.set_width_chars(15);
    verb_row.append(&verb_label);
    let verb_labels = ["1: 簡潔", "2: 標準", "3: 詳細", "4: 超詳細"];
    let verb_list = gtk4::StringList::new(&verb_labels);
    let verb = gtk4::DropDown::new(Some(verb_list), None::<gtk4::Expression>);
    verb.set_selected((sish_verbosity - 1).max(0).min(3) as u32);
    verb_row.append(&verb);
    content.append(&verb_row);

    // Terminal background opacity
    let opacity_row = GtkBox::new(Orientation::Horizontal, 8);
    let opacity_label = Label::new(Some("背景透明度"));
    opacity_label.set_xalign(0.0);
    opacity_label.set_width_chars(15);
    opacity_row.append(&opacity_label);
    let opacity_scale = gtk4::Scale::with_range(gtk4::Orientation::Horizontal, 0.0, 1.0, 0.05);
    opacity_scale.set_value(cfg.terminal.background_opacity);
    opacity_scale.set_hexpand(true);
    opacity_scale.set_draw_value(true);
    opacity_scale.set_value_pos(gtk4::PositionType::Right);
    opacity_row.append(&opacity_scale);
    content.append(&opacity_row);

    // Letter spacing
    let spacing_row = GtkBox::new(Orientation::Horizontal, 8);
    let spacing_label = Label::new(Some("文字間隔"));
    spacing_label.set_xalign(0.0);
    spacing_label.set_width_chars(15);
    spacing_row.append(&spacing_label);
    let spacing_scale = gtk4::Scale::with_range(gtk4::Orientation::Horizontal, 0.0, 0.1, 0.005);
    spacing_scale.set_value(cfg.terminal.letter_spacing);
    spacing_scale.set_hexpand(true);
    spacing_scale.set_draw_value(true);
    spacing_scale.set_value_pos(gtk4::PositionType::Right);
    spacing_row.append(&spacing_scale);
    content.append(&spacing_row);

    // Character layer enabled
    let character_row = GtkBox::new(Orientation::Horizontal, 8);
    let character_label = Label::new(Some("キャラクター表示"));
    character_label.set_xalign(0.0);
    character_label.set_width_chars(15);
    character_row.append(&character_label);
    let character_check = gtk4::CheckButton::new();
    character_check.set_active(cfg.character.enabled);
    character_row.append(&character_check);
    let character_note = Label::new(Some("※再起動後に反映"));
    character_note.add_css_class("dim-label");
    character_note.set_xalign(0.0);
    character_row.append(&character_note);
    content.append(&character_row);

    // Button box
    let button_box = GtkBox::new(Orientation::Horizontal, 8);
    button_box.set_halign(gtk4::Align::End);
    button_box.set_margin_top(16);

    let cancel_btn = Button::with_label("キャンセル");
    cancel_btn.add_css_class("text-button");
    let save_btn = Button::with_label("保存");

    button_box.append(&cancel_btn);
    button_box.append(&save_btn);

    main_box.append(&content);
    main_box.append(&button_box);
    dialog.set_child(Some(&main_box));

    // Browse wallpaper
    browse_btn.connect_clicked(glib::clone!(@weak dialog, @weak wallpaper_entry => move |_| {
        let file_dialog = gtk4::FileDialog::builder()
            .title("壁紙を選択")
            .modal(true)
            .build();

        file_dialog.open(Some(&dialog), None::<&gio::Cancellable>, glib::clone!(@weak wallpaper_entry => move |result| {
            if let Ok(file) = result {
                if let Some(path) = file.path() {
                    wallpaper_entry.set_text(&path.to_string_lossy());
                }
            }
        }));
    }));

    cancel_btn.connect_clicked(glib::clone!(@weak dialog => move |_| {
        dialog.close();
    }));

    save_btn.connect_clicked(glib::clone!(@weak dialog, @weak notebook, @weak wallpaper, @strong config, @weak wallpaper_entry, @weak shell_entry, @weak font_spin, @weak sish_theme, @weak verb, @weak opacity_scale, @weak spacing_scale, @weak character_check => move |_| {
        let mut new_cfg = config.borrow().clone();

        // Wallpaper
        let wp = wallpaper_entry.text().to_string();
        new_cfg.window.background_image = if wp.trim().is_empty() { None } else { Some(wp.clone()) };

        // Terminal
        new_cfg.terminal.shell = shell_entry.text().to_string();
        new_cfg.terminal.font_size = font_spin.value() as u32;
        new_cfg.terminal.background_opacity = opacity_scale.value();
        new_cfg.terminal.letter_spacing = spacing_scale.value();
        
        // Character
        new_cfg.character.enabled = character_check.is_active();

        if let Err(e) = new_cfg.save() {
            log::error!("Failed to save config: {}", e);
        }

        let themes = ["pink", "blue", "green", "purple", "orange", "rainbow"];
        let theme = themes.get(sish_theme.selected() as usize).unwrap_or(&"pink");
        let verbosity = (verb.selected() + 1) as i32;
        if let Err(e) = write_sishrc(theme, verbosity) {
            log::error!("Failed to write ~/.sishrc: {}", e);
        }

        apply_wallpaper(&wallpaper, new_cfg.window.background_image.as_deref());
        apply_terminal_font_to_all_tabs(&notebook, &new_cfg);
        
        // Reload CSS with new settings
        load_css(&new_cfg);

        *config.borrow_mut() = new_cfg;
        dialog.close();
    }));

    dialog.present();
}

