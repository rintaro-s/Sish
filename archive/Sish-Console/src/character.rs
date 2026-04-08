//! Character Layer module
//!
//! Displays the Sish mascot character with emotional expressions.

use gtk4::prelude::*;
use gtk4::{DrawingArea, Align};
use cairo::Context;
use std::cell::RefCell;
use std::rc::Rc;

use crate::config::CharacterConfig;
use crate::socket::SishEvent;

/// Emotion states for the character
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Emotion {
    Happy,
    Sad,
    Confused,
    Angry,
    Thinking,
    Excited,
    Sleepy,
    Neutral,
}

impl From<&str> for Emotion {
    fn from(s: &str) -> Self {
        match s.to_lowercase().as_str() {
            "happy" => Emotion::Happy,
            "sad" => Emotion::Sad,
            "confused" => Emotion::Confused,
            "angry" => Emotion::Angry,
            "thinking" => Emotion::Thinking,
            "excited" => Emotion::Excited,
            "sleepy" => Emotion::Sleepy,
            _ => Emotion::Neutral,
        }
    }
}

/// Character Layer widget
#[derive(Clone)]
pub struct CharacterLayer {
    drawing_area: DrawingArea,
    emotion: Rc<RefCell<Emotion>>,
    config: CharacterConfig,
}

impl CharacterLayer {
    /// Create a new character layer
    pub fn new(config: &CharacterConfig) -> Self {
        let drawing_area = DrawingArea::new();
        let emotion = Rc::new(RefCell::new(Emotion::Neutral));
        
        // Set size
        drawing_area.set_size_request(config.size as i32, config.size as i32);
        
        // Set alignment based on position
        let (halign, valign) = match config.position.as_str() {
            "bottom-right" => (Align::End, Align::End),
            "bottom-left" => (Align::Start, Align::End),
            "top-right" => (Align::End, Align::Start),
            "top-left" => (Align::Start, Align::Start),
            _ => (Align::End, Align::End),
        };
        
        drawing_area.set_halign(halign);
        drawing_area.set_valign(valign);
        drawing_area.set_margin_top(20);
        drawing_area.set_margin_bottom(20);
        drawing_area.set_margin_start(20);
        drawing_area.set_margin_end(20);
        
        // Set up drawing
        let emotion_clone = emotion.clone();
        let size = config.size;
        let opacity = config.opacity;
        
        drawing_area.set_draw_func(move |_area, cr, width, height| {
            let current_emotion = *emotion_clone.borrow();
            draw_character(cr, width, height, current_emotion, size, opacity);
        });
        
        Self {
            drawing_area,
            emotion,
            config: config.clone(),
        }
    }
    
    /// Get the underlying widget
    pub fn widget(&self) -> &DrawingArea {
        &self.drawing_area
    }
    
    /// Set the character's emotion
    pub fn set_emotion(&self, emotion: Emotion) {
        *self.emotion.borrow_mut() = emotion;
        self.drawing_area.queue_draw();
    }
    
    /// Handle events from Sish shell
    pub fn handle_event(&self, event: &SishEvent) {
        match event.event_type.as_str() {
            "emotion" => {
                let emotion = Emotion::from(event.data.as_str());
                self.set_emotion(emotion);
            }
            "preexec" => {
                self.set_emotion(Emotion::Thinking);
            }
            "precmd" => {
                // Check exit status
                if event.data == "0" {
                    self.set_emotion(Emotion::Happy);
                } else {
                    self.set_emotion(Emotion::Sad);
                }
            }
            "shell_start" => {
                self.set_emotion(Emotion::Excited);
            }
            "shell_exit" => {
                self.set_emotion(Emotion::Sleepy);
            }
            _ => {}
        }
    }
}

