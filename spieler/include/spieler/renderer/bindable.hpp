#pragma once

#include "renderer_object.hpp"

namespace Spieler
{

    class Bindable : public RendererObject
    {
    public:
        virtual void Bind() const = 0;
    };

} // namespace Spieler