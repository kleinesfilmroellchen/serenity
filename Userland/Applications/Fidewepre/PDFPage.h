/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SlideObject.h"
#include <AK/StringView.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/Forward.h>
#include <LibPDF/Document.h>
#include <LibPDF/Error.h>
#include <LibPDF/Page.h>
#include <LibThreading/BackgroundAction.h>

#pragma once

// An object rendering a single PDF page.
class PDFPage : public SlideObject {
    C_OBJECT(SlideObject);

public:
    PDFPage(NonnullRefPtr<GUI::Window>, String presentation_path);
    virtual ~PDFPage() = default;

    virtual void paint(Gfx::Painter&, Gfx::FloatSize display_scale) const override;

    void set_pdf_path(String m_pdf_path);
    StringView pdf_path() const { return m_pdf_path.string(); }

    void set_page(u32 page);
    u32 get_page_number() const { return m_page_index + 1; }

private:
    PDF::PDFErrorOr<void> reload_document() const;
    void execute_rerender() const;

    String m_presentation_path;
    NonnullRefPtr<GUI::Window> m_window;
    mutable RefPtr<Threading::BackgroundAction<int>> m_render_action;

    LexicalPath m_pdf_path { ""sv };
    u32 m_page_index {};

    mutable Threading::MutexProtected<RefPtr<PDF::Document>> m_pdf_document;
    // PDF::Document holds a reference to this.
    mutable ByteBuffer m_document_bytes;
};