/// Draw the character based on current emotion
fn draw_character(cr: &Context, _width: i32, _height: i32, emotion: Emotion, size: u32, opacity: f64) {
    let s = size as f64;
    let center_x = s / 2.0;
    let center_y = s / 2.0;
    
    // Set global opacity
    cr.push_group();
    
    // Draw character body (simple kawaii style)
    // Head/body circle
    cr.set_source_rgb(0.96, 0.76, 0.91); // Pink color
    cr.arc(center_x, center_y, s * 0.35, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Hair/top
    cr.set_source_rgb(0.55, 0.36, 0.51); // Dark pink/purple
    cr.arc(center_x, center_y - s * 0.15, s * 0.25, std::f64::consts::PI, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Draw face based on emotion
    match emotion {
        Emotion::Happy => {
            // Happy eyes (^_^)
            draw_happy_eyes(cr, center_x, center_y, s);
            draw_smile(cr, center_x, center_y, s);
        }
        Emotion::Sad => {
            // Sad eyes (;_;)
            draw_sad_eyes(cr, center_x, center_y, s);
            draw_frown(cr, center_x, center_y, s);
        }
        Emotion::Confused => {
            // Confused eyes (?.?)
            draw_confused_eyes(cr, center_x, center_y, s);
            draw_wavy_mouth(cr, center_x, center_y, s);
        }
        Emotion::Angry => {
            // Angry eyes (>_<)
            draw_angry_eyes(cr, center_x, center_y, s);
            draw_frown(cr, center_x, center_y, s);
        }
        Emotion::Thinking => {
            // Thinking eyes (-.-)
            draw_thinking_eyes(cr, center_x, center_y, s);
            draw_small_mouth(cr, center_x, center_y, s);
        }
        Emotion::Excited => {
            // Excited eyes (*o*)
            draw_excited_eyes(cr, center_x, center_y, s);
            draw_open_mouth(cr, center_x, center_y, s);
        }
        Emotion::Sleepy => {
            // Sleepy eyes (-_-)
            draw_sleepy_eyes(cr, center_x, center_y, s);
            draw_small_mouth(cr, center_x, center_y, s);
        }
        Emotion::Neutral => {
            // Neutral eyes (・_・)
            draw_neutral_eyes(cr, center_x, center_y, s);
            draw_neutral_mouth(cr, center_x, center_y, s);
        }
    }
    
    // Draw blush (always present)
    draw_blush(cr, center_x, center_y, s);
    
    // Apply opacity
    cr.pop_group_to_source().ok();
    cr.paint_with_alpha(opacity).ok();
}

// Helper functions for drawing facial features

fn draw_happy_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.02);
    
    // Left eye (^)
    cr.move_to(cx - s * 0.15, cy - s * 0.05);
    cr.line_to(cx - s * 0.10, cy - s * 0.10);
    cr.line_to(cx - s * 0.05, cy - s * 0.05);
    cr.stroke().ok();
    
    // Right eye (^)
    cr.move_to(cx + s * 0.05, cy - s * 0.05);
    cr.line_to(cx + s * 0.10, cy - s * 0.10);
    cr.line_to(cx + s * 0.15, cy - s * 0.05);
    cr.stroke().ok();
}

fn draw_sad_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    
    // Left eye (dot with tear)
    cr.arc(cx - s * 0.10, cy - s * 0.05, s * 0.03, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Right eye
    cr.arc(cx + s * 0.10, cy - s * 0.05, s * 0.03, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Tears (blue)
    cr.set_source_rgb(0.5, 0.7, 0.95);
    cr.arc(cx - s * 0.10, cy + s * 0.02, s * 0.02, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
}

fn draw_confused_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    
    // Left eye (?)
    cr.arc(cx - s * 0.10, cy - s * 0.05, s * 0.03, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Right eye (?)
    cr.arc(cx + s * 0.10, cy - s * 0.05, s * 0.025, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Question mark above head
    cr.set_line_width(s * 0.02);
    cr.arc(cx + s * 0.20, cy - s * 0.25, s * 0.03, std::f64::consts::PI, 2.0 * std::f64::consts::PI);
    cr.stroke().ok();
    cr.arc(cx + s * 0.20, cy - s * 0.18, s * 0.01, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
}

fn draw_angry_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.025);
    
    // Left eye (>)
    cr.move_to(cx - s * 0.15, cy - s * 0.10);
    cr.line_to(cx - s * 0.07, cy - s * 0.05);
    cr.line_to(cx - s * 0.15, cy);
    cr.stroke().ok();
    
    // Right eye (<)
    cr.move_to(cx + s * 0.07, cy - s * 0.10);
    cr.line_to(cx + s * 0.15, cy - s * 0.05);
    cr.line_to(cx + s * 0.07, cy);
    cr.stroke().ok();
}

fn draw_thinking_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.025);
    
    // Left eye (-)
    cr.move_to(cx - s * 0.13, cy - s * 0.05);
    cr.line_to(cx - s * 0.05, cy - s * 0.05);
    cr.stroke().ok();
    
    // Right eye (-)
    cr.move_to(cx + s * 0.05, cy - s * 0.05);
    cr.line_to(cx + s * 0.13, cy - s * 0.05);
    cr.stroke().ok();
    
    // Thinking dots
    cr.arc(cx + s * 0.25, cy - s * 0.15, s * 0.015, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    cr.arc(cx + s * 0.30, cy - s * 0.22, s * 0.02, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    cr.arc(cx + s * 0.35, cy - s * 0.30, s * 0.025, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
}

fn draw_excited_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    
    // Left eye (star/sparkle)
    draw_star(cr, cx - s * 0.10, cy - s * 0.05, s * 0.04);
    
    // Right eye (star/sparkle)
    draw_star(cr, cx + s * 0.10, cy - s * 0.05, s * 0.04);
}

fn draw_sleepy_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.02);
    
    // Left eye (-)
    cr.move_to(cx - s * 0.13, cy - s * 0.05);
    cr.line_to(cx - s * 0.05, cy - s * 0.05);
    cr.stroke().ok();
    
    // Right eye (-)
    cr.move_to(cx + s * 0.05, cy - s * 0.05);
    cr.line_to(cx + s * 0.13, cy - s * 0.05);
    cr.stroke().ok();
    
    // ZZZ
    cr.set_font_size(s * 0.08);
    cr.move_to(cx + s * 0.20, cy - s * 0.20);
    cr.show_text("z").ok();
    cr.move_to(cx + s * 0.25, cy - s * 0.28);
    cr.show_text("z").ok();
    cr.move_to(cx + s * 0.30, cy - s * 0.36);
    cr.show_text("Z").ok();
}

