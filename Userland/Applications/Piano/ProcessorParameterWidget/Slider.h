/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "WidgetWithLabel.h"
#include <AK/WeakPtr.h>
#include <LibDSP/ProcessorParameter.h>
#include <LibGUI/Label.h>
#include <LibGUI/Slider.h>
#include <LibGfx/Orientation.h>

class ProcessorParameterSlider : public GUI::Slider
    , public WidgetWithLabel
    , public LibDSP::ProcessorParameterClient<LibDSP::ParameterFixedPoint> {
    C_OBJECT(ProcessorParameterSlider);

public:
    ProcessorParameterSlider(Orientation, LibDSP::ProcessorRangeParameter&, RefPtr<GUI::Label>);
    ~ProcessorParameterSlider();

    void value_changed(LibDSP::ParameterFixedPoint) override;

protected:
    WeakPtr<LibDSP::ProcessorRangeParameter> m_parameter;
};
