// A notification area icon for Windows. The collector runs as a logon task with
// no console, so without this there is nothing to click: the log and the config
// are just files somewhere, and there is no sign the thing is alive.
//
// Windows only. The whole module is compiled out elsewhere, where the launchd
// service already offers `service.sh logs`.

use std::path::PathBuf;
use std::process::Command;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::Duration;

use tray_icon::menu::{Menu, MenuEvent, MenuItem, PredefinedMenuItem};
use tray_icon::{Icon, TrayIconBuilder};

use windows_sys::Win32::UI::WindowsAndMessaging::{
    DispatchMessageW, PeekMessageW, TranslateMessage, MSG, PM_REMOVE,
};

// What the tooltip says. The collector loop owns the wording; this is only the
// handover point between it and the tray thread.
static STATUS: OnceLock<Mutex<String>> = OnceLock::new();
static QUIT: AtomicBool = AtomicBool::new(false);

fn status_cell() -> &'static Mutex<String> {
    STATUS.get_or_init(|| Mutex::new("Starting".to_string()))
}

/// Called from the collector loop whenever its state changes.
pub fn set_status(text: impl Into<String>) {
    if let Ok(mut held) = status_cell().lock() {
        *held = text.into();
    }
}

/// True once the tray menu has asked the collector to stop.
pub fn quit_requested() -> bool {
    QUIT.load(Ordering::Relaxed)
}

// A small monochrome screen, which is what the board is. Drawn here rather than
// shipped as an .ico so the binary stays the only build artefact.
fn icon() -> Option<Icon> {
    const SIZE: u32 = 32;
    let mut rgba = Vec::with_capacity((SIZE * SIZE * 4) as usize);
    for y in 0..SIZE {
        for x in 0..SIZE {
            let border = x < 3 || y < 3 || x >= SIZE - 3 || y >= SIZE - 3;
            // Three bars across the screen, standing in for rows of text.
            let bar = !border
                && x >= 7
                && x < SIZE - 7
                && matches!(y, 10..=12 | 15..=17 | 20..=22);
            let (r, g, b) = if border {
                (0x20, 0x20, 0x20)
            } else if bar {
                (0x20, 0x20, 0x20)
            } else {
                (0xEC, 0xEC, 0xEC)
            };
            rgba.extend_from_slice(&[r, g, b, 0xFF]);
        }
    }
    Icon::from_rgba(rgba, SIZE, SIZE).ok()
}

// Opens a path with whatever Windows associates with it. `start` needs an empty
// first argument, which it treats as the window title, or a quoted path would
// be taken for one.
fn open(path: &PathBuf) {
    let _ = Command::new("cmd")
        .args(["/C", "start", "", &path.to_string_lossy()])
        .spawn();
}

/// Runs the icon and its message loop until the menu asks to quit. Blocks, so
/// call it on a thread of its own.
pub fn run(log: PathBuf, config: PathBuf) {
    let open_log = MenuItem::new("Open log", true, None);
    let open_config = MenuItem::new("Edit config", true, None);
    let open_folder = MenuItem::new("Open folder", true, None);
    let quit = MenuItem::new("Quit collector", true, None);
    let menu = Menu::new();
    let separator = PredefinedMenuItem::separator();
    if menu
        .append_items(&[&open_log, &open_config, &open_folder, &separator, &quit])
        .is_err()
    {
        return;
    }

    let tray = TrayIconBuilder::new()
        .with_menu(Box::new(menu))
        .with_tooltip("RLCD Monitor host")
        .with_menu_on_left_click(true);
    let tray = match icon() {
        Some(icon) => tray.with_icon(icon),
        None => tray,
    };
    let tray = match tray.build() {
        Ok(tray) => tray,
        // No tray is not a reason to stop collecting.
        Err(error) => {
            eprintln!("Tray icon unavailable: {error}");
            return;
        }
    };

    let folder = config.parent().map(PathBuf::from).unwrap_or_default();
    let menu_events = MenuEvent::receiver();
    let mut shown = String::new();
    // PeekMessage rather than GetMessage: the loop also has to poll for menu
    // events and refresh the tooltip, so it must not block on the queue.
    loop {
        unsafe {
            let mut message: MSG = std::mem::zeroed();
            while PeekMessageW(&mut message, std::ptr::null_mut(), 0, 0, PM_REMOVE) != 0 {
                let _ = TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        while let Ok(event) = menu_events.try_recv() {
            if event.id == open_log.id() {
                open(&log);
            } else if event.id == open_config.id() {
                open(&config);
            } else if event.id == open_folder.id() {
                open(&folder);
            } else if event.id == quit.id() {
                QUIT.store(true, Ordering::Relaxed);
                return;
            }
        }
        if let Ok(current) = status_cell().lock() {
            if *current != shown {
                shown = current.clone();
                let _ = tray.set_tooltip(Some(format!("RLCD Monitor host\n{shown}")));
            }
        }
        std::thread::sleep(Duration::from_millis(200));
    }
}