fn draw_neutral_eyes(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    
    // Left eye (dot)
    cr.arc(cx - s * 0.10, cy - s * 0.05, s * 0.03, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Right eye (dot)
    cr.arc(cx + s * 0.10, cy - s * 0.05, s * 0.03, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
}

fn draw_smile(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.02);
    cr.arc(cx, cy + s * 0.05, s * 0.08, 0.2, std::f64::consts::PI - 0.2);
    cr.stroke().ok();
}

fn draw_frown(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.02);
    cr.arc(cx, cy + s * 0.15, s * 0.06, std::f64::consts::PI + 0.3, 2.0 * std::f64::consts::PI - 0.3);
    cr.stroke().ok();
}

fn draw_wavy_mouth(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.02);
    
    cr.move_to(cx - s * 0.08, cy + s * 0.08);
    cr.curve_to(
        cx - s * 0.04, cy + s * 0.06,
        cx + s * 0.04, cy + s * 0.10,
        cx + s * 0.08, cy + s * 0.08
    );
    cr.stroke().ok();
}

fn draw_small_mouth(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.02);
    cr.move_to(cx - s * 0.04, cy + s * 0.08);
    cr.line_to(cx + s * 0.04, cy + s * 0.08);
    cr.stroke().ok();
}

fn draw_open_mouth(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.arc(cx, cy + s * 0.08, s * 0.05, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Inner pink
    cr.set_source_rgb(0.9, 0.6, 0.7);
    cr.arc(cx, cy + s * 0.08, s * 0.035, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
}

fn draw_neutral_mouth(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgb(0.2, 0.2, 0.2);
    cr.set_line_width(s * 0.02);
    cr.move_to(cx - s * 0.05, cy + s * 0.08);
    cr.line_to(cx + s * 0.05, cy + s * 0.08);
    cr.stroke().ok();
}

fn draw_blush(cr: &Context, cx: f64, cy: f64, s: f64) {
    cr.set_source_rgba(0.95, 0.6, 0.6, 0.5);
    
    // Left blush
    cr.arc(cx - s * 0.18, cy + s * 0.02, s * 0.04, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
    
    // Right blush
    cr.arc(cx + s * 0.18, cy + s * 0.02, s * 0.04, 0.0, 2.0 * std::f64::consts::PI);
    cr.fill().ok();
}

fn draw_star(cr: &Context, cx: f64, cy: f64, r: f64) {
    let points = 5;
    let inner_r = r * 0.4;
    
    cr.move_to(cx, cy - r);
    
    for i in 0..points * 2 {
        let angle = std::f64::consts::PI / 2.0 + (i as f64) * std::f64::consts::PI / (points as f64);
        let radius = if i % 2 == 0 { r } else { inner_r };
        let x = cx + radius * angle.cos();
        let y = cy - radius * angle.sin();
        cr.line_to(x, y);
    }
    
    cr.close_path();
    cr.fill().ok();
}
