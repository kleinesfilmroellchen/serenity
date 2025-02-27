/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2019-2020, William McPherson <willmcpherson2@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "KeysWidget.h"
#include <LibDSP/Keyboard.h>
#include <AK/Array.h>
#include <AK/StringView.h>
#include <LibGUI/Painter.h>

KeysWidget::KeysWidget(NonnullRefPtr<LibDSP::TrackManager> track_manager, NonnullRefPtr<LibDSP::Keyboard> keyboard)
    : m_track_manager(move(track_manager))
    , m_keyboard(move(keyboard))
{
    set_fill_with_background_color(true);
}

KeysWidget::~KeysWidget()
{
}

int KeysWidget::mouse_note() const
{
    if (m_mouse_down && m_mouse_note + m_keyboard->virtual_keyboard_octave_base() < note_count)
        return m_mouse_note; // Can be -1.
    else
        return -1;
}

void KeysWidget::set_key(i8 key, LibDSP::Keyboard::Switch switch_key)
{
    if (key == -1 || key + m_keyboard->virtual_keyboard_octave_base() >= note_count)
        return;

    if (switch_key == LibDSP::Keyboard::Switch::On) {
        ++m_key_on[key];
    } else {
        if (m_key_on[key] >= 1)
            --m_key_on[key];
    }
    VERIFY(m_key_on[key] <= 2);

    m_keyboard->set_keyboard_note_by_octave(key, switch_key);
}

bool KeysWidget::note_is_set(int note) const
{
    if (note < m_keyboard->virtual_keyboard_octave_base())
        return false;

    if (note >= m_keyboard->virtual_keyboard_octave_base() + note_count)
        return false;

    return m_key_on[note - m_keyboard->virtual_keyboard_octave_base()] != 0;
}

int KeysWidget::key_code_to_key(int key_code) const
{
    switch (key_code) {
    case Key_A:
        return 0;
    case Key_W:
        return 1;
    case Key_S:
        return 2;
    case Key_E:
        return 3;
    case Key_D:
        return 4;
    case Key_F:
        return 5;
    case Key_T:
        return 6;
    case Key_G:
        return 7;
    case Key_Y:
        return 8;
    case Key_H:
        return 9;
    case Key_U:
        return 10;
    case Key_J:
        return 11;
    case Key_K:
        return 12;
    case Key_O:
        return 13;
    case Key_L:
        return 14;
    case Key_P:
        return 15;
    case Key_Semicolon:
        return 16;
    case Key_Apostrophe:
        return 17;
    case Key_RightBracket:
        return 18;
    case Key_Return:
        return 19;
    default:
        return -1;
    }
}

constexpr int white_key_width = 24;
constexpr int black_key_width = 16;
constexpr int black_key_x_offset = black_key_width / 2;
constexpr int black_key_height = 60;

constexpr int white_key_labels_count = 12;
constexpr Array<StringView, white_key_labels_count> white_key_labels = {
    "A",
    "S",
    "D",
    "F",
    "G",
    "H",
    "J",
    "K",
    "L",
    ";",
    "\'",
    "\u23CE", // Return key symbol
};

constexpr int black_key_labels_count = 8;
constexpr Array<StringView, black_key_labels_count> black_key_labels = {
    "W",
    "E",
    "T",
    "Y",
    "U",
    "O",
    "P",
    "]",
};

constexpr int black_key_offsets[] = {
    white_key_width,
    white_key_width * 2,
    white_key_width,
    white_key_width,
    white_key_width * 2,
};

constexpr int white_key_note_accumulator[] = {
    2,
    2,
    1,
    2,
    2,
    2,
    1,
};

constexpr int black_key_note_accumulator[] = {
    2,
    3,
    2,
    2,
    3,
};

