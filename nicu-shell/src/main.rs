mod app;
mod config;
mod shell;

fn main() {
    if let Err(err) = app::run() {
        eprintln!("nicu: {err}");
        std::process::exit(1);
    }
}
