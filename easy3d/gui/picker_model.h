
/**
 * Copyright (C) 2015 by Liangliang Nan (liangliang.nan@gmail.com)
 * https://3d.bk.tudelft.nl/liangliang/
 *
 * This file is part of Easy3D. If it is useful in your research/work,
 * I would be grateful if you show your appreciation by citing it:
 * ------------------------------------------------------------------
 *      Liangliang Nan.
 *      Easy3D: a lightweight, easy-to-use, and efficient C++
 *      library for processing and rendering 3D data. 2018.
 * ------------------------------------------------------------------
 * Easy3D is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 3
 * as published by the Free Software Foundation.
 *
 * Easy3D is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EASY3D_GUI_PICKER_MODEL_H
#define EASY3D_GUI_PICKER_MODEL_H

#include <easy3d/gui/picker.h>
#include <easy3d/viewer/camera.h>

#include <vector>
#include <unordered_map>


namespace easy3d {

    class Model;

    class Drawable;

    class PickerModel : public Picker {
    public:
        PickerModel(Camera *cam);

        ~PickerModel();

        Model *pick(const std::vector<Model *> &models, int x, int y);

    private:

        // render each model of the scene with a unique color
        void draw(const std::vector<Model *> &models);

        // render the drawable with color
        void draw(Drawable *drawable, const vec4 &color);

        // restore each model's original rendering state
        void restore(const std::vector<Model *> &models);

        // restore drawable's original rendering state
        void restore(Drawable *drawable);

    private:

        struct State {
            bool lighting_;
            bool per_vertex_color_;
            vec4 default_color_;
        };
        std::unordered_map<Drawable *, State> states_;

    };

}


#endif  // EASY3D_GUI_PICKER_MODEL_H