void KeysWidget::paint_event(GUI::PaintEvent& event)
{
    GUI::Painter painter(*this);
    painter.translate(frame_thickness(), frame_thickness());

    int note = 0;
    int x = 0;
    int i = 0;
    for (;;) {
        Gfx::IntRect rect(x, 0, white_key_width, frame_inner_rect().height());
        painter.fill_rect(rect, m_key_on[note] ? note_pressed_color : Color::White);
        painter.draw_rect(rect, Color::Black);
        if (i < white_key_labels_count) {
            rect.set_height(rect.height() * 1.5);
            painter.draw_text(rect, white_key_labels[i], Gfx::TextAlignment::Center, Color::Black);
        }

        note += white_key_note_accumulator[i % white_keys_per_octave];
        x += white_key_width;
        ++i;

        if (note + m_keyboard->virtual_keyboard_octave_base() >= note_count)
            break;
        if (x >= frame_inner_rect().width())
            break;
    }

    note = 1;
    x = white_key_width - black_key_x_offset;
    i = 0;
    for (;;) {
        Gfx::IntRect rect(x, 0, black_key_width, black_key_height);
        painter.fill_rect(rect, m_key_on[note] ? note_pressed_color : Color::Black);
        painter.draw_rect(rect, Color::Black);
        if (i < black_key_labels_count) {
            rect.set_height(rect.height() * 1.5);
            painter.draw_text(rect, black_key_labels[i], Gfx::TextAlignment::Center, Color::White);
        }

        note += black_key_note_accumulator[i % black_keys_per_octave];
        x += black_key_offsets[i % black_keys_per_octave];
        ++i;

        if (note + m_keyboard->virtual_keyboard_octave_base() >= note_count)
            break;
        if (x >= frame_inner_rect().width())
            break;
    }

    GUI::Frame::paint_event(event);
}

constexpr int notes_per_white_key[] = {
    1,
    3,
    5,
    6,
    8,
    10,
    12,
};

// Keep in mind that in any of these functions a note value can be out of
// bounds. Bounds checking is done in set_key().

static inline int note_from_white_keys(int white_keys)
{
    int octaves = white_keys / white_keys_per_octave;
    int remainder = white_keys % white_keys_per_octave;
    int notes_from_octaves = octaves * notes_per_octave;
    int notes_from_remainder = notes_per_white_key[remainder];
    int note = (notes_from_octaves + notes_from_remainder) - 1;
    return note;
}

int KeysWidget::note_for_event_position(const Gfx::IntPoint& a_point) const
{
    if (!frame_inner_rect().contains(a_point))
        return -1;

    auto point = a_point;
    point.translate_by(-frame_thickness(), -frame_thickness());

    int white_keys = point.x() / white_key_width;
    int note = note_from_white_keys(white_keys);

    bool black_key_on_left = note != 0 && key_pattern[(note - 1) % notes_per_octave] == Black;
    if (black_key_on_left) {
        int black_key_x = (white_keys * white_key_width) - black_key_x_offset;
        Gfx::IntRect black_key(black_key_x, 0, black_key_width, black_key_height);
        if (black_key.contains(point))
            return note - 1;
    }

    bool black_key_on_right = key_pattern[(note + 1) % notes_per_octave] == Black;
    if (black_key_on_right) {
        int black_key_x = ((white_keys + 1) * white_key_width) - black_key_x_offset;
        Gfx::IntRect black_key(black_key_x, 0, black_key_width, black_key_height);
        if (black_key.contains(point))
            return note + 1;
    }

    return note;
}

void KeysWidget::mousedown_event(GUI::MouseEvent& event)
{
    if (event.button() != GUI::MouseButton::Primary)
        return;

    m_mouse_down = true;

    m_mouse_note = note_for_event_position(event.position());

    set_key(m_mouse_note, LibDSP::Keyboard::Switch::On);
    update();
}

void KeysWidget::mouseup_event(GUI::MouseEvent& event)
{
    if (event.button() != GUI::MouseButton::Primary)
        return;

    m_mouse_down = false;

    set_key(m_mouse_note, LibDSP::Keyboard::Switch::Off);
    update();
}

void KeysWidget::mousemove_event(GUI::MouseEvent& event)
{
    if (!m_mouse_down)
        return;

    int new_mouse_note = note_for_event_position(event.position());

    if (m_mouse_note == new_mouse_note)
        return;

    set_key(m_mouse_note, LibDSP::Keyboard::Switch::Off);
    set_key(new_mouse_note, LibDSP::Keyboard::Switch::On);
    update();

    m_mouse_note = new_mouse_note;
}
