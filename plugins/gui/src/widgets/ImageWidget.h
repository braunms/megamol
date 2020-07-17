/*
 * ImageWidget.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOL_GUI_IMAGEWIDGET_INCLUDED
#define MEGAMOL_GUI_IMAGEWIDGET_INCLUDED


#include "glowl/Texture2D.hpp"

#include "FileUtils.h"
#include "GUIUtils.h"
#include "widgets/HoverToolTip.h"

#include "mmcore/misc/PngBitmapCodec.h"

#include "vislib/graphics/BitmapImage.h"
#include "vislib/sys/Log.h"


namespace megamol {
namespace gui {


/**
 * String search widget.
 */
class ImageWidget {
public:
    ImageWidget(void);

    ~ImageWidget(void) = default;

    bool IsLoaded(void) {
        if (this->tex_ptr == nullptr) return false;
        return (this->tex_ptr->getName() != 0); // OpenGL texture id
    }

    /**
     * Load texture from file.
     */
    bool LoadTextureFromFile(const std::string& filename);

    /**
     * Load texture from data.
     */
    bool LoadTextureFromData(int width, int height, float* data);

    /**
     * Draw texture as simple image.
     */
    void Widget(ImVec2 size);

    /**
     * Draw texture as button.
     */
    bool Button(const std::string& tooltip, ImVec2 size);

private:
    // VARIABLES --------------------------------------------------------------

    std::shared_ptr<glowl::Texture2D> tex_ptr;

    // Widgets
    HoverToolTip tooltip;
};


} // namespace gui
} // namespace megamol

#endif // MEGAMOL_GUI_IMAGEWIDGET_INCLUDED
