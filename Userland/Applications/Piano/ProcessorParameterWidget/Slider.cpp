/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Slider.h"
#include "WidgetWithLabel.h"

ProcessorParameterSlider::ProcessorParameterSlider(Orientation orientation, LibDSP::ProcessorRangeParameter& parameter, RefPtr<GUI::Label> value_label)
    : Slider(orientation)
    , WidgetWithLabel(move(value_label))
    , m_parameter(parameter)
{
    // We reasonably assume the object wasn't deleted while we're constructing.
    set_range(m_parameter->min_value().raw(), m_parameter->max_value().raw());
    set_value(m_parameter->value().raw());
    set_step((m_parameter->min_value() - m_parameter->max_value()).raw() / 128);
    set_tooltip(m_parameter->name());
    m_value_label->set_text(String::formatted("{:.2f}", static_cast<double>(*m_parameter)));

    on_change = [this](auto value) {
        LibDSP::ParameterFixedPoint real_value;
        real_value.raw() = value;
        if (!m_parameter.is_null())
            m_parameter->set_value(real_value);
        if (m_value_label) {
            double value = static_cast<double>(*m_parameter);
            String label_text = String::formatted("{:.2f}", value);
            // FIXME: This is a magic value; we know that with normal font sizes, the label will disappear starting from approximately this length.
            //        Can we do this dynamically?
            if (label_text.length() > 7)
                m_value_label->set_text(String::formatted("{:.0f}", value));
            else
                m_value_label->set_text(label_text);
        }
    };
    m_parameter->add_client(*this);
}

ProcessorParameterSlider::~ProcessorParameterSlider()
{
    if (!m_parameter.is_null())
        m_parameter->remove_client(*this);
}

void ProcessorParameterSlider::value_changed(LibDSP::ParameterFixedPoint value)
{
    set_value(value.raw());
}
