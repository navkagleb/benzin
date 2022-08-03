#include "bootstrap.hpp"

#include <spieler/core/application.hpp>
#include <spieler/core/logger.hpp>

#include <spieler/system/key_event.hpp>
#include <spieler/system/event_dispatcher.hpp>

#include "layers/test_layer.hpp"
#include "layers/tessellation_layer.hpp"
#include "layers/dynamic_indexing_layer.hpp"

namespace sandbox
{

    class MainLayer final : public spieler::Layer
    {
    public:
        bool OnAttach()
        {
            UpdateLayer();

            return true;
        }

        void OnEvent(spieler::Event& event)
        {
            spieler::EventDispatcher dispatcher{ event };

            dispatcher.Dispatch<spieler::KeyPressedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnKeyPressed));
        }

    private:
        bool OnKeyPressed(spieler::KeyPressedEvent& event)
        {
            const size_t maxLayerCount{ 3 };

            bool isTriggered{ true };

            switch (event.GetKeyCode())
            {
                case spieler::KeyCode::Q:
                {
                    m_CurrentLayerIndex = (m_CurrentLayerIndex - 1) % maxLayerCount;
                    break;
                }
                case spieler::KeyCode::E:
                {
                    m_CurrentLayerIndex = (m_CurrentLayerIndex + 1) % maxLayerCount;
                    break;
                }
                default:
                {
                    isTriggered = false;
                    break;
                }
            }

            if (isTriggered)
            {
                UpdateLayer();
            }

            SPIELER_INFO("{}", event.ToString());
            SPIELER_INFO("{}", m_CurrentLayerIndex);

            return false;
        }

        void UpdateLayer()
        {
            auto& application{ spieler::Application::GetInstance() };

            if (m_CurrentLayer)
            {
                application.GetLayerStack().Pop(m_CurrentLayer);
            }

            switch (m_CurrentLayerIndex)
            {
                case 0:
                {
                    m_CurrentLayer = application.GetLayerStack().Push<TestLayer>();
                    break;
                }
                case 1:
                {
                    m_CurrentLayer = application.GetLayerStack().Push<TessellationLayer>();
                    break;
                }
                case 2:
                {
                    m_CurrentLayer = application.GetLayerStack().Push<DynamicIndexingLayer>();
                    break;
                }
            }
        }

    private:
        size_t m_CurrentLayerIndex{ 1 };
        std::shared_ptr<spieler::Layer> m_CurrentLayer;
    };

    class Application final : public spieler::Application
    {
    public:
        Application(const spieler::Application::Config& config)
            : spieler::Application{ config }
        {
            SPIELER_ASSERT(m_LayerStack.Push<MainLayer>());
        }
    };

} // namespace sandbox

spieler::Application* spieler::CreateApplication()
{
    const spieler::Application::Config config
    {
        .Title{ "Sandbox" },
        .Width{ 1280 },
        .Height{ 720 }
    };

    return new sandbox::Application{ config };
}
