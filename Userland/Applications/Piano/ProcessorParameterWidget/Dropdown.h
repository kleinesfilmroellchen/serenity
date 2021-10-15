/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "WidgetWithLabel.h"
#include <AK/Vector.h>
#include <AK/WeakPtr.h>
#include <LibDSP/ProcessorParameter.h>
#include <LibGUI/ComboBox.h>
#include <LibGUI/ItemListModel.h>
#include <LibGUI/Label.h>
#include <LibGUI/ModelIndex.h>

template<typename EnumT>
requires(IsEnum<EnumT>) class ProcessorParameterDropdown : public GUI::ComboBox
    , public LibDSP::ProcessorParameterClient<EnumT> {
    C_OBJECT(ProcessorParameterDropdown);

public:
    ProcessorParameterDropdown(LibDSP::ProcessorEnumParameter<EnumT>& parameter, Vector<String> modes)
        : ComboBox()
        , m_parameter(parameter)
        , m_modes(move(modes))
    {
        auto model = GUI::ItemListModel<EnumT, Vector<String>>::create(m_modes);
        set_model(model);
        set_only_allow_values_from_model(true);
        set_model_column(0);
        set_selected_index(0);
        // We reasonably assume the object wasn't deleted while we're constructing.
        m_parameter->set_value(static_cast<EnumT>(0));

        on_change = [this]([[maybe_unused]] auto name, GUI::ModelIndex model_index) {
            auto value = static_cast<EnumT>(model_index.row());
            if (!m_parameter.is_null())
                m_parameter->set_value(value);
        };
        m_parameter->add_client(*this);
    }

    ~ProcessorParameterDropdown()
    {
        if (!m_parameter.is_null())
            m_parameter->remove_client(*this);
    }

    virtual void value_changed(EnumT new_value) override
    {
        set_selected_index(static_cast<int>(new_value));
    }

    // Release focus when escape is pressed
    virtual void keydown_event(GUI::KeyEvent& event) override
    {
        if (event.key() == Key_Escape) {
            if (is_focused())
                set_focus(false);
            event.accept();
        } else
            GUI::ComboBox::keydown_event(event);
    }

private:
    WeakPtr<LibDSP::ProcessorEnumParameter<EnumT>> m_parameter;
    Vector<String> m_modes;
};
