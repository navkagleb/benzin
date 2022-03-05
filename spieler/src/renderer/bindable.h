#pragma once

#include "renderer_object.h"

namespace Spieler
{

    class Bindable : public RendererObject
    {
    public:
        virtual void Bind() = 0;
    };

} // namespace Spieler