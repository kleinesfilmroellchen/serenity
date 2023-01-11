@GUI::Widget {
    fill_with_background_color: true
    layout: @GUI::VerticalBoxLayout {
        margins: [10]
        spacing: 5
    }

    @GUI::Label {
        text: "These settings control Presenter's performance."
        text_alignment: "TopLeft"
        // FIXME: Use dynamic sizing once supported.
        min_height: 50
    }

    @GUI::Widget {
        tooltip: "How many slides are cached at maximum. Set to 0 to disable caching."
        layout: @GUI::HorizontalBoxLayout {
            margins: [4]
            spacing: 5
        }

        @GUI::Label {
            text: "Cache size"
        }

        @GUI::SpinBox {
            name: "cache_size"
            min: 0
            max: 100
        }
    }

    @GUI::Widget {
        tooltip: "How many slides are pre-rendered in the background. Note that pre-rendered slides are part of the normal cache."
        layout: @GUI::HorizontalBoxLayout {
            margins: [4]
            spacing: 5
        }

        @GUI::Label {
            text: "Prerendered slides"
        }

        @GUI::SpinBox {
            name: "prerender_count"
            min: 0
            max: 100
        }
    }
}
