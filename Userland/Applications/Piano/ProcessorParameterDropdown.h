/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Concepts.h>
#include <LibDSP/ProcessorParameter.h>
#include <LibGUI/ComboBox.h>
#include <LibGUI/ItemListModel.h>

template<typename EnumT>
requires(IsEnum<EnumT>) class ProcessorParameterDropdown : public GUI::ComboBox {
    C_OBJECT(ProcessorParameterDropdown);

public:
    ProcessorParameterDropdown(LibDSP::ProcessorEnumParameter<EnumT>& parameter, Vector<String> modes)
        : ComboBox()
        , m_parameter(parameter)
        , m_modes(move(modes))
    {
        auto& model = GUI::ItemListModel<EnumT>::create(m_modes);
    }

private:
    LibDSP::ProcessorEnumParameter<EnumT>& m_parameter;
    Vector<EnumT> m_modes;
};